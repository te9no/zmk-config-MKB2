import {build} from "esbuild";
import {copyFile, mkdir, readFile, readdir, writeFile} from "node:fs/promises";
await mkdir("dist", {recursive: true});
await build({entryPoints: ["app.mjs", "rpc.mjs"], bundle: true, splitting: true, chunkNames: "studio-client", minify: true,
  outdir: "dist", format: "esm", outExtension: {".js": ".mjs"}, loader: {".proto": "text"}});
await copyFile("index.html", "dist/index.html");
await copyFile("style.css", "dist/style.css");

const deps = ["@zmkfirmware/zmk-studio-ts-client", "protobufjs", "long",
  ...(await readdir("node_modules/@protobufjs")).map(name => "@protobufjs/" + name)];
const licenses = [await readFile("../LICENSE.studio-template", "utf8")];
for (const dep of deps) {
  licenses.push(dep + "\n" + await readFile("node_modules/" + dep + "/LICENSE", "utf8"));
}
await writeFile("dist/LICENSES.txt", licenses.join("\n\n"));
