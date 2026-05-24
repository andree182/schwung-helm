/*
 * Helm DSP Plugin for Move Anything
 *
 * GPL-3.0 License
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

/* Plugin API definitions */
extern "C" {
#include <stdint.h>

#define MOVE_PLUGIN_API_VERSION 1
#define MOVE_SAMPLE_RATE 44100
#define MOVE_FRAMES_PER_BLOCK 128

typedef struct host_api_v1 {
  uint32_t api_version;
  int sample_rate;
  int frames_per_block;
  uint8_t *mapped_memory;
  int audio_out_offset;
  int audio_in_offset;
  void (*log)(const char *msg);
  int (*midi_send_internal)(const uint8_t *msg, int len);
  int (*midi_send_external)(const uint8_t *msg, int len);
} host_api_v1_t;

#define MOVE_PLUGIN_API_VERSION_2 2

typedef struct plugin_api_v2 {
  uint32_t api_version;
  void *(*create_instance)(const char *module_dir, const char *json_defaults);
  void (*destroy_instance)(void *instance);
  void (*on_midi)(void *instance, const uint8_t *msg, int len, int source);
  void (*set_param)(void *instance, const char *key, const char *val);
  int (*get_param)(void *instance, const char *key, char *buf, int buf_len);
  int (*get_error)(void *instance, char *buf, int buf_len);
  void (*render_block)(void *instance, int16_t *out_interleaved_lr, int frames);
} plugin_api_v2_t;

typedef plugin_api_v2_t *(*move_plugin_init_v2_fn)(const host_api_v1_t *host);
#define MOVE_PLUGIN_INIT_V2_SYMBOL "move_plugin_init_v2"
}

/* Helm/JUCE headers */
#include "load_save.h"
#include "midi_manager.h"
#include "synth_base.h"

/* Host API reference */
static const host_api_v1_t *g_host = nullptr;

extern "C" void plugin_log(const char *msg) {
  if (g_host && g_host->log) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[helm] %s", msg);
    g_host->log(buf);
  }
}

/* =====================================================================
 * Headless Helm instance class
 * ===================================================================== */
class HelmMoveInstance : public SynthBase {
public:
  HelmMoveInstance() : SynthBase() {}
  ~HelmMoveInstance() override {}

  const juce::CriticalSection &getCriticalSection() override { return lock_; }
  SynthGuiInterface *getGuiInterface() override { return nullptr; }

  void prepare(double sample_rate, int buffer_size) {
    engine_.setSampleRate(sample_rate);
    engine_.setBufferSize(buffer_size);
    midi_manager_->setSampleRate(sample_rate);
  }

  void process(juce::AudioSampleBuffer *buffer, juce::MidiBuffer &midi_messages,
               int num_samples) {
    processControlChanges();
    processModulationChanges();
    processMidi(midi_messages, 0, num_samples);
    processAudio(buffer, 2, num_samples, 0);
  }

private:
  juce::CriticalSection lock_;
};

/* =====================================================================
 * Instance structure
 * ===================================================================== */
typedef struct helm_instance_t {
  char module_dir[256];
  char error_msg[256];

  HelmMoveInstance *synth;

  int current_preset;
  int preset_count;
  int octave_transpose;
  float output_gain;
  char preset_name[64];

  /* Preset list */
  juce::Array<juce::File> patches;
  std::map<std::string, juce::String> save_info;

  /* Patch Categories */
  struct category_entry {
    char name[64];
    int first_idx;
  };
  std::vector<category_entry> categories;

  /* Thread-safe MIDI queue */
  juce::CriticalSection midi_lock;
  juce::MidiBuffer midi_queue;

  /* Pre-built JSON strings */
  char *ui_hierarchy_json;
  char *chain_params_json;

  helm_instance_t() {
    memset(module_dir, 0, sizeof(module_dir));
    memset(error_msg, 0, sizeof(error_msg));
    synth = nullptr;
    current_preset = 0;
    preset_count = 0;
    octave_transpose = 0;
    output_gain = 0.5f;
    memset(preset_name, 0, sizeof(preset_name));
    ui_hierarchy_json = nullptr;
    chain_params_json = nullptr;
  }
} helm_instance_t;

/* =====================================================================
 * Preset loading
 * ===================================================================== */
