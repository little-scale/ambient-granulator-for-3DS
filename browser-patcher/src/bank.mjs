export const BANK_MAGIC = "NDSGRN01";
export const BANK_VERSION = 1;
export const TARGET_RATE = 16384;
export const MAX_ENTRIES = 64;
export const MAX_BANK_BYTES = 16 * 1024 * 1024;
export const ENTRY_SIZE = 64;
export const ENTRIES_OFFSET = 64;
export const DATA_OFFSET = ENTRIES_OFFSET + ENTRY_SIZE * MAX_ENTRIES;

const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8");
let fallbackId = 0;

function createId() {
  if (globalThis.crypto?.randomUUID) return globalThis.crypto.randomUUID();
  fallbackId += 1;
  return `sample-${Date.now()}-${fallbackId}`;
}

function readU32(bytes, offset) {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
    .getUint32(offset, true);
}

function writeU32(view, offset, value) {
  view.setUint32(offset, value >>> 0, true);
}

function readName(bytes, offset) {
  let end = offset;
  while (end < offset + 32 && bytes[end] !== 0) end += 1;
  return decoder.decode(bytes.subarray(offset, end));
}

function safeName(name) {
  return String(name).replace(/[^\x20-\x7e]/g, " ").trim().slice(0, 31)
    || "UNTITLED";
}

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export function makeSample({ name, sourceRate, data, origin = "LOCAL AUDIO" }) {
  if (!(data instanceof Float32Array) || data.length === 0)
    throw new Error("Decoded audio contains no samples.");
  if (!Number.isFinite(sourceRate) || sourceRate <= 0)
    throw new Error("Decoded audio has an invalid sample rate.");
  return {
    id: createId(),
    name: safeName(name),
    sourceRate,
    data,
    trimStart: 0,
    trimEnd: data.length,
    gainDb: 0,
    origin,
  };
}

export function resampleSample(sample) {
  const start = Math.max(0, Math.min(sample.data.length - 1,
    Math.round(sample.trimStart)));
  const end = Math.max(start + 1, Math.min(sample.data.length,
    Math.round(sample.trimEnd)));
  const inputLength = end - start;
  const outputLength = Math.max(1,
    Math.round(inputLength * TARGET_RATE / sample.sourceRate));
  const output = new Float32Array(outputLength);
  const ratio = sample.sourceRate / TARGET_RATE;
  const gain = Math.pow(10, sample.gainDb / 20);
  for (let index = 0; index < outputLength; index += 1) {
    const position = Math.min(inputLength - 1, index * ratio);
    const first = Math.floor(position);
    const second = Math.min(first + 1, inputLength - 1);
    const fraction = position - first;
    const value = sample.data[start + first] * (1 - fraction)
      + sample.data[start + second] * fraction;
    output[index] = Math.max(-1, Math.min(1, value * gain));
  }
  return output;
}

export function estimatedBankBytes(samples) {
  let cursor = DATA_OFFSET;
  for (const sample of samples) {
    cursor = (cursor + 31) & ~31;
    const inputLength = Math.max(1,
      Math.round(sample.trimEnd) - Math.round(sample.trimStart));
    cursor += Math.max(1,
      Math.round(inputLength * TARGET_RATE / sample.sourceRate)) * 2;
  }
  return cursor;
}

function pcmBytes(data) {
  const bytes = new Uint8Array(data.length * 2);
  const view = new DataView(bytes.buffer);
  for (let index = 0; index < data.length; index += 1) {
    const value = Math.max(-1, Math.min(1, data[index]));
    view.setInt16(index * 2,
      Math.round(value < 0 ? value * 32768 : value * 32767), true);
  }
  return bytes;
}

