# 3DS Granulator standalone sample-bank patcher

This is a single-file, offline browser editor for the native 3DS Granulator
sample bank. It does not upload audio, call an API, require an account, or need
a local web server.

Open `standalone/3ds-granulator-patcher.html` directly in a current browser.
The patcher can:

- start with local audio or open an existing `NDSGRN01` `sample_bank.bin`;
- decode multiple browser-supported audio files and downmix them to mono;
- preview, rename, trim, gain-adjust, reorder, and remove samples;
- downmix and convert to signed PCM16 mono at 48 kHz with a cached 32-tap
  Blackman-windowed sinc resampler;
- open legacy 16.384 kHz banks and upgrade them to 48 kHz on export;
- CRC-check opened samples and CRC-protect every exported sample;
- display the 16 MiB conservative capacity and 64-entry limit;
- download a compact `sample_bank.bin` containing only used data.

Copy the result to this exact path on the 2DS/3DS SD card:

```text
/3ds/3ds-granulator/sample_bank.bin
```

The native app prefers this override to its embedded bank and preloads it before
starting NDSP, so later sample selection remains interruption-free.

## Rebuilding the single file

The finished HTML is committed/generated in `standalone/`. Rebuilding and
testing the authoring sources requires only Node.js; there are no packages to
install:

```sh
npm test
npm run build
```

To rebuild the app's bundled bank from the ordered WAV files in the repository's
local `samples/` directory:

```sh
npm run bank
```

The source WAVs stay local by default; the generated, rights-cleared
`romfs/sample_bank.bin` is the distributable app asset.
