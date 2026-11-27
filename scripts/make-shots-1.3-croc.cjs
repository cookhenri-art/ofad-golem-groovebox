// Groovebox — OFAD — AGPL-3.0-or-later
// 1.3 (27 nov.) : captures de Croc Glacé (Frost Fang en anglais) pour la notice, la page GitHub et les posts,
// prises dans l'aperçu de l'interface. PC : 1600 × 1000 en ×2 ; téléphone : 915 × 412 en ×2.
// Les sons d'usine de Croc Glacé passent par des fonctions (coins de l'ORB, niveaux) : leurs réglages sont donc
// lus dans un fichier JSON { nom : [valeurs 0..1 dans l'ordre des réglages], __categories : { nom : banque } }
// écrit par le moteur lui-même
// (petit programme qui parcourt croc::desc().presets, mêmes conversions que toNorm ci-dessous).
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : CROC_VALUES=<fichier.json> node scripts/make-shots-1.3-croc.cjs <dossier de sortie> [nom…]
//         (GOLEM_LANG=en : interface anglaise ; CHROME=<chemin> : navigateur à utiliser)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const ROOT = path.join(__dirname, "..");
const out = process.argv[2] || "captures-1.3-croc";
const only = process.argv.slice(3);
fs.mkdirSync(out, { recursive: true });
const URL = "http://localhost:4323/";
const LANG = process.env.GOLEM_LANG === "en" ? "en" : "fr";
const EN = LANG === "en" ? JSON.parse(fs.readFileSync(path.join(ROOT, "ui", "src", "i18n", "en.json"), "utf8")) : {};
const T = (fr) => (LANG === "en" ? EN[fr] ?? fr : fr);
const rx = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const PC = { width: 1600, height: 1000 }, TEL = { width: 915, height: 412 };
const wait = (p, ms = 300) => p.waitForTimeout(ms);
const VALUES = JSON.parse(fs.readFileSync(process.env.CROC_VALUES || path.join(out, "croc-values.json"), "utf8"));
const NAME = "Croc Glacé";

function shot(name) {
  if (!VALUES[name]) throw new Error("son introuvable : " + name);
  return { type: "croc", name, values: VALUES[name] };
}
async function nav(p, name) { await p.getByRole("button", { name: new RegExp(`^${rx(T(name))}$`, "i") }).first().click(); await wait(p, 350); }
async function openCroc(p, preset) {
  await p.evaluate((s) => { window.__shot = s; }, shot(preset));
  await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await wait(p, 200);
  await p.getByRole("tab", { name: T("Nappes") }).first().click(); await wait(p, 200);
  await p.locator(".browser").getByText(T(NAME), { exact: true }).first().dblclick(); await wait(p, 600);
  await nav(p, "Pistes");
  const n = p.locator(`input[aria-label="Nom de la piste"], input[aria-label="${T("Nom de la piste")}"]`).first();
  if (await n.count()) { await n.fill(T(NAME)); await n.press("Tab"); await wait(p, 300); }
  await p.getByRole("button", { name: /ouvrir (le )?synth|open (the )?synth/i }).first().click(); await wait(p, 1000);
  const m = p.getByRole("button", { name: /^(Masquer|Hide)/ }); if (await m.count()) { await m.first().click(); await wait(p); }
  const big = p.getByRole("button", { name: new RegExp(`^${rx(T("Taille : grande"))}$`) }); if (await big.count()) { await big.first().click(); await wait(p, 700); }
  // le son d'usine suivant (▶) : l'aperçu pose alors les réglages lus plus haut ; au téléphone, par la liste
  if ((p.viewportSize()?.width ?? 0) > 1000) { await p.locator(".face-head button").filter({ hasText: "▶" }).first().click(); await wait(p, 900); return; }
  const pr = p.getByRole("button", { name: new RegExp(`^${rx(T("Préréglages"))}$`) }).first();
  await pr.click(); await wait(p, 600);
  const bank = (VALUES.__categories || {})[preset] || "";
  await p.locator("button:visible").filter({ hasText: new RegExp(`^\\W*${rx(T(bank))}$`) }).last().click({ timeout: 8000 }); await wait(p, 400);
  await p.locator("button:visible").filter({ hasText: new RegExp(`${rx(preset)}$`) }).first().click({ timeout: 8000 }); await wait(p, 600);
  await p.getByRole("button", { name: new RegExp(`^${rx(T("Fermer"))}$`) }).first().click({ timeout: 8000 }); await wait(p, 600);
}
async function page(p, label) { await p.locator(".face [role=tab], .face button").filter({ hasText: new RegExp(`^${rx(T(label))}$`) }).first().click(); await wait(p, 900); }

const SHOTS = {
  "pc-croc": { vp: PC, run: async (p) => { await openCroc(p, "Drone en orbite"); } },
  "pc-croc-orb": { vp: PC, run: async (p) => { await openCroc(p, "Glitch mutant"); await page(p, "ORB"); await wait(p, 1500); } },
  "pc-croc-seq": { vp: PC, run: async (p) => { await openCroc(p, "Squelch séquencé"); await page(p, "SEQ"); } },
  "pc-croc-fx": { vp: PC, run: async (p) => { await openCroc(p, "Glitch gelé"); await page(p, "Effets"); } },
  "tel-croc-orb": { vp: TEL, run: async (p) => { await openCroc(p, "Glitch mutant"); await page(p, "ORB"); await wait(p, 1500); } },
  "gal-croc": { vp: { width: 1920, height: 1080 }, scale: 1, run: async (p) => { await openCroc(p, "Glitch mutant"); await page(p, "ORB"); await wait(p, 1500); } },
};

(async () => {
  const b = await chromium.launch(process.env.CHROME ? { executablePath: process.env.CHROME } : {});
  for (const [name, s] of Object.entries(SHOTS)) {
    if (only.length && !only.includes(name)) continue;
    const p = await b.newPage({ viewport: s.vp, deviceScaleFactor: s.scale ?? 2 });
    const errs = []; p.on("pageerror", (e) => errs.push(e.message));
    try {
      if (LANG === "en") await p.addInitScript(() => { try { localStorage.setItem("groovebox.lang", "en"); } catch {} });
      await p.goto(URL); await wait(p, 1600);
      await s.run(p); await wait(p, 2600); // les messages (« Préréglage … ») s'effacent
      await p.evaluate(() => document.querySelectorAll(".face-body, .face, .fp-page").forEach((e) => { e.scrollTop = 0; }));
      await wait(p, 300);
      await p.screenshot({ path: path.join(out, name + ".png") });
      console.log("ok", name, errs.length ? "ERREURS " + errs.slice(0, 2).join(" | ") : "");
    } catch (e) { console.log("ÉCHEC", name, String(e).split("\n")[0]); if (process.env.SHOT_DEBUG) await p.screenshot({ path: path.join(out, name + "-echec.png") }); }
    await p.close();
  }
  await b.close();
})();
