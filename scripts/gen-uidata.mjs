// Groovebox — OFAD — AGPL-3.0-or-later
// Génère android/generated/UiData.h et UiData.cpp à partir de ui/dist (interface compilée),
// avec la même interface que le juce_add_binary_data de CMake (espace de noms UiData).
// Usage : node scripts/gen-uidata.mjs   (après « npm run build » dans ui/)
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const dist = path.join(root, "ui", "dist");
const out = path.join(root, "android", "generated");
if (!fs.existsSync(dist)) { console.error("Interface non compilée : dans ui/, lancer 'npm install' puis 'npm run build'."); process.exit(1); }
fs.mkdirSync(out, { recursive: true });

const files = [];
const walk = (d) => { for (const n of fs.readdirSync(d).sort()) { const p = path.join(d, n); if (fs.statSync(p).isDirectory()) walk(p); else files.push(p); } };
walk(dist);
files.sort();
const id = (i) => `res${String(i).padStart(3, "0")}`;

const h = ["// Fichier genere par scripts/gen-uidata.mjs : ne pas modifier.", "#pragma once", "", "namespace UiData", "{"];
files.forEach((f, i) => h.push(`    extern const char* ${id(i)};\n    const int ${id(i)}Size = ${fs.statSync(f).size};`));
h.push("", "    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);",
  "    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);",
  "    extern const char* namedResourceList[];", "    extern const char* originalFilenames[];",
  `    const int namedResourceListSize = ${files.length};`, "}", "");
fs.writeFileSync(path.join(out, "UiData.h"), h.join("\n"), "ascii");

const c = ["// Fichier genere par scripts/gen-uidata.mjs : ne pas modifier.", '#include "UiData.h"', "#include <cstring>", "", "namespace UiData", "{"];
files.forEach((f, i) => {
  const data = fs.readFileSync(f);
  c.push(`static const unsigned char data${String(i).padStart(3, "0")}[] = {`);
  for (let k = 0; k < data.length; k += 24) c.push("  " + Array.from(data.subarray(k, k + 24)).join(",") + ",");
  c.push("  0 };");
  c.push(`const char* ${id(i)} = (const char*) data${String(i).padStart(3, "0")};`);
});
c.push(`const char* namedResourceList[] = { ${files.map((_, i) => `"${id(i)}"`).join(", ")} };`);
c.push(`const char* originalFilenames[] = { ${files.map((f) => `"${path.basename(f)}"`).join(", ")} };`);
c.push("", "const char* getNamedResource (const char* name, int& size)", "{",
  "    static const int sizes[] = { " + files.map((f) => fs.statSync(f).size).join(", ") + " };",
  "    static const char* const ptrs[] = { " + files.map((_, i) => `(const char*) data${String(i).padStart(3, "0")}`).join(", ") + " };",
  "    for (int i = 0; i < namedResourceListSize; ++i)",
  "        if (std::strcmp (name, namedResourceList[i]) == 0) { size = sizes[i]; return ptrs[i]; }",
  "    size = 0;", "    return nullptr;", "}", "",
  "const char* getNamedResourceOriginalFilename (const char* name)", "{",
  "    for (int i = 0; i < namedResourceListSize; ++i)",
  "        if (std::strcmp (name, namedResourceList[i]) == 0) return originalFilenames[i];",
  "    return nullptr;", "}", "}", "");
fs.writeFileSync(path.join(out, "UiData.cpp"), c.join("\n"), "ascii");
console.log(`${files.length} fichiers d'interface -> ${out}`);
