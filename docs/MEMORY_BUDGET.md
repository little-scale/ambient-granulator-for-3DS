# Memory budget

The native build remains comfortably inside a conservative old-model hardware
budget. Measurements for the milestone-6 ELF and bank are:

- loaded ARM code and read-only data: approximately 155 KiB;
- static writable/TLS/BSS data: approximately 37 KiB;
- resident nine-sample PCM library: approximately 3.47 MiB;
- cached min/max waveforms for all nine samples: 11.25 KiB;
- NDSP stream buffers: 8 KiB;
- expanded four-line FDN storage: 187.5 KiB;
- sixteen grain voice states: less than 1 KiB;
- stereo four-stage phaser state: less than 100 bytes;
- final-output peak/clip meter state: 6 bytes;
- compact embedded RomFS bank: approximately 3.5 MiB on the `.3dsx` image.

Counting both the embedded 3.47 MiB RomFS bank image and its preloaded PCM copy
pessimistically as resident, the project's explicit allocations remain below
8 MiB before Citro2D, libctru, emulator/system services, stacks and allocator
overhead. The expanded FDN adds 150 KiB over the original native allocation.

All current samples are loaded and CRC-checked before NDSP starts, and all
waveforms are analyzed at the same time. The bank file is then closed. This
uses roughly 2.9 MiB more heap than the former largest-sample-only policy, but
removes file I/O, allocation, CRC work, and full waveform analysis from the
live sample-switch path. That trade is intentional: uninterrupted reverb and
NDSP buffer submission are instrument-critical.

This is a build-time accounting, not a substitute for a physical-console peak
measurement. The later headroom milestone should record application and linear
heap availability on both old and new hardware while running maximum grain
density, maximum Size, Wi-Fi and microphone services together.

Milestone 7 raises the adjustable voice ceiling from four to sixteen. The
additional state is only a few hundred bytes; CPU time, not RAM, is the relevant
physical-hardware acceptance risk. The Poly field can reduce the live limit to
four (or lower) without rebuilding.
