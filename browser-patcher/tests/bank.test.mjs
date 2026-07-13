import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  BANK_MAGIC,
  MAX_BANK_BYTES,
  TARGET_RATE,
  buildBank,
  decodeBank,
  estimatedBankBytes,
} from "../src/bank.mjs";

const realBank = new Uint8Array(readFileSync(
  new URL("../../romfs/sample_bank.bin", import.meta.url)));

test("opens and exactly rebuilds the real nine-sample bank", () => {
  const decoded = decodeBank(realBank);
  assert.equal(decoded.samples.length, 9);
  assert.equal(decoded.sampleRate, TARGET_RATE);
  assert.equal(decoded.samples[0].name, "1");
  assert.equal(decoded.samples[8].name, "sample1");
  assert.equal(estimatedBankBytes(decoded.samples), realBank.length);

  const rebuilt = buildBank(decoded.samples);
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
  assert.ok(output.length < realBank.length);
  const reopened = decodeBank(output);
  assert.equal(reopened.samples[0].name, decoded.samples[0].name);
  assert.equal(reopened.samples[1].name, "ROUND TRIP TEST");
  assert.ok(Math.abs(reopened.samples[1].data.length
    - first.data.length / 2) < 2);
});

test("rejects invalid and oversized banks", () => {
  assert.throws(() => decodeBank(new Uint8Array(5000)), /signature/);
  const decoded = decodeBank(realBank);
  assert.throws(() => buildBank(decoded.samples, 5000), /exceeds/);
  assert.ok(realBank.length < MAX_BANK_BYTES);
});
