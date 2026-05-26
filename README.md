# Helm for Schwung

Hybrid synthesizer module based on [Helm](https://github.com/andree182/helm) by Matt Tytel.

## Features

+- the same as original, including modulations (editable in the Move interface):

* 32 voice polyphony
* Interactive visual interface
* Powerful modulation system with live visual feedback
* Dual oscillators with cross modulation and up to 15 unison oscillators each
* Sub oscillator with shuffle waveshaping
* Oscillator feedback and saturation for waveshaping
* 12 different waveforms
* Blending between 12 or 24dB low/band/high pass filter
* Low/Band/High Shelf filters
* 2 monophonic and 1 polyphonic LFO
* Step sequencer
* Lots of modulation sources including polyphonic aftertouch (e.g. Move's pads)
* Simple arpeggiator
* Effects: Formant filter, stutter, delay, distortion, reverb

## Prerequisites

- [Schwung](https://github.com/charlesvestal/schwung) installed on your Ableton Move
- SSH access enabled: http://move.local/development/ssh

## Install

### Via Module Store (Recommended)

1. Launch Move Everything on your Move
2. Select **Module Store** from the main menu
3. Navigate to **Sound Generators** > Helm
4. Select **Install**

### Build from Source

Requires Docker (recommended) or ARM64 cross-compiler.

```bash
git clone --recursive https://github.com/charlesvestal/move-everything-helm
cd move-anything-helm
./scripts/build.sh
./scripts/install.sh
```

## Controls

| Control | Function |
|---------|----------|
| Jog wheel | Browse presets / navigate menus |
| Knobs 1-8 | Adjust parameters for current category |

In Shadow UI / Signal Chain, parameters are organized into navigable categories...

## License

GPL-3.0 - See [LICENSE](LICENSE)

Based on Helm, which is also GPL-3.0 licensed.
