// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : cartes (titre, galerie, plateformes, fin, fond des scènes téléphone) et légendes de la vidéo
// « de la 0 à la 1.1 », FR et EN, dans /tmp/vid11/cards-<langue>. Polices et images : /tmp/vid/web.
// Usage : node cards11.cjs
const { chromium } = require("playwright");
const fs = require("fs");
const SC = require("./scenes11.cjs");
const W = "/tmp/vid/web";
const G = "/home/claude/groovebox/docs/img/galerie";

const TXT = {
  fr: {
    intro: ["OFAD GOLEM 1.1", "Tout ce que fait la groovebox, de la 0 à la 1.1", "Avec la démo Tribe · Windows · Mac · Android · iPhone"],
    plat: ["Gratuit et libre", ["Windows : application et plugin VST3", "Mac : application, VST3 et AU (version d'essai)", "Android, iPhone / iPad", "Français · English"], "github.com/cookhenri-art/ofad-golem-groovebox"],
    outro: ["OFAD GOLEM", "Dites-moi quelle machine vous pilotez avec", "One For All Distribution"],
    galCap: ["38 instruments, 54 effets", "Batteries, basses, synthés, FM, vocodeurs… et les nouveaux Titan FX et outils de restauration"],
    badge: "NOUVEAU 1.1",
  },
  en: {
    intro: ["OFAD GOLEM 1.1", "Everything the groovebox does, from 0 to 1.1", "With the Tribe demo · Windows · Mac · Android · iPhone"],
    plat: ["Free and open source", ["Windows: app and VST3 plugin", "Mac: app, VST3 and AU (preview)", "Android, iPhone / iPad", "English · Français"], "github.com/cookhenri-art/ofad-golem-groovebox"],
    outro: ["OFAD GOLEM", "Tell me which machine you drive with it", "One For All Distribution"],
    galCap: ["38 instruments, 54 effects", "Drums, basses, synths, FM, vocoders… and the new Titan FX and restoration tools"],
    badge: "NEW IN 1.1",
  },
};

const BASE = `<style>
@font-face { font-family: Inter; src: url(${W}/inter-latin-wght-normal.woff2) format("woff2"); font-weight: 100 900; }
@font-face { font-family: Inter; src: url(${W}/inter-latin-ext-wght-normal.woff2) format("woff2"); font-weight: 100 900; unicode-range: U+0100-024F; }
@font-face { font-family: Bebas; src: url(${W}/bebas-neue-latin-400-normal.woff2) format("woff2"); }
* { box-sizing: border-box; margin: 0; }
html, body { width: 1920px; height: 1080px; overflow: hidden; font-family: Inter, sans-serif; color: #eef1f5; }
.bg { position: absolute; inset: 0; background: radial-gradient(120% 90% at 50% 20%, #23272f, #0c0e12 70%); }
.gold { color: #f2b84b; }
</style>`;
const card = (inner) => `<!doctype html><html><head><meta charset="utf-8">${BASE}</head><body><div class="bg"></div>${inner}</body></html>`;

const intro = (t) => card(`<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:70px">
  <img src="${W}/golem-source.png" style="width:520px;height:520px;filter:drop-shadow(0 30px 40px rgba(0,0,0,.6))">
  <div style="max-width:1150px"><div style="font:400 160px/0.9 Bebas;letter-spacing:.05em">${t[0]}</div>
  <div style="font:650 44px/1.2 Inter;margin-top:26px" class="gold">${t[1]}</div>
  <div style="font:500 30px Inter;margin-top:26px;opacity:.75;letter-spacing:.04em">${t[2]}</div></div></div>`);
const plat = (t) => card(`<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:80px">
  <img src="${W}/golem-source.png" style="width:420px;height:420px">
  <div><div style="font:400 120px/0.95 Bebas;letter-spacing:.05em" class="gold">${t[0]}</div>
  <ul style="list-style:none;padding:0;margin-top:30px;font:500 38px/1.7 Inter">${t[1].map((x) => `<li>✓ ${x}</li>`).join("")}</ul>
  <div style="margin-top:36px;font:600 36px Inter;padding:16px 26px;border-radius:14px;background:rgba(242,184,75,.14);border:2px solid #f2b84b;display:inline-block">${t[2]}</div></div></div>`);
