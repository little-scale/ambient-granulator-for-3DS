# CC0 sample pack

SPDX-License-Identifier: CC0-1.0

The repository owner affirms that they own the following original recordings
and, by publishing this notice, dedicates them to the public domain under
[CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/):

- `01 - piano.wav`
- `03 - piano.wav`
- `05 - piano.wav`
- `07 - piano.wav`
- `18 - piano.wav`

This dedication also covers the audio content derived from these recordings in
`romfs/sample_bank.bin` and in distributed application builds. The CC0
dedication does not apply to the project's software source code, documentation,
names, logos, or third-party components.

The complete CC0 1.0 legal code is included in [`LICENSE`](LICENSE).

To rebuild the embedded 48 kHz mono PCM16 bank from these source WAVs:

```sh
cd browser-patcher
npm run bank
```
