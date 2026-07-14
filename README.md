# Ambient Granulator for 3DS

[Latest release](https://github.com/little-scale/ambient-granulator-for-3DS/releases/latest)
· [All releases](https://github.com/little-scale/ambient-granulator-for-3DS/releases)
· [Changelog](CHANGELOG.md)

Native Nintendo 3DS/2DS homebrew port of the working Nintendo DS Touch
Granulator. This is a separate project; the preserved sibling
`nds-locked-granulator ` source project and tested ROM are used only as a
behavioural and visual reference.

The Homebrew Launcher menu title is **Ambient Granulator for 3DS**. The
`3ds_granulator.3dsx` filename and `/3ds/3ds-granulator/` installation path are
retained for compatibility with existing SD-card installs and sample-bank
overrides.

## Quick start

### 1. Prepare the console

Ambient Granulator requires a Nintendo 3DS/2DS that can run `.3dsx` homebrew.
On this platform the menu is normally called **Homebrew Launcher** rather than
Homebrew Channel.

- For an unmodified console, begin with the maintained
  [3DS Hacks Guide](https://3ds.hacks.guide/) and let its
  [Get Started](https://3ds.hacks.guide/get-started.html) page select the method
  appropriate for the exact console model and system version. Do not treat this
  repository as a substitute for that guide.
- Complete the guide's
  [Finalizing Setup](https://3ds.hacks.guide/finalizing-setup.html), including
  Homebrew Launcher installation and its **RTC and DSP setup** section. The DSP
  firmware dump is required for native audio.
- If the console already has a current boot9strap/Luma3DS setup, Homebrew
  Launcher, and dumped DSP firmware, no additional modification is needed.

### 2. Download and install the application

Download `ambient-granulator-for-3DS-v0.1-sd.zip` from the
[latest release](https://github.com/little-scale/ambient-granulator-for-3DS/releases/latest).
Extract it, then merge its `3ds` directory into the root of the console SD card.
The finished layout is:

```text
/3ds/3ds-granulator/
├── 3ds_granulator.3dsx
└── 3ds_granulator.smdh
```

For a manual install, download the direct `.3dsx` asset, rename it to
`3ds_granulator.3dsx`, and use this minimal layout. The `.smdh` file is optional
because equivalent menu metadata is embedded in the application.

```text
/3ds/3ds-granulator/
└── 3ds_granulator.3dsx
```

Reinsert the SD card, open Homebrew Launcher, and select **Ambient Granulator
for 3DS**. If the top screen reports `DSP FIRM MISSING`, complete the DSP step
linked above before troubleshooting the sampler itself.

### 3. Make a self-contained custom sample build

Open
[`browser-patcher/standalone/3ds-granulator-patcher.html`](browser-patcher/standalone/3ds-granulator-patcher.html)
directly in a current desktop browser. Open `3ds_granulator.3dsx`, edit or add
samples, and choose **Download Patched .3DSX**. Keep a backup of the original,
rename the download to `3ds_granulator.3dsx`, and replace the file on the SD
card. The edited sample bank is embedded in the downloaded application; no
second bank file is required. Remove any older
`/3ds/3ds-granulator/sample_bank.bin` override so it does not take priority over
the newly embedded bank.

## Releases and versioning

Downloadable builds are published on the
[GitHub Releases page](https://github.com/little-scale/ambient-granulator-for-3DS/releases).
The root [`VERSION`](VERSION) file is the application and tag source of truth.
The first numbered release is `v0.1`; subsequent releases increase by `0.01`,
so the sequence continues `v0.11`, `v0.12`, `v0.13`, and so on. The top screen
shows that version plus the short Git commit, with `+` only on locally modified
builds. Release history is recorded in [`CHANGELOG.md`](CHANGELOG.md).

## Current status

Milestones 1–6 have an initial native baseline:

- pinned official devkitPro container toolchain;
- Homebrew Launcher `.3dsx` output;
- native Citro2D rendering on both screens;
- the release version and short Git hash on the top screen, with `+` marking a
  build made from uncommitted changes;
- touchscreen, buttons, D-pad grammar, and Circle Pad input;
- callback-driven four-buffer 48 kHz stereo PCM streaming through NDSP on a
  dedicated audio-service thread, independent of display frame rate and kept
  at main-thread priority to prevent UI starvation under sustained DSP load;
- non-blocking audio-status snapshots and marker reads keep the GUI responsive
  while the audio worker renders, with control and DSP state synchronized by
  separate short-lived locks;
- the DS-compatible `NDSGRN01` bank format with bounds and CRC validation;
- all ten source samples embedded in an 11.62 MiB RomFS bank, with an optional
  SD-card bank override;
- all ten originals downmixed and converted with a cached 32-tap
  windowed-sinc resampler to 48 kHz mono PCM16, then preloaded with their
  display waveforms before NDSP starts;
- the actual loaded sample rendered as a min/max waveform;
- an adjustable 1–16 voice granular engine with linear source interpolation;
- a 32-bit grain-to-effects mix with a low-cost soft overload knee, avoiding
  the former hard PCM16 clamp before phaser and reverb excitation;
- a single-file offline browser patcher that opens the native `.3dsx`, edits
  its embedded bank, and emits a new self-contained application;
- sample-clock burst scheduling independent of display VBlank;
- free-running timing and BPM-synchronised 1/8, 1/16, 1/32 and 1/64 divisions;
- continuous touch/A gating and discrete B-triggered bursts;
- bipolar and forward-only range, pitch and pitch deviation, timing jitter,
  grain count, length, attack, release, clip gain, output volume, pan and pan
  divergence;
- cent-resolution fine tuning and independent per-grain fine deviation;
- waveform markers emitted by actual audio-scheduled grain launches;
- memory-only live sample switching that does not interrupt frozen or decaying
  reverb tails;
- hold-R native microphone capture into a temporary RAM sample, with each take
  destructively punched in from the current waveform position while all
  unrecorded regions and earlier snippets remain intact;
- adjustable 10.5–70.0 dB console-microphone gain, with a 44.0 dB default and
  compact always-live input meter, plus fixed-point punch conversion and
  changed-region waveform analysis for old-model NDSP headroom;
- a sample-rate-normalized stereo four-line Hadamard FDN reverb with wet/dry,
  feedback, size and damping;
- true Freeze: blocked excitation, unity feedback, damping bypass, and an
  independently audible dry path;
- a lightweight four-stage stereo phaser before the reverb, with depth and a
  deliberately slow 0.01–1.00 Hz sweep;
- stereo first-order HPF and LPF after the wet/dry mix;
- a low-cost, unity-gain final soft clipper after both filters and outside the
  feedback loop, bit-exact below -2 dBFS and active during Freeze;
- post-chain stereo peak bars, a decaying dBFS readout, and a held `CLIP`
  warning covering final PCM, pre-effects grain overload, and internal FDN
  overload;
- retained internal audio diagnostics for full NDSP queue exhaustion,
  dangerously late three-buffer refills, and wholly silent grain-engine blocks
  while a grain remains active; these counters are hidden from the performance
  interface for v0.1 but remain available for later debugging;
- host-side audio regression checks and native artifact validation;
- an Azahar launch script for the emulator loop.

Azahar graphical boot is confirmed at 60 FPS on the development Mac. The first
boot correctly diagnosed missing DSP firmware as `D880A7FA`; after importing a
firmware dump from the user's own 2DS, `NDSP READY`, advancing stream buffers,
touch input, and audible sample output are confirmed.

The native engine/effects baseline is confirmed working reliably on the user's
physical Nintendo 2DS through Homebrew Launcher. Hands-on testing covers native
NDSP audio, touch and button control, Circle Pad navigation/editing, sample
selection, granular playback, reverb and Freeze, phaser, filters, and output
metering without observed hardware issues. The new 48 kHz source bank passes
host and artifact validation but still needs its own hardware listening pass.
The native microphone punch-in path passes host and build validation and is the
next physical-console acceptance pass. Sleep/resume and networking remain
separate acceptance passes.

This is now a real granular sampler with its core stereo DSP path, not a tone or
raw-sample stand-in. OSC networking remains to port; microphone punch-in,
polyphony, and quality improvements continue through real-hardware profiling.

The inspected DS invariants and the decisions based on them are recorded in
[`docs/DS_REFERENCE.md`](docs/DS_REFERENCE.md).
The temporary RAM-layer and destructive-punch behavior is specified in
[`docs/MICROPHONE_RECORDING.md`](docs/MICROPHONE_RECORDING.md).

## Toolchain

The reproducible build uses the official multi-architecture image:

```text
devkitpro/devkitarm:20260610
sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5
devkitARM r68-1
libctru 2.7.0-1
citro2d 1.7.0-1
citro3d 1.7.1-2
```

Docker Desktop is the only required host dependency for the native build. A
normal local devkitPro installation can still run `make`, but the container is
the supported reproducible path.

```sh
./scripts/setup.sh
./scripts/build.sh
```

The Homebrew Launcher artifact is `3ds_granulator.3dsx`. The `.elf`, `.map`,
and `.smdh` files are debugging/metadata outputs.

To produce an SD-ready directory:

```sh
./scripts/package-sd.sh
```

Copy `dist/3ds/3ds-granulator` into the SD card's `/3ds/` directory, then launch
3DS Granulator from Homebrew Launcher.

After committing a release version, produce the direct binary, SD-ready ZIP,
and checksum file from a clean worktree with:

```sh
./scripts/package-release.sh
```

The assets are written to `dist/release/v<version>/` using the root `VERSION`
value.

## Automated checks

```sh
./scripts/test.sh          # platform-neutral stereo synthesis checks
./scripts/verify-build.sh  # tests, build, 3DSX magic, ARM ELF, checksum
```

The host tests retain the oscillator checks and also open the real ten-sample
bank, validate its metadata and every sample CRC, analyze all waveforms, and
cover sample silence, centered playback, hard-left panning, position, linear
rate conversion, and timed trigger/release. A deterministic granular suite then
checks exact free/synced launch frames, held-gate repetition, signed range
modes, pitch ratios, stereo pan, and attack/release envelopes. The effects suite
checks delay scaling, stereo tails, Freeze excitation blocking and unity
feedback, dry-path behaviour, and both output filters. The meter suite checks
stereo peak capture, decay, full-scale detection, and clip hold. A live-switch integration
test verifies that closing the bank and swapping the grain source cannot clear a
frozen tail, and that the tail accepts new excitation after unfreezing. This
covers a destructive RAM punch-in as the replacement source. Dedicated capture
tests also cover linear and wrapped microphone-ring reads, bounded four-second
storage, exact preservation outside a punch, repeated snippets, resampling, and
end-of-sample truncation. Together these catch data, scheduler, and audio-math
regressions without requiring 3DS services.

## Sample bank and loading policy

The port deliberately reads the existing DS `NDSGRN01` format rather than
inventing a second container. Its existing header already records a sample
rate, so the native loader accepts both legacy 16.384 kHz banks and new 48 kHz
banks. The bundled bank is rebuilt from the original WAV files in the chosen
order, downmixed to mono, converted to 48 kHz with a cached 32-tap
Blackman-windowed sinc resampler, quantized to signed PCM16, and CRC-protected.

The project owner has confirmed that every sample currently included in the
bundled bank is non-copyrighted material and may be kept with the project
source. This statement applies to the present bank only; users remain
responsible for the rights to audio placed into replacement banks.

To reproduce the native bank from local source WAVs in `samples/`:

```sh
./scripts/normalize-wav-samples.mjs samples -1.0
cd browser-patcher
npm run bank
```

The normalization helper preserves each WAV's channel count, sample rate, bit
depth, duration, and non-audio RIFF chunks while setting its integer PCM sample
peak to -1.0 dBFS. The bank builder then performs its normal mono downmix and
48 kHz conversion, measures that final signal, and independently normalizes
each bundled mono sample to -1.0 dBFS immediately before PCM16 quantization.
The standalone browser patcher continues to preserve user-authored Gain values;
automatic final normalization applies only to the reproducible bundled-bank
script.

The legacy import helper remains available for behavioural comparison with the
preserved DS project:

```sh
./scripts/import-ds-sample-bank.sh
```

The resulting `romfs/sample_bank.bin` is embedded into the `.3dsx`, making the
Homebrew Launcher package self-contained. At runtime the app first checks for
this optional override:

```text
/3ds/3ds-granulator/sample_bank.bin
```

It falls back to the embedded RomFS bank when no override is present. The app
CRC-checks and preloads every sample plus every 320-column waveform before NDSP
starts, then closes the bank file. Live selection therefore performs no SD or
RomFS reads, no allocation, no CRC pass, and no full waveform scan: it only
swaps a resident PCM pointer and copies 1.25 KiB of cached display data. This
intentional departure from the DS one-sample policy prevents buffer starvation
while preserving reverb state. The current ten-sample library uses about
11.62 MiB of heap PCM. Bundled source PCM is signed 16-bit mono at 48 kHz, while
legacy 16.384 kHz replacement banks remain supported. Each grain uses a
source/output-rate-correct Q16 step and linear interpolation in the native
48 kHz stereo NDSP stream. Runtime Pan and Pan Deviation spatialize the mono
grain voices independently.

## Standalone browser sample-bank patcher

[`browser-patcher/standalone/3ds-granulator-patcher.html`](browser-patcher/standalone/3ds-granulator-patcher.html)
is a self-contained offline `.3dsx` editor. Open it directly from Finder or
another file browser; it does not need a local server, installation, account,
upload, or network connection.

It opens `3ds_granulator.3dsx`, validates its RomFS layout, extracts the embedded
16.384 or 48 kHz `NDSGRN01` bank, and allows up to 64 samples to be previewed,
renamed, trimmed, gain-adjusted, reordered, removed, or added. Exported audio is
downmixed and converted with the same cached 32-tap windowed-sinc resampler to
48 kHz signed mono PCM16. The editor replaces the RomFS bank and downloads a
new self-contained `.3dsx`; no separate sample bank is required on the SD card.
Copy the patched application to:

```text
/3ds/3ds-granulator/3ds_granulator-patched.3dsx
```

The existing SD-card `sample_bank.bin` override remains supported for development
and recovery, but is not part of the normal browser-patcher workflow. The
patcher uses a conservative 16 MiB capacity limit and processes audio only
inside the browser. Its authoring and test instructions are in
[`browser-patcher/README.md`](browser-patcher/README.md).

Azahar is the practical interactive emulator loop:

```sh
./scripts/run-emulator.sh
```

The script finds a standard macOS Azahar installation, an older Citra install,
or the executable named by `AZAHAR_BIN`. No emulator is bundled into the
project. Emulator automation is limited to host tests and build inspection for
now because current macOS Azahar is primarily an interactive GUI application.

### Enabling NDSP in Azahar

libctru must load the console DSP component before it can use NDSP. A direct
`.3dsx` launch in Azahar does not provide the Homebrew Launcher `hb:ndsp` handle,
so Azahar's virtual SD must contain `/3ds/dspfirm.cdc`. Without it, the app
continues as a visual/input test and displays:

```text
ERR D880A7FA
DSP FIRM MISSING
```

Obtain this file only from your own 2DS/3DS. On a Luma3DS console, open Rosalina
with L + D-pad Down + Select, choose **Miscellaneous options**, then **Dump DSP
firmware**. With the console SD card mounted on the Mac, import the resulting
file into Azahar:

```sh
./scripts/install-emulator-dsp.sh /Volumes/YOUR_SD/3ds/dspfirm.cdc
./scripts/run-emulator.sh
```

The helper only copies the supplied local file into Azahar's virtual SD. DSP
firmware is console-derived, ignored by Git, and is never bundled or downloaded
by this project.

## Granular controls

- Touch/drag the lower waveform to set position and continuously repeat complete
  grain bursts. Releasing touch lets the current burst finish.
- Tap B to trigger one configured grain burst at the current position.
- Hold A for continuous grain bursts without touching the screen.
- Plain D-pad moves around the two-column parameter grid.
- The Circle Pad and, on New 3DS models, C-stick also move around the parameter
  grid, with a dead zone and hysteresis to prevent accidental steps near centre.
- Holding D-pad, Circle Pad, or C-stick navigation repeats after 250 ms at 20
  fields/second, accelerating after one second.
- Hold Y and use D-pad, Circle Pad, or C-stick Left/Right to move the current
  waveform position one display column at a time, with the same held-repeat
  acceleration.
- Hold B, then use D-pad, Circle Pad, or C-stick Left/Right for fine edits or
  Up/Down for coarse edits. The inputs use the same held-repeat acceleration.
  Holding the direction repeats after 250 ms at 20 changes/second, accelerating
  to 60 changes/second after one second.
- Fine tunes the source by ±100 cents. F Dev adds an independent random
  deviation of 0–100 cents to every grain, in addition to semitone P Dev.
- Phase controls the pre-reverb phaser depth. P Spd covers 0.01–1.00 Hz; the
  default depth is 100%.
- The performance-oriented space defaults are REV 100%, Feedback 90.0%, Size
  100%, and Damp 5%; X restores these values along with the other defaults.
- BPM defaults to 96 and Pan Deviation defaults to 100%.
- Edit Sample with B + D-pad to load any of the ten bank entries and redraw
  the waveform.
- Hold R to record from the console microphone. Recording begins at the current
  waveform position and stops when R is released, or automatically at four
  seconds. The take destructively replaces only that span in a temporary RAM
  copy; the rest of the waveform and earlier punch-ins remain intact. The top
  screen shows `REC`, its start position, elapsed time, and a compact `IN` meter.
- Mic Gain covers 10.5–70.0 dB in 0.5 dB steps and defaults to 44.0 dB. Set it
  before holding R; the `IN` meter continuously shows the post-gain microphone
  level, whether or not a take is being recorded.
- After the first successful take the Source field reads `RAM`. Move the
  waveform position and hold R again to place another snippet in the same RAM
  sample. Editing Sample loads a bank entry and leaves RAM mode. RAM recordings
  are temporary and are lost when the app exits; very short R taps are ignored.
- Poly sets the maximum simultaneous grains from 1 to 16. It defaults to 16;
  lower it if physical old-model hardware profiling exposes audio underruns.
- Circle Pad/C-stick live-pan control has been replaced by grid navigation; Pan
  remains directly editable in the Voice section.
- L toggles functional FDN Freeze without changing the saved Feedback or Damp
  values.
- X resets the hardware-test parameters.
- Start exits cleanly to Homebrew Launcher.

The top screen retains the DS/snesdj black-on-white grid, 5x7 tracker font, and
inverted selected values. Parameters are grouped as Grain, Clock, Source,
Voice, Space, and Output; persistent help text is omitted. Two full-width
post-filter stereo meters occupy the bottom of the screen, with peak dBFS and a
held `CLIP` block in Output. A compact logarithmic microphone input meter sits
above them. The interface has a one-row top inset, and the
current bank sample name is shown untruncated below the peak indication and
above the meters. The lower screen remains black with a
white waveform, solid position, dashed range guides, and short grain-position
trails.

## Real hardware versus emulation

- The 2DS uses the same native 3DS software path and 320x240 touch display; it
  does not need DS compatibility mode for this build.
- NDSP requires the console's DSP service/firmware to be available. If it is
  missing, the application remains usable as a visual/input test and reports
  the exact result plus `DSP FIRM MISSING` instead of crashing.
- Emulator success does not prove speaker polarity, headphone routing, exact
  NDSP buffering latency, touch calibration, Home/sleep/resume behaviour, Wi-Fi
  timing, microphone behaviour, or old-model CPU headroom. Those need a 2DS/3DS
  hardware pass.
- Microphone availability and host-input routing vary between emulator builds.
  A successful Azahar recording is useful, but the console microphone, acoustic
  speaker pickup, four-second auto-stop, and uninterrupted effects tail must be
  accepted on physical hardware. Headphones reduce re-recording of the 3DS
  speakers during punch-in.
- The emulator may add input and audio latency. Granular scheduler acceptance
  tests will therefore use rendered PCM and real-hardware listening as separate
  gates rather than treating emulator timing as authoritative.
- Camera work is intentionally out of scope until the sampler, DSP, and OSC
  milestones are stable, and will require physical-camera testing.

## Port roadmap

1. **Complete:** reproducible native toolchain and documented build.
2. **Complete baseline:** two-screen, input, touch, Circle Pad, and stereo NDSP
   `.3dsx` hardware test.
3. **Complete baseline:** host audio tests, artifact validation, and interactive
   emulator launcher. Real-hardware acceptance remains manual by design.
4. **Complete baseline:** legacy and native-rate bank loading, CRC validation,
   compact RomFS bank, SD override policy, real waveform rendering, and native
   48 kHz audition.
5. **Complete baseline:** audio-clock granular scheduler with dry DS
   control parity, 48 kHz conversion, interpolation, and audio-origin markers.
   Granular playback is confirmed in Azahar.
6. **Complete baseline:** sample-rate-normalized stereo FDN, true Freeze,
   wet/dry, feedback, size, damping, and stereo HPF/LPF. Azahar listening
   acceptance is pending; real-hardware stability remains a separate gate.
7. **In progress:** adjustable polyphony now raises the engine from the DS
   baseline of four voices to 16, and the bundled originals now use high-quality
   offline conversion to 48 kHz. Native hold-R microphone recording now provides
   positional destructive punch-in to a temporary RAM sample. Runtime
   interpolation quality, DSP headroom measurement, and physical old-model
   microphone acceptance remain.
8. Port non-blocking UDP OSC on port 9000 using modern 3DS networking.
9. Investigate camera-driven control only after the audio instrument is stable.
