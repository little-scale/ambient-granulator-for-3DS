import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import test from "node:test";

import { buildBank, decodeBank } from "../src/bank.mjs";
import { extract3dsxBank, patch3dsxBank } from "../src/three-dsx.mjs";

const realBank = new Uint8Array(readFileSync(
  new URL("../../romfs/sample_bank.bin", import.meta.url)));

function writeU16(bytes, offset, value) {
  new DataView(bytes.buffer).setUint16(offset, value, true);
}

function writeU32(bytes, offset, value) {
  new DataView(bytes.buffer).setUint32(offset, value >>> 0, true);
}

function writeU64(bytes, offset, value) {
  writeU32(bytes, offset, value);
  writeU32(bytes, offset + 4, Math.floor(value / 0x100000000));
}

function make3dsx(bank) {
  const romfs = 0x80;
  const data = 0x98;
  const output = new Uint8Array(romfs + data + bank.length);
  output.set(new TextEncoder().encode("3DSX"), 0);
  writeU16(output, 4, 0x2c);
  writeU32(output, 0x28, romfs);

  writeU32(output, romfs, 0x28);
  writeU32(output, romfs + 0x04, 0x28);
  writeU32(output, romfs + 0x08, 0x0c);
  writeU32(output, romfs + 0x0c, 0x34);
  writeU32(output, romfs + 0x10, 0x18);
  writeU32(output, romfs + 0x14, 0x4c);
  writeU32(output, romfs + 0x18, 0x0c);
  writeU32(output, romfs + 0x1c, 0x58);
  writeU32(output, romfs + 0x20, 0x40);
  writeU32(output, romfs + 0x24, data);

  const root = romfs + 0x34;
  writeU32(output, root, 0);
  writeU32(output, root + 4, 0xffffffff);
  writeU32(output, root + 8, 0xffffffff);
  writeU32(output, root + 12, 0);
  writeU32(output, root + 16, 0xffffffff);
  writeU32(output, root + 20, 0);

  const file = romfs + 0x58;
  const name = "sample_bank.bin";
  writeU32(output, file, 0);
  writeU32(output, file + 4, 0xffffffff);
  writeU64(output, file + 8, 0);
  writeU64(output, file + 16, bank.length);
  writeU32(output, file + 24, 0xffffffff);
  writeU32(output, file + 28, name.length * 2);
  for (let index = 0; index < name.length; index += 1)
    writeU16(output, file + 32 + index * 2, name.charCodeAt(index));
  output.set(bank, romfs + data);
  return output;
}

test("extracts and resizes an embedded 3DSX sample bank", () => {
  const fixture = make3dsx(realBank);
  const extracted = extract3dsxBank(fixture);
  assert.deepEqual(extracted.bank, realBank);
  assert.equal(extracted.bankOffset, 0x118);

  const decoded = decodeBank(realBank);
  const replacement = buildBank(decoded.samples.slice(0, 1));
  const patched = patch3dsxBank(fixture, replacement);
  assert.equal(patched.length, extracted.bankOffset + replacement.length);
  assert.deepEqual(extract3dsxBank(patched).bank, replacement);
  assert.equal(decodeBank(extract3dsxBank(patched).bank).samples.length, 1);
  assert.deepEqual(patched.subarray(0, 0x80), fixture.subarray(0, 0x80));
});

test("rejects files that cannot be safely patched", () => {
  assert.throws(() => extract3dsxBank(new Uint8Array(256)), /not a Nintendo/);
  const withoutRomfs = new Uint8Array(256);
  withoutRomfs.set(new TextEncoder().encode("3DSX"));
  writeU16(withoutRomfs, 4, 0x2c);
  assert.throws(() => extract3dsxBank(withoutRomfs), /readable RomFS/);

  const fixture = make3dsx(realBank);
  const withTrailingData = new Uint8Array(fixture.length + 1);
  withTrailingData.set(fixture);
  assert.throws(() => patch3dsxBank(withTrailingData, realBank),
    /cannot be safely resized/);
  assert.throws(() => patch3dsxBank(fixture, new Uint8Array(5000)),
    /not NDSGRN01/);
});

const native3dsxUrl = new URL("../../3ds_granulator.3dsx", import.meta.url);
test("extracts and patches the current native build when present", {
  skip: !existsSync(native3dsxUrl),
}, () => {
  const native3dsx = new Uint8Array(readFileSync(native3dsxUrl));
  const extracted = extract3dsxBank(native3dsx);
  assert.deepEqual(extracted.bank, realBank);

  const decoded = decodeBank(extracted.bank);
  const replacement = buildBank(decoded.samples.slice(0, 2));
  const patched = patch3dsxBank(native3dsx, replacement);
  const reopened = extract3dsxBank(patched);
  assert.deepEqual(reopened.bank, replacement);
  assert.equal(decodeBank(reopened.bank).samples.length, 2);
});
