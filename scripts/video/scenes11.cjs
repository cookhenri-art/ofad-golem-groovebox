// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : la vidéo « de la 0 à la 1.1 », scène par scène : durée, légende FR / EN, son (pistes entendues,
// pavé vectoriel, crossfader) et gestes dans l'aperçu. Partagé par capture11.cjs, cards11.cjs et render11.py
// (node scenes11.cjs json > scenes11.json).
// Pistes de la démo Tribe : 1 Glutamake · 2 Machine électronique · 3 Good Doctor · 4 Spectral Vocal · 5 Cut of the Beat
const at = (i, n, f) => i === Math.round(n * f);
const ALL = "toutes";

function scenes(lang) {
  const en = lang === "en";
  const T = (fr, e) => (en ? e : fr);
  return [
    { id: "s01", card: "intro", secs: 7, audio: [[7, "pistes=2 vec=1"]] },
    { id: "s02", secs: 8, query: "?demo=tribe&accueil", stopped: true,
      setup: async (p) => { await p.waitForTimeout(1600); }, // l'accueil apparaît par une animation CSS (temps réel)
      cap: [T("Deux démos au démarrage", "Two demos at startup"), T("Techno ou Tribe : appuyez sur Lecture, c'est parti", "Techno or Tribe: press Play and off you go")],
      audio: [[8, "pistes=2"]],
      step: async (p, h, i, n) => { if (at(i, n, 0.55)) await p.getByRole("button", { name: T("Démo Tribe", "Tribe demo"), exact: true }).first().click(); } },
    { id: "s03", secs: 10,
      cap: [T("16 pistes, un instrument par piste", "16 tracks, one instrument per track"), T("Batteries, basses, synthés, sampler, vocodeurs…", "Drum machines, basses, synths, sampler, vocoders…")],
      audio: [[10, "pistes=1,2"]],
      setup: async (p, h) => { await h.nav(T("Pistes", "Tracks")); } },
    { id: "s04", secs: 10,
      cap: [T("38 instruments recréés de zéro", "38 instruments rebuilt from scratch"), T("Choisis par leurs façades", "Picked by their panels")],
      audio: [[10, "pistes=1,2,5"]],
      setup: async (p, h) => { await h.nav(T("Pistes", "Tracks")); await p.locator(".track-row").nth(5).click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => {
        const tabs = en ? ["Drum kits", "Bass", "Polys", "Pads", "FM synths", "Drones"] : ["Batteries", "Basses", "Polys", "Nappes", "Synthés FM", "Drones"];
        for (let k = 0; k < tabs.length; ++k) if (at(i, n, 0.08 + k * 0.15)) await p.getByRole("tab", { name: tabs[k], exact: true }).first().click().catch(() => {});
      } },
    { id: "s05", secs: 12,
      cap: [T("La façade de chaque synthé", "Every synth has its own panel"), T("Glutamake : tables d'ondes et bille en mouvement", "Glutamake: wavetables and a moving ball")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(0); await h.face(); } },
    { id: "s06", secs: 10,
      cap: [T("Good Doctor", "Good Doctor"), T("Trois oscillateurs à tables d'ondes, deux filtres", "Three wavetable oscillators, two filters")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(2); await h.face(); } },
    { id: "s07", secs: 10,
      cap: [T("Boîtes à rythmes et pads", "Drum machines and pads"), T("Chaque pad a ses réglages", "Every pad has its own settings")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(1); await h.face(); } },
    { id: "s08", secs: 12,
      cap: [T("L'écran SÉQUENCE", "The SEQUENCE screen"), T("Une ligne par pad, ou un rouleau de notes", "One line per pad, or a piano roll")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(1); await h.nav(T("Séquence", "Sequence")); } },
    { id: "s09", secs: 14,
      cap: [T("Verrous à pas", "Parameter locks"), T("Chaque pas garde ses propres réglages", "Every step keeps its own settings")],
      audio: [[14, ALL]],
      setup: async (p, h) => { await h.track(2); await h.face(); await p.locator(".steps-dock").getByRole("radio", { name: T("À verrous", "Locks") }).click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => {
        const keys = p.locator(".steps-dock .ot-steps-keys button");
        if (at(i, n, 0.15)) await keys.nth(4).click();
        if (at(i, n, 0.35)) await h.longPress(keys.nth(4));
        if (at(i, n, 0.75)) await h.esc();
      } },
    { id: "s10", secs: 12,
      cap: [T("Séquenceur acide", "Acid sequencer"), T("Note, octave, accent et glissé, sous le clavier", "Note, octave, accent and slide, under the keyboard")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(0); await h.face(); await p.locator(".steps-dock").getByRole("radio", { name: T("Acide", "Acid") }).click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => {
        const acc = p.locator(".steps-dock").getByRole("button", { name: T("Acc.", "Acc."), exact: true });
        const gl = p.locator(".steps-dock").getByRole("button", { name: T("Gl.", "Sl."), exact: true });
        for (const [f, loc, k] of [[0.2, acc, 2], [0.3, gl, 3], [0.4, acc, 6], [0.5, acc, 10], [0.6, gl, 11], [0.7, acc, 14]]) if (at(i, n, f)) await loc.nth(k).click().catch(() => {});
      } },
    { id: "s11", secs: 10,
      cap: [T("Conditions, probabilité, répétitions", "Conditions, probability, ratchets"), T("Des pas qui ne jouent pas à chaque fois", "Steps that don't play every time")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(1); await h.nav(T("Séquence", "Sequence")); },
      step: async (p, h, i, n) => { if (at(i, n, 0.15)) await h.longPress(p.locator('button[aria-label*=", pas 5,"], button[aria-label*=", step 5,"]').first()); } },
    { id: "s12", secs: 10,
      cap: [T("Les clips", "Clips"), T("Les patterns se lancent rangée par rangée", "Patterns launch row by row")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.nav("Clips"); } },
    { id: "s13", secs: 12, v11: true,
      cap: [T("Courbes à tirer au doigt (1.1)", "Curves you drag with a finger (1.1)"), T("Enveloppes, filtres, formes d'onde suivent le son", "Envelopes, filters and waveforms follow the sound")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(0); await h.nav(T("Son", "Sound")); await h.tab(T("Enveloppe", "Envelope")).catch(() => {}); } },
    { id: "s14", secs: 10,
      cap: [T("4 effets par piste, en pédales", "4 effects per track, as pedals"), T("54 effets, avec leurs préréglages", "54 effects, with their presets")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(2); await h.nav(T("Effets", "Effects")); },
      step: async (p, h, i, n) => { if (at(i, n, 0.45)) await p.getByRole("button", { name: T("Pédalier ↗", "Pedalboard ↗") }).first().click(); } },
    { id: "s15", secs: 12, v11: true,
      cap: [T("Golem Titan FX (1.1)", "Golem Titan FX (1.1)"), T("9 effets haut de gamme : réverbérations, limiteur, écho, saturations…", "9 high-end effects: reverbs, limiter, echo, saturation…")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(1); await h.nav(T("Effets", "Effects")); await p.locator("button").filter({ hasText: "Titan Espace" }).first().click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => { if (at(i, n, 0.3)) await p.getByRole("button", { name: T("Ouvrir le module", "Open the module") }).first().click(); } },
    { id: "s16", secs: 10, v11: true,
      cap: [T("Restauration (1.1)", "Restoration (1.1)"), T("Débruiteur, anti-ronflement, anti-clics, dé-esseur, dé-réverb…", "Denoiser, de-hum, de-click, de-esser, de-reverb…")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(4); await h.nav(T("Effets", "Effects")); },
      step: async (p, h, i, n) => {
        if (at(i, n, 0.08)) await p.getByText(T("+ Ajouter", "+ Add"), { exact: true }).first().click();
        if (at(i, n, 0.2)) await p.getByText(T("Restauration", "Restoration"), { exact: true }).first().click();
        if (at(i, n, 0.32)) await p.getByText(T("Débruiteur", "Denoiser"), { exact: true }).first().click();
        if (at(i, n, 0.45)) await p.getByRole("button", { name: /^(Mettre|Put)/ }).first().click();
        if (at(i, n, 0.6)) await p.getByRole("button", { name: T("Pédalier ↗", "Pedalboard ↗") }).first().click();
      } },
    { id: "s17", secs: 16,
      cap: [T("Le mixeur : coupures et solos", "The mixer: mutes and solos"), T("Pistes, groupes, retours, principal", "Tracks, groups, returns, master")],
      audio: [[4, ALL], [4, "pistes=1,3,4,5"], [4, "pistes=4"], [4, ALL]],
      setup: async (p, h) => { await h.nav(T("Mixeur", "Mixer")); },
      step: async (p, h, i, n) => {
        const M = p.getByRole("button", { name: "M", exact: true }), S = p.getByRole("button", { name: "S", exact: true });
        if (at(i, n, 0.25)) await M.nth(1).click();
        if (at(i, n, 0.5)) { await M.nth(1).click(); await S.nth(3).click(); }
        if (at(i, n, 0.75)) await S.nth(3).click();
      } },
    { id: "s18", secs: 14,
      cap: [T("Scènes A / B au crossfader", "A / B scenes on the crossfader"), T("Passer d'un mix à l'autre, sans coupure", "Morph from one mix to another, seamlessly")],
      audio: [[7, "vec=0 xfade=0>1"], [7, "vec=0 xfade=1>0"]],
      setup: async (p, h) => { await h.nav("Live"); await p.locator('button:visible:text-is("Crossfader")').first().click(); await p.clock.runFor(500); },
      step: async (p, h, i, n) => {
        const tr = p.locator(".livexf-track:visible").first();
        if (!(await tr.count())) return;
        const bb = await tr.boundingBox(); if (!bb) return;
        const ph = i / Math.max(1, n - 1), k = ph < 0.5 ? ph * 2 : 2 - ph * 2;
        const x = bb.x + 8 + (bb.width - 16) * k, y = bb.y + bb.height / 2;
        if (i === 0) { await p.mouse.move(x, y); await p.mouse.down(); } else await p.mouse.move(x, y);
        if (i === n - 1) await p.mouse.up();
      } },
    { id: "s19", secs: 16,
      cap: [T("Le pavé vectoriel en cercle", "The vector pad on a circle"), T("Quatre scènes mélangées par une forme en mouvement", "Four scenes blended by a moving shape")],
      audio: [[8, "vec=1"], [8, "vec=0 xfade=0.5"]],
      setup: async (p, h) => { await h.nav("Live"); },
      step: async (p, h, i, n) => { if (at(i, n, 0.5)) await p.locator('button:visible:text-is("' + T("Pavé actif", "Pad active") + '")').first().click(); } },
    { id: "s20", secs: 10,
      cap: [T("32 scènes de réglages", "32 scenes of settings"), T("Chaque scène garde ses réglages, lancée avec une rangée", "Each scene keeps its settings, launched with a row")],
      audio: [[10, "vec=1"]],
      setup: async (p, h) => { await h.nav(T("Scènes", "Scenes")); } },
    { id: "s21", secs: 16,
      cap: [T("L'écran Jeu", "The Play view"), T("Pads en Mute / Solo, Fill, Break, tempo", "Pads in Mute / Solo, Fill, Break, tempo")],
      audio: [[4, ALL], [4, "pistes=1,3,4,5"], [4, "pistes=1,3,4"], [4, ALL]],
      setup: async (p, h) => { await p.locator(".zoom-rail-jeu").first().click(); await p.clock.runFor(400); await p.getByRole("button", { name: "MUTE" }).first().click().catch(() => p.getByText("Mute", { exact: true }).first().click()); },
      step: async (p, h, i, n) => {
        const pad = (k) => p.locator(".pad").nth(k);
        if (at(i, n, 0.25)) await pad(13).click();
        if (at(i, n, 0.5)) await pad(8).click();
        if (at(i, n, 0.75)) { await pad(13).click(); await pad(8).click(); }
      } },
    { id: "s22", secs: 12,
      cap: [T("Tout s'affecte en un geste", "Assign anything in one gesture"), T("16 boutons assignables, apprentissage MIDI", "16 assignable knobs, MIDI learn")],
      audio: [[12, ALL]],
      setup: async (p, h) => { await h.track(2); await p.locator(".zoom-rail-jeu").first().click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => {
        if (at(i, n, 0.1)) await h.right(p.locator(".jeu-knobs .aknob .encoder-cap").nth(2));
        if (at(i, n, 0.3)) await p.getByText(T("Assigner à…", "Assign to…")).first().click();
        if (at(i, n, 0.45)) await h.nav(T("Son", "Sound"));
        if (at(i, n, 0.65)) await p.locator(".screen-encoders .enc-label").nth(1).click();
      } },
    { id: "s23", secs: 8,
      cap: [T("Modulations des boutons", "Knob modulations"), T("LFO et fonctions sur les 16 boutons", "LFOs and functions on the 16 knobs")],
      audio: [[8, ALL]],
      setup: async (p, h) => { await p.getByRole("button", { name: T("Mod. boutons", "Knob mods") }).first().click(); await p.clock.runFor(500); } },
    { id: "s24", secs: 14,
      cap: [T("Le morceau", "Song mode"), T("La démo Tribe : intro, montée, break, retour", "The Tribe demo: intro, build-up, break, drop")],
      audio: [[7, "pistes=3,4"], [7, "pistes=1,3,4,5"]],
      setup: async (p, h) => { await h.nav("Clips"); await h.tab(T("Morceau", "Song")); } },
    { id: "s25", secs: 8,
      cap: [T("Montage audio", "Audio arrangement"), T("Prises en clips, fondus, export WAV / MP3", "Takes as clips, fades, WAV / MP3 export")],
      audio: [[8, ALL]],
      setup: async (p, h) => { await h.nav("Clips"); await h.tab("Audio"); } },
    { id: "s26", secs: 14, v11: true,
      cap: [T("L'écran AUDIO (1.1)", "The AUDIO screen (1.1)"), T("Master, pistes, entrée ; calé, boucle parfaite ; chaîne de 4 effets", "Master, tracks, input; synced, perfect loop; 4-effect chain")],
      audio: [[14, ALL]],
      setup: async (p, h) => { await p.getByRole("button", { name: /audio/i }).first().click(); await p.clock.runFor(600); },
      step: async (p, h, i, n) => {
        if (at(i, n, 0.15)) await p.locator(".aud").getByRole("button", { name: "2", exact: true }).first().click();
        if (at(i, n, 0.25)) await p.locator(".aud").getByRole("button", { name: "4", exact: true }).first().click();
        if (at(i, n, 0.4)) await p.getByText(T("+ Ajouter un effet", "+ Add an effect")).first().click();
        if (at(i, n, 0.55)) await p.locator(".fxpick .ipick-card").filter({ hasText: T("Débruiteur", "Denoiser") }).first().click();
      } },
    { id: "s27", secs: 18, v11: true,
      cap: [T("Piste MIDI (1.1)", "MIDI track (1.1)"), T("GOLEM joue vos synthés et boîtes à rythmes externes", "GOLEM plays your external synths and drum machines")],
      audio: [[18, ALL]],
      setup: async (p, h) => {
        await h.track(5); await h.tab(T("MIDI externe", "External MIDI"));
        await p.locator(".browser").getByText(T("Piste MIDI", "MIDI track"), { exact: true }).first().dblclick(); await p.clock.runFor(600); await h.face();
      },
      step: async (p, h, i, n) => {
        const names = T(["Filtre", "Enveloppes", "Effets", "Arpège"], ["Filter", "Envelopes", "Effects", "Arp"]);
        for (let k = 0; k < 4; ++k) if (at(i, n, 0.1 + k * 0.08)) { await p.locator(".midi-sec-title").nth(k).click(); await p.locator(".midi-sec-input").fill(names[k]); await p.keyboard.press("Enter"); }
        if (at(i, n, 0.5)) await p.locator(".midi-knob-gear").nth(1).click();
        if (at(i, n, 0.75)) await p.getByRole("button", { name: "OK", exact: true }).first().click();
      } },
    { id: "s28", secs: 9, v11: true,
      cap: [T("Son du pas (1.1)", "Step sound (1.1)"), T("Un préréglage différent sur chaque pas", "A different preset on every step")],
      audio: [[9, ALL]],
      setup: async (p, h) => { await h.track(2); await h.face(); await p.locator(".steps-dock").getByRole("radio", { name: T("À verrous", "Locks") }).click(); await p.clock.runFor(300); },
      step: async (p, h, i, n) => {
        const k = p.locator(".steps-dock .ot-steps-keys button").nth(4);
        if (at(i, n, 0.05)) await k.click();
        if (at(i, n, 0.2)) await h.longPress(k);
        if (at(i, n, 0.45)) await p.locator('select[aria-label="Son joué sur ce pas"], select[aria-label="Sound played on this step"]').selectOption("3");
      } },
    { id: "s29", secs: 10,
      cap: [T("FM Player (1.0)", "FM Player (1.0)"), T("Les firmwares libres du FM-1, écran compris", "Free FM-1 firmwares, screen included")],
      audio: [[10, ALL]],
      setup: async (p, h) => { await h.track(6); await h.tab("Polys"); await p.locator(".browser").getByText("FM Player", { exact: true }).first().dblclick(); await p.clock.runFor(800); await h.face(); } },
    { id: "s30", secs: 8,
      cap: [T("Réglages", "Settings"), T("Français ou anglais, dossiers, MIDI, enregistrement", "English or French, folders, MIDI, recording")],
      audio: [[8, ALL]],
      setup: async (p, h) => { await h.nav(T("Réglages", "Settings")); } },
    { id: "s31", secs: 30, phone: true, v11: true, query: "?demo=tribe&accueil", stopped: true,
      cap: [T("La télécommande (1.1)", "The remote (1.1)"), T("Le téléphone pilote GOLEM sur le PC (USB ou Wi-Fi) ou un appareil MIDI", "Your phone controls GOLEM on the PC (USB or Wi-Fi) or a MIDI device")],
      audio: [[30, ALL]],
      setup: async (p, h) => { await p.getByText(T("Télécommande · piloter le PC ou un FM-1", "Remote · control the PC or an FM-1")).first().click(); await p.clock.runFor(700); },
      step: async (p, h, i, n) => {
        const tabs = T(["Mixage", "Boutons", "Chaos", "FM-1"], ["Mix", "Knobs", "Chaos", "FM-1"]);
        for (let k = 0; k < 4; ++k) if (at(i, n, 0.17 + k * 0.2)) await p.getByRole("button", { name: tabs[k], exact: true }).first().click();
        // Chaos : l'orbe fait un tour au doigt
        const c0 = Math.round(n * 0.6), c1 = Math.round(n * 0.75);
        if (i >= c0 && i <= c1) {
          const pad = p.locator(".rc-chaos-pad, .rc-pad, [class*=chaos] [class*=pad]").first();
          const bb = await pad.boundingBox().catch(() => null);
          if (bb) {
            const a = ((i - c0) / (c1 - c0)) * Math.PI * 2, x = bb.x + bb.width / 2 + Math.cos(a) * bb.width * 0.3, y = bb.y + bb.height / 2 + Math.sin(a) * bb.height * 0.3;
            if (i === c0) { await p.mouse.move(x, y); await p.mouse.down(); } else await p.mouse.move(x, y);
            if (i === c1) await p.mouse.up();
          }
        }
      } },
    { id: "s32", secs: 10, phone: true,
      cap: [T("Toute la groovebox sur téléphone", "The whole groovebox on a phone"), T("Android, iPhone / iPad, avec un contrôleur MIDI", "Android, iPhone / iPad, with a MIDI controller")],
      audio: [[10, "pistes=1,2,5"]],
      setup: async (p, h) => { await p.locator(".zoom-rail-jeu").first().click(); await p.clock.runFor(400); },
      step: async (p, h, i, n) => { if (at(i, n, 0.5)) await h.nav("Live"); } },
    { id: "s33", card: "gallery", secs: 16, audio: [[16, "pistes=2,5"]] },
    { id: "s34", card: "plat", secs: 10, audio: [[10, "pistes=2"]] },
    { id: "s35", card: "outro", secs: 7, audio: [[7, "pistes=2"]] },
  ];
}
module.exports = { scenes };
if (require.main === module && process.argv[2] === "json") {
  const s = scenes(process.argv[3] || "fr").map(({ id, secs, card, cap, audio, phone, v11 }) => ({ id, secs, card, cap, audio, phone: !!phone, v11: !!v11 }));
  console.log(JSON.stringify(s, null, 1));
}
