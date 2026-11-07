// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix12 : captures ANNOTÉES de la notice (docs/img/*-reperes.png) — cadres orange et étiquettes
// posés par-dessus l'interface simulée pour montrer où se trouve chaque commande dont parle le texte.
// Lancer « npx vite preview --port 4323 » dans ui/, puis :
//   NODE_PATH=$(npm root -g) node scripts/make-doc-reperes.cjs  (puis conversion en JPEG 1100 px)
const { chromium } = require("playwright");
const path = require("path");
const OUT = path.join(__dirname, "..", "docs", "img");
const URL = "http://localhost:4323/";

/** Pose les repères : { sel | rect, nth, text, pos: t|b|l|r|in, dx, dy }. */
async function mark(p, items, letters = []) {
  const boxes = [];
  for (const it of items) {
    let r = it.rect;
    if (!r) {
      const loc = it.text0 ? p.getByText(it.text0, { exact: true }).nth(it.nth || 0) : p.locator(it.sel).nth(it.nth || 0);
      const bb = await loc.boundingBox();
      if (!bb) { console.warn("introuvable :", it.sel || it.text0); continue; }
      r = [bb.x, bb.y, bb.width, bb.height];
      if (it.sel2) { // cadre qui englobe deux éléments
        const b2 = await p.locator(it.sel2).nth(it.nth2 || 0).boundingBox();
        if (b2) { const x0 = Math.min(r[0], b2.x), y0 = Math.min(r[1], b2.y); r = [x0, y0, Math.max(r[0] + r[2], b2.x + b2.width) - x0, Math.max(r[1] + r[3], b2.y + b2.height) - y0]; }
      }
    }
    boxes.push({ r, text: it.text, pos: it.pos || "b", dx: it.dx || 0, dy: it.dy || 0, n: it.n });
  }
  await p.evaluate(([boxes, letters]) => {
    const host = document.createElement("div");
    host.style.cssText = "position:fixed;inset:0;pointer-events:none;z-index:99999;font-family:Inter,system-ui,sans-serif";
    document.body.appendChild(host);
    const W = window.innerWidth, H = window.innerHeight;
    for (const b of boxes) {
      const [x, y, w, h] = b.r;
      const f = document.createElement("div");
      f.style.cssText = `position:absolute;left:${x - 3}px;top:${y - 3}px;width:${w + 6}px;height:${h + 6}px;border:3px solid #ff8a1f;border-radius:7px;box-shadow:0 0 0 2px rgba(0,0,0,.55),0 0 14px rgba(255,138,31,.55)`;
      host.appendChild(f);
      if (!b.n) continue;
      const t = document.createElement("div");
      t.textContent = String(b.n);
      t.style.cssText = "position:absolute;width:30px;height:30px;line-height:30px;border-radius:15px;background:#ff8a1f;color:#111;text-align:center;font:800 18px Inter,system-ui,sans-serif;box-shadow:0 0 0 3px #15171b,0 3px 10px rgba(0,0,0,.6)";
      host.appendChild(t);
      let lx = x - 17 + b.dx, ly = y - 17 + b.dy;
      lx = Math.max(4, Math.min(W - 34, lx)); ly = Math.max(4, Math.min(H - 34, ly));
      t.style.left = lx + "px"; t.style.top = ly + "px";
    }
    // lettres du clavier de l'ordinateur sur les pads
    letters.forEach(([sel, i, l]) => {
      const el = document.querySelectorAll(sel)[i];
      if (!el) return;
      const r = el.getBoundingClientRect();
      const k = document.createElement("div");
      k.textContent = l;
      k.style.cssText = `position:absolute;left:${r.x + r.width / 2 - 22}px;top:${r.y + r.height / 2 - 22}px;width:44px;height:44px;line-height:40px;text-align:center;border-radius:9px;background:#15171b;color:#ffb36b;border:2px solid #ff8a1f;font:700 24px Inter,system-ui,sans-serif;box-shadow:0 3px 10px rgba(0,0,0,.6)`;
      host.appendChild(k);
    });
  }, [boxes, letters]);
}

