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
  // v0.46 fix27 : clic droit (appui long) sur une valeur du bas de l'écran
  const rightClick = (sel, nth = 0) => async (p) => { const bb = await p.locator(sel).nth(nth).boundingBox(); await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2, { button: "right" }); await p.waitForTimeout(450); };
  const pick = (name) => async (p) => { await p.getByText("Pistes", { exact: true }).first().click(); await p.waitForTimeout(350); await p.getByText(name).first().click(); await p.waitForTimeout(400); };
  await shot("menu-bouton", ["Son", rightClick(".screen-encoders .enc-label", 1)]);
  // 16 boutons assignables : le menu d'un bouton, puis l'attente d'une valeur à piloter
  await shot("boutons-assignables", ["Son", rightClick(".aknob .encoder-cap", 0)]);
  await shot("assignation", ["Séquence", rightClick(".aknob .encoder-cap", 2), async (p) => { await p.getByText("Assigner à…").first().click(); await p.waitForTimeout(500); }]);
  // pavé vectoriel (Scènes › Vecteur) et sa vue en grand
  await shot("vecteur", ["Scènes", "Vecteur", "Infini"]);
  await shot("vecteur-grand", ["Scènes", "Vecteur", "Triangle", "⛶ En grand"], { full: true });
  // façade d'un synthé : la rangée de pas au-dessus du clavier
  await shot("facade-pas", [pick("Fatsub"), "Ouvrir le synthé ↗"]);
  // valeur d'un pas (force de l'accent) : appui long sur une case ACCENT
  await shot("pas-valeur", [pick("Acid Lover"), "Séquence", rightClick(".flag-row--acc .fcell", 4), async (p) => {
    for (let i = 0; i < 4; i++) { await p.locator(".stepamt-btn").nth(1).dispatchEvent("pointerdown"); await p.waitForTimeout(40); await p.evaluate(() => window.dispatchEvent(new PointerEvent("pointerup"))); await p.waitForTimeout(120); }
  }], { full: true });
  await b.close();
})();
