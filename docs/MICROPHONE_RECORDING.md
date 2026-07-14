# Native microphone destructive punch-in

Hold R to capture signed 16-bit mono audio from the 3DS microphone. Release R
to commit the take. Capture stops and commits automatically after four seconds.
The libctru `MICU_SAMPLE_RATE_32730` setting is treated as 32,728 Hz for the
sample-rate conversion. Hardware gain is set to 67, corresponding to 44 dB and
the strongest legacy DS microphone-gain setting; input clamping is enabled.

## RAM sample model

The first recording after selecting a bank sample creates an exact mutable copy
of that sample: same PCM, sample count, and sample rate. The current waveform
position is converted to a source-sample index using the same 0–319 mapping as
the granular engine. On release, microphone PCM is linearly resampled to the
RAM sample's rate and destructively overwrites from that index.

The RAM sample never changes length. A take that reaches the end is truncated;
there is no wraparound. Samples before and after the written span remain
bit-for-bit unchanged. Later R takes reuse the same RAM layer, so separate
snippets can coexist and an overlapping take replaces only the overlap. This is
destructive punch-in, not overdub mixing.

The Source field and header show `RAM` after a successful commit, while the
sample-name line retains the underlying bank name with a `RAM` prefix. Editing
the Sample parameter selects a resident bank sample and leaves RAM mode. RAM
audio is intentionally temporary and is freed when the application exits.

## Live audio contract

Recording does not reset NDSP or the effects chain. At R-down, active grain
voices and pending bursts are cancelled and new touch/A/B excitation is blocked
for the take. Reverb, Freeze, phaser, filters, meter state, and stream buffers
continue running. Before both the initial clone and the commit scan, any
finished NDSP buffers are refilled to maximize queue headroom.

Commit conversion uses one fixed-point phase increment rather than a 64-bit
division for every destination sample. Waveform analysis is restricted to the
display columns touched by the punch. Both choices keep release-time work out
of the NDSP queue deadline on old-model hardware.

The microphone service uses an aligned 64 KiB shared ring, drained once per
display frame into a bounded four-second capture buffer. R taps shorter than
512 samples (about 16 ms) do not modify the RAM sample. Service, allocation, and
capture errors leave the current playable source unchanged and are reported on
the top screen.

## Acceptance

Portable tests cover wrapped ring reads, capacity stops, exact region
preservation, repeated punch-ins, rate conversion, and end truncation. The
native source compiles against the pinned libctru microphone API. Physical
2DS/3DS testing remains authoritative for microphone gain, acoustic speaker
pickup, service behavior through Home/sleep, and old-model buffer headroom.
