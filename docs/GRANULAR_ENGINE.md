# Native granular engine contract

The native engine is platform-neutral. `audio_output.c` only feeds rendered
stereo PCM to NDSP; scheduling and voice state live in `granular_engine.c` and
are exercised by host tests.

## Behaviour preserved from Nintendo DS

- A trigger requests the configured number of grains immediately.
- A held touch/A gate repeats complete bursts; gate release does not truncate
  the burst already in progress. A completed burst commits its next repeat only
  if the gate is still held when the following interval arrives, so a short
  one-grain touch does not leave a second burst armed.
- Positive Range is bipolar around the requested centre. Negative Range is
  forward-only, and zero has no spread.
- Free timing uses Interval milliseconds. Sync timing uses BPM and the selected
  1/8, 1/16, 1/32 or 1/64 subdivision.
- Jitter is a signed percentage of each base interval.
- Every launch independently chooses range, pitch deviation and pan divergence.
- Fine adds ±100 cents to the base pitch, while Fine Deviation independently
  randomizes each launch by up to ±100 cents.
- Attack and Release are percentages of the source-domain grain length.
- Every grain also receives a fixed 3 ms output-domain de-click fade at its
  start and natural end. Because this safety envelope is measured at 48 kHz,
  lowering pitch cannot stretch a source-domain edge into a longer click.
- B remains stateful: B + D-pad edits, while a B tap triggers on release.
- Markers are queued when the audio scheduler launches a grain and show its
  actual randomized start.

## Native-rate conversion

The bundled bank is 48 kHz signed 16-bit mono, while legacy 16.384 kHz
`NDSGRN01` banks remain accepted. NDSP runs at 48 kHz stereo. Grain positions
use Q16 source indices; their step combines semitone and cent-resolution ratios
with `source_rate / output_rate`, and samples are linearly interpolated. The
cent ratio is calculated only when a grain launches, not per output sample.
Timing intervals are calculated directly in 48 kHz output frames, so display
refresh never controls launches.

The milestone-7 engine provides an adjustable 1–16 voice limit, defaulting to
16. It fills idle slots first and, only when all allowed slots are occupied,
replaces the grain with the least playback time remaining. The fourfold ceiling
increase has negligible storage cost; old-model 2DS/3DS CPU headroom still
requires physical profiling under maximum-density grains and effects.

Replacement voices, forced recording stops, and voices removed by lowering
Poly continue from their next source sample in a 3 ms fade-out tail. The new
grain starts with the same 3 ms safety fade, avoiding both a discontinuity and
the doubled first sample that a hard overlap would create. Tail storage is a
fixed pool with no allocation or audio-service reset.

The engine feeds the separate native stereo FDN and output-filter chain. That
separation keeps grain scheduling testable without effects state. Hold-R
microphone recording cancels grain voices and pending bursts without resetting
the rendered clock, FDN, phaser, filters, Freeze, meter, or NDSP queue. On
release, the completed RAM layer becomes the grain source through the same
memory-only source handoff used by bank selection. OSC networking and camera
control are not connected yet.
