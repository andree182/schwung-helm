# Helm for Schwung

Polyphonic synthesizer module, quite easy to try various base synthesis principles, or quickly get good digital sounds.

Based on [Helm](https://github.com/andree182/helm) by Matt Tytel.

## Features

Main feature highlights:
* 32 voice polyphony
* Integrates into Schwung UI and covers (probably) 100% of available configuration
* Dual oscillators with cross modulation and up to 15 unison oscillators each
* Sub oscillator
* Oscillator feedback and saturation
* 12/24dB/Shelf low/band/high pass filter
* 2 monophonic and 1 polyphonic LFO
* Step sequencer
* Simple arpeggiator
* Effects: Formant filter, stutter, delay, distortion, reverb
* Almost everything can be modulated, including by polyphonic aftertouch (e.g. Move's pads)

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

The original interface features shown below was translated almost 1:1 into Move, except there are no visual representations of e.g. the waveforms and envelopes.

![alt tag](http://tytel.org/static/images/helm_screenshot.png)

## License

GPL-3.0 - See [LICENSE](LICENSE)

Based on Helm, which is also GPL-3.0 licensed.