static void load_preset_by_index(helm_instance_t *inst, int idx) {
  if (!inst->synth || idx < 0 || idx >= inst->preset_count)
    return;

  LoadSave::loadPatchFile(inst->patches[idx], inst->synth, inst->save_info);
  inst->current_preset = idx;

  juce::String name = inst->patches[idx].getFileNameWithoutExtension();
  strncpy(inst->preset_name, name.toRawUTF8(), sizeof(inst->preset_name) - 1);
  inst->preset_name[sizeof(inst->preset_name) - 1] = '\0';

  char msg[128];
  snprintf(msg, sizeof(msg), "Loaded preset [%d]: %s", idx, inst->preset_name);
  plugin_log(msg);
}

/* =====================================================================
 * JSON builders for ui_hierarchy and chain_params
 * ===================================================================== */
static void build_ui_hierarchy(helm_instance_t *inst) {
  const int bufsize = 4096;
  inst->ui_hierarchy_json = (char *)malloc(bufsize);
  if (!inst->ui_hierarchy_json)
    return;

  snprintf(inst->ui_hierarchy_json, bufsize,
      "{"
      "\"modes\":null,"
      "\"levels\":{"
          "\"root\":{"
              "\"list_param\":\"preset\","
              "\"count_param\":\"preset_count\","
              "\"name_param\":\"preset_name\","
              "\"children\":\"main\","
              "\"knobs\":[\"octave_transpose\"],"
              "\"params\":[]"
          "},"
          "\"main\":{"
              "\"children\":null,"
              "\"knobs\":[\"octave_transpose\"],"
              "\"params\":["
                  "{\"level\":\"category_jump\",\"label\":\"Jump to Category\"}"
              "]"
          "},"
          "\"category_jump\":{"
              "\"label\":\"Jump to Category\","
              "\"items_param\":\"category_list\","
              "\"select_param\":\"jump_to_category\","
              "\"navigate_to\":\"root\","
              "\"children\":null,"
              "\"knobs\":[],"
              "\"params\":[]"
          "}"
      "}"
      "}");
}

static void build_chain_params(helm_instance_t *inst) {
  const int bufsize = 2048;
  inst->chain_params_json = (char *)malloc(bufsize);
  if (!inst->chain_params_json)
    return;

  snprintf(inst->chain_params_json, bufsize,
           "["
           "{\"key\":\"preset\",\"name\":\"Preset\",\"type\":\"int\",\"min\":0,"
           "\"max\":9999}"
           ",{\"key\":\"octave_transpose\",\"name\":\"Octave\",\"type\":"
           "\"int\",\"min\":-3,\"max\":3}"
           "]");
}

/* =====================================================================
 * Plugin API v2 Implementation
 * ===================================================================== */
static void *v2_create_instance(const char *module_dir,
                                const char *json_defaults) {
  (void)json_defaults;
  plugin_log("create_instance called");

  helm_instance_t *inst = new helm_instance_t();
  if (!inst)
    return nullptr;

  strncpy(inst->module_dir, module_dir, sizeof(inst->module_dir) - 1);
  inst->output_gain = 0.5f;
  snprintf(inst->preset_name, sizeof(inst->preset_name), "Init");
  inst->error_msg[0] = '\0';
  inst->octave_transpose = 0;

  /* Redirect JUCE HOME and config paths to writable directory on Move */
  char helm_home_path[512];
  snprintf(helm_home_path, sizeof(helm_home_path),
           "/data/UserData/schwung/helm-config");
  setenv("HOME", helm_home_path, 1);
  setenv("XDG_DATA_HOME", helm_home_path, 1);

  try {
    inst->synth = new HelmMoveInstance();
    plugin_log("HelmMoveInstance created OK");
  } catch (const std::exception &e) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Exception creating Helm instance: %s",
             e.what());
    plugin_log(msg);
    delete inst;
    return nullptr;
  } catch (...) {
    plugin_log("Unknown exception creating Helm instance");
    delete inst;
    return nullptr;
  }

  /* Configure engine for Move specs */
  inst->synth->prepare((double)MOVE_SAMPLE_RATE, MOVE_FRAMES_PER_BLOCK);

  /* Scan and load presets */
  char patches_path[512];
  snprintf(patches_path, sizeof(patches_path), "%s/helm-data/patches",
           module_dir);
  LoadSave::setCustomBankDirectory(juce::File(patches_path));

  inst->patches = LoadSave::getAllPatches();
  inst->preset_count = inst->patches.size();

  inst->categories.clear();
  std::string last_cat_name = "";
  for (int i = 0; i < inst->preset_count; i++) {
    juce::String category = inst->patches[i].getParentDirectory().getFileName();
    std::string cat_name = category.toStdString();
    if (cat_name != last_cat_name) {
      helm_instance_t::category_entry entry;
      strncpy(entry.name, cat_name.c_str(), sizeof(entry.name) - 1);
      entry.name[sizeof(entry.name) - 1] = '\0';
      entry.first_idx = i;
      inst->categories.push_back(entry);
      last_cat_name = cat_name;
    }
  }

  char msg[128];
  snprintf(msg, sizeof(msg), "Scanned %d presets and %d categories in %s",
           inst->preset_count, (int)inst->categories.size(), patches_path);
  plugin_log(msg);

  if (inst->preset_count > 0) {
    load_preset_by_index(inst, 0);
  }

  build_ui_hierarchy(inst);
  build_chain_params(inst);

  return inst;
}

