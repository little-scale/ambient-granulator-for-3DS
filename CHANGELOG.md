# Changelog

This project uses a decimal release sequence beginning with `v0.1` and
increasing by `0.01`: `v0.11`, `v0.12`, `v0.13`, and so on.

## [v0.1] - 2026-07-14

First public native Nintendo 3DS/2DS release.

### Added

- Native Homebrew Launcher `.3dsx` application using libctru, Citro2D, and
  NDSP, with a reproducible pinned devkitARM build.
- Ten embedded, rights-cleared mono samples converted from the supplied
  originals to 48 kHz PCM16, with runtime sample selection and waveform views.
- Polyphonic granular playback with touch/A gating, B-triggered bursts,
  bipolar or forward position range, semitone and cent tuning, pitch/fine
  deviation, grain count, length, attack, release, gain, volume, pan, and pan
  divergence.
- Free-running and BPM-synchronised triggering at 1/8, 1/16, 1/32, and 1/64,
  including timing jitter.
- Stereo four-line feedback-delay-network reverb with wet/dry, feedback, size,
  damping, and excitation-blocking unity-feedback Freeze.
- Pre-reverb slow stereo phaser, post-mix HPF/LPF, output soft clipping, stereo
  metering, peak readout, and clip indication.
- Native console-microphone destructive punch-in from the current waveform
  position, with adjustable 10.5–70.0 dB gain and an always-live input meter.
- D-pad, Circle Pad, and optional New 3DS C-stick navigation and editing,
  including held repeat; Y plus Left/Right moves the waveform position.
- Offline browser patcher that opens the release `.3dsx` and exports a new
  self-contained application with a replacement embedded sample bank.

### Fixed

- Removed grain-boundary and voice-stealing discontinuities that caused clicks,
  including the single-grain double-trigger path.
- Kept reverb and Freeze tails continuous while switching samples.
- Moved audio rendering to a dedicated callback-driven thread and made UI
  status reads non-blocking, preventing visual stalls under sustained DSP load.
- Kept frozen reverb independent from new grain creation.
- Prevented pre-recording microphone-ring audio from leaking into an R-held
  punch-in.

### Known limitations

- OSC networking and camera-driven control are not included yet.
- This is a Homebrew Launcher `.3dsx` release, not a CIA package.
- DSP firmware setup is required for audio; follow the linked 3DS Hacks Guide.
- Emulator microphone routing and timing differ from physical 2DS/3DS hardware.

[v0.1]: https://github.com/little-scale/ambient-granulator-for-3DS/releases/tag/v0.1
