// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix9 : captures d'écran de la notice (docs/img/), prises dans l'interface simulée du
// navigateur (données de démonstration). Lancer « npx vite preview --port 4323 » dans ui/, puis :
//   NODE_PATH=$(npm root -g) node scripts/make-doc-shots.cjs  (puis conversion en JPEG 1100 px)
const { chromium } = require("playwright");
const path = require("path");
const OUT = path.join(__dirname, "..", "docs", "img");
const URL = "http://localhost:4323/";

(async () => {
  const b = await chromium.launch();
  const shot = async (name, steps, opts = {}) => {
    const p = await b.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
    await p.goto(URL + (opts.query || "")); await p.waitForTimeout(1600);
    for (const s of steps) {
      if (typeof s === "function") await s(p);
      else { await p.getByText(s, { exact: true }).first().click(); await p.waitForTimeout(450); }
    }
    await p.waitForTimeout(400);
    const dev = await p.locator(".device").boundingBox();
    await p.screenshot({ path: path.join(OUT, name + ".png"), clip: opts.full ? undefined : { x: dev.x, y: dev.y, width: dev.width, height: dev.height } });
    await p.close();
    console.log(name);
  };
  await shot("accueil", [], { query: "?accueil" });
  await shot("appareil", ["Lancement"]);
  await shot("pistes", ["Pistes"]);
  await shot("sequence", ["Séquence"]);
  await shot("son", ["Son"]);
  await shot("facade", ["Ouvrir le synthé ↗"]);
  await shot("pedalier", ["Ouvrir le synthé ↗", "FX · Effets"]);
  await shot("effets", ["Effets"]);
  await shot("mixeur", ["Mixeur"]);
  await shot("scenes", ["Scènes"]);
  await shot("morceau", ["Lancement", "Morceau"]);
  await shot("montage", ["Lancement", "Audio", async (p) => { const c = p.locator(".mt-clip").nth(2); const bb = await c.boundingBox(); await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.waitForTimeout(300); }]);
  await shot("reglages", ["Réglages"]);
  await shot("menu-bouton", ["Son", async (p) => { const e = p.locator(".encoder-row [role='slider'], .encoder-row .encoder").first(); const bb = await e.boundingBox(); await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2, { button: "right" }); await p.waitForTimeout(400); }]);
  await b.close();
})();