static void v2_destroy_instance(void *instance) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst)
    return;

  free(inst->ui_hierarchy_json);
  free(inst->chain_params_json);
  delete inst->synth;
  delete inst;
  plugin_log("Instance destroyed");
}

static void v2_on_midi(void *instance, const uint8_t *msg, int len,
                       int source) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst || !inst->synth || len < 1)
    return;
  (void)source;

  uint8_t status = msg[0] & 0xF0;
  uint8_t channel = msg[0] & 0x0F;
  uint8_t data1 = (len > 1) ? msg[1] : 0;
  uint8_t data2 = (len > 2) ? msg[2] : 0;

  /* Octave transpose for Note On / Note Off */
  if (status == 0x90 || status == 0x80) {
    int note = data1 + inst->octave_transpose * 12;
    if (note < 0)
      note = 0;
    if (note > 127)
      note = 127;

    /* Reconstruct modified MIDI message bytes */
    uint8_t mod_msg[3] = {msg[0], (uint8_t)note, data2};
    juce::ScopedLock l(inst->midi_lock);
    inst->midi_queue.addEvent(juce::MidiMessage(mod_msg, len), 0);
  } else {
    juce::ScopedLock l(inst->midi_lock);
    inst->midi_queue.addEvent(juce::MidiMessage(msg, len), 0);
  }
}

static void v2_set_param(void *instance, const char *key, const char *val) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst || !inst->synth)
    return;

  if (strcmp(key, "preset") == 0) {
    int idx = atoi(val);
    if (idx >= 0 && idx < inst->preset_count && idx != inst->current_preset) {
      load_preset_by_index(inst, idx);
    }
    return;
  }

  if (strcmp(key, "octave_transpose") == 0) {
    inst->octave_transpose = atoi(val);
    if (inst->octave_transpose < -3)
      inst->octave_transpose = -3;
    if (inst->octave_transpose > 3)
      inst->octave_transpose = 3;
    return;
  }

  if (strcmp(key, "all_notes_off") == 0) {
    juce::ScopedLock l(inst->midi_lock);
    inst->midi_queue.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    return;
  }

  if (strcmp(key, "jump_to_category") == 0) {
    int idx = atoi(val);
    if (idx >= 0 && idx < (int)inst->categories.size()) {
      load_preset_by_index(inst, inst->categories[idx].first_idx);
    }
    return;
  }
}