export function buildBank(samples, maximumBytes = MAX_BANK_BYTES) {
  if (!samples.length) throw new Error("Add at least one sample.");
  if (samples.length > MAX_ENTRIES)
    throw new Error(`A bank can contain at most ${MAX_ENTRIES} samples.`);

  const prepared = samples.map(sample => pcmBytes(resampleSample(sample)));
  const offsets = [];
  let cursor = DATA_OFFSET;
  for (const bytes of prepared) {
    cursor = (cursor + 31) & ~31;
    offsets.push(cursor);
    cursor += bytes.length;
  }
  if (cursor > maximumBytes) {
    throw new Error(`Bank exceeds the ${formatBytes(maximumBytes)} limit by ${formatBytes(cursor - maximumBytes)}.`);
  }

  const output = new Uint8Array(cursor);
  const view = new DataView(output.buffer);
  output.set(encoder.encode(BANK_MAGIC), 0);
  writeU32(view, 8, BANK_VERSION);
  writeU32(view, 12, output.length);
  writeU32(view, 16, samples.length);
  writeU32(view, 20, output.length);
  writeU32(view, 24, TARGET_RATE);
  writeU32(view, 28, ENTRY_SIZE);
  writeU32(view, 32, MAX_ENTRIES);
  writeU32(view, 36, DATA_OFFSET);

  prepared.forEach((bytes, index) => {
    const entry = ENTRIES_OFFSET + index * ENTRY_SIZE;
    output.set(encoder.encode(safeName(samples[index].name)), entry);
    writeU32(view, entry + 32, offsets[index]);
    writeU32(view, entry + 36, bytes.length);
    writeU32(view, entry + 40, crc32(bytes));
    output.set(bytes, offsets[index]);
  });
  return output;
}

export function decodeBank(input) {
  const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (bytes.length < DATA_OFFSET)
    throw new Error("File is too small to be an NDSGRN01 sample bank.");
  if (decoder.decode(bytes.subarray(0, 8)) !== BANK_MAGIC)
    throw new Error("No NDSGRN01 sample-bank signature was found.");
  const version = readU32(bytes, 8);
  const capacity = readU32(bytes, 12);
  const count = readU32(bytes, 16);
  const used = readU32(bytes, 20);
  const sampleRate = readU32(bytes, 24);
  if (version !== BANK_VERSION)
    throw new Error(`Unsupported sample-bank version ${version}.`);
  if (capacity < DATA_OFFSET || capacity > bytes.length
      || used < DATA_OFFSET || used > capacity
      || count < 1 || count > MAX_ENTRIES
      || sampleRate !== TARGET_RATE
      || readU32(bytes, 28) !== ENTRY_SIZE
      || readU32(bytes, 32) !== MAX_ENTRIES
      || readU32(bytes, 36) !== DATA_OFFSET)
    throw new Error("The sample-bank header is invalid.");

  const samples = [];
  for (let index = 0; index < count; index += 1) {
    const entry = ENTRIES_OFFSET + index * ENTRY_SIZE;
    const offset = readU32(bytes, entry + 32);
    const length = readU32(bytes, entry + 36);
    const expectedCrc = readU32(bytes, entry + 40);
    if (length < 2 || (length & 1) !== 0 || offset < DATA_OFFSET
        || offset > used || length > used - offset)
      throw new Error(`Sample ${index + 1} has an invalid bank entry.`);
    const pcm = bytes.subarray(offset, offset + length);
    if (crc32(pcm) !== expectedCrc)
      throw new Error(`Sample ${index + 1} failed its CRC check.`);
    const pcmView = new DataView(pcm.buffer, pcm.byteOffset, pcm.byteLength);
    const data = new Float32Array(length / 2);
    for (let frame = 0; frame < data.length; frame += 1) {
      const value = pcmView.getInt16(frame * 2, true);
      data[frame] = value < 0 ? value / 32768 : value / 32767;
    }
    samples.push(makeSample({
      name: readName(bytes, entry) || `SAMPLE ${index + 1}`,
      sourceRate: sampleRate,
      data,
      origin: "SAMPLE_BANK.BIN",
    }));
  }
  return { samples, capacity, used, sampleRate };
}

export function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
}
