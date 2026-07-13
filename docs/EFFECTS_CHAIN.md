# Stereo effects-chain contract

The native effects chain processes the granular engine's 48 kHz interleaved
stereo output before NDSP submission. Grain voices accumulate into a signed
32-bit scratch mix so polyphonic overload is still available to the effects
input rather than being hard-clamped prematurely to PCM16.

The signal order is:

```text
granular voices -> wide mix -> soft knee -> stereo phaser -> FDN wet/dry
                -> HPF -> LPF -> meter -> NDSP
```

## Phaser

The phaser is a four-stage first-order all-pass cascade per stereo channel. A
cheap triangle LFO sweeps the all-pass coefficient, with the right channel
offset by one quarter cycle for restrained stereo movement. Depth runs from
0–100%; Speed runs from 0.01–1.00 Hz and defaults to 0.10 Hz. At zero Depth the
all-pass work and LFO update are bypassed entirely.

Placing it before the FDN lets the reverb soften the moving phase colour and
means Freeze captures a stable tail rather than continually sweeping the held
reverb. The phased signal is both the direct side of the wet/dry crossfade and
the signal that excites the FDN.

## FDN reverb

The DS four-line orthogonal Hadamard topology and stereo encode/decode matrix
are retained. Delay lengths are converted from the DS 16.384 kHz clock to
equivalent elapsed times at 48 kHz, then scaled by Size. Damping's one-pole
coefficient is also normalized across sample rates rather than copied directly.
Size 0–55 retains the DS-derived curve and the default 55 is unchanged. Above
55, a quadratic extension reaches roughly 205–495 ms across the four lines at
Size 100, creating much larger spaces without sacrificing default compatibility.

Wet/Dry affects only the output crossfade. The FDN continues receiving dry
excitation at zero Wet, matching the DS behaviour, so raising Wet can reveal an
existing tail.

Feedback is adjustable from 0.0% to 99.9% in 0.1% fine steps and 1% coarse
steps. The upper limit deliberately remains just below unity for a very long
decay; Freeze is the exact unity-feedback mode.

## Freeze

Freeze has the same state semantics as the reference implementation:

- no new dry excitation enters the FDN;
- feedback is forced to unity;
- damping is bypassed;
- saved Feedback and Damp parameter values remain untouched;
- dry grains remain audible according to Wet/Dry.

L toggles the same Freeze parameter that can be selected and edited in the
grid.

## Output filters

Independent first-order state is maintained for left and right channels. HPF
and LPF run after the reverb wet/dry mix, with coefficients recalculated for
48 kHz.
The control range remains DS-compatible: HPF 0–4 kHz, LPF 200 Hz–8 kHz, with
HPF 0 and LPF 8 kHz displayed as `OFF` and bypassed. B + Left/Right edits in
10 Hz steps; B + Up/Down retains 500 Hz coarse sweeps.

## Output meter

The stereo bars and dBFS readout observe the final signed 16-bit PCM after both
filters and immediately before cache flush and NDSP submission. Each 512-frame
audio block captures independent absolute L/R peaks. The full-width stereo bars
use fast attack and a roughly one-second visual decay, while the numerical
readout reports the louder channel in dBFS.

The wide grain input and non-frozen FDN use a cheap rational soft knee above
30000 rather than a flat hard clip. Signals below the knee remain bit-exact.
If the raw grain mix, an internal FDN operation, or final PCM exceeds signed
16-bit range, the inverted `CLIP` block is held for approximately one second.
Consequently a 100%-wet reverb can now report an overloaded excitation even if
the delayed output peak itself is modest. Gain, Vol, density, reverb level, or
Feedback should be reduced when `CLIP` remains active.

## Acceptance limits

Host-rendered tests cover coefficients, dry identity, wide polyphonic sums,
soft overload behavior and telemetry, stereo phaser motion, stereo tail
generation, live sample changes through a frozen tail, Freeze excitation
blocking/unity feedback, and filter responses. Azahar can confirm interactive
sound and control changes. Feedback stability, speaker and headphone
presentation, sleep/resume, and old-model CPU headroom require a physical
2DS/3DS test.
