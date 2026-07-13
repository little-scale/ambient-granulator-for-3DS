import assert from "node:assert/strict";
import test from "node:test";

import { decodePcmWav } from "../src/wav.mjs";

function pcmWav(bits, frames) {
  const channels = 2;
  const bytesPerSample = bits / 8;
  const blockAlign = channels * bytesPerSample;
  const output = new Uint8Array(44 + frames.length * blockAlign);
  const view = new DataView(output.buffer);
  const text = (offset, value) => {
    for (let index = 0; index < value.length; index += 1)
      output[offset + index] = value.charCodeAt(index);
  };
  text(0, "RIFF");
  view.setUint32(4, output.length - 8, true);
  text(8, "WAVE");
  text(12, "fmt ");
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, channels, true);
  view.setUint32(24, 44100, true);
  view.setUint32(28, 44100 * blockAlign, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bits, true);
  text(36, "data");
  view.setUint32(40, frames.length * blockAlign, true);
  frames.forEach(([left, right], frame) => {
    [left, right].forEach((value, channel) => {
      const offset = 44 + frame * blockAlign + channel * bytesPerSample;
      if (bits === 16) view.setInt16(offset, value, true);
      else {
        const encoded = value < 0 ? value + 0x1000000 : value;
        output[offset] = encoded & 0xff;
        output[offset + 1] = (encoded >>> 8) & 0xff;
        output[offset + 2] = (encoded >>> 16) & 0xff;
      }
    });
  });
  return output;
}

test("decodes and downmixes PCM16 stereo WAV", () => {
  const decoded = decodePcmWav(pcmWav(16, [
    [32767, -32768],
    [16384, 16384],
  ]));
  assert.equal(decoded.sourceRate, 44100);
  assert.equal(decoded.channels, 2);
  assert.equal(decoded.bitsPerSample, 16);
  assert.ok(Math.abs(decoded.data[0]) < .0001);
  assert.ok(Math.abs(decoded.data[1] - .5) < .0001);
});

test("decodes and downmixes PCM24 stereo WAV", () => {
  const decoded = decodePcmWav(pcmWav(24, [
    [8388607, 8388607],
    [-8388608, 0],
  ]));
  assert.equal(decoded.bitsPerSample, 24);
  assert.ok(decoded.data[0] > .9999);
  assert.ok(Math.abs(decoded.data[1] + .5) < .0001);
});
