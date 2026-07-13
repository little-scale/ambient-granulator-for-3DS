# Local source samples

Source WAV files are kept out of Git by default. The generated mono 48 kHz
PCM16 bank at `romfs/sample_bank.bin` is the distributable application asset.

`npm run bank` from `browser-patcher/` uses this order:

1. `1.wav`
2. `2.wav`
3. `3.wav`
4. `4.wav`
5. `5.wav`
6. `6.wav`
7. `110bpm F - 01 - Hiskee Vocalpack.wav`
8. `130bpm Am - 05 - Hiskee Vocalpack.wav`
9. `sample1.wav`

Stereo input is intentionally averaged to mono. The native granular engine
spatializes each grain at runtime with Pan and Pan Deviation.
