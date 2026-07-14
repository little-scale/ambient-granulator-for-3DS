#!/usr/bin/env node

import { readdir, readFile, rename, writeFile } from "node:fs/promises";
import { basename, resolve } from "node:path";

const directory = resolve(process.argv[2] ?? "samples");
const targetDb = Number(process.argv[3] ?? "-1.0");
if (!Number.isFinite(targetDb) || targetDb > 0 || targetDb < -60)
  throw new Error("Target peak must be between -60 and 0 dBFS.");

function fourCc(bytes, offset) {
  return String.fromCharCode(bytes[offset], bytes[offset + 1],
    bytes[offset + 2], bytes[offset + 3]);
}

function readInt24(view, offset) {
  let value = view.getUint8(offset)
    | (view.getUint8(offset + 1) << 8)
    | (view.getUint8(offset + 2) << 16);
  if (value & 0x800000) value -= 0x1000000;
  return value;
}

function writeInt24(view, offset, value) {
  if (value < 0) value += 0x1000000;
  view.setUint8(offset, value & 0xff);
  view.setUint8(offset + 1, (value >> 8) & 0xff);
  view.setUint8(offset + 2, (value >> 16) & 0xff);
}

function parseWav(bytes) {
  if (bytes.length < 44 || fourCc(bytes, 0) !== "RIFF"
      || fourCc(bytes, 8) !== "WAVE")
    throw new Error("not a RIFF/WAVE file");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let format = null;
  const dataChunks = [];
  for (let offset = 12; offset + 8 <= bytes.length;) {
    const id = fourCc(bytes, offset);
    const length = view.getUint32(offset + 4, true);
    const payload = offset + 8;
    if (payload + length > bytes.length)
      throw new Error(`chunk ${id} extends beyond the file`);
    if (id === "fmt ") {
      if (length < 16) throw new Error("incomplete format chunk");
      let encoding = view.getUint16(payload, true);
      if (encoding === 0xfffe && length >= 40)
        encoding = view.getUint16(payload + 24, true);
      format = {
        encoding,
        channels: view.getUint16(payload + 2, true),
        sampleRate: view.getUint32(payload + 4, true),
        blockAlign: view.getUint16(payload + 12, true),
        bitsPerSample: view.getUint16(payload + 14, true),
      };
    } else if (id === "data") {
      dataChunks.push({ offset: payload, length });
    }
    offset = payload + length + (length & 1);
  }
  if (!format || dataChunks.length === 0)
    throw new Error("missing format or audio data");
  if (format.encoding !== 1 || format.channels < 1
      || ![16, 24].includes(format.bitsPerSample))
    throw new Error("only 16-bit or 24-bit integer PCM is supported");
  const bytesPerSample = format.bitsPerSample / 8;
  if (format.blockAlign < format.channels * bytesPerSample)
    throw new Error("invalid block alignment");
  return { view, format, dataChunks, bytesPerSample };
}

function eachSample(parsed, callback) {
  const { view, format, dataChunks, bytesPerSample } = parsed;
  for (const chunk of dataChunks) {
    const frames = Math.floor(chunk.length / format.blockAlign);
    for (let frame = 0; frame < frames; frame++) {
      const base = chunk.offset + frame * format.blockAlign;
      for (let channel = 0; channel < format.channels; channel++) {
        const offset = base + channel * bytesPerSample;
        callback(view, offset, format.bitsPerSample);
      }
    }
  }
}

function sampleValue(view, offset, bits) {
  return bits === 16 ? view.getInt16(offset, true) : readInt24(view, offset);
}

function writeSample(view, offset, bits, value) {
  if (bits === 16) view.setInt16(offset, value, true);
  else writeInt24(view, offset, value);
}

function dbfs(peak, fullScale) {
  return peak === 0 ? "-INF" : (20 * Math.log10(peak / fullScale)).toFixed(2);
}

const filenames = (await readdir(directory))
  .filter((name) => name.toLowerCase().endsWith(".wav"))
  .sort((left, right) => left.localeCompare(right, undefined,
    { numeric: true }));
if (filenames.length === 0)
  throw new Error(`No WAV files found in ${directory}`);

for (const filename of filenames) {
  const path = resolve(directory, filename);
  const bytes = new Uint8Array(await readFile(path));
  const parsed = parseWav(bytes);
  const fullScale = parsed.format.bitsPerSample === 16 ? 32768 : 8388608;
  const positiveMaximum = fullScale - 1;
  const target = Math.floor(positiveMaximum * 10 ** (targetDb / 20));
  let peak = 0;
  eachSample(parsed, (view, offset, bits) => {
    peak = Math.max(peak, Math.abs(sampleValue(view, offset, bits)));
  });
  if (peak === 0) {
    console.log(`${filename}: silent; unchanged`);
    continue;
  }
  const scale = target / peak;
  let normalizedPeak = 0;
  eachSample(parsed, (view, offset, bits) => {
    const input = sampleValue(view, offset, bits);
    const scaled = Math.max(-target, Math.min(target, Math.round(input * scale)));
    writeSample(view, offset, bits, scaled);
    normalizedPeak = Math.max(normalizedPeak, Math.abs(scaled));
  });
  const temporary = `${path}.normalizing`;
  await writeFile(temporary, bytes);
  await rename(temporary, path);
  console.log(`${basename(path)}: ${parsed.format.sampleRate} Hz, `
    + `${parsed.format.bitsPerSample}-bit, ${parsed.format.channels} ch, `
    + `${dbfs(peak, fullScale)} -> ${dbfs(normalizedPeak, fullScale)} dBFS, `
    + `${scale.toFixed(4)}x`);
}
