# Memory budget

The native build remains comfortably inside a conservative old-model hardware
budget. Measurements for the current native ELF and bank are:

- loaded ARM code and read-only data: approximately 155 KiB;
- static writable/TLS/BSS data: approximately 37 KiB;
- resident ten-sample PCM library: approximately 11.62 MiB;
- cached min/max waveforms for all ten samples: 12.5 KiB;
- NDSP stream buffers: 8 KiB;
- signed 32-bit grain/effects scratch buffer: 4 KiB;
- expanded four-line FDN storage: 187.5 KiB;
- sixteen grain voices plus the fixed de-click tail pool: less than 4 KiB;
- stereo four-stage phaser state: less than 100 bytes;
- final-output peak/clip meter state: 6 bytes;
- microphone service ring: 64 KiB;
- four-second signed PCM16 microphone capture: approximately 255.7 KiB;
- destructive-punch RAM layer: one additional copy of the active sample,
  currently at most approximately 1.65 MiB for the bundled bank;
- compact embedded RomFS bank: approximately 11.62 MiB on the `.3dsx` image.

Counting both the embedded 11.62 MiB RomFS bank image and its preloaded PCM copy
pessimistically as resident, the project's explicit allocations remain below
24 MiB before Citro2D, libctru, emulator/system services, stacks and allocator
overhead. Activating microphone punch-in raises the current bundled-bank peak by
approximately 1.97 MiB. The expanded FDN adds 150 KiB over the original native
allocation.

All current samples are loaded and CRC-checked before NDSP starts, and all
waveforms are analyzed at the same time. The bank file is then closed. This
uses roughly 9.6 MiB more heap than the former largest-sample-only policy, but
removes file I/O, allocation, CRC work, and full waveform analysis from the
live sample-switch path. That trade is intentional: uninterrupted reverb and
NDSP buffer submission are instrument-critical.

This is a build-time accounting, not a substitute for a physical-console peak
measurement. The later headroom milestone should record application and linear
heap availability on both old and new hardware while running maximum grain
density, maximum Size, Wi-Fi and microphone services together.

The RAM layer is allocated only when R is first pressed. It preserves the
active sample's exact length and sample rate, so its cost equals that sample's
PCM byte length; subsequent punch-ins reuse the allocation. A very large custom
bank can therefore require more RAM than the bundled figures above. Allocation
failure is non-destructive and is reported on screen as `RAM ALLOC FAILED`.

Milestone 7 raises the adjustable voice ceiling from four to sixteen. The
additional state is only a few hundred bytes; CPU time, not RAM, is the relevant
physical-hardware acceptance risk. The Poly field can reduce the live limit to
four (or lower) without rebuilding.
