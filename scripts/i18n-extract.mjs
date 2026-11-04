// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix30 : recense les textes français affichés (interface et catalogue des modules) pour la
// traduction. Écrit scripts/i18n/strings.json : { exact: [textes], patterns: [modèles « a {1} b »] }.
//   node scripts/i18n-extract.mjs
// Les textes sont gardés tels quels (clé = texte français) : la traduction se fait à l'affichage
// (ui/src/i18n), sans toucher au code ; les identifiants (familles, ids…) ne changent pas.
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "..");
// API du compilateur TypeScript 5 (TypeScript 7 n'en a plus en JavaScript) : I18N_TS=<dossier contenant
// node_modules/typescript>, sinon celui de l'interface
const require = createRequire(path.join(process.env.I18N_TS ?? path.join(ROOT, "ui"), "package.json"));
const ts = require("typescript");
if (!ts.ScriptTarget) { console.error("Il faut l'API de TypeScript 5 : npm i typescript@5 dans un dossier, puis I18N_TS=ce dossier"); process.exit(1); }

const exact = new Map();   // texte -> nombre d'occurrences
const patterns = new Map(); // modèle -> nombre
const FRENCH = /[a-zA-ZÀ-ÿ]/;
const ACCENT = /[À-ÿœŒ]/;

/** Un texte à traduire : des lettres, et une phrase (espace), un accent, ou une majuscule en tête. */
function wanted(s) {
  const t = s.trim();
  if (t.length < 2 || !FRENCH.test(t)) return false;
  if (/^[a-z0-9_.:/#-]+$/.test(t)) return false;               // identifiants, classes, chemins
  if (/^(https?:|data:|var\(|oklch|rgb|calc\(|translate|rotate|scale)/.test(t)) return false;
  if (/^[A-Z0-9_]+$/.test(t) && t.length <= 4) return false;   // A, B, MIDI, CC…
  if (/^[a-z]+([A-Z][a-z0-9]*)+$/.test(t)) return false;       // camelCase
  if (/^--?[a-z]/.test(t) || /\s--[a-z]/.test(t)) return false; // classes CSS
  if (/^[a-z-]+( [a-z-]+)*$/.test(t) && !ACCENT.test(t) && t.split(" ").every((w) => /-|^(on|is|has)$/.test(w) || w.length < 3)) return false;
  return /\s/.test(t) || ACCENT.test(t) || /^[A-ZÀ-Ý]/.test(t) || /[a-zà-ÿ]{3,}/.test(t);
}
const add = (s) => { const t = s.replace(/\s+/g, " ").trim(); if (wanted(t)) exact.set(t, (exact.get(t) ?? 0) + 1); };

function walk(dir, out) {
  for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, f.name);
    if (f.isDirectory()) { if (f.name !== "i18n") walk(p, out); }
    else if (/\.(tsx?|ts)$/.test(f.name) && !/\.d\.ts$/.test(f.name) && f.name !== "mock.ts") out.push(p);
  }
  return out;
}

for (const file of walk(path.join(ROOT, "ui", "src"), [])) {
  const src = ts.createSourceFile(file, fs.readFileSync(file, "utf8"), ts.ScriptTarget.Latest, true, file.endsWith("x") ? ts.ScriptKind.TSX : ts.ScriptKind.TS);
  const visit = (n) => {
    // imports, clés d'objets, comparaisons de types : pas des textes affichés
    if (ts.isImportDeclaration(n) || ts.isExportDeclaration(n)) return;
    if (ts.isJsxText(n)) add(n.getText());
    else if (ts.isStringLiteral(n) || ts.isNoSubstitutionTemplateLiteral(n)) {
      const p = n.parent;
      const isKey = (ts.isPropertyAssignment(p) && p.name === n) || ts.isElementAccessExpression(p) || ts.isLiteralTypeNode(p);
      const attr = ts.isJsxAttribute(p) ? p.name.getText() : null;
      const skipAttr = attr && !/^(title|aria-label|aria-valuetext|label|placeholder|alt|hint|text|name|data-ctl-label|what)$/.test(attr);
      const inCall = ts.isCallExpression(p) && /^(getItem|setItem|readPref|writePref|call|callJson|hasNative|querySelector|querySelectorAll|addEventListener|removeEventListener|setProperty|getPropertyValue|startsWith|endsWith|includes|split|replace|test|match|emitEvent|createElement|classList\..*)$/.test(p.expression.getText().split(".").pop() ?? "");
      const isCompare = ts.isBinaryExpression(p) && /^(===|!==|==|!=)$/.test(p.operatorToken.getText());
      const isCase = ts.isCaseClause(p);
      if (!isKey && !skipAttr && !inCall && !isCompare && !isCase) add(n.text);
    } else if (ts.isTemplateExpression(n)) {
      // `a ${x} b ${y}` -> modèle « a {1} b {2} » ; les morceaux qui ne sont que des espaces ne comptent pas
      const parts = [n.head.text, ...n.templateSpans.map((s) => s.literal.text)];
      const statics = parts.join("").replace(/\s+/g, " ").trim();
      if (wanted(statics) && !/^[\s{}0-9.,%:/()·×+−-]*$/.test(statics)) {
        let pat = n.head.text;
        n.templateSpans.forEach((s, i) => { pat += `{${i + 1}}` + s.literal.text; });
        pat = pat.replace(/\s+/g, " ").trim();
        if (!/^\{1\}$/.test(pat) && !/^[a-z0-9_.{}\s-]+$/.test(pat)) patterns.set(pat, (patterns.get(pat) ?? 0) + 1);
      }
    }
    ts.forEachChild(n, visit);
  };
  visit(src);
}

// catalogue des modules : noms, familles, principes, réglages, choix, sections, catégories de sons
const cat = JSON.parse(fs.readFileSync(path.join(ROOT, "ui", "src", "engine", "catalog.json"), "utf8"));
const visitJson = (v, key) => {
  if (typeof v === "string") { if (/^(name|family|principle|title|label|role|use|form|choices|labels|presetCats|tab)$/.test(key ?? "") || key === undefined) add(v); return; }
  if (Array.isArray(v)) { for (const x of v) visitJson(x, key === "presets" ? "__preset" : key); return; }
  if (v && typeof v === "object") for (const [k, x] of Object.entries(v)) if (!/^(id|type|unit|presets|visual)$/.test(k)) visitJson(x, k);
};
visitJson(cat, "root");

const outDir = path.join(ROOT, "scripts", "i18n");
fs.mkdirSync(outDir, { recursive: true });
const sortMap = (m) => [...m.entries()].sort((a, b) => a[0].localeCompare(b[0], "fr")).map(([k]) => k);
fs.writeFileSync(path.join(outDir, "strings.json"), JSON.stringify({ exact: sortMap(exact), patterns: sortMap(patterns) }, null, 1));
console.log(`${exact.size} textes, ${patterns.size} modèles -> scripts/i18n/strings.json`);