static int v2_get_param(void *instance, const char *key, char *buf,
                        int buf_len) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst)
    return -1;

  if (strcmp(key, "preset") == 0)
    return snprintf(buf, buf_len, "%d", inst->current_preset);
  if (strcmp(key, "preset_count") == 0)
    return snprintf(buf, buf_len, "%d", inst->preset_count);
  if (strcmp(key, "preset_name") == 0)
    return snprintf(buf, buf_len, "%s", inst->preset_name);
  if (strcmp(key, "name") == 0)
    return snprintf(buf, buf_len, "Helm");
  if (strcmp(key, "octave_transpose") == 0)
    return snprintf(buf, buf_len, "%d", inst->octave_transpose);

  if (strcmp(key, "bank_name") == 0) {
    if (inst->current_preset >= 0 &&
        inst->current_preset < inst->preset_count) {
      juce::String category = inst->patches[inst->current_preset]
                                  .getParentDirectory()
                                  .getFileName();
      return snprintf(buf, buf_len, "%s", category.toRawUTF8());
    }
    return snprintf(buf, buf_len, "Factory Presets");
  }

  if (strcmp(key, "category_list") == 0) {
    std::string json = "[";
    for (size_t i = 0; i < inst->categories.size(); i++) {
      if (i > 0) json += ",";
      json += "{\"index\":" + std::to_string(i) + ",\"label\":\"";
      for (const char *p = inst->categories[i].name; *p; p++) {
        if (*p == '"' || *p == '\\') {
          json += '\\';
        }
        json += *p;
      }
      json += "\"}";
    }
    json += "]";
    int len = (int)json.size();
    if (len < buf_len) {
      strcpy(buf, json.c_str());
      return len;
    }
    return -1;
  }

  if (strcmp(key, "ui_hierarchy") == 0 && inst->ui_hierarchy_json) {
    int len = strlen(inst->ui_hierarchy_json);
    if (len < buf_len) {
      strcpy(buf, inst->ui_hierarchy_json);
      return len;
    }
    return -1;
  }
  if (strcmp(key, "chain_params") == 0 && inst->chain_params_json) {
    int len = strlen(inst->chain_params_json);
    if (len < buf_len) {
      strcpy(buf, inst->chain_params_json);
      return len;
    }
    return -1;
  }

  return -1;
}

static void v2_render_block(void *instance, int16_t *out_interleaved_lr,
                            int frames) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst || !inst->synth) {
    memset(out_interleaved_lr, 0, frames * 4);
    return;
  }

  /* Thread-safe retrieval of MIDI events */
  juce::MidiBuffer local_midi;
  {
    juce::ScopedLock l(inst->midi_lock);
    local_midi.addEvents(inst->midi_queue, 0, frames, 0);
    inst->midi_queue.clear();
  }

  /* Render float audio from Helm engine */
  juce::AudioSampleBuffer float_buffer(2, frames);
  float_buffer.clear();
  inst->synth->process(&float_buffer, local_midi, frames);

  /* Convert and interleave output */
  const float *left_chan = float_buffer.getReadPointer(0);
  const float *right_chan = float_buffer.getReadPointer(1);

  for (int i = 0; i < frames; i++) {
    float left = left_chan[i] * inst->output_gain;
    float right = right_chan[i] * inst->output_gain;

    int32_t l = (int32_t)(left * 32767.0f);
    int32_t r = (int32_t)(right * 32767.0f);
    if (l > 32767)
      l = 32767;
    if (l < -32768)
      l = -32768;
    if (r > 32767)
      r = 32767;
    if (r < -32768)
      r = -32768;

    out_interleaved_lr[i * 2] = (int16_t)l;
    out_interleaved_lr[i * 2 + 1] = (int16_t)r;
  }
}

static int v2_get_error(void *instance, char *buf, int buf_len) {
  helm_instance_t *inst = (helm_instance_t *)instance;
  if (!inst || inst->error_msg[0] == '\0')
    return 0;
  return snprintf(buf, buf_len, "%s", inst->error_msg);
}

/* =====================================================================
 * Plugin API v2 export
 * ===================================================================== */
static plugin_api_v2_t g_plugin_api_v2;

extern "C" plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
  g_host = host;

  memset(&g_plugin_api_v2, 0, sizeof(g_plugin_api_v2));
  g_plugin_api_v2.api_version = MOVE_PLUGIN_API_VERSION_2;
  g_plugin_api_v2.create_instance = v2_create_instance;
  g_plugin_api_v2.destroy_instance = v2_destroy_instance;
  g_plugin_api_v2.on_midi = v2_on_midi;
  g_plugin_api_v2.set_param = v2_set_param;
  g_plugin_api_v2.get_param = v2_get_param;
  g_plugin_api_v2.get_error = v2_get_error;
  g_plugin_api_v2.render_block = v2_render_block;

  return &g_plugin_api_v2;
}

/* Stub for missing juce::Colour constructor to avoid compiling full
 * juce_graphics */
namespace juce {
Colour::Colour(uint32 argb) noexcept { (void)argb; }
} // namespace juce
