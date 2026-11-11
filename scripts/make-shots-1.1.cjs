// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : captures des nouveautés de la 1.1 (page GitHub, notice), prises dans l'aperçu de l'interface
// (moteur simulé du navigateur). PC : 1600 × 1000 en ×2 ; téléphone : 915 × 412 en ×2.
// Prérequis : « npx vite --port 4323 » dans ui/ ; Playwright installé.
// Usage : node scripts/make-shots-1.1.cjs <dossier de sortie> [nom…]   (PNG, à convertir en JPEG)
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");
const out = process.argv[2] || "captures-1.1";
const only = process.argv.slice(3);
fs.mkdirSync(out, { recursive: true });
const URL = "http://localhost:4323/";
const PC = { width: 1600, height: 1000 }, TEL = { width: 915, height: 412 };

const wait = (p, ms = 300) => p.waitForTimeout(ms);
async function openTrack(p, row, tab, name) {
  await p.getByRole("button", { name: /^pistes$/i }).first().click(); await wait(p, 250);
  await p.click(`.track-row >> nth=${row}`); await wait(p, 200);
  if (tab) {
    await p.getByRole("tab", { name: tab }).first().click(); await wait(p, 200);
    await p.locator(".browser").getByText(name, { exact: true }).first().dblclick(); await wait(p, 600);
  }
}
const openFace = async (p) => { await p.getByRole("button", { name: /ouvrir (le )?synth/i }).first().click(); await wait(p, 900); };
async function longPress(p, loc) {
  const bb = await loc.boundingBox();
  await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.mouse.down(); await wait(p, 650); await p.mouse.up(); await wait(p, 400);
}
async function chooseStep(p, n) {
  const dock = p.locator(".steps-dock");
  await dock.getByRole("radio", { name: "À verrous" }).click(); await wait(p);
  const step = dock.locator(".ot-steps-keys button").nth(n);
  await step.click(); await wait(p, 200);
  await longPress(p, step);
}
async function midiTrack(p) {
  await openTrack(p, 5, "MIDI externe", "Piste MIDI");
  await openFace(p);
}
async function renameSections(p) {
  const names = ["Filtre", "Enveloppes", "Effets", "Arpège"];
  for (let i = 0; i < names.length; ++i) {
    await p.locator(".midi-sec-title").nth(i).click();
    await p.locator(".midi-sec-input").fill(names[i]); await p.keyboard.press("Enter"); await wait(p, 200);
  }
}

const PC_TALL = { width: 1600, height: 1300 };
async function nav(p, name) { await p.getByRole("button", { name: new RegExp(`^${name}$`, "i") }).first().click(); await wait(p, 350); }
async function midiTrackNamed(p, tel = false) {
  await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await wait(p, 200);
  await p.getByRole("tab", { name: "MIDI externe" }).first().click(); await wait(p, 200);
  await p.locator(".browser").getByText("Piste MIDI", { exact: true }).first().dblclick(); await wait(p, 600);
  await nav(p, "Pistes");
  const n = p.locator('input[aria-label="Nom de la piste"]').first();
  if (await n.count()) { await n.fill("Synthé externe"); await n.press("Tab"); await wait(p, 300); }
  await openFace(p);
}
const hideDock = async (p) => { const m = p.getByRole("button", { name: /^Masquer/ }); if (await m.count()) { await m.first().click(); await wait(p); } };
async function pickEffect(p, current, name) {
  await nav(p, "Effets");
  await p.locator("button").filter({ hasText: current }).first().click(); await wait(p);
  await p.getByRole("button", { name: /⇄ Effet/ }).first().click(); await wait(p, 400);
  await p.locator(".fxpick .ipick-card").filter({ hasText: name }).first().click(); await wait(p, 600);
}
async function remote(p, tab) {
  await p.getByText("Télécommande · piloter le PC ou un FM-1").click(); await wait(p, 700);
  if (tab) { await p.getByRole("button", { name: tab, exact: true }).first().click(); await wait(p, 500); }
}

