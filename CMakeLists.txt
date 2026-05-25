cmake_minimum_required(VERSION 3.10)
project(helm-move-plugin CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions to disable GUI/X11/OpenGL/ALSA/CURL in JUCE
add_definitions(
    -DLINUX=1
    -DNDEBUG=1
    -DJUCE_HEADLESS=1
    -DJUCE_GUI_SUPPORT=0
    -DJUCE_AUDIOPROCESSOR_NO_GUI=1
    -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    -DJUCE_STANDALONE_APPLICATION=0
    -DJucePlugin_Build_Standalone=0
    -DJucePlugin_Build_VST=0
    -DJucePlugin_Build_VST3=0
    -DJucePlugin_Build_AU=0
    -DJucePlugin_Build_RTAS=0
    -DJucePlugin_Build_AAX=0
    -DJUCE_USE_CURL=0
    -DJUCE_WEB_BROWSER=0
)

# Header search paths
include_directories(
    src/dsp
    src/dsp/helm/JuceLibraryCode
    src/dsp/helm/third_party/JUCE/modules
    src/dsp/helm/mopo/src
    src/dsp/helm/src/synthesis
    src/dsp/helm/src/common
    src/dsp/helm/concurrentqueue
)

# Find sources
file(GLOB MOPO_SOURCES "src/dsp/helm/mopo/src/*.cpp")
file(GLOB SYNTHESIS_SOURCES "src/dsp/helm/src/synthesis/*.cpp")

set(JUCE_SOURCES
    src/dsp/helm/third_party/JUCE/modules/juce_core/juce_core.cpp
    src/dsp/helm/third_party/JUCE/modules/juce_core/juce_core_CompilationTime.cpp
    src/dsp/helm/third_party/JUCE/modules/juce_events/juce_events.cpp
    src/dsp/helm/third_party/JUCE/modules/juce_audio_basics/juce_audio_basics.cpp
    src/dsp/helm/third_party/JUCE/modules/juce_data_structures/juce_data_structures.cpp
)

set(HELM_COMMON_SOURCES
    src/dsp/helm/src/common/helm_common.cpp
    src/dsp/helm/src/common/load_save.cpp
    src/dsp/helm/src/common/midi_manager.cpp
    src/dsp/helm/src/common/synth_base.cpp
)

set(PLUGIN_SOURCES
    src/dsp/helm_plugin.cpp
)

# Combine all source files
set(ALL_SOURCES
    ${MOPO_SOURCES}
    ${SYNTHESIS_SOURCES}
    ${JUCE_SOURCES}
    ${HELM_COMMON_SOURCES}
    ${PLUGIN_SOURCES}
)

# Build shared library (dsp.so)
add_library(helm-move-plugin SHARED ${ALL_SOURCES})

# Target properties to output dsp.so directly
set_target_properties(helm-move-plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "dsp"
)

# Link standard libraries
target_link_libraries(helm-move-plugin PRIVATE
    pthread
    rt
    dl
)

# Link libstdc++ and libgcc statically to avoid GLIBCXX version issues on the Move
target_link_options(helm-move-plugin PRIVATE "-static-libstdc++" "-static-libgcc")
