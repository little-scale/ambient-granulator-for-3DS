# DS reference contract

The source project at `/Users/a1106632/Documents/nds-locked-granulator ` was
inspected before choosing the 3DS architecture. Its trailing space is
intentional. The source tree and ROM remain untouched.

## Behaviour that must survive the port

- Touch is a gate as well as a position control. A held stylus repeats complete
  bursts, dragging updates the centre, and release lets the current burst end.
- B is stateful: a tap triggers, while B held before a D-pad press edits the
  selected value. There is no ambiguous simultaneous-button timing window.
- Positive Range is bipolar (`+-N`); negative Range is forward-only; zero has
  no spread.
- Grain launches are scheduled by the audio clock, including fractional BPM
  relationships and timing jitter. Display VBlank is never the timing source.
- Each launch independently chooses start, pitch deviation, and pan divergence.
- Attack and release are percentages of the grain duration.
- Freeze blocks reverb excitation, sets feedback to unity, bypasses damping,
  and does not alter the saved Feedback or Damp values. Dry grains remain
  audible according to the wet/dry mix.
- HPF and LPF are stereo output stages after the wet/dry crossfade.
- Grain markers show actual randomized starts, not the requested centre.

## Visual and input reference

The DS renderer uses a custom uppercase 5x7 font in 8x8 cells, a white settings
screen, black text, black selected cells with white glyphs, two parameter
columns, and a black waveform screen. The waveform has a white solid position,
two-pixel dashed range boundaries, and four-pixel grain marks that invert only
the waveform amplitude. The 3DS hardware test reuses the font data and the same
selection grammar rather than substituting a proportional UI.

The related `snesdj` project was also inspected. Its documented rule—"the
button already held selects what the next press means"—matches the DS program's
input implementation and is treated as an interaction invariant.

## DS implementation facts relevant to the port

- 16.384 kHz signed 16-bit mono samples in a versioned `NDSGRN01` bank.
- Four software grain voices with Q16 playback positions and a fixed pitch
  lookup table.
- A fixed-point four-line Hadamard FDN with orthogonal stereo encode/decode.
- First-order per-channel output filters.
- Audio-clock scheduling inside the Maxmod stream callback.
- Only the selected ROM sample is resident; the bank index remains in memory.
- DS microphone recordings are temporary, four seconds maximum, 16.384 kHz,
  and normalized for known hardware/emulator PCM representation differences.
- Non-blocking UDP OSC on port 9000, capped to eight packets per display frame.

## Native 3DS decisions made now

- libctru/NDSP owns platform audio; the synthesis code remains independent of
  the graphics/input loop.
- The native test runs at 48 kHz stereo. Ported DSP coefficients and timing will
  therefore be recalculated from the runtime sample rate instead of copying DS
  constants.
- Citro2D provides the two render targets, but the visual language remains a
  code-owned bitmap grid. This avoids coupling the UI to a font asset.
- Audio generation is buffered independently of VBlank. Host tests exercise the
  platform-neutral generator without an emulator.
- The existing `NDSGRN01` container remains the compatibility format. Its fixed
  16 MiB DS-ROM reservation is compacted to the used length for 3DS RomFS, with
  PCM and entry CRCs unchanged. An SD bank at
  `/3ds/3ds-granulator/sample_bank.bin` overrides the embedded bank.
- The native port preloads all nine samples and cached waveforms before NDSP
  starts, then closes the bank file. This intentionally differs from the DS
  one-sample policy so live sample changes cannot starve audio buffers or
  interrupt a frozen reverb. Source PCM stays signed 16-bit mono at 16.384 kHz
  and is linearly resampled into the native 48 kHz stereo stream; later
  scheduler and DSP coefficients use runtime sample rates.
- The dry four-voice scheduler is ported at 48 kHz with the DS burst,
  signed-range, pitch, timing, envelope, gain and pan semantics. Launch timing
  and visible markers originate in the audio renderer rather than VBlank.
