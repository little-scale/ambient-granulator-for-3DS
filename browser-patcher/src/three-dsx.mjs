const THREEDSX_MAGIC = "3DSX";
const THREEDSX_MINIMUM_HEADER_SIZE = 0x2c;
const THREEDSX_ROMFS_OFFSET_FIELD = 0x28;
const ROMFS_MINIMUM_HEADER_SIZE = 0x28;
const ROMFS_NO_ENTRY = 0xffffffff;
const EMBEDDED_BANK_NAME = "sample_bank.bin";
const embeddedBankMagic = new TextEncoder().encode("NDSGRN01");

function read3dsxU16(bytes, offset) {
  if (offset < 0 || offset + 2 > bytes.length)
    throw new Error("The .3dsx structure is truncated.");
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
    .getUint16(offset, true);
}

function read3dsxU32(bytes, offset) {
  if (offset < 0 || offset + 4 > bytes.length)
    throw new Error("The .3dsx structure is truncated.");
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
    .getUint32(offset, true);
}

function read3dsxU64(bytes, offset) {
  const low = read3dsxU32(bytes, offset);
  const high = read3dsxU32(bytes, offset + 4);
  if (high > 0x1fffff)
    throw new Error("The embedded RomFS uses unsupported 64-bit offsets.");
  return high * 0x100000000 + low;
}

function write3dsxU64(bytes, offset, value) {
  if (!Number.isSafeInteger(value) || value < 0)
    throw new Error("The replacement bank size is invalid.");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  view.setUint32(offset, value >>> 0, true);
  view.setUint32(offset + 4, Math.floor(value / 0x100000000), true);
}

function has3dsxAscii(bytes, offset, text) {
  if (offset < 0 || offset + text.length > bytes.length) return false;
  for (let index = 0; index < text.length; index += 1) {
    if (bytes[offset + index] !== text.charCodeAt(index)) return false;
  }
  return true;
}

function decodeRomfsName(bytes, offset, byteLength) {
  if ((byteLength & 1) !== 0 || offset < 0
      || offset + byteLength > bytes.length)
    throw new Error("The embedded RomFS filename table is invalid.");
  let name = "";
  for (let cursor = offset; cursor < offset + byteLength; cursor += 2)
    name += String.fromCharCode(bytes[cursor] | (bytes[cursor + 1] << 8));
  return name;
}

function locateEmbeddedBank(input) {
  const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (bytes.length < THREEDSX_MINIMUM_HEADER_SIZE
      || !has3dsxAscii(bytes, 0, THREEDSX_MAGIC))
    throw new Error("This is not a Nintendo 3DS .3dsx application.");
  if (read3dsxU16(bytes, 4) < THREEDSX_MINIMUM_HEADER_SIZE)
    throw new Error("The .3dsx header is unsupported.");

  const romfsOffset = read3dsxU32(bytes, THREEDSX_ROMFS_OFFSET_FIELD);
  if (romfsOffset < THREEDSX_MINIMUM_HEADER_SIZE
      || romfsOffset + ROMFS_MINIMUM_HEADER_SIZE > bytes.length)
    throw new Error("This .3dsx does not contain a readable RomFS.");

  const romfsHeaderSize = read3dsxU32(bytes, romfsOffset);
  const directoryMetadataOffset = read3dsxU32(bytes, romfsOffset + 0x0c);
  const directoryMetadataSize = read3dsxU32(bytes, romfsOffset + 0x10);
  const fileMetadataOffset = read3dsxU32(bytes, romfsOffset + 0x1c);
  const fileMetadataSize = read3dsxU32(bytes, romfsOffset + 0x20);
  const fileDataOffset = read3dsxU32(bytes, romfsOffset + 0x24);
  if (romfsHeaderSize < ROMFS_MINIMUM_HEADER_SIZE
      || directoryMetadataSize < 24 || fileMetadataSize < 32
      || directoryMetadataOffset + directoryMetadataSize > fileDataOffset
      || fileMetadataOffset + fileMetadataSize > fileDataOffset
      || romfsOffset + fileDataOffset > bytes.length)
    throw new Error("The embedded RomFS header is invalid.");

  const directoryMetadata = romfsOffset + directoryMetadataOffset;
  let entryOffset = read3dsxU32(bytes, directoryMetadata + 12);
  const visited = new Set();
  while (entryOffset !== ROMFS_NO_ENTRY) {
    if (visited.has(entryOffset) || entryOffset + 32 > fileMetadataSize)
      throw new Error("The embedded RomFS file table is invalid.");
    visited.add(entryOffset);
    const entry = romfsOffset + fileMetadataOffset + entryOffset;
    const sibling = read3dsxU32(bytes, entry + 4);
    const dataOffset = read3dsxU64(bytes, entry + 8);
    const dataSize = read3dsxU64(bytes, entry + 16);
    const nameLength = read3dsxU32(bytes, entry + 28);
    if (entryOffset + 32 + nameLength > fileMetadataSize)
      throw new Error("The embedded RomFS file table is truncated.");
    const name = decodeRomfsName(bytes, entry + 32, nameLength);
    if (name === EMBEDDED_BANK_NAME) {
      const bankOffset = romfsOffset + fileDataOffset + dataOffset;
      const bankEnd = bankOffset + dataSize;
      if (!Number.isSafeInteger(bankOffset) || !Number.isSafeInteger(bankEnd)
          || bankOffset < romfsOffset + fileDataOffset
          || bankEnd > bytes.length)
        throw new Error("The embedded sample bank lies outside the .3dsx.");
      if (bankEnd !== bytes.length)
        throw new Error("This .3dsx RomFS layout cannot be safely resized.");
      for (let index = 0; index < embeddedBankMagic.length; index += 1) {
        if (bytes[bankOffset + index] !== embeddedBankMagic[index])
          throw new Error("The embedded sample_bank.bin is not NDSGRN01.");
      }
      return {
        bytes,
        bankOffset,
        bankSize: dataSize,
        bankSizeField: entry + 16,
      };
    }
    entryOffset = sibling;
  }
  throw new Error("No embedded sample_bank.bin was found in this .3dsx.");
}

export function extract3dsxBank(input) {
  const located = locateEmbeddedBank(input);
  return {
    bank: located.bytes.slice(
      located.bankOffset, located.bankOffset + located.bankSize),
    bankOffset: located.bankOffset,
    bankSize: located.bankSize,
  };
}

export function patch3dsxBank(input, replacementBank) {
  const located = locateEmbeddedBank(input);
  const bank = replacementBank instanceof Uint8Array
    ? replacementBank : new Uint8Array(replacementBank);
  if (bank.length < embeddedBankMagic.length)
    throw new Error("The replacement sample bank is empty or truncated.");
  for (let index = 0; index < embeddedBankMagic.length; index += 1) {
    if (bank[index] !== embeddedBankMagic[index])
      throw new Error("The replacement bank is not NDSGRN01.");
  }

  const output = new Uint8Array(located.bankOffset + bank.length);
  output.set(located.bytes.subarray(0, located.bankOffset));
  output.set(bank, located.bankOffset);
  write3dsxU64(output, located.bankSizeField, bank.length);
  return output;
}
