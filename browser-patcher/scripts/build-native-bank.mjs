import { createHash } from "node:crypto";
import { readFile, writeFile } from "node:fs/promises";
import { basename, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  TARGET_RATE,
  buildBank,
  decodeBank,
  formatBytes,
  makeSample,
  resampleSample,
} from "../src/bank.mjs";
import { decodePcmWav } from "../src/wav.mjs";

const patcherDirectory = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repository = resolve(patcherDirectory, "..");
const sourceDirectory = resolve(process.argv[2] ?? resolve(repository, "samples"));
const outputFile = resolve(process.argv[3]
  ?? resolve(repository, "romfs/sample_bank.bin"));
const order = [
  "1.wav",
  "2.wav",
  "3.wav",
  "4.wav",
  "5.wav",
  "6.wav",
  "110bpm F - 01 - Hiskee Vocalpack.wav",
  "130bpm Am - 05 - Hiskee Vocalpack.wav",
  "sample1.wav",
  "piano.wav",
];
const TARGET_PEAK_DBFS = -1.0;
const TARGET_PEAK = 10 ** (TARGET_PEAK_DBFS / 20);

const samples = [];
for (const filename of order) {
  const input = new Uint8Array(await readFile(resolve(sourceDirectory, filename)));
  const decoded = decodePcmWav(input);
  samples.push(makeSample({
    name: basename(filename, ".wav"),
    sourceRate: decoded.sourceRate,
    data: decoded.data,
    origin: filename,
  }));
  const difference = decoded.stereoDifferenceDb === null ? "MONO"
    : `${decoded.stereoDifferenceDb.toFixed(1)} DB L-R`;
  console.log(`${filename}: ${decoded.sourceRate} Hz, ${decoded.bitsPerSample}-bit, ${decoded.channels} ch -> mono; ${difference}`);
}

for (const sample of samples) {
  const preview = resampleSample(sample, TARGET_RATE);
  let peak = 0;
  for (const value of preview) peak = Math.max(peak, Math.abs(value));
  if (peak === 0) {
    console.log(`${sample.origin}: silent after conversion; normalization skipped`);
    continue;
  }
  const adjustmentDb = 20 * Math.log10(TARGET_PEAK / peak);
  sample.gainDb += adjustmentDb;
  console.log(`${sample.origin}: final mono normalization ${adjustmentDb >= 0 ? "+" : ""}${adjustmentDb.toFixed(2)} dB -> ${TARGET_PEAK_DBFS.toFixed(1)} dBFS`);
}

const bank = buildBank(samples);
const verification = decodeBank(bank);
if (verification.sampleRate !== TARGET_RATE
    || verification.samples.length !== order.length)
  throw new Error("Generated bank did not pass its round-trip verification.");
await writeFile(outputFile, bank);
const digest = createHash("sha256").update(bank).digest("hex");
console.log(`${outputFile}: ${verification.samples.length} samples, ${TARGET_RATE} Hz mono PCM16, ${formatBytes(bank.length)}`);
console.log(`sha256 ${digest}`);
