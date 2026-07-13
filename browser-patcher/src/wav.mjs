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

export function decodePcmWav(input) {
  const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (bytes.length < 44 || fourCc(bytes, 0) !== "RIFF"
      || fourCc(bytes, 8) !== "WAVE")
    throw new Error("File is not a RIFF/WAVE audio file.");

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let format = null;
  let dataOffset = -1;
  let dataLength = 0;
  for (let offset = 12; offset + 8 <= bytes.length;) {
    const id = fourCc(bytes, offset);
    const length = view.getUint32(offset + 4, true);
    const payload = offset + 8;
    if (payload + length > bytes.length)
      throw new Error(`WAV ${id} chunk extends beyond the file.`);
    if (id === "fmt ") {
      if (length < 16) throw new Error("WAV format chunk is incomplete.");
      let encoding = view.getUint16(payload, true);
      const channels = view.getUint16(payload + 2, true);
      const sampleRate = view.getUint32(payload + 4, true);
      const blockAlign = view.getUint16(payload + 12, true);
      const bitsPerSample = view.getUint16(payload + 14, true);
      if (encoding === 0xfffe && length >= 40)
        encoding = view.getUint16(payload + 24, true);
      format = { encoding, channels, sampleRate, blockAlign, bitsPerSample };
    } else if (id === "data") {
      dataOffset = payload;
      dataLength = length;
    }
    offset = payload + length + (length & 1);
  }

  if (!format || dataOffset < 0)
    throw new Error("WAV file has no usable format or data chunk.");
  if (format.encoding !== 1 || format.channels < 1 || format.channels > 8
      || ![16, 24].includes(format.bitsPerSample)
      || format.sampleRate < 8000 || format.sampleRate > 192000) {
    throw new Error("Only 16-bit or 24-bit integer PCM WAV audio is supported.");
  }
  const bytesPerSample = format.bitsPerSample / 8;
  if (format.blockAlign < format.channels * bytesPerSample)
    throw new Error("WAV block alignment is invalid.");

  const frames = Math.floor(dataLength / format.blockAlign);
  if (frames < 1) throw new Error("WAV file contains no complete audio frames.");
  const data = new Float32Array(frames);
  let stereoEnergy = 0;
  let differenceEnergy = 0;
  for (let frame = 0; frame < frames; frame += 1) {
    const base = dataOffset + frame * format.blockAlign;
    let sum = 0;
    let left = 0;
    let right = 0;
    for (let channel = 0; channel < format.channels; channel += 1) {
      const offset = base + channel * bytesPerSample;
      const value = format.bitsPerSample === 16
        ? view.getInt16(offset, true) / 32768
        : readInt24(view, offset) / 8388608;
      if (channel === 0) left = value;
      if (channel === 1) right = value;
      sum += value;
    }
    data[frame] = sum / format.channels;
    if (format.channels >= 2) {
      stereoEnergy += (left * left + right * right) * .5;
      const difference = left - right;
      differenceEnergy += difference * difference * .5;
    }
  }
  const stereoDifferenceDb = format.channels >= 2 && stereoEnergy > 0
    ? 10 * Math.log10(Math.max(Number.EPSILON,
      differenceEnergy / stereoEnergy)) : null;
  return {
    data,
    sourceRate: format.sampleRate,
    channels: format.channels,
    bitsPerSample: format.bitsPerSample,
    stereoDifferenceDb,
  };
}
