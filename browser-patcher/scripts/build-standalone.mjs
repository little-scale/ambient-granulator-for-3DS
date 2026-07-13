import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const project = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const [template, css, bank, wav, app] = await Promise.all([
  readFile(resolve(project, "src/template.html"), "utf8"),
  readFile(resolve(project, "src/styles.css"), "utf8"),
  readFile(resolve(project, "src/bank.mjs"), "utf8"),
  readFile(resolve(project, "src/wav.mjs"), "utf8"),
  readFile(resolve(project, "src/app.mjs"), "utf8"),
]);

const javascript = `${bank}\n${wav}\n${app}`.replace(/<\/script/gi, "<\\/script");
const output = template
  .replace("/*__PATCHER_CSS__*/", css)
  .replace("/*__PATCHER_JS__*/", javascript);
if (/__PATCHER_(CSS|JS)__/.test(output))
  throw new Error("Standalone template markers were not replaced.");

const outputDirectory = resolve(project, "standalone");
const outputFile = resolve(outputDirectory, "3ds-granulator-patcher.html");
await mkdir(outputDirectory, { recursive: true });
await writeFile(outputFile, output);
console.log(outputFile);
