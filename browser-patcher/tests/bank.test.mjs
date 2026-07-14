import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  BANK_MAGIC,
  LEGACY_RATE,
  MAX_BANK_BYTES,
  TARGET_RATE,
  buildBank,
  decodeBank,
  estimatedBankBytes,
} from "../src/bank.mjs";

const realBank = new Uint8Array(readFileSync(
  new URL("../../romfs/sample_bank.bin", import.meta.url)));

test("opens and exactly rebuilds the real ten-sample bank", () => {
  const decoded = decodeBank(realBank);
  assert.equal(decoded.samples.length, 10);
  assert.ok([16384, TARGET_RATE].includes(decoded.sampleRate));
  assert.equal(decoded.samples[0].name, "1");
  assert.equal(decoded.samples[8].name, "sample1");
  assert.equal(decoded.samples[9].name, "piano");
  assert.equal(estimatedBankBytes(decoded.samples, decoded.sampleRate),
    realBank.length);

  const rebuilt = buildBank(decoded.samples, MAX_BANK_BYTES,
    decoded.sampleRate);
  assert.equal(new TextDecoder().decode(rebuilt.subarray(0, 8)), BANK_MAGIC);
  assert.deepEqual(rebuilt, realBank);
});

test("trim, gain, rename, reorder, and compact output survive round-trip", () => {
  const decoded = decodeBank(realBank);
  const first = decoded.samples[0];
  first.name = "ROUND TRIP TEST";
  first.trimStart = Math.floor(first.data.length / 4);
  first.trimEnd = Math.floor(first.data.length * 3 / 4);
  first.gainDb = -3;
  [decoded.samples[0], decoded.samples[1]]
    = [decoded.samples[1], decoded.samples[0]];

  const output = buildBank(decoded.samples);
  assert.ok(output.length < MAX_BANK_BYTES);
  const reopened = decodeBank(output);
  assert.equal(reopened.sampleRate, TARGET_RATE);
  assert.equal(reopened.samples[0].name, decoded.samples[0].name);
  assert.equal(reopened.samples[1].name, "ROUND TRIP TEST");
  const expectedFrames = first.data.length / 2
    * TARGET_RATE / first.sourceRate;
  assert.ok(Math.abs(reopened.samples[1].data.length
    - expectedFrames) < 2);
});

test("opens a legacy-rate bank and upgrades it to native rate", () => {
  const decoded = decodeBank(realBank);
  const legacy = buildBank(decoded.samples.slice(0, 1), MAX_BANK_BYTES,
    LEGACY_RATE);
  const reopenedLegacy = decodeBank(legacy);
  assert.equal(reopenedLegacy.sampleRate, LEGACY_RATE);

  const upgraded = decodeBank(buildBank(reopenedLegacy.samples));
  assert.equal(upgraded.sampleRate, TARGET_RATE);
  assert.equal(upgraded.samples.length, 1);
  assert.ok(Math.abs(upgraded.samples[0].data.length / TARGET_RATE
    - reopenedLegacy.samples[0].data.length / LEGACY_RATE) < 1 / LEGACY_RATE);
});

test("rejects invalid and oversized banks", () => {
  assert.throws(() => decodeBank(new Uint8Array(5000)), /signature/);
  const decoded = decodeBank(realBank);
  assert.throws(() => buildBank(decoded.samples, 5000), /exceeds/);
  assert.ok(realBank.length < MAX_BANK_BYTES);
});
