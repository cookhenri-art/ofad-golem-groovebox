// Groovebox — OFAD — AGPL-3.0-or-later
// Galerie (v0.46) : une image de chaque façade d'instrument (toutes ses pages) et de chaque effet
// (réglages et pédale), prises dans l'aperçu de l'interface, pour la documentation et les livraisons.
// Prérequis : « npm run build » puis « npx vite preview --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-gallery.cjs <dossier de sortie>
//   i01-nom.png (page Son), i01-nom-2.png… (autres pages), e01-nom.png (réglages), p01-nom.png (pédale)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || "galerie";
const part = process.argv[3] || "tout"; // « instruments », « effets » ou « tout »
const cat = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "ui", "src", "engine", "catalog.json"), "utf8"));
fs.mkdirSync(out, { recursive: true });
const slug = (s) => s.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
const num = (n) => String(n).padStart(2, "0");

(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
  p.on("pageerror", (e) => console.log("ERREUR", e.message));
  await p.goto("http://localhost:4323/"); await p.waitForTimeout(1500);
  let i = 0;
  for (const inst of part === "effets" ? [] : cat.instruments) {
    ++i;
    try {
      await p.getByRole("button", { name: /^pistes$/i }).first().click(); await p.waitForTimeout(200);
      await p.click(".track-row >> nth=5"); await p.waitForTimeout(150);
      await p.getByRole("tab", { name: inst.family }).first().click(); await p.waitForTimeout(200);
      await p.locator(".browser").getByText(inst.name, { exact: true }).first().click(); await p.waitForTimeout(200);
      await p.getByRole("button", { name: /Mettre sur la piste|Recharger sur/ }).first().click(); await p.waitForTimeout(400);
      await p.getByRole("button", { name: /ouvrir le synth/i }).first().click(); await p.waitForTimeout(1000);
      const base = `i${num(i)}-${slug(inst.name)}`;
      // la façade garde la page ouverte en dernier : on revient à la première (Son)
      const tabs = p.locator(".face-pages [role=tab], .face-pages button");
      if (await tabs.count()) { await tabs.first().click(); await p.waitForTimeout(600); }
      await p.screenshot({ path: path.join(out, `${base}.png`) });
      // autres pages de la façade (hors Modulations de la piste et Macros, communes à tous)
      const n = await tabs.count();
      let k = 1;
      for (let t = 1; t < n; ++t) {
        const label = (await tabs.nth(t).innerText()).trim();
        if (/Modulations de la piste|Macros/.test(label)) continue;
        await tabs.nth(t).click(); await p.waitForTimeout(600);
        await p.screenshot({ path: path.join(out, `${base}-${++k}.png`) });
      }
      await p.getByRole("button", { name: /Refermer la façade/i }).first().click().catch(() => {}); await p.waitForTimeout(300);
      console.log("instrument", base);
    } catch (e) { console.log("RATÉ", inst.type, e.message.split("\n")[0]); await p.keyboard.press("Escape").catch(() => {}); }
  }
  // effets : la pédale du premier emplacement, puis la page de réglages
  await p.getByRole("button", { name: /^effets$/i }).first().click(); await p.waitForTimeout(400);
  let j = 0;
  for (const fx of part === "instruments" ? [] : cat.effects) {
    ++j;
    try {
      // choix de l'effet dans l'emplacement 1 depuis l'écran Effets (« ⇄ Effet », ou « + Ajouter » au départ)
      const swap = p.locator("button", { hasText: "⇄ Effet" }).first();
      if (await swap.count()) await swap.click(); else await p.locator("button", { hasText: "+ Ajouter" }).first().click();
      await p.waitForTimeout(400);
      await p.locator(".fxpick .ipick-card", { hasText: fx.name }).first().click(); await p.waitForTimeout(900);
      const base = `${num(j)}-${slug(fx.name)}`;
      await p.locator(".fx-chain-btns button", { hasText: "Pédalier" }).first().click(); await p.waitForTimeout(700);
      await p.locator(".pb-board").first().locator(":scope > .pd, :scope > .rk").first().screenshot({ path: path.join(out, `p${base}.png`) });
      await p.getByRole("button", { name: "Fermer le pédalier" }).first().click(); await p.waitForTimeout(600);
      await p.screenshot({ path: path.join(out, `e${base}.png`) });
      console.log("effet", base);
    } catch (e) { console.log("RATÉ", fx.type, e.message.split("\n")[0]); await p.keyboard.press("Escape").catch(() => {}); await p.getByRole("button", { name: "Fermer le pédalier" }).first().click({ timeout: 2000 }).catch(() => {}); }
  }
  await b.close();
})();
