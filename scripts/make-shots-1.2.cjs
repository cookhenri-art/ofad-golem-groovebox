// Groovebox — OFAD — AGPL-3.0-or-later
// 1.2 : captures des nouveautés (Kick Forge, Good Doctor 2, Micro Tonnerre) pour la page GitHub, la notice et
// les posts, prises dans l'aperçu de l'interface. PC : 1600 × 1000 en ×2 ; téléphone : 915 × 412 en ×2.
// Kick Forge : l'afficheur montre un VRAI calcul du moteur (réglages et vues d'un son d'usine, écrits par
// l'outil kfview : « kfview "Hardstyle 01" sortie.json »), posé dans l'aperçu par window.__kfShot.
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-shots-1.2.cjs <dossier de sortie> <dossier des .json de kfview> [nom…]
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || "captures-1.2";
const kfDir = process.argv[3] || ".";
const only = process.argv.slice(4);
fs.mkdirSync(out, { recursive: true });
const URL = "http://localhost:4323/";
// langue des captures : GOLEM_LANG=en pour l'interface anglaise (posts en anglais) ; les libellés cherchés sont traduits
const LANG = process.env.GOLEM_LANG === "en" ? "en" : "fr";
const EN = LANG === "en" ? JSON.parse(fs.readFileSync(path.join(__dirname, "..", "ui", "src", "i18n", "en.json"), "utf8")) : {};
const T = (fr) => (LANG === "en" ? EN[fr] ?? fr : fr);
const rx = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const PC = { width: 1600, height: 1000 }, PC_TALL = { width: 1600, height: 1150 }, TEL = { width: 915, height: 412 };
const BIG = { width: 1920, height: 1080 };

const wait = (p, ms = 300) => p.waitForTimeout(ms);
async function nav(p, name) { await p.getByRole("button", { name: new RegExp(`^${rx(T(name))}$`, "i") }).first().click(); await wait(p, 350); }
async function openInst(p, family, name) {
  await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await wait(p, 200);
  await p.getByRole("tab", { name: T(family) }).first().click(); await wait(p, 200);
  await p.locator(".browser").getByText(name, { exact: true }).first().dblclick(); await wait(p, 600);
  // la piste prend le nom de l'instrument (la démo de l'aperçu nomme ses pistes d'après leur ancien instrument)
  await nav(p, "Pistes");
  const n = p.locator(`input[aria-label="Nom de la piste"], input[aria-label="${T("Nom de la piste")}"]`).first();
  if (await n.count()) { await n.fill(name); await n.press("Tab"); await wait(p, 300); }
  await p.getByRole("button", { name: /ouvrir (le )?synth|open the synth/i }).first().click(); await wait(p, 1000);
  const m = p.getByRole("button", { name: /^(Masquer|Hide)/ }); if (await m.count()) { await m.first().click(); await wait(p); }
  // taille « ajustée » : toute la façade tient à l'écran (PC) ; au téléphone la façade défile toujours
  const big = p.getByRole("button", { name: new RegExp(`^${rx(T("Taille : grande"))}$`) }); if (await big.count()) { await big.first().click(); await wait(p, 700); }
}
async function page(p, label) { await p.locator(".face [role=tab], .face button").filter({ hasText: new RegExp(`^${rx(T(label))}$`) }).first().click(); await wait(p, 700); }
async function kick(p, preset, layer = 0) {
  const shot = JSON.parse(fs.readFileSync(path.join(kfDir, preset.replace(/ /g, "_") + ".json"), "utf8"));
  shot.name = preset;
  await p.evaluate((s) => { window.__kfShot = s; }, shot);
  await openInst(p, "Batteries", "Kick Forge");
  // le son d'usine suivant (▶ à côté du nom) : l'aperçu pose alors les réglages du vrai calcul
  await p.locator(".face-head button").filter({ hasText: "▶" }).first().click(); await wait(p, 900);
  await p.locator(".kf-slot").nth(layer).click(); await wait(p, 500);
}

// Good Doctor 2 et Micro Tonnerre : un son d'usine (réglages écrits par l'outil presetvals : « gd2.json », « mt.json »)
async function withPreset(p, file, family, name) {
  const shot = JSON.parse(fs.readFileSync(path.join(kfDir, file), "utf8"));
  await p.evaluate((s) => { window.__shot = s; }, shot);
  await openInst(p, family, name);
  await p.locator(".face-head button").filter({ hasText: "▶" }).first().click(); await wait(p, 900);
}
const GD2 = (p) => withPreset(p, "gd2.json", "Polys", "Good Doctor 2");
const MT = (p) => withPreset(p, "mt.json", "Batteries", "Micro Tonnerre");

