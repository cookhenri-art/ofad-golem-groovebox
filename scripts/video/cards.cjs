// Cartes (titre, galeries, téléphones, plateformes, fin) et légendes de la vidéo OFAD GOLEM, FR et EN
const { chromium } = require("playwright");
const fs = require("fs");
const W = "/tmp/vid/web";

const TXT = {
  fr: {
    intro: ["OFAD GOLEM", "La groovebox libre et gratuite", "Windows · Mac · Android"],
    caps: {
      s02: ["16 pistes, 16 pads", "Les patterns se lancent comme des clips, rangée par rangée"],
      s03: ["Le séquenceur pas à pas", "Verrous de réglages, conditions, Fill et Break en direct"],
      s04: ["Le séquenceur acide, sur la façade", "Note, octave, accent et glissé, pas par pas"],
      s05: ["34 instruments recréés de zéro", "Batteries, basses, synthés analogiques, FM, nappes, vocodeurs…"],
      s06: ["38 effets en pédales et en racks", "Avec leurs préréglages, et les vôtres"],
      s07: ["Scènes A / B au crossfader", "Passer d'un mix à l'autre, sans coupure"],
      s08: ["Le pavé vectoriel", "Quatre scènes mélangées par une forme en mouvement"],
      s09: ["Le mixeur", "Pistes, groupes, retours, effets et vumètres"],
      s10: ["Sur téléphone et tablette", "Un contrôleur MIDI USB, et c'est un live complet"],
    },
    plat: ["Gratuit et libre", ["Windows : application et plugin VST3", "Mac : application, VST3 et AU (version d'essai)", "Android : téléphone et tablette", "Français · English"], "github.com/cookhenri-art/ofad-golem-groovebox"],
    outro: ["OFAD GOLEM", "Dites-moi ce que vous voulez y ajouter", "One For All Distribution"],
    phone: ["Commandes", "Vue live"],
  },
  en: {
    intro: ["OFAD GOLEM", "The free, open-source groovebox", "Windows · Mac · Android"],
    caps: {
      s02: ["16 tracks, 16 pads", "Patterns launch like clips, row by row"],
      s03: ["The step sequencer", "Parameter locks, trig conditions, live Fill and Break"],
      s04: ["The acid sequencer, on the panel", "Note, octave, accent and slide, step by step"],
      s05: ["34 instruments rebuilt from scratch", "Drum machines, basses, analog synths, FM, pads, vocoders…"],
      s06: ["38 effects as pedals and racks", "With their presets, and yours"],
      s07: ["A / B scenes on the crossfader", "Morph from one mix to another, seamlessly"],
      s08: ["The vector pad", "Four scenes blended by a moving shape"],
      s09: ["The mixer", "Tracks, groups, returns, effects and meters"],
      s10: ["On phone and tablet", "Plug in a USB MIDI controller: that's a full live rig"],
    },
    plat: ["Free and open source", ["Windows: app and VST3 plugin", "Mac: app, VST3 and AU (preview)", "Android: phone and tablet", "English · Français"], "github.com/cookhenri-art/ofad-golem-groovebox"],
    outro: ["OFAD GOLEM", "Tell me what you'd like to add", "One For All Distribution"],
    phone: ["Controls", "Live view"],
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

function intro(t) {
  return card(`<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:70px">
    <img src="${W}/golem-source.png" style="width:560px;height:560px;filter:drop-shadow(0 30px 40px rgba(0,0,0,.6))">
    <div><div style="font:400 170px/0.9 Bebas;letter-spacing:.06em">${t[0]}</div>
    <div style="font:600 52px Inter;margin-top:26px" class="gold">${t[1]}</div>
    <div style="font:500 34px Inter;margin-top:24px;opacity:.75;letter-spacing:.08em">${t[2]}</div></div></div>`);
}
function plat(t) {
  return card(`<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:80px">
    <img src="${W}/golem-source.png" style="width:420px;height:420px">
    <div><div style="font:400 120px/0.95 Bebas;letter-spacing:.05em" class="gold">${t[0]}</div>
    <ul style="list-style:none;padding:0;margin-top:30px;font:500 38px/1.7 Inter">${t[1].map((x) => `<li>✓ ${x}</li>`).join("")}</ul>
    <div style="margin-top:36px;font:600 36px Inter;padding:16px 26px;border-radius:14px;background:rgba(242,184,75,.14);border:2px solid #f2b84b;display:inline-block">${t[2]}</div></div></div>`);
}
function outro(t) {
  return card(`<div style="position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center">
    <img src="${W}/golem-source.png" style="width:420px;height:420px">
    <div style="font:400 130px/0.9 Bebas;letter-spacing:.06em;margin-top:10px">${t[0]}</div>
    <div style="font:600 46px Inter;margin-top:18px" class="gold">${t[1]}</div>
    <div style="font:500 30px Inter;margin-top:22px;opacity:.7;letter-spacing:.1em">${t[2]}</div></div>`);
}
function gallery(img) {
  return card(`<div style="position:absolute;inset:40px 60px 200px;display:flex;align-items:center;justify-content:center">
    <img src="${W}/${img}" style="max-width:100%;max-height:100%;border-radius:18px;box-shadow:0 30px 60px rgba(0,0,0,.6)"></div>`);
}
function phones(lang, labels) {
  const im = (k) => `/tmp/vid/cap-${lang}/s10/phone-${k}.jpg`;
  const ph = (k, l) => `<div style="text-align:center"><div style="padding:18px;border-radius:46px;background:#050608;box-shadow:0 0 0 3px #3a3f48,0 30px 60px rgba(0,0,0,.6)">
    <img src="${im(k)}" style="width:820px;border-radius:24px;display:block"></div><div style="font:600 32px Inter;margin-top:22px;opacity:.85">${l}</div></div>`;
  return card(`<div style="position:absolute;inset:0 0 170px;display:flex;align-items:center;justify-content:center;gap:60px">${ph(1, labels[0])}${ph(2, labels[1])}</div>`);
}
function caption(c) {
  return `<!doctype html><html><head><meta charset="utf-8">${BASE}<style>html,body{background:transparent}</style></head><body>
  <div style="position:absolute;left:56px;bottom:52px;max-width:1300px;padding:26px 34px 28px;border-radius:20px;background:rgba(10,12,16,.82);
    box-shadow:0 18px 50px rgba(0,0,0,.55),inset 0 0 0 1px rgba(255,255,255,.08);border-left:6px solid #f2b84b">
    <div style="font:700 22px Inter;letter-spacing:.22em" class="gold">OFAD GOLEM</div>
    <div style="font:750 54px/1.15 Inter;margin-top:8px">${c[0]}</div>
    <div style="font:500 32px/1.3 Inter;margin-top:10px;opacity:.85">${c[1]}</div></div></body></html>`;
}

(async () => {
  const b = await chromium.launch();
  const p = await (await b.newContext({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 })).newPage();
  const shot = async (html, path, transparent) => {
    fs.writeFileSync("/tmp/vid/web/_tmp.html", html);
    await p.goto("file:///tmp/vid/web/_tmp.html"); await p.waitForTimeout(250);
    await p.evaluate(() => document.fonts.ready);
    await p.screenshot({ path, omitBackground: !!transparent });
  };
  for (const lang of ["fr", "en"]) {
    const t = TXT[lang], d = `/tmp/vid/cards-${lang}`;
    fs.mkdirSync(d, { recursive: true });
    await shot(intro(t.intro), `${d}/s01.png`);
    await shot(plat(t.plat), `${d}/s11.png`);
    await shot(outro(t.outro), `${d}/s12.png`);
    for (const [i, g] of ["instruments-1.jpg", "instruments-2.jpg", "instruments-3.jpg", "instruments-4.jpg"].entries()) await shot(gallery(g), `${d}/s05-${i + 1}.png`);
    if (fs.existsSync(`/tmp/vid/cap-${lang}/s10/phone-2.jpg`)) await shot(phones(lang, t.phone), `${d}/s10.png`);
    for (const [k, c] of Object.entries(t.caps)) await shot(caption(c), `${d}/cap-${k}.png`, true);
    console.log(lang, "cartes faites");
  }
  await b.close();
})();
