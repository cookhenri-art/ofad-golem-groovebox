// Capture des scènes de la vidéo OFAD GOLEM depuis la maquette (images JPEG, 15 i/s)
const { chromium } = require("playwright");
const fs = require("fs");
const lang = process.argv[2] || "fr";
const only = process.argv[3] || "";
const out = `/tmp/vid/cap-${lang}`;
const FPS = 15;
const T = lang === "en" ? { play: "Play", pedal: "Pedalboard" } : { play: "Lecture", pedal: "Pédalier" };

async function film(p, name, secs, step) {
  const dir = `${out}/${name}`;
  fs.rmSync(dir, { recursive: true, force: true }); fs.mkdirSync(dir, { recursive: true });
  const n = Math.max(1, Math.round(secs * FPS * (process.env.SHORT ? 0.07 : 1)));
  const t0 = Date.now();
  for (let i = 0; i < n; i++) {
    if (step) await step(i, n);
    const due = t0 + (i * 1000) / FPS; const w = due - Date.now(); if (w > 0) await p.waitForTimeout(w);
    await p.screenshot({ path: `${dir}/f${String(i).padStart(4, "0")}.jpg`, type: "jpeg", quality: 92 });
  }
  console.log(name, n, "images en", ((Date.now() - t0) / 1000).toFixed(1), "s");
}
const clickText = (p, txt) => p.evaluate((t) => { const b = [...document.querySelectorAll("button")].find((x) => x.textContent.trim() === t || x.textContent.trim().startsWith(t)); if (b) b.click(); return !!b; }, txt);
const screen = (p, i) => p.locator(".button-row--screens .hw").nth(i).click();
const play = (p) => p.locator("button.hw--lg").nth(1).click();

(async () => {
  const b = await chromium.launch();
  const ctx = await b.newContext({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 });
  await ctx.addInitScript((l) => { try { localStorage.setItem("groovebox.lang", l); } catch {} }, lang);
  const p = await ctx.newPage();
  p.on("pageerror", (e) => console.log("PAGEERR", e.message));
  await p.goto("http://localhost:4323"); await p.waitForTimeout(2000);
  await play(p); await p.waitForTimeout(800);
  const want = (n) => !only || only.split(",").includes(n);

  if (want("s02")) { await screen(p, 1); await p.waitForTimeout(600); await film(p, "s02", 12); }
  if (want("s03")) { await screen(p, 2); await p.waitForTimeout(600); await film(p, "s03", 14); }
  if (want("s04")) {
    // façade de la basse acide (piste 2)
    await screen(p, 1); await p.waitForTimeout(400);
    await p.locator('.launch-head[data-col="1"]').click(); await p.waitForTimeout(400);
    await p.locator(".top-synth").click(); await p.waitForTimeout(1200);
    await film(p, "s04", 14);
    await p.keyboard.press("Escape"); await p.waitForTimeout(500);
  }
  if (want("s06")) {
    await screen(p, 1); await p.waitForTimeout(400);
    await p.locator('.launch-head[data-col="0"]').click(); await p.waitForTimeout(300);
    await screen(p, 4); await p.waitForTimeout(500);
    await p.locator(".fx-chain-btn").nth(1).click(); await p.waitForTimeout(1200);
    await film(p, "s06a", 8);
    await p.locator(".rk-open").first().click(); await p.waitForTimeout(900);
    await film(p, "s06b", 4);
    await p.locator(".mp-presets-btn").click(); await p.waitForTimeout(700);
    await film(p, "s06c", 4);
    await p.keyboard.press("Escape"); await p.waitForTimeout(300); await p.keyboard.press("Escape"); await p.waitForTimeout(500);
  }
  if (want("s07") || want("s08")) {
    await screen(p, 6); await p.waitForTimeout(500);
    await p.locator(".chip--live").first().click(); await p.waitForTimeout(900);
    if (want("s07")) {
      await p.locator(".vecfull-mode button").nth(0).click(); await p.waitForTimeout(700);
      const box = await p.locator(".livexf-track").first().boundingBox().catch(() => null);
      if (box) {
        const y = box.y + box.height / 2, x0 = box.x + 8, x1 = box.x + box.width - 8;
        await p.mouse.move(x0, y); await p.mouse.down();
        await film(p, "s07", 14, async (i, n) => { const ph = i / (n - 1); const k = ph < 0.5 ? ph * 2 : 2 - ph * 2; await p.mouse.move(x0 + (x1 - x0) * k, y); });
        await p.mouse.up();
      } else { console.log("pas de crossfader"); await film(p, "s07", 14); }
    }
    if (want("s08")) {
      await p.locator(".vecfull-mode button").nth(1).click(); await p.waitForTimeout(500);
      const on = await p.locator(".vecfull-active.is-on").count();
      if (!on) { await p.locator(".vecfull-active").click().catch(() => {}); await p.waitForTimeout(300); }
      await p.getByRole("button", { name: lang === "en" ? "Circle" : "Cercle", exact: true }).first().click().catch((e) => console.log("cercle ?", e.message));
      await p.waitForTimeout(600);
      const pb = await p.locator(".vecfull-pad").first().boundingBox().catch(() => null);
      if (pb) {
        const cx = pb.x + pb.width / 2, cy = pb.y + pb.height / 2, r = Math.min(pb.width, pb.height) * 0.3;
        await p.mouse.move(cx, cy); await p.mouse.down();
        await film(p, "s08", 14, async (i, n) => {
          const a = (i / n) * Math.PI * 4, rr = r * Math.min(1, i / 20);
          await p.mouse.move(cx + Math.cos(a) * rr, cy + Math.sin(a) * rr * 0.8);
        });
        await p.mouse.up();
      } else await film(p, "s08", 14);
    }
    await p.keyboard.press("Escape"); await p.waitForTimeout(500);
  }
  if (want("s09")) { await screen(p, 5); await p.waitForTimeout(600); await film(p, "s09", 14); }
  await ctx.close();

  if (want("s10")) {
    // téléphone (paysage) : Commandes et Vue live, en lecture
    const c2 = await b.newContext({ viewport: { width: 891, height: 411 }, deviceScaleFactor: 2, hasTouch: true });
    await c2.addInitScript((l) => { try { localStorage.setItem("groovebox.lang", l); localStorage.setItem("grv.phoneTab", "ctrl"); } catch {} }, lang);
    const q = await c2.newPage();
    await q.goto("http://localhost:4323"); await q.waitForTimeout(2000);
    await q.locator("button.hw--lg").nth(1).click().catch(() => {}); await q.waitForTimeout(1500);
    fs.mkdirSync(`${out}/s10`, { recursive: true });
    await q.screenshot({ path: `${out}/s10/phone-1.jpg`, type: "jpeg", quality: 92 });
    await q.locator(".button-row--screens .hw").nth(6).click().catch(() => {});
    await q.waitForTimeout(600);
    await q.locator(".chip--live").first().click().catch(() => {}); await q.waitForTimeout(1200);
    await q.screenshot({ path: `${out}/s10/phone-2.jpg`, type: "jpeg", quality: 92 });
    await c2.close();
    console.log("s10 ok");
  }
  await b.close();
})();
