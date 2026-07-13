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
];

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

const bank = buildBank(samples);
const verification = decodeBank(bank);
if (verification.sampleRate !== TARGET_RATE
    || verification.samples.length !== order.length)
  throw new Error("Generated bank did not pass its round-trip verification.");
await writeFile(outputFile, bank);
const digest = createHash("sha256").update(bank).digest("hex");
console.log(`${outputFile}: ${verification.samples.length} samples, ${TARGET_RATE} Hz mono PCM16, ${formatBytes(bank.length)}`);
console.log(`sha256 ${digest}`);
