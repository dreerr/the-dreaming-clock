// Bundles the frontend from web/ into data/, which is what gets flashed to
// LittleFS. Every output is also written pre-compressed: AsyncWebServer's
// serveStatic serves "<file>.gz" with Content-Encoding: gzip automatically, so
// the device never spends cycles compressing and the captive-portal page load
// is a fraction of the transfer.
//
//   node web/build.mjs
//
// The output in data/ is committed, so flashing the filesystem does not
// require a JS toolchain.

import { build } from "esbuild";
import { gzipSync, constants } from "node:zlib";
import { mkdirSync, readFileSync, writeFileSync, rmSync, readdirSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const src = join(root, "web");
const out = join(root, "data");

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });

await build({
  entryPoints: [join(src, "js/index.js")],
  bundle: true,
  minify: true,
  format: "esm",
  target: ["es2020"],
  outdir: out,
  loader: { ".json": "json" },
  logLevel: "info",
});

// HTML and CSS are copied through, minified only in the cheap, safe ways.
for (const file of ["index.html"]) {
  const html = readFileSync(join(src, file), "utf8")
    .replace(/\n\s+/g, "\n")
    .trim();
  writeFileSync(join(out, file), html);
}

const css = readFileSync(join(src, "style.css"), "utf8")
  .replace(/\/\*[\s\S]*?\*\//g, "")
  .replace(/\s*([{}:;,])\s*/g, "$1")
  .replace(/;\}/g, "}")
  .replace(/\n{2,}/g, "\n")
  .trim();
writeFileSync(join(out, "style.css"), css);

// Pre-compress everything worth compressing.
let raw = 0;
let compressed = 0;
for (const name of readdirSync(out)) {
  const path = join(out, name);
  if (!statSync(path).isFile()) continue;
  const data = readFileSync(path);
  raw += data.length;
  if (data.length < 512) {
    compressed += data.length;
    continue; // not worth a second file
  }
  const gz = gzipSync(data, { level: constants.Z_BEST_COMPRESSION });
  writeFileSync(`${path}.gz`, gz);
  rmSync(path); // serveStatic finds the .gz on its own
  compressed += gz.length;
}

const pct = ((1 - compressed / raw) * 100).toFixed(0);
console.log(`\ndata/  ${raw} B raw -> ${compressed} B shipped (-${pct}%)`);
