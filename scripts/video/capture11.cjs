// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : vidéo « de la 0 à la 1.1 » : capture des scènes dans l'aperçu de l'interface, avec la démo Tribe
// chargée (?demo=tribe). Images JPEG à 15 i/s, une sous-dossier par scène, dans /tmp/vid11/cap-<langue>.
// Usage : node capture11.cjs fr|en [scène,scène…]      (PROBE=1 : 2 images par scène, pour vérifier)
const { chromium } = require("playwright");
const fs = require("fs");
const SC = require("./scenes11.cjs");
const lang = process.argv[2] || "fr";
const only = (process.argv[3] || "").split(",").filter(Boolean);
const OUT = `/tmp/vid11/cap-${lang}`;
const FPS = 15;
const PROBE = !!process.env.PROBE;
const URL = "http://localhost:4323/";

// horloge simulée (page.clock) : les animations de l'aperçu avancent image par image, quelle que soit
// la vitesse des captures ; les attentes font avancer cette horloge
const wait = async (p, ms = 300) => { await p.clock.runFor(ms); await p.waitForTimeout(15); };
async function film(p, name, secs, step) {
  const dir = `${OUT}/${name}`;
  fs.rmSync(dir, { recursive: true, force: true }); fs.mkdirSync(dir, { recursive: true });
  const N = Math.max(1, Math.round(secs * FPS));
  // PROBE : tous les gestes, sans attendre, et trois images (30 %, 60 %, fin) pour vérifier
  const keep = PROBE ? new Set([Math.round(N * 0.3), Math.round(N * 0.6), N - 1]) : null;
  const t0 = Date.now();
  let shot = 0;
  for (let i = 0; i < N; i++) {
    if (step) { try { await step(i, N); } catch (e) { console.log("  pas", name, i, String(e).split("\n")[0]); } }
    if (keep && !keep.has(i)) { await p.clock.runFor(1000 / FPS); continue; }
    await wait(p, 1000 / FPS);
    await p.screenshot({ path: `${dir}/f${String(shot++).padStart(4, "0")}.jpg`, type: "jpeg", quality: 92 });
  }
  const real = (Date.now() - t0) / 1000;
  fs.writeFileSync(`${dir}/info.json`, JSON.stringify({ frames: shot, secs, real }));
  console.log(name, shot, "images en", real.toFixed(1), "s");
}

// --- gestes ----------------------------------------------------------------------------------------
const H = (p) => ({
  nav: async (name) => { await p.getByRole("button", { name: new RegExp(`^${name}$`, "i") }).first().click(); await wait(p, 400); },
  btn: async (name, exact = true) => { await p.getByRole("button", { name, exact }).first().click(); await wait(p, 300); },
  tab: async (name) => { await p.getByRole("tab", { name, exact: true }).first().click(); await wait(p, 350); },
  text: async (t, n = 0) => { await p.getByText(t, { exact: true }).nth(n).click(); await wait(p, 300); },
  track: async (row) => { await p.getByRole("button", { name: /^(pistes|tracks)$/i }).first().click(); await wait(p, 300); await p.locator(".track-row").nth(row).click(); await wait(p, 300); },
  face: async () => { await p.getByRole("button", { name: /ouvrir (le )?synth|open the synth/i }).first().click(); await wait(p, 1000); },
  closeFace: async () => { const b = p.getByRole("button", { name: "Refermer la façade" }); if (await b.count()) { await b.first().click(); await wait(p, 500); } },
  esc: async () => { await p.keyboard.press("Escape"); await wait(p, 400); },
  center: async (loc) => { const bb = await loc.boundingBox(); return bb ? [bb.x + bb.width / 2, bb.y + bb.height / 2, bb] : null; },
  longPress: async (loc) => { const bb = await loc.boundingBox(); await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.mouse.down(); await wait(p, 700); await p.mouse.up(); await wait(p, 400); },
  right: async (loc) => { const bb = await loc.boundingBox(); await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2, { button: "right" }); await wait(p, 450); },
  // glisser un bouton vers le haut ou le bas (k de 0 à 1 le long du geste)
  dragV: async (loc, dy) => { const bb = await loc.boundingBox(); const x = bb.x + bb.width / 2, y = bb.y + bb.height / 2; await p.mouse.move(x, y); await p.mouse.down(); for (let i = 1; i <= 8; i++) { await p.mouse.move(x, y - (dy * i) / 8); await wait(p, 30); } await p.mouse.up(); },
});

(async () => {
  const b = await chromium.launch();
  for (const s of SC.scenes(lang)) {
    if (s.card) continue; // cartes : cards11.cjs
    if (only.length && !only.includes(s.id)) continue;
    const ctx = await b.newContext({ viewport: s.phone ? { width: 915, height: 412 } : { width: 1920, height: 1080 },
      deviceScaleFactor: s.phone ? 2 : 1, hasTouch: !!s.phone });
    await ctx.addInitScript((l) => { try { localStorage.setItem("groovebox.lang", l); localStorage.setItem("grv.welcome", "0"); } catch {} }, lang);
    const p = await ctx.newPage();
    await p.clock.install({ time: new Date("2026-11-11T12:00:00") });
    p.setDefaultTimeout(5000); // un geste raté ne bloque pas la capture
    p.on("pageerror", (e) => console.log("  ERREUR page", s.id, e.message));
    await p.goto(URL + (s.query ?? "?demo=tribe")); await wait(p, 2000);
    const h = H(p);
    try {
      if (!s.stopped) { await p.getByRole("button", { name: lang === "en" ? "Play" : "Lecture", exact: true }).first().click().catch(() => {}); await wait(p, 300); }
      if (s.setup) await s.setup(p, h);
      await film(p, s.id, s.secs, s.step ? (i, n) => s.step(p, h, i, n) : null);
    } catch (e) { console.log("ÉCHEC", s.id, String(e).split("\n")[0]); }
    await ctx.close();
  }
  await b.close();
})();
