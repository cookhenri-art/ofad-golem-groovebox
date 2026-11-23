// OFAD GOLEM — convertit ui/src/screens/faces/layouts.ts en mpc/faces.json (façades lues par skins.py).
// Usage : node mpc/faces_to_json.cjs <dossier contenant node_modules/typescript> ui/src/screens/faces/layouts.ts mpc/faces.json
const ts = require(process.argv[2] + "/node_modules/typescript");
const fs = require("fs");
const src = fs.readFileSync(process.argv[3], "utf8");
const out = ts.transpileModule(src, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 } });
const m = { exports: {} };
new Function("module", "exports", "require", out.outputText)(m, m.exports, (x) => ({}));
fs.writeFileSync(process.argv[4], JSON.stringify(m.exports.FACE_LAYOUTS, null, 1));