const SHOTS = {
  // --- Kick Forge ---------------------------------------------------------------------------------
  "pc-kickforge": { vp: PC_TALL, run: async (p) => { await kick(p, "Hardstyle 01", 0); } },
  "pc-kickforge-rumble": { vp: PC_TALL, run: async (p) => { await kick(p, "Rumble 01", 2); } },
  "pc-kickforge-bibliotheque": { vp: PC_TALL, run: async (p) => { await kick(p, "Reverse bass 01", 2); await p.locator(".kf-libbtn").click(); await wait(p, 500); } },
  "pc-kickforge-somme": { vp: PC_TALL, run: async (p) => { await kick(p, "Schranz 01", 0); await p.locator(".kf-views button").filter({ hasText: T("Après effets") }).click(); await wait(p, 400); } },
  "pc-kickforge-effets": { vp: PC_TALL, run: async (p) => {
    await kick(p, "Hardstyle 01", 0); await page(p, "Effets du kick");
    await p.locator(".kf-mod-name").filter({ hasText: T("Finaliseur") }).click(); await wait(p, 400);
    await p.locator(".kf-views button").filter({ hasText: T("Écrêtage") }).click(); await wait(p, 400);
  } },
  "pc-kickforge-faconneur": { vp: PC_TALL, run: async (p) => {
    await kick(p, "Psy 02", 0); await page(p, "Effets du kick");
    await p.locator(".kf-addmod .kf-btn").click(); await wait(p, 200);
    await p.locator(".kf-addmod-list button").filter({ hasText: T("Façonneur de kick") }).click(); await wait(p, 400);
    const c = p.locator(".kf-curve circle");
    for (const [i, dy] of [[2, 30], [3, 60], [4, 80], [5, 95], [6, 110], [7, 125]]) {
      const bb = await c.nth(i).boundingBox(); if (!bb) continue;
      await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.mouse.down();
      await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2 + dy, { steps: 4 }); await p.mouse.up(); await wait(p, 120);
    }
  } },
  "tel-kickforge": { vp: TEL, run: async (p) => { await kick(p, "Hardstyle 01", 0); } },
  // --- Good Doctor 2 ------------------------------------------------------------------------------
  "pc-gooddoc2": { vp: PC_TALL, run: async (p) => { await GD2(p); } },
  "pc-gooddoc2-effets": { vp: PC_TALL, run: async (p) => { await GD2(p); await page(p, "Effets"); } },
  "pc-gooddoc2-editeur": { vp: PC_TALL, run: async (p) => { await GD2(p); await page(p, "Éditeur de tables"); } },
  "pc-gooddoc2-melangeur": { vp: PC_TALL, run: async (p) => { await GD2(p); await page(p, "Mélangeur"); } },
  "tel-gooddoc2": { vp: TEL, run: async (p) => { await GD2(p); } },
  // --- Micro Tonnerre -----------------------------------------------------------------------------
  "pc-microton": { vp: PC_TALL, run: async (p) => { await MT(p); } },
  "pc-microton-kit": { vp: PC_TALL, run: async (p) => { await MT(p); await page(p, "Le kit"); } },
  "tel-microton": { vp: TEL, run: async (p) => { await MT(p); } },
  // --- galerie (même cadrage que les autres images de la galerie) ---------------------------------
  "gal-gooddoc2": { vp: BIG, scale: 1, run: async (p) => { await GD2(p); } },
  "gal-microton": { vp: BIG, scale: 1, run: async (p) => { await MT(p); } },
  "gal-kickforge": { vp: BIG, scale: 1, run: async (p) => { await kick(p, "Hardstyle 01", 0); } },
};

(async () => {
  const b = await chromium.launch();
  for (const [name, s] of Object.entries(SHOTS)) {
    if (only.length && !only.includes(name)) continue;
    const p = await b.newPage({ viewport: s.vp, deviceScaleFactor: s.scale ?? 2 });
    const errs = []; p.on("pageerror", (e) => errs.push(e.message));
    try {
      if (LANG === "en") await p.addInitScript(() => { try { localStorage.setItem("groovebox.lang", "en"); } catch {} });
      await p.goto(URL + (s.query || "")); await wait(p, 1600);
      await s.run(p); await wait(p, 2600); // les messages (« Préréglage … ») s'effacent
      await p.evaluate(() => document.querySelectorAll(".face-body, .face, .fp-page").forEach((e) => { e.scrollTop = 0; }));
      await wait(p, 300);
      await p.screenshot({ path: path.join(out, name + ".png") });
      console.log("ok", name, errs.length ? "ERREURS " + errs.slice(0, 2).join(" | ") : "");
    } catch (e) { console.log("ÉCHEC", name, String(e).split("\n")[0]); }
    await p.close();
  }
  await b.close();
})();