(async () => {
  const b = await chromium.launch();
  // 1.0 : une seule vue (écran + rail à droite). « rail:N » ouvre le N-ième écran du rail (0 = Pistes … 7 = Réglages).
  const shot = async (name, steps, marks, opts = {}) => {
    const p = await b.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
    await p.addInitScript(() => { try { localStorage.setItem("groovebox.sidePads", "0"); } catch { /* */ } });
    await p.goto(URL); await p.waitForTimeout(1800);
    await p.evaluate(() => { try { localStorage.removeItem("grv.stepView.synth"); } catch { /* */ } });
    for (const s of steps) {
      if (typeof s === "function") await s(p);
      else if (s.startsWith("rail:")) { await p.locator(".zoom-rail > button").nth(Number(s.slice(5))).click(); await p.waitForTimeout(500); }
      else if (s.startsWith("css:")) { await p.locator(s.slice(4)).first().click(); await p.waitForTimeout(450); }
      else { await p.getByText(s, { exact: true }).first().click(); await p.waitForTimeout(450); }
    }
    await p.waitForTimeout(400);
    await mark(p, marks, opts.letters || []);
    await p.waitForTimeout(150);
    const clip = opts.clip ? await p.locator(opts.clip).boundingBox() : await p.locator(".device").boundingBox();
    const pad = opts.pad ?? 0;
    await p.screenshot({ path: path.join(OUT, name + ".png"), clip: { x: Math.max(0, clip.x - pad), y: Math.max(0, clip.y - pad), width: clip.width + 2 * pad, height: clip.height + 2 * pad } });
    await p.close();
    console.log(name);
  };
  const acid = ["rail:0", async (p) => { await p.locator(".track-row", { hasText: "Basse acide" }).first().click(); await p.waitForTimeout(400); }];
  const pads = async (p) => { if (!(await p.locator(".zoom-pads").count())) { await p.locator(".zoom-pads-tab").click(); await p.waitForTimeout(500); } };

  // 1. L'appareil : l'écran, le rail, l'en-tête
  await shot("appareil-reperes", ["rail:1"], [
    { sel: ".top-synth", text: "Façade du synthé en grand", n: 1 },
    { sel: ".top-hist", text: "Annuler · Rétablir · Photo · Rappel · Mod. boutons", n: 2 },
    { sel: ".top-rec", text: "Enregistrement audio", n: 3 },
    { sel: ".top-full", text: "Plein écran (F11)", n: 4 },
    { sel: ".zoom-rail > button", nth: 0, sel2: ".zoom-rail > button", nth2: 7, text: "Les 8 écrans", n: 5 },
    { sel: ".zoom-rail-duo", text: "Live · Jeu", n: 6 },
    { sel: ".zoom-rail-tr", text: "Lecture · Arrêt", n: 7 },
    { sel: ".zoom-rail-recbig", text: "Enregistrer", n: 8 },
    { sel: ".zoom-pads-tab", text: "Déplier les pads", n: 9, dx: -20 },
    { sel: ".screen-encoders", text: "Valeurs du bas", n: 10 },
  ]);

  // 1 bis. Les pads dépliés à droite de l'écran
  await shot("pads-reperes", ["rail:2", pads], [
    { sel: ".zoom-pads .jeu-tracknav", text: "Piste jouée", n: 1 },
    { sel: ".zoom-pads .button-row--padmodes", text: "Jouer · Mute · Solo · Scènes · Pas", n: 2 },
    { sel: ".zoom-pads .padgrid", text: "16 pads", n: 3 },
    { sel: ".zoom-pads > .button-row", nth: 1, text: "Banque de scènes · Éditer A / B", n: 4 },
    { sel: ".zoom-pads-tab", text: "Replier", n: 5, dx: -20 },
  ]);

  // 1 ter. L'écran Jeu
  await shot("jeu-reperes", ["css:.zoom-rail-jeu"], [
    { sel: ".jeu-tracknav", text: "Piste jouée", n: 1 },
    { sel: ".jeu-head .rowlaunch", text: "Rangées A à H", n: 2 },
    { sel: ".jeu-pads .button-row--padmodes", text: "Rôle des pads", n: 3 },
    { sel: ".jeu-pads .padgrid", text: "16 pads (Mute / Solo : les 16 pistes)", n: 4 },
    { sel: ".jeu-knobs", text: "16 boutons assignables", n: 5 },
    { sel: ".jeu-ctrl .phone-row--photo", text: "Photo · Rappel · Mod. boutons", n: 6 },
    { sel: ".jeu-ctrl .jeu-row", nth: 0, text: "Fill · Break · Break 1 mes. · Maj", n: 7 },
    { sel: ".jeu-row--xf", text: "Octave · crossfader", n: 8 },
    { sel: ".jeu-ctrl .phone-row--tempo", text: "Tempo", n: 9 },
  ]);

  // 2. Les pads au clavier de l'ordinateur (AZERTY)
  const keys = ["1", "2", "3", "4", "A", "Z", "E", "R", "Q", "S", "D", "F", "W", "X", "C", "V"];
  await shot("pads-clavier", ["rail:2", pads], [
    { sel: ".zoom-pads .button-row--padmodes", text: "Rôle des pads : Jouer · Mute · Solo · Scènes · Pas" },
  ], { clip: ".zoom-pads", pad: 12, letters: keys.map((k, i) => [".zoom-pads .padgrid .pad", i, k]) });

  // 3. Boutons et valeurs : une valeur du bas de l'écran et son menu (clic droit ou appui long)
  await shot("boutons-reperes", ["rail:3", async (p) => {
    const e = p.locator(".screen-encoders .enc-label").nth(1); const bb = await e.boundingBox();
    await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2, { button: "right" }); await p.waitForTimeout(400);
  }], [
    { sel: ".screen-encoders .enc-label", nth: 0, text: "Glisser ↑↓ · double-clic : valeur d'origine", n: 1 },
    { sel: ".mod-menu", text: "Appui long / clic droit : valeur exacte · modulation · MIDI · bouton 1 à 16", n: 2 },
  ]);

  // 4. Séquence : rouleau de notes de la basse, lignes Accent / Glissé, pages
  await shot("sequence-reperes", [...acid, "rail:2", "css:.screen-status .chip--button", "8", "Fermer"], [
    { sel: ".seq-tracknav", text: "Piste affichée", n: 1 },
    { sel: ".screen-status .chip--button", text: "Pattern : mesures · division · vitesse", n: 2 },
    { sel: ".page-nav", text: "Tourner les pages ◀ ▶", n: 3 },
    { sel: ".flag-row--acc", sel2: ".flag-row--slide", text: "Lignes Accent et Glissé", n: 4 },
    { sel: ".roll", text: "Clic : poser / retirer une note · tirer : déplacer", pos: "in", n: 5 },
    { sel: ".seq-tools", text: "Sélection · Pinceau · Tout sélect. · patterns A à H", n: 6 },
    { sel: ".screen-tabs", text: "Notes / Courbes (mouvements ENR.)", n: 7 },
    { sel: ".screen-encoders", text: "Réglages de la piste (rien de choisi)", n: 8 },
  ]);

  // 4 bis. Séquence d'une batterie : un pas choisi (ses réglages en bas) et les boutons du bas
  const pickStep = async (p) => { await p.locator(".cell--on").nth(2).click(); await p.waitForTimeout(450); };
  await shot("sequence-pas", ["rail:2", pickStep], [
    { sel: ".seq-name", nth: 0, text: "Ligne (pad) : la choisir et l'écouter", n: 1 },
    { sel: ".seq-ms", nth: 0, sel2: ".seq-ms", nth2: 1, text: "M / S de la ligne", n: 2 },
    { sel: ".cell--on", nth: 2, text: "Pas choisi", n: 3 },
    { sel: ".flag-row--acc", sel2: ".flag-row--trigless", text: "Accent · Verrous seuls", n: 4 },
    { sel: ".seq-tools", text: "Outils", n: 5 },
    { sel: ".screen-encoders", text: "Réglages du pas choisi", n: 6 },
  ]);
  // 4 ter. Tout sélectionné : les valeurs du bas règlent tous les pas à la fois
  await shot("sequence-selection", ["rail:2", "Tout sélect.", async (p) => { await p.waitForTimeout(6500); }], [
    { sel: ".seq-body", text: "Sélection", n: 1 },
    { sel: ".screen-encoders", text: "Tous les pas choisis à la fois", n: 2 },
    { sel: ".seq-tools", text: "Outils de la sélection", n: 3 },
  ]);

  // 5. Clips › Grille
  await shot("lancement-reperes", ["rail:1"], [
    { sel: ".screen-tabs", text: "Grille · Morceau · Audio", n: 1 },
    { sel: ".launch-hscroll", text: "Toucher une case : lancer et éditer", pos: "r", n: 2 },
    { sel: ".launch-actions", text: "Copier · effacer · éditer le pattern ou la rangée", n: 3 },
    { sel: ".screen-encoders", text: "Piste · pattern · quantification du lancement · rangée", n: 4 },
  ]);

  // 6. Clips › Morceau
  await shot("morceau-reperes", ["rail:1", "Morceau"], [
    { sel: ".song-bar button", nth: 0, text: "Mode morceau / Boucle", n: 1 },
    { sel: ".song-rec", nth: 0, sel2: ".song-rec", nth2: 1, text: "Enregistrer le jeu (rangées) ou les mouvements (courbes)", n: 2 },
    { sel: ".song-scroll", text: "Rangées jouées l'une après l'autre ; courbes en bas", pos: "in", n: 3 },
    { sel: ".song-actions", text: "Ajouter ce qui joue · vider", n: 4 },
  ]);

  // 7. Clips › Audio (montage)
  await shot("montage-reperes", ["rail:1", "Audio", async (p) => { const c = p.locator(".mt-clip").nth(2); const bb = await c.boundingBox(); await p.mouse.click(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.waitForTimeout(300); }], [
    { sel: ".mt-bar button", nth: 1, text: "Prise à la tête de lecture", n: 1 },
    { sel: ".mt-bar button", nth: 0, text: "Écouter le montage", n: 2 },
    { sel: ".mt-clip", nth: 2, text: "Clip : glisser · rogner les bords · clic droit : menu", n: 3 },
    { sel: "text=Export WAV", text: "Export WAV / MP3", n: 4 },
    { sel: ".mt-ruler", text: "Règle : tête de lecture ; clic droit + tirer : boucle", n: 5 },
    { sel: ".segmented--screen", text: "Choisir / Ciseaux", n: 6 },
    { sel: "text=Boucle", text: "Boucle", n: 7 },
  ]);

  // 8. Façade de la basse : séquenceur acide
  await shot("facade-acide", [...acid, "css:.bz-open"], [
    { sel: ".steps-choice-head", text: "Écriture : Acide ou À verrous", n: 1 },
    { sel: ".steps303-pages button", text: "Pages de 16 pas", n: 2 },
    { sel: ".step303-key", nth: 2, text: "Note : toucher pose / retire · tirer ↑↓ ou molette : demi-ton", n: 3 },
    { sel: ".step303 .step303-fn", nth: 10, text: "▲▼ octave", n: 4 },
    { sel: ".step303 .step303-vals", nth: 8, text: "Vélocité · durée", n: 5 },
    { sel: ".step303 .step303-fn", nth: 25, text: "Accent · Glissé", n: 6 },
    { text0: "FX · Effets", text: "Les effets en pédales", n: 7 },
    { sel: "text=Modulations de la piste >> xpath=..", text: "Son · modulations · macros", n: 8 },
    { sel: ".face-btn--swap", text: "Changer d'instrument", n: 9 },
  ]);

  // 9. Façade de la basse : séquenceur à verrous
  await shot("facade-verrous", [...acid, "css:.bz-open", "À verrous", "Verrous", async (p) => { await p.locator(".ot-step").nth(4).click(); await p.waitForTimeout(400); }], [
    { sel: ".ot-steps-pages", text: "Pages 1 à 4 (16 pas chacune)", n: 1 },
    { text0: "Suivre", text: "La page suit la lecture", n: 2 },
    { sel: ".ot-steps-bars", text: "Longueur : 1 à 4 mesures", n: 3 },
    { sel: ".ot-step.sel", text: "Pas choisi : les boutons écrivent dans ce pas", n: 4 },
    { sel: ".ot-steps-locks", text: "Copier · Coller · Effacer les verrous", n: 5 },
  ]);

  // 10. Scènes
  await shot("scenes-reperes", ["rail:6"], [
    { sel: ".slot--A", text: "Scène A : « Éditer » puis tourner les réglages", pos: "in", n: 1 },
    { sel: ".slot--B", text: "Scène B", pos: "in", n: 2 },
    { sel: ".xf-meter", text: "Crossfader A ↔ B", n: 3 },
    { sel: ".scenes-grid", text: "Toucher : lancer (Maj : de l'autre côté) · clic droit : muette", n: 4 },
    { sel: ".scenes-bottom .slot", text: "Scène de Break", n: 5 },
    { sel: ".rowlaunch", text: "Rangées A à H (lancent aussi leur scène liée)", n: 6 },
  ]);
  await b.close();
})();
