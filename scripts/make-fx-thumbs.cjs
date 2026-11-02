// Groovebox — OFAD — AGPL-3.0-or-later
// Vignettes des effets (v0.34) — capture de chaque effet dans le pédalier de l'aperçu de l'interface.
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-fx-thumbs.cjs <dossier de sortie>   (PNG ; puis conversion en WebP 320 × 240)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || "fxthumbs-shots";
const only = process.argv[3] ? process.argv[3].split(",") : null; // types à refaire (facultatif)
const cat = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "ui", "src", "engine", "catalog.json"), "utf8"));
fs.mkdirSync(out, { recursive: true });
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
  await p.goto("http://localhost:4323/"); await p.waitForTimeout(1500);
  await p.getByRole("button", { name: /^effets$/i }).first().click(); await p.waitForTimeout(400);
  await p.locator(".fx-chain-btns button", { hasText: "Pédalier" }).first().click(); await p.waitForTimeout(500);
  for (const fx of cat.effects.filter((e) => !only || only.includes(e.type))) {
    const first = p.locator(".pb-board").first().locator(":scope > .pd, :scope > .rk").first();
    await first.locator(".pd-swap, .pd-add").first().click({ force: true }); await p.waitForTimeout(400);
    await p.locator(".fxpick .ipick-card", { hasText: fx.name }).first().click(); await p.waitForTimeout(900);
    await p.locator(".pb-board").first().locator(":scope > .pd, :scope > .rk").first().screenshot({ path: path.join(out, `${fx.type}.png`) });
  }
  await b.close();
})();
