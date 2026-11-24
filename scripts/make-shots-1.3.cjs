// Groovebox — OFAD — AGPL-3.0-or-later
// 1.3 : captures de Gamer Lover pour la notice, la page GitHub et les posts, prises dans l'aperçu de
// l'interface. PC : 1600 × 1000 en ×2 ; téléphone : 915 × 412 en ×2.
// Les réglages d'un son d'usine sont lus dans src/instruments/GamerLover.h (lignes « add (… ) » et niveaux
// propres à GOLEM), ramenés en valeurs 0..1 avec le catalogue, puis posés dans l'aperçu par window.__shot.
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-shots-1.3.cjs <dossier de sortie> [nom…]   (GOLEM_LANG=en : interface anglaise)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const ROOT = path.join(__dirname, "..");
const out = process.argv[2] || "captures-1.3";
const only = process.argv.slice(3);
fs.mkdirSync(out, { recursive: true });
const URL = "http://localhost:4323/";
const LANG = process.env.GOLEM_LANG === "en" ? "en" : "fr";
const EN = LANG === "en" ? JSON.parse(fs.readFileSync(path.join(ROOT, "ui", "src", "i18n", "en.json"), "utf8")) : {};
const T = (fr) => (LANG === "en" ? EN[fr] ?? fr : fr);
const rx = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const PC = { width: 1600, height: 1000 }, TEL = { width: 915, height: 412 };
const wait = (p, ms = 300) => p.waitForTimeout(ms);

// --- réglages d'un son d'usine (mêmes valeurs que le moteur)
const cat = JSON.parse(fs.readFileSync(path.join(ROOT, "ui", "src", "engine", "catalog.json"), "utf8"));
const desc = cat.instruments.find((i) => i.type === "gamer");
const src = fs.readFileSync(path.join(ROOT, "src", "instruments", "GamerLover.h"), "utf8");
function toNorm(p, plain) {
  if (p.choices && p.choices.length) { const c = p.choices.length; const i = Math.min(c - 1, Math.max(0, Math.round(plain))); return c <= 1 ? 0 : (i + 0.5) / c; }
  if (p.max <= p.min) return 0;
  const x = Math.min(1, Math.max(0, (plain - p.min) / (p.max - p.min)));
  return p.skew === 1 ? x : Math.pow(x, p.skew);
}
function shot(name) {
  const line = src.split("\n").find((l) => l.includes(`add ("${name}",`));
  if (!line) throw new Error("son introuvable : " + name);
  const plain = Object.fromEntries(desc.params.map((p) => [p.id, p.def]));
  for (const m of line.matchAll(/\{ "(\w+)", (-?[\d.]+)f? \}/g)) plain[m[1]] = Number(m[2]);
  const lv = src.match(new RegExp(`\\{ "${rx(name)}", (-?[\\d.]+)f? \\}`)); // niveau propre à GOLEM (sons partagés avec la MPC)
  if (lv && !line.includes(`{ "${name}", `)) plain.level = Number(lv[1]);
  return { type: "gamer", name, values: desc.params.map((p) => toNorm(p, plain[p.id])) };
}

async function nav(p, name) { await p.getByRole("button", { name: new RegExp(`^${rx(T(name))}$`, "i") }).first().click(); await wait(p, 350); }
async function openGamer(p, preset) {
  await p.evaluate((s) => { window.__shot = s; }, shot(preset));
  await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await wait(p, 200);
  await p.getByRole("tab", { name: T("Polys") }).first().click(); await wait(p, 200);
  await p.locator(".browser").getByText("Gamer Lover", { exact: true }).first().dblclick(); await wait(p, 600);
  await nav(p, "Pistes");
  const n = p.locator(`input[aria-label="Nom de la piste"], input[aria-label="${T("Nom de la piste")}"]`).first();
  if (await n.count()) { await n.fill("Gamer Lover"); await n.press("Tab"); await wait(p, 300); }
  await p.getByRole("button", { name: /ouvrir (le )?synth|open (the )?synth/i }).first().click(); await wait(p, 1000);
  const m = p.getByRole("button", { name: /^(Masquer|Hide)/ }); if (await m.count()) { await m.first().click(); await wait(p); }
  const big = p.getByRole("button", { name: new RegExp(`^${rx(T("Taille : grande"))}$`) }); if (await big.count()) { await big.first().click(); await wait(p, 700); }
  // le son d'usine suivant (▶ à côté du nom) : l'aperçu pose alors les réglages lus plus haut ;
  // au téléphone (pas de ◀ ▶), on le choisit dans la liste des préréglages
  if ((p.viewportSize()?.width ?? 0) > 1000) { await p.locator(".face-head button").filter({ hasText: "▶" }).first().click(); await wait(p, 900); return; }
  const pr = p.getByRole("button", { name: new RegExp(`^${rx(T("Préréglages"))}$`) }).first();
  await pr.click(); await wait(p, 600);
  const bank = src.split("\n").find((l) => l.includes(`add ("${preset}",`)).match(/add \("[^"]+", "([^"]+)"/)[1];
  await p.locator("button:visible").filter({ hasText: new RegExp(`^\\W*${rx(T(bank))}$`) }).last().click({ timeout: 8000 }); await wait(p, 400);
  await p.locator("button:visible").filter({ hasText: new RegExp(`${rx(T(preset))}$`) }).first().click({ timeout: 8000 }); await wait(p, 600);
  await p.getByRole("button", { name: new RegExp(`^${rx(T("Fermer"))}$`) }).first().click({ timeout: 8000 }); await wait(p, 600);
}
async function page(p, label) { await p.locator(".face [role=tab], .face button").filter({ hasText: new RegExp(`^${rx(T(label))}$`) }).first().click(); await wait(p, 700); }
async function akNav(p, i) { await p.locator(".gk-navbtn").nth(i).click(); await wait(p, 300); }

const SHOTS = {
  "pc-gamer": { vp: PC, run: async (p) => { await openGamer(p, "Lead heros"); } },
  "pc-gamer-fm": { vp: PC, run: async (p) => { await openGamer(p, "Nappe FM verre"); } },
  "pc-gamer-tables": { vp: PC, run: async (p) => { await openGamer(p, "Batterie 16 bits"); await page(p, "Tables et sortie"); } },
  "tel-gamer": { vp: TEL, run: async (p) => { await openGamer(p, "Lead FM cuivre"); await akNav(p, 2); } },
  "gal-gamer": { vp: { width: 1920, height: 1080 }, scale: 1, run: async (p) => { await openGamer(p, "Lead heros"); } },
  "tel-gamer-portable": { vp: TEL, run: async (p) => { await openGamer(p, "Lead onde voix"); await akNav(p, 1); } },
};

(async () => {
  const b = await chromium.launch();
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
    } catch (e) { console.log("ÉCHEC", name, String(e).split("\n")[0]); if (process.env.SHOT_DEBUG) await p.screenshot({ path: "/tmp/gsh/fail.png" }); }
    await p.close();
  }
  await b.close();
})();