const SHOTS = {
  // --- Piste MIDI ---------------------------------------------------------------------------------
  "pc-midi-piste": { vp: PC_TALL, run: async (p) => { await midiTrackNamed(p); await renameSections(p); await hideDock(p); } },
  "pc-midi-reglage-bouton": { vp: PC_TALL, run: async (p) => { await midiTrackNamed(p); await renameSections(p); await hideDock(p); await p.locator(".midi-knob-gear").nth(1).click(); await wait(p); } },
  "pc-midi-pads": { vp: PC_TALL, run: async (p) => {
    await midiTrackNamed(p); await renameSections(p); await hideDock(p);
    await p.locator(".midi-pad-wrap .midi-knob-gear").nth(2).click(); await wait(p);
    await p.locator('select[aria-label="Type de pad"]').selectOption("2"); await wait(p);
    for (let i = 0; i < 2; ++i) { await p.locator(".midi-pad").nth(2).click(); await wait(p, 150); }
  } },
  "pc-midi-son-du-pas": { vp: PC, run: async (p) => {
    await midiTrackNamed(p); await chooseStep(p, 4);
    await p.locator('select[aria-label="Son joué sur ce pas"]').selectOption("12"); await wait(p);
  } },
  "tel-midi-piste": { vp: TEL, run: async (p) => { await midiTrackNamed(p, true); } },
  // --- Son du pas sur un synthé -------------------------------------------------------------------
  "pc-son-du-pas": { vp: PC, run: async (p) => {
    await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await openFace(p); await chooseStep(p, 2);
    await p.locator('select[aria-label="Son joué sur ce pas"]').selectOption("3"); await wait(p);
  } },
  // --- Courbes de l'écran Son --------------------------------------------------------------------
  "pc-courbes": { vp: PC, run: async (p) => { await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await nav(p, "Son"); await p.getByRole("tab", { name: "Enveloppe", exact: true }).first().click(); await wait(p, 500); } },
  // --- Écran Audio --------------------------------------------------------------------------------
  "pc-audio": { vp: PC, run: async (p) => {
    await p.getByRole("button", { name: /audio/i }).first().click(); await wait(p, 500);
    for (const t of ["2", "3", "6"]) { await p.locator(".aud").getByRole("button", { name: t, exact: true }).first().click(); await wait(p, 150); }
    await p.getByText("+ Ajouter un effet").first().click(); await wait(p, 400);
    await p.locator(".fxpick .ipick-card").filter({ hasText: "Débruiteur" }).first().click(); await wait(p, 500);
  } },
  // --- Éditeur d'échantillon ----------------------------------------------------------------------
  "pc-editeur-echantillon": { vp: PC, run: async (p) => {
    await nav(p, "Pistes"); const r = p.locator(".track-row").nth(9); await r.scrollIntoViewIfNeeded(); await r.click({ button: "right" }); await wait(p, 900);
  } },
  // --- Golem Titan FX et Restauration ------------------------------------------------------------
  "pc-titan-espace": { vp: PC, run: async (p) => { await pickEffect(p, "Cement", "Titan Espace"); await p.getByRole("button", { name: /Ouvrir le module/ }).first().click(); await wait(p, 700); } },
  "pc-titan-pedalier": { vp: PC, run: async (p) => {
    await pickEffect(p, "Prism", "Titan Vintage"); await pickEffect(p, "Cement", "Titan Écho"); await pickEffect(p, "Ricochet", "Titan Décapiteur");
    await p.getByRole("button", { name: /Pédalier/ }).first().click(); await wait(p, 800);
  } },
  "pc-titan-limiteur": { vp: PC, run: async (p) => { await pickEffect(p, "Sentinel", "Titan Limiteur"); await p.getByRole("button", { name: /Ouvrir le module/ }).first().click(); await wait(p, 700); } },
  "pc-restauration": { vp: PC, run: async (p) => {
    await pickEffect(p, "Prism", "Débruiteur"); await pickEffect(p, "Cement", "Anti-ronflement"); await pickEffect(p, "Ricochet", "Dé-réverb");
    await p.getByRole("button", { name: /Pédalier/ }).first().click(); await wait(p, 800);
  } },
  "pc-latence": { vp: PC, run: async (p) => { await pickEffect(p, "Prism", "Débruiteur"); await nav(p, "Mixeur"); } },
  "tel-courbes": { vp: TEL, run: async (p) => { await nav(p, "Pistes"); await p.click(".track-row >> nth=5"); await nav(p, "Son"); } },
  "tel-audio": { vp: TEL, run: async (p) => { await p.getByRole("button", { name: /audio/i }).first().click(); await wait(p, 600); } },
  // --- Télécommande (téléphone) -------------------------------------------------------------------
  "tel-telecommande-connexion": { vp: TEL, query: "?accueil", run: async (p) => { await remote(p); } },
  "tel-telecommande-mixage": { vp: TEL, query: "?accueil", run: async (p) => { await remote(p, "Mixage"); } },
  "tel-telecommande-boutons": { vp: TEL, query: "?accueil", run: async (p) => { await remote(p, "Boutons"); } },
  "tel-telecommande-chaos": { vp: TEL, query: "?accueil", run: async (p) => { await remote(p, "Chaos"); } },
  "tel-telecommande-fm1": { vp: TEL, query: "?accueil", run: async (p) => { await remote(p, "FM-1"); } },
};

(async () => {
  const b = await chromium.launch();
  for (const [name, s] of Object.entries(SHOTS)) {
    if (only.length && !only.includes(name)) continue;
    const p = await b.newPage({ viewport: s.vp, deviceScaleFactor: 2 });
    const errs = []; p.on("pageerror", (e) => errs.push(e.message));
    try {
      await p.goto(URL + (s.query || "")); await wait(p, 1600);
      await s.run(p); await wait(p, 400);
      await p.screenshot({ path: path.join(out, name + ".png") });
      console.log("ok", name, errs.length ? "ERREURS " + errs.slice(0, 2).join(" | ") : "");
    } catch (e) { console.log("ÉCHEC", name, String(e).split("\n")[0]); }
    await p.close();
  }
  await b.close();
})();