const outro = (t) => card(`<div style="position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center">
  <img src="${W}/golem-source.png" style="width:420px;height:420px">
  <div style="font:400 130px/0.9 Bebas;letter-spacing:.06em;margin-top:10px">${t[0]}</div>
  <div style="font:600 46px Inter;margin-top:18px" class="gold">${t[1]}</div>
  <div style="font:500 30px Inter;margin-top:22px;opacity:.7;letter-spacing:.1em">${t[2]}</div></div>`);
const gallery = (img) => card(`<div style="position:absolute;inset:40px 60px 220px;display:flex;align-items:center;justify-content:center">
  <img src="file://${G}/${img}" style="max-width:100%;max-height:100%;border-radius:18px;box-shadow:0 30px 60px rgba(0,0,0,.6)"></div>`);
// fond des scènes téléphone : un cadre de téléphone en paysage (l'image de l'écran est posée dedans au montage)
const phoneBg = () => card(`<div style="position:absolute;left:50%;top:28px;transform:translateX(-50%);width:1800px;height:832px;border-radius:64px;
  background:#050608;box-shadow:0 0 0 4px #3a3f48,0 40px 80px rgba(0,0,0,.65)"></div>`);
const caption = (c, badge) => `<!doctype html><html><head><meta charset="utf-8">${BASE}<style>html,body{background:transparent}</style></head><body>
  <div style="position:absolute;left:56px;bottom:52px;max-width:1360px;padding:24px 34px 26px;border-radius:20px;background:rgba(10,12,16,.84);
    box-shadow:0 18px 50px rgba(0,0,0,.55),inset 0 0 0 1px rgba(255,255,255,.08);border-left:6px solid #f2b84b">
    <div style="display:flex;gap:16px;align-items:center"><span style="font:700 22px Inter;letter-spacing:.22em" class="gold">OFAD GOLEM</span>
    ${badge ? `<span style="font:800 20px Inter;letter-spacing:.12em;color:#0c0e12;background:#f2b84b;padding:4px 12px;border-radius:8px">${badge}</span>` : ""}</div>
    <div style="font:750 52px/1.15 Inter;margin-top:8px">${c[0]}</div>
    <div style="font:500 31px/1.3 Inter;margin-top:10px;opacity:.86">${c[1]}</div></div></body></html>`;

(async () => {
  const b = await chromium.launch();
  const p = await (await b.newContext({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 })).newPage();
  const shot = async (html, path, transparent) => {
    fs.writeFileSync(`${W}/_t11.html`, html);
    await p.goto(`file://${W}/_t11.html`); await p.waitForTimeout(250);
    await p.evaluate(() => document.fonts.ready);
    await p.screenshot({ path, omitBackground: !!transparent });
  };
  for (const lang of ["fr", "en"]) {
    const t = TXT[lang], d = `/tmp/vid11/cards-${lang}`;
    fs.mkdirSync(d, { recursive: true });
    await shot(intro(t.intro), `${d}/s01.png`);
    await shot(plat(t.plat), `${d}/s34.png`);
    await shot(outro(t.outro), `${d}/s35.png`);
    await shot(phoneBg(), `${d}/phone-bg.png`);
    for (const [i, g] of ["instruments-1.jpg", "instruments-4.jpg", "instruments-5.jpg", "effets-5.jpg"].entries()) await shot(gallery(g), `${d}/s33-${i + 1}.png`);
    await shot(caption(t.galCap, ""), `${d}/cap-s33.png`, true);
    for (const s of SC.scenes(lang)) if (s.cap) await shot(caption(s.cap, s.v11 ? t.badge : ""), `${d}/cap-${s.id}.png`, true);
    console.log(lang, "cartes faites");
  }
  await b.close();
})();
