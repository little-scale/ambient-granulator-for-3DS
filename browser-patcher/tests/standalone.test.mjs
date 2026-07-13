import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const html = readFileSync(new URL(
  "../standalone/3ds-granulator-patcher.html", import.meta.url), "utf8");

test("standalone artifact embeds all runtime code and styling", () => {
  assert.match(html, /^<!doctype html>/i);
  assert.match(html, /<style>[\s\S]+<\/style>/i);
  assert.match(html, /<script type="module">[\s\S]+<\/script>/i);
  assert.doesNotMatch(html, /__PATCHER_(CSS|JS)__/);
  assert.doesNotMatch(html, /<script[^>]+src=/i);
  assert.doesNotMatch(html, /<link[^>]+stylesheet/i);
  assert.doesNotMatch(html, /\bfetch\s*\(/);
  assert.match(html, /OPEN \.3DSX/);
  assert.match(html, /DOWNLOAD PATCHED \.3DSX/);
  assert.match(html, /function extract3dsxBank/);
  assert.match(html, /function patch3dsxBank/);
  assert.ok(Buffer.byteLength(html) > 35000);
});
