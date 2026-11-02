// Groovebox — OFAD — AGPL-3.0-or-later
// Miniatures des façades (v0.32) — étape 1/2 : capture de chaque façade dans l'aperçu de l'interface.
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-thumbs.cjs <dossier de sortie> [type…]   puis   python3 scripts/make-thumbs.py <dossier>
//   (types facultatifs : ne refaire que ces miniatures, ex. « mono »)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || "thumbs-shots";
const cat = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "ui", "src", "engine", "catalog.json"), "utf8"));
fs.mkdirSync(out, { recursive: true });
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
  await p.goto("http://localhost:4323/"); await p.waitForTimeout(1500);
  const only = process.argv.slice(3);
  for (const inst of cat.instruments.filter((i) => only.length === 0 || only.includes(i.type))) {
    await p.getByRole("button", { name: /^pistes$/i }).first().click(); await p.waitForTimeout(200);
    await p.click(".track-row >> nth=5"); await p.waitForTimeout(150);
    await p.getByRole("tab", { name: inst.family }).first().click(); await p.waitForTimeout(200);
    await p.locator(".browser").getByText(inst.name, { exact: true }).first().click(); await p.waitForTimeout(200);
    await p.getByRole("button", { name: /Mettre sur la piste|Recharger sur/ }).first().click(); await p.waitForTimeout(400);
    await p.getByRole("button", { name: /ouvrir le synth/i }).first().click(); await p.waitForTimeout(900);
    // v0.46 : la façade doit être ouverte (sinon la miniature montrait l'écran Pistes)
    await p.locator(".face").first().waitFor({ state: "visible", timeout: 5000 });
    const tabs = p.locator(".face-pages [role=tab], .face-pages button");
    if (await tabs.count()) { await tabs.first().click(); await p.waitForTimeout(500); }
    await p.screenshot({ path: path.join(out, `${inst.type}.png`) });
    await p.getByRole("button", { name: /fermer/i }).first().click().catch(() => {}); await p.waitForTimeout(300);
  }
  await b.close();
})();
