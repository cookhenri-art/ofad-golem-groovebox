// Groovebox — OFAD — AGPL-3.0-or-later
// Moteur simulé pour ouvrir l'interface dans un navigateur (npm run dev) sans le plugin.
// Il imite les fonctions natives (mêmes réponses JSON que src/engine/Bridge.cpp) et l'événement
// « tick ». Le catalogue est celui du vrai moteur (catalog.json, écrit par groovebox_catalog).
// Il n'est installé que si la page ne tourne PAS dans le plugin (window.__JUCE__ absent).

import catalogJson from "./catalog.json";
import type { Catalog, ModuleDesc, ParamDesc } from "./protocol";
import { lastChannel, mixerFor, numGroups, toNorm, toPlain } from "./params";

type Listener = (payload: unknown) => void;

export function installMockBackend() {
  if (typeof window.__JUCE__ !== "undefined") return;
  const cat = catalogJson as unknown as Catalog;
  const L = cat.layout;
  const S = L.slotsPerChannel;
  const listeners = new Map<string, Map<number, Listener>>();
  let nextId = 0;
  const emit = (id: string, payload: unknown) => listeners.get(id)?.forEach((l) => l(payload));

  // --- État
  const CH = lastChannel(L) + 1;
  const values = Array.from({ length: CH }, () => new Float32Array(L.slotsPerChannel));
  const tracks = Array.from({ length: L.numTracks }, (_, i) => ({ name: `Piste ${i + 1}`, color: i % 8, instrument: "", preset: "", out: 0 }));
  const inserts = Array.from({ length: CH }, () => ["", "", "", ""]);
  let specOn = false; // analyseur de spectre demandé (v0.45.1)
  const padInserts = Array.from({ length: L.numTracks }, () => Array.from({ length: 32 }, () => ""));
  const mods: { source: number; target: number; amountIndex: number }[][] = Array.from({ length: CH }, () => []);
  const samples: (string | null)[][] = Array.from({ length: L.numTracks }, () => new Array(16).fill(null));
  const scenes = Array.from({ length: L.numScenes + 1 }, () => ({ muted: false, locks: [] as [number, number][] }));
  const extras = new Map<string, { lane: number; step: number; cond: number; a: number; b: number; prob: number; rat?: number; rs?: number; flam?: number; locks: [number, number][] }>();
  const mkLane = () => ({ dir: 0, length: 0, mute: false, solo: false, vel: new Array(L.maxSteps).fill(0), note: new Array(L.maxSteps).fill(60), len: new Array(L.maxSteps).fill(8), micro: new Array(L.maxSteps).fill(0) });
  const NP = L.numPatterns ?? 8;
  const mkPattern = () => ({ length: 16, speed: 4, flags: new Array(L.maxSteps).fill(0) as number[], lanes: Array.from({ length: L.numLanes }, mkLane) });
  const pats = Array.from({ length: L.numTracks }, () => Array.from({ length: NP }, mkPattern));
  const trackSeq = Array.from({ length: L.numTracks }, () => ({ mute: false, solo: false, playing: 0, queued: -1, editing: 0, accent: 50 }));
  // vue « pattern édité » : mêmes champs que l'ancien modèle (longueur, vitesse, lignes + coupure, solo)
  const patterns = new Proxy([] as { length: number; speed: number; mute: boolean; solo: boolean; flags: number[]; lanes: ReturnType<typeof mkLane>[] }[], {
    get(_, key) {
      if (typeof key !== "string" || !/^\d+$/.test(key)) return undefined;
      const t = Number(key), ts = trackSeq[t], pt = pats[t][ts.editing];
      return new Proxy(pt, {
        get(target, k) { return k === "mute" || k === "solo" ? ts[k] : (target as Record<string | symbol, unknown>)[k]; },
        set(target, k, v) { if (k === "mute" || k === "solo") ts[k] = v; else (target as Record<string | symbol, unknown>)[k] = v; return true; },
      });
    },
  });
  let recQuant = 100, launchQuant = 2, muteTails = false;
  const rowScenes = new Array(NP).fill(-1);
  const midiMaps: [number, number, number, number][] = [];
  let midiLearn = -1;
  const divs = Array.from({ length: 16 }, () => new Array(8).fill(2) as number[]); // division par piste et par pattern
  const divisionOf = (t: number) => divs[t]?.[trackSeq[t]?.editing ?? 0] ?? 2;
  const globals = { xfade: 0, sceneA: 1, sceneB: 9, bpm: 122, swing: 54 };
  const macros = Array.from({ length: 8 }, () => ({ v: 0.5, rel: true, links: [] as [number, number, number][] }));
  const transport = { playing: false, rec: false, ppq: -1, fill: false, brk: false };
  let focus = 0;

  const mixer = (ch: number) => mixerFor(cat, ch);
  const defaults = (ch: number, base: number, d: ModuleDesc) => d.params.forEach((p, i) => { values[ch][base + i] = toNorm(p, p.def); });
  const instDesc = (t: number) => cat.instruments.find((i) => i.type === tracks[t]?.instrument);
  const fxDesc = (ch: number, s: number) => cat.effects.find((e) => e.type === inserts[ch][s]);
  const mockFolders = () => ({
    canChoose: true,
    samples: { path: "Documents/Groovebox/Échantillons", custom: false },
    projects: { path: "Documents/Groovebox/Projets", custom: false },
    presets: { path: "Documents/Groovebox/Préréglages", custom: false },
  });
  const userPresets: { folder: string; bank: string; name: string; kind: string; vals: [string, number][] }[] = [];
  const presetFolder = (scope: string, ch: number, slot: number) =>
    scope === "fx" ? fxDesc(ch, slot)?.type : scope === "pad" ? (instDesc(ch)?.drumKit ? "Pads" : undefined) : instDesc(ch)?.type;
  for (let ch = 0; ch < CH; ++ch) {
    defaults(ch, L.mixerBase, mixer(ch));
    for (let s = 0; s < L.numModSources; ++s) {
      defaults(ch, L.modSourceBase + s * L.modSourceSlots, cat.modSource);
      // Même réglage d'origine que le moteur : LFO, LFO, fonction, fonction, suiveur, séquenceur.
      values[ch][L.modSourceBase + s * L.modSourceSlots] = toNorm(cat.modSource.params[0], [0, 0, 1, 1, 2, 3][s]);
    }
    for (let a = 0; a < 64; ++a) values[ch][L.modAmountBase + a] = 0.5;
  }
  const describe = (g: number): ParamDesc | undefined => {
    const ch = Math.floor(g / L.slotsPerChannel), local = g % L.slotsPerChannel;
    if (ch < 0 || ch >= CH) return undefined;
    if (local < L.instrumentBase) return mixer(ch).params[local];
    if (local < L.insertBase) return ch < L.numTracks ? instDesc(ch)?.params[local - L.instrumentBase] : undefined;
    if (local < L.modSourceBase) return fxDesc(ch, Math.floor((local - L.insertBase) / L.insertSlots))?.params[(local - L.insertBase) % L.insertSlots];
    if (local < L.modAmountBase) return cat.modSource.params[(local - L.modSourceBase) % L.modSourceSlots];
    return undefined;
  };
  const find = (ch: number, area: string, id: string) => {
    if (area === "mix") return ch * L.slotsPerChannel + mixer(ch).params.findIndex((p) => p.id === id);
    const d = instDesc(ch);
    return d ? ch * L.slotsPerChannel + L.instrumentBase + d.params.findIndex((p) => p.id === id) : -1;
  };
  const toPlainLocal = (ch: number, id: string) => { const g = find(ch, "inst", id); const d = describe(g); return d ? toPlain(d, values[ch][g % L.slotsPerChannel]) : 0; };
  const setPlain = (ch: number, area: string, id: string, v: number) => { const g = find(ch, area, id); const d = describe(g); if (d) values[ch][g % L.slotsPerChannel] = toNorm(d, v); };

  const setInstrument = (t: number, type: string) => {
    tracks[t].instrument = type; tracks[t].preset = "";
    for (let i = 0; i < L.insertBase - L.instrumentBase; ++i) values[t][L.instrumentBase + i] = 0;
    const d = instDesc(t);
    if (d) { defaults(t, L.instrumentBase, d); if (tracks[t].name.startsWith("Piste ")) tracks[t].name = d.name; }
    mods[t] = mods[t].filter((m) => m.target < L.instrumentBase || m.target >= L.insertBase);
    samples[t].fill(null);
    padInserts[t].fill("");
  };
  const setInsert = (ch: number, s: number, type: string) => {
    inserts[ch][s] = type;
    const base = L.insertBase + s * L.insertSlots;
    for (let i = 0; i < L.insertSlots; ++i) values[ch][base + i] = 0;
    const d = fxDesc(ch, s);
    if (d) defaults(ch, base, d);
    values[ch][6 + s] = 0.25;
  };
  const PB = L.padBypassBase ?? 1600, PI = L.padInsertBase ?? 2048;
  const setPadInsert = (t: number, pad: number, s: number, type: string) => {
    const k = pad * 2 + s;
    padInserts[t][k] = type;
    const base = PI + k * L.insertSlots;
    for (let i = 0; i < L.insertSlots; ++i) values[t][base + i] = 0;
    const d = cat.effects.find((e) => e.type === type);
    if (d) defaults(t, base, d);
    values[t][PB + k] = 0;
  };

  // Projet de départ (le même que le vrai moteur)
  setInstrument(0, "drumA"); tracks[0].name = "Batterie";
  setInstrument(1, "acid"); tracks[1].name = "Basse acide";
  setInstrument(2, "polyj"); tracks[2].name = "Accords"; tracks[2].preset = "Accords house";
  setInstrument(3, "fm6"); tracks[3].name = "Old Metal"; // aperçu : FM 6 opérateurs
  // aperçu seulement : une piste par instrument pour voir les façades
  ["mono", "poly5", "hybrid", "morph", "drumImp", "drumNrv", "octa", "dfam", "sh101", "blo", "ob6", "orbit"].forEach((type, k) => { setInstrument(4 + k, type); });
  samples[12][0] = "Boucle de batterie.wav";
  samples[10][0] = "Boucle de batterie.wav"; // aperçu : Pieuvre 8, T1 · 1
  setInsert(16, 0, "reverb"); setInsert(17, 0, "delay"); setInsert(20, 0, "limiter");
  setInsert(1, 0, "dist"); setInsert(1, 1, "opto"); setInsert(0, 0, "eq"); setInsert(0, 1, "bus"); setInsert(0, 2, "delay"); setInsert(0, 3, "gate"); // aperçu : deux racks et plus (pédalier)
  setPlain(2, "mix", "sendA", 25); setPlain(2, "mix", "sendB", 18); setPlain(3, "mix", "sendA", 40);
  const P = patterns;
  [0, 4, 8, 12].forEach((s) => { P[0].lanes[0].vel[s] = 118; });
  [4, 12].forEach((s) => { P[0].lanes[1].vel[s] = 105; });
  for (let s = 0; s < 16; ++s) P[0].lanes[4].vel[s] = s % 4 === 2 ? 78 : s % 2 ? 40 : 0;
  P[0].lanes[5].vel[14] = 70; P[0].lanes[2].vel[15] = 60;
  extras.set("0:0:2:15", { lane: 2, step: 15, cond: 1, a: 2, b: 2, prob: 100, locks: [] });
  // pattern B de la batterie : variation
  [0, 6, 10].forEach((s) => { pats[0][1].lanes[0].vel[s] = 118; });
  [4, 12].forEach((s) => { pats[0][1].lanes[1].vel[s] = 105; });
  for (let s = 0; s < 16; s += 2) pats[0][1].lanes[4].vel[s] = 70;
  const bass = [36, 0, 36, 48, 0, 36, 39, 0, 36, 0, 46, 36, 0, 43, 41, 0];
  [3, 13].forEach((s) => { pats[1][0].flags[s] = 2; });
  [0, 6, 10].forEach((s) => { pats[1][0].flags[s] |= 1; });
  bass.forEach((n, s) => { if (n) { P[1].lanes[0].vel[s] = s % 4 === 0 ? 120 : 90; P[1].lanes[0].note[s] = n; P[1].lanes[0].len[s] = s === 3 || s === 13 ? 12 : 5; } });
  [57, 60, 64].forEach((n, k) => { [0, 3].forEach((s) => { P[2].lanes[k].vel[s] = 95; P[2].lanes[k].note[s] = n; P[2].lanes[k].len[s] = 6; }); [8, 11].forEach((s) => { P[2].lanes[k].vel[s] = 95; P[2].lanes[k].note[s] = n + 5; }); P[3].lanes[k].vel[0] = 90; P[3].lanes[k].note[0] = n - 12; P[3].lanes[k].len[0] = 120; });
  mods[1].push({ source: 0, target: L.instrumentBase + 3, amountIndex: 0 }); values[1][L.modAmountBase] = 0.7;
  scenes[8].locks = [[1 * L.slotsPerChannel + L.instrumentBase + 3, 0.85], [3 * L.slotsPerChannel + 2, 0.9]];
  scenes[L.numScenes].locks = [[0, 0]];
  samples[0][14] = null;

  // --- Réponses (mêmes formes que Bridge.cpp)
  const hex = (a: number[]) => a.map((v) => (v & 0xff).toString(16).padStart(2, "0")).join("");
  const hex3 = (a: number[]) => a.map((v) => (v & 0xfff).toString(16).padStart(3, "0")).join("");
  const channelJson = (ch: number) => {
    const v = values[ch];
    const d = ch < L.numTracks ? instDesc(ch) : undefined;
    return JSON.stringify({
      channel: ch,
      mix: Array.from(v.slice(0, mixer(ch).params.length)),
      ...(ch < L.numTracks ? { instrument: d?.type ?? "", inst: Array.from(v.slice(L.instrumentBase, L.instrumentBase + (d?.params.length ?? 0))), preset: tracks[ch].preset, samples: samples[ch] } : {}),
      ...(ch < L.numTracks ? { padfx: padInserts[ch].flatMap((type, k) => { const f = cat.effects.find((e) => e.type === type); const b = PI + k * L.insertSlots;
        return f ? [{ pad: k >> 1, slot: k & 1, type, bypass: v[PB + k], values: Array.from(v.slice(b, b + f.params.length)) }] : []; }) } : {}),
      fx: [0, 1, 2, 3].map((s) => { const f = fxDesc(ch, s); const b = L.insertBase + s * L.insertSlots; return { type: f?.type ?? "", values: Array.from(v.slice(b, b + (f?.params.length ?? 0))) }; }),
      mod: Array.from({ length: L.numModSources }, (_, s) => Array.from(v.slice(L.modSourceBase + s * L.modSourceSlots, L.modSourceBase + (s + 1) * L.modSourceSlots))),
      mods: mods[ch].map((m) => ({ source: m.source, target: m.target, amount: v[L.modAmountBase + m.amountIndex] * 2 - 1, amountSlot: L.modAmountBase + m.amountIndex })),
    });
  };
  const insertPresets = Array.from({ length: CH }, () => ["", "", "", ""]);
  const buses = Array.from({ length: 9 }, () => ({ mute: false, solo: false, name: "" })); // v0.31
  const tracksJson = () => JSON.stringify({ tracks: tracks.map((t, i) => ({ ...t, mute: trackSeq[i].mute, solo: trackSeq[i].solo })), inserts, insertPresets, buses });
  const patternJson = (t: number) => {
    const p = patterns[t];
    return JSON.stringify({ track: t, length: p.length, speed: p.speed, mute: p.mute, solo: p.solo, division: divisionOf(t),
      pattern: trackSeq[t].editing, playing: trackSeq[t].playing, recQuant, accent: trackSeq[t].accent,
      flags: p.flags.map((f) => (f & 15).toString(16)).join(""),
      lanes: p.lanes.map((l) => ({ dir: l.dir, length: l.length, mute: l.mute, solo: l.solo, vel: hex(l.vel), note: hex(l.note), len3: hex3(l.len), micro: hex(l.micro) })) });
  };
  const exKey = (t: number, l: number, s: number) => `${t}:${trackSeq[t].editing}:${l}:${s}`;
  const extrasJson = (t: number) => JSON.stringify([...extras.entries()].filter(([k]) => k.startsWith(`${t}:${trackSeq[t].editing}:`)).map(([, e]) => e));
  const launchJson = () => JSON.stringify({
    quant: launchQuant, rowScenes, muteTails,
    tracks: trackSeq.map((ts, t) => ({ playing: ts.playing, queued: ts.queued, editing: ts.editing,
      filled: pats[t].map((pt) => (pt.lanes.some((l) => l.vel.some((v) => v > 0)) ? 1 : 0)), length: pats[t].map((pt) => pt.length), division: divs[t] })),
  });
  const midiRoute = { note: 0, ctrl: 0, ctrlOn: true, tracks: new Array(16).fill(0) as number[] };
  const midiJson = () => JSON.stringify({ learn: midiLearn, maps: midiMaps, route: midiRoute });
  const scenesJson = () => JSON.stringify(scenes);

  const ex = (t: number, l: number, s: number) => {
    const k = exKey(t, l, s);
    if (!extras.has(k)) extras.set(k, { lane: l, step: s, cond: 0, a: 1, b: 2, prob: 100, locks: [] });
    return extras.get(k)!;
  };
  const pattern = (fn: (p: (typeof patterns)[number], a: number[]) => void) => (t: number, ...a: number[]) => { fn(patterns[t], a); return patternJson(t); };

  const fns: Record<string, (...a: any[]) => unknown> = {
    getCatalog: () => JSON.stringify(cat),
    getTracks: tracksJson,
    getChannel: (ch: number) => channelJson(ch),
    getPattern: (t: number) => patternJson(t),
    getExtras: (t: number) => extrasJson(t),
    getScenes: scenesJson,
    getWave: (t: number, i: number) => samples[t]?.[i] ? JSON.stringify({ name: samples[t][i], seconds: 2, stereo: true, peaks: Array.from({ length: 800 }, (_, k) => (k % 2 ? 1 : -1) * Math.abs(Math.sin(k * 0.05)) * Math.exp(-((k / 2) % 50) / 20)), onsets: Array.from({ length: 8 }, (_, k) => k / 8) }) : "null",
    uiLog: () => null,
    setFocus: (ch: number) => { focus = ch; return channelJson(ch); },
    setParam: (g: number, v: number) => { if (describe(g)) values[Math.floor(g / S)][g % S] = Math.min(1, Math.max(0, v)); return null; },
    resetParam: (g: number) => { const d = describe(g); if (d) values[Math.floor(g / S)][g % S] = toNorm(d, d.def); return values[Math.floor(g / S)][g % S]; },
    setInstrument: (t: number, type: string) => { setInstrument(t, type); return channelJson(t); },
    setInsert: (ch: number, s: number, type: string) => { setInsert(ch, s, type); return channelJson(ch); },
    setPadInsert: (t: number, pad: number, s: number, type: string) => { setPadInsert(t, pad, s, type); return channelJson(t); },
    moveInsert: (ch: number, a: number, b: number) => {
      [inserts[ch][a], inserts[ch][b]] = [inserts[ch][b], inserts[ch][a]];
      for (let i = 0; i < L.insertSlots; ++i) { const x = L.insertBase + a * L.insertSlots + i, y = L.insertBase + b * L.insertSlots + i; [values[ch][x], values[ch][y]] = [values[ch][y], values[ch][x]]; }
      return channelJson(ch);
    },
    applyPreset: (ch: number, slot: number, name: string) => { if (slot < 0 && ch < L.numTracks) tracks[ch].preset = name; else if (slot >= 0) insertPresets[ch][slot] = name; return channelJson(ch); },
    setTrackName: (t: number, n: string) => { tracks[t].name = String(n).slice(0, 40); return tracksJson(); },
    setTrackSpeech: (t: number, n: string) => { const v = String(n).slice(0, 160); if (v) (tracks[t] as { speech?: string }).speech = v; else delete (tracks[t] as { speech?: string }).speech; return tracksJson(); },
    busMute: (ch: number, on: boolean) => { if (buses[ch - 16]) buses[ch - 16].mute = !!on; return tracksJson(); },
    busSolo: (ch: number, on: boolean) => { if (buses[ch - 16] && ch >= 21) buses[ch - 16].solo = !!on; return tracksJson(); },
    setGroupName: (k: number, n: string) => { if (buses[5 + k]) buses[5 + k].name = String(n).slice(0, 40); return tracksJson(); },
    // déplacer / insérer (v0.23) : aperçu simplifié (pistes, séquences, patterns, inserts)
    moveTrack: (from: number, to: number) => {
      const mv = <T,>(arr: T[]) => { const [x] = arr.splice(from, 1); arr.splice(to, 0, x); };
      mv(tracks); mv(trackSeq); mv(pats); mv(inserts); mv(insertPresets);
      tracks.forEach((t, i) => { if (/^Piste \d+$/.test(t.name)) t.name = `Piste ${i + 1}`; });
      return tracksJson();
    },
    insertTrack: (at: number) => {
      let free = -1;
      for (let t = L.numTracks - 1; t >= at && free < 0; --t) if (!tracks[t].instrument) free = t;
      for (let t = at - 1; t >= 0 && free < 0; --t) if (!tracks[t].instrument) free = t;
      if (free < 0) return JSON.stringify("Les 16 pistes sont prises : libérez-en une pour insérer");
      const mv = <T,>(arr: T[]) => { const [x] = arr.splice(free, 1); arr.splice(at, 0, x); };
      mv(tracks); mv(trackSeq); mv(pats); mv(inserts); mv(insertPresets);
      tracks.forEach((t, i) => { if (/^Piste \d+$/.test(t.name)) t.name = `Piste ${i + 1}`; });
      return tracksJson();
    },
    // copie de piste à piste (v0.41) : aperçu simplifié (piste, séquences, patterns, inserts, réglages)
    copyLane: () => tracksJson(),
    setPlayLane: () => null,
    copyTrack: (from: number, to: number) => {
      if (!tracks[from]?.instrument) return JSON.stringify("La piste copiée est libre : rien à copier");
      const clone = <T,>(x: T): T => JSON.parse(JSON.stringify(x)) as T;
      tracks[to] = { ...clone(tracks[from]), name: /^Piste \d+$/.test(tracks[from].name) ? `Piste ${to + 1}` : tracks[from].name };
      trackSeq[to] = clone(trackSeq[from]); pats[to] = clone(pats[from]);
      inserts[to] = clone(inserts[from]); insertPresets[to] = clone(insertPresets[from]);
      values[to] = values[from].slice();
      return tracksJson();
    },
    setTrackColor: (t: number, c: number) => { tracks[t].color = c; return tracksJson(); },
    setTrackOutput: (t: number, o: number) => { if (tracks[t]) tracks[t].out = Math.max(0, Math.min(numGroups(L), Math.round(o))); return tracksJson(); },
    addMod: (ch: number, src: number, target: number, amount: number) => {
      const m = mods[ch].find((x) => x.target === target);
      const idx = m ? m.amountIndex : [...Array(64).keys()].find((i) => !mods[ch].some((x) => x.amountIndex === i)) ?? 0;
      if (m) m.source = src; else mods[ch].push({ source: src, target, amountIndex: idx });
      values[ch][L.modAmountBase + idx] = (amount + 1) / 2;
      return channelJson(ch);
    },
    removeMod: (ch: number, target: number) => { mods[ch] = mods[ch].filter((m) => m.target !== target); return channelJson(ch); },
    noteOn: (t: number, n: number) => { emit("tick", JSON.stringify({ ...baseTick(), hits: [[t, instDesc(t)?.drumKit ? n : n % 16]] })); return null; },
    noteOff: () => null,
    expression: () => null,
    seqSetStep: pattern((p, [l, s, v]) => { p.lanes[l].vel[s] = v; }),
    seqSetNote: pattern((p, [l, s, v]) => { p.lanes[l].note[s] = v; }),
    seqSetLength8: pattern((p, [l, s, v]) => { p.lanes[l].len[s] = v; }),
    seqSetMicro: pattern((p, [l, s, v]) => { p.lanes[l].micro[s] = v; }),
    seqTrackLength: pattern((p, [n]) => { p.length = Math.max(1, Math.min(L.maxSteps, n)); }),
    seqTrackSpeed: pattern((p, [n]) => { p.speed = n; }),
    seqTrackMute: pattern((p, [b]) => { p.mute = !!b; }),
    seqTrackSolo: pattern((p, [b]) => { p.solo = !!b; }),
    seqLaneDir: pattern((p, [l, d]) => { p.lanes[l].dir = d; }),
    seqLaneMute: pattern((p, [l, b]) => { p.lanes[l].mute = !!b; }),
    seqLaneSolo: pattern((p, [l, b]) => { p.lanes[l].solo = !!b; }),
    seqDivision: (t: number, d: number) => { if (divs[t]) divs[t][trackSeq[t].editing] = d; return patternJson(t); },
    seqClearLane: pattern((p, [l]) => { p.lanes[l].vel.fill(0); }),
    seqClearTrack: pattern((p) => { p.lanes.forEach((l) => l.vel.fill(0)); }),
    // v0.46 fix16 : décalage d'un pas, en boucle sur la longueur de chaque ligne
    seqShift: pattern((p, [dir, lane]) => {
      const rot = (a: number[], n: number) => { if (n < 2) return; const d = ((dir % n) + n) % n; const tmp = a.slice(0, n); for (let i = 0; i < n; ++i) a[(i + d) % n] = tmp[i]; };
      p.lanes.forEach((l, k) => {
        if (lane >= 0 && k !== lane) return;
        const n = (l as { length?: number }).length || p.length;
        rot(l.vel, n); rot(l.note, n); rot(l.len, n); rot(l.micro, n);
      });
      if (lane < 0) rot(p.flags, p.length);
    }),
    seqCopyPage: pattern((p, [from, to, n]) => { p.lanes.forEach((l) => { for (let k = 0; k < n; ++k) { l.vel[to + k] = l.vel[from + k]; l.note[to + k] = l.note[from + k]; l.len[to + k] = l.len[from + k]; l.micro[to + k] = l.micro[from + k]; } }); for (let k = 0; k < n; ++k) p.flags[to + k] = p.flags[from + k]; }),
    seqEditCells: (t: number, json: string) => {
      const v = JSON.parse(json) as { clear: [number, number][]; set: number[][]; ex: [number, number, number, number, number, number, [number, number][], number?, number?, number?][] };
      const p = pats[t][trackSeq[t].editing];
      for (const [l, st] of v.clear) { if (p.lanes[l]) p.lanes[l].vel[st] = 0; extras.delete(exKey(t, l, st)); }
      for (const [l, st, vel, note, len, micro] of v.set) {
        if (!p.lanes[l] || st >= L.maxSteps) continue;
        p.lanes[l].note[st] = note; p.lanes[l].len[st] = len; p.lanes[l].micro[st] = micro; p.lanes[l].vel[st] = vel;
        extras.delete(exKey(t, l, st));
      }
      for (const [l, st, cond, a, b, prob, locks, rat, rs, flam] of v.ex) Object.assign(ex(t, l, st), { cond, a, b, prob, rat: rat ?? 1, rs: rs ?? 0, flam: flam ?? 0, locks: locks.map((x) => [x[0], x[1]]) });
      return patternJson(t);
    },
    stepsLock: (t: number, cellsJson: string, g: number, val: number, clear: boolean) => {
      for (const [l, st] of JSON.parse(cellsJson) as [number, number][]) {
        const e = ex(t, l, st);
        e.locks = e.locks.filter((x) => x[0] !== g);
        if (!clear) e.locks.push([g, val]);
      }
      return extrasJson(t);
    },
    stepCondition: (t: number, l: number, s: number, cond: number, a: number, b: number, prob: number) => { Object.assign(ex(t, l, s), { cond, a, b, prob }); return extrasJson(t); },
    stepLock: (t: number, l: number, s: number, g: number, v: number) => { const e = ex(t, l, s); const k = e.locks.find((x) => x[0] === g); if (k) k[1] = v; else e.locks.push([g, v]); return extrasJson(t); },
    stepClearLock: (t: number, l: number, s: number, g: number) => { const e = ex(t, l, s); e.locks = e.locks.filter((x) => x[0] !== g); return extrasJson(t); },
    stepRatchet: (t: number, l: number, s: number, n: number, rs: number) => { Object.assign(ex(t, l, s), { rat: Math.max(1, Math.min(8, n)), rs }); return extrasJson(t); },
    stepsRatchet: (t: number, cells: string, n: number, rs: number) => { for (const [l, s] of JSON.parse(cells) as [number, number][]) Object.assign(ex(t, l, s), { rat: n, rs }); return extrasJson(t); },
    stepClear: (t: number, l: number, s: number) => { extras.delete(exKey(t, l, s)); return extrasJson(t); },
    seqEditPattern: (t: number, pt: number) => { trackSeq[t].editing = pt; return patternJson(t); },
    seqCopyPattern: (t: number, a: number, b: number) => { pats[t][b] = JSON.parse(JSON.stringify(pats[t][a])); return patternJson(t); },
    seqCopyPatternFrom: (t: number, a: number, b: number, st: number) => { pats[t][b] = JSON.parse(JSON.stringify(pats[st][a])); return patternJson(t); },
    seqClearPattern: (t: number, pt: number) => { pats[t][pt].lanes.forEach((l) => l.vel.fill(0)); return patternJson(t); },
    seqRecQuant: (t: number, q: number) => { recQuant = q; return patternJson(t); },
    seqStepFlags: pattern((p, [st, f]) => { p.flags[st] = f & 15; }),
    seqStepsFlag: (t: number, stepsJson: string, bit: number, on: boolean) => {
      const p = patterns[t];
      for (const st of JSON.parse(stepsJson) as number[]) p.flags[st] = on ? p.flags[st] | bit : p.flags[st] & ~bit;
      return patternJson(t);
    },
    seqLaneLength: pattern((p, [l, n]) => { p.lanes[l].length = Math.max(0, Math.min(L.maxSteps, n)); }),
    seqAccent: (t: number, a: number) => { trackSeq[t].accent = Math.max(0, Math.min(100, a)); return patternJson(t); },
    stepsFlam: (t: number, cells: string, ms: number) => { for (const [l, s] of JSON.parse(cells) as [number, number][]) Object.assign(ex(t, l, s), { flam: ms }); return extrasJson(t); },
    getLaunch: launchJson,
    launchPattern: (t: number, pt: number) => { if (transport.playing) trackSeq[t].queued = pt; else { trackSeq[t].playing = pt >= NP ? -1 : pt; } return launchJson(); },
    launchRow: (r: number) => {
      // case vide (aucune note) : la piste s'arrête (v0.23, comme le moteur)
      trackSeq.forEach((ts, t) => {
        const empty = !pats[t][r].lanes.some((l: { vel: number[] }) => l.vel.some((v) => v > 0));
        const target = empty ? NP : r;
        if (transport.playing) ts.queued = target; else ts.playing = empty ? -1 : r;
      });
      return launchJson();
    },
    setLaunchQuant: (q: number) => { launchQuant = q; return launchJson(); },
    setRowScene: (r: number, sc: number) => { rowScenes[r] = sc; return launchJson(); },
    setMuteTails: (on: boolean) => { muteTails = !!on; return launchJson(); },
    copyRow: (from: number, to: number) => { for (let t = 0; t < pats.length; ++t) pats[t][to] = JSON.parse(JSON.stringify(pats[t][from])); return launchJson(); },
    clearRow: (p: number) => { for (let t = 0; t < pats.length; ++t) pats[t][p] = mkPattern(); return launchJson(); },
    getMidi: midiJson,
    setMidiRoute: (n: number, c: number, on: boolean) => { midiRoute.note = n; midiRoute.ctrl = c; midiRoute.ctrlOn = !!on; return midiJson(); },
    setTrackMidiChannel: (t: number, c: number) => { midiRoute.tracks[t] = c; return midiJson(); },
    midiLearn: (target: number) => { midiLearn = target; if (target !== -1) window.setTimeout(() => { midiMaps.push([0, 0, 20 + midiMaps.length, target]); midiLearn = -1; }, 1500); return midiJson(); },
    midiForget: (target: number) => { for (let i = midiMaps.length - 1; i >= 0; --i) if (midiMaps[i][3] === target) midiMaps.splice(i, 1); return midiJson(); },
    // préréglages de l'utilisateur : en mémoire dans l'aperçu (fichiers dans l'application)
    listPresets: (scope: string, ch: number, slot: number, pad: number) => {
      const folder = presetFolder(scope, ch, slot);
      if (!folder) return "null";
      const list = userPresets.filter((p) => p.folder === folder);
      const kind = scope === "pad" ? instDesc(ch)?.padKinds[pad] ?? "" : "";
      return JSON.stringify({ folder, kind, banks: [...new Set(list.map((p) => p.bank))].sort(), user: list.map((p) => [p.bank, p.name, p.kind]), root: "Documents/Groovebox/Préréglages" });
    },
    savePreset: (scope: string, ch: number, slot: number, pad: number, bank: string, name: string, overwrite: boolean) => {
      const folder = presetFolder(scope, ch, slot);
      const b = String(bank).trim().slice(0, 60), n = String(name).trim().slice(0, 60);
      if (!folder || !b || !n) return JSON.stringify("invalid");
      const i = userPresets.findIndex((p) => p.folder === folder && p.bank === b && p.name === n);
      if (i >= 0 && !overwrite) return JSON.stringify("exists");
      const prefix = scope === "pad" ? `p${String(pad + 1).padStart(2, "0")}_` : "";
      const d = scope === "fx" ? fxDesc(ch, slot) : instDesc(ch);
      const base = scope === "fx" ? L.insertBase + slot * L.insertSlots : L.instrumentBase;
      const vals: [string, number][] = (d?.params ?? []).map((p, k) => [p.id, values[ch][base + k]] as [string, number]).filter(([id]) => id.startsWith(prefix)).map(([id, v]) => [id.slice(prefix.length), v]);
      const entry = { folder, bank: b, name: n, kind: scope === "pad" ? instDesc(ch)?.padKinds[pad] ?? "" : "", vals };
      if (i >= 0) userPresets[i] = entry; else userPresets.push(entry);
      if (scope === "inst" && ch < L.numTracks) tracks[ch].preset = n;
      if (scope === "fx") insertPresets[ch][slot] = n;
      return JSON.stringify("ok");
    },
    loadPreset: (scope: string, ch: number, slot: number, pad: number, bank: string, name: string) => {
      const folder = presetFolder(scope, ch, slot);
      const e = userPresets.find((p) => p.folder === folder && p.bank === bank && p.name === name);
      if (!e) return "null";
      const prefix = scope === "pad" ? `p${String(pad + 1).padStart(2, "0")}_` : "";
      const d = scope === "fx" ? fxDesc(ch, slot) : instDesc(ch);
      const base = scope === "fx" ? L.insertBase + slot * L.insertSlots : L.instrumentBase;
      (d?.params ?? []).forEach((p, k) => {
        if (!p.id.startsWith(prefix)) return;
        const v = e.vals.find(([id]) => id === p.id.slice(prefix.length));
        values[ch][base + k] = v ? v[1] : toNorm(p, p.def);
      });
      if (scope === "inst" && ch < L.numTracks) tracks[ch].preset = name;
      if (scope === "fx") insertPresets[ch][slot] = name;
      return channelJson(ch);
    },
    deletePreset: (scope: string, ch: number, slot: number, _pad: number, bank: string, name: string) => {
      const folder = presetFolder(scope, ch, slot);
      const i = userPresets.findIndex((p) => p.folder === folder && p.bank === bank && p.name === name);
      if (i >= 0) userPresets.splice(i, 1);
      return i >= 0;
    },
    revealPresets: () => null,
    // dossiers de travail : une petite bibliothèque factice dans l'aperçu
    getFolders: () => JSON.stringify(mockFolders()),
    chooseFolder: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "folder", error: "Choix du dossier : disponible dans l'application" })), 0); return null; },
    resetFolder: () => JSON.stringify(mockFolders()),
    revealFolder: () => null,
    listFolder: (kind: string, rel: string) => {
      const tree: Record<string, [string, number, number][]> = kind === "samples"
        ? { "": [["Batteries", 1, 0], ["Boucles", 1, 0], ["voix lead.wav", 0, 1843200]], "Batteries": [["kick 01.wav", 0, 96000], ["snare sec.wav", 0, 64000]], "Boucles": [["break 92 bpm.wav", 0, 2400000]] }
        : { "": [["Démo hip-hop.gbx", 0, 1200000], ["Session du jeudi.gbx", 0, 860000]] };
      const items = tree[String(rel)];
      return JSON.stringify(items ? { ok: true, items } : { ok: false, items: [] });
    },
    loadSampleFile: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "sample", error: "Bibliothèque : disponible dans l'application" })), 0); return null; },
    openProjectFile: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "open", error: "Ouverture : disponible dans l'application" })), 0); return null; },
    saveProjectNamed: () => JSON.stringify("error"),
    applyChain: (ch: number, name: string) => {
      const c = cat.chains.find((x) => x.name === name);
      if (c) for (let s = 0; s < 4; ++s) setInsert(ch, s, c.inserts[s]?.[0] ?? "");
      return channelJson(ch);
    },
    getSpectrum: (_ch: number, _slot: number, bands: number) => JSON.stringify(Array.from({ length: bands }, (_, b) => -30 - b * 0.35 + (transport.playing ? 8 * Math.sin(b * 0.4 + performance.now() / 200) : -40) + (b < 12 ? 10 : 0))),
    resetLoudness: () => null,
    chooseSample: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "sample", error: "Sélecteur de fichiers : disponible dans l'application" })), 0); return null; },
    saveProject: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "save", error: "Enregistrement : disponible dans l'application" })), 0); return null; },
    openProject: () => { window.setTimeout(() => emit("fileDone", JSON.stringify({ kind: "open", error: "Ouverture : disponible dans l'application" })), 0); return null; },
    sceneSetLock: (n: number, g: number, v: number) => { const sc = scenes[n]; const k = sc.locks.find((x) => x[0] === g); if (k) k[1] = v; else sc.locks.push([g, v]); sc.locks.sort((a, b) => a[0] - b[0]); return scenesJson(); },
    sceneClearLock: (n: number, g: number) => { scenes[n].locks = scenes[n].locks.filter((x) => x[0] !== g); return scenesJson(); },
    sceneClear: (n: number) => { scenes[n].locks = []; return scenesJson(); },
    sceneMute: (n: number, b: boolean) => { scenes[n].muted = !!b; return scenesJson(); },
    sceneCopy: (n: number, to: number) => { scenes[to].locks = scenes[n].locks.map((x) => [...x] as [number, number]); return scenesJson(); },
    sceneLaunch: (n: number, other: boolean) => { const toB = (globals.xfade > 0.5) !== !!other; if (toB) globals.sceneB = n + 1; else globals.sceneA = n + 1; return null; },
    play: () => { transport.playing = true; transport.ppq = 0; return null; },
    stop: () => { transport.playing = false; transport.ppq = -1; return null; },
    record: (b: boolean) => { transport.rec = !!b; return null; },
    fill: (b: boolean) => { transport.fill = !!b; return null; },
    breakKey: (b: boolean) => { transport.brk = !!b; return null; },
    breakTimed: (beats: number) => { transport.brk = beats > 0; (transport as { tbrk?: boolean }).tbrk = beats > 0; if (beats > 0) window.setTimeout(() => { transport.brk = false; (transport as { tbrk?: boolean }).tbrk = false; }, (beats * 60000) / 120); return null; },
    setGlobal: (id: string, v: number) => {
      if (id in globals) globals[id as keyof typeof globals] = v;
      const mk = /^macro(\d)$/.exec(id); if (mk) macros[Number(mk[1]) - 1].v = v;
      return null;
    },
    getMacros: () => JSON.stringify(macros),
    setMacro: (m: number, g: number) => {
      const l = macros[m].links; const i = l.findIndex((x) => x[0] === g);
      if (i >= 0) l.splice(i, 1); else if (l.length < 8) l.push([g, 0, 1]);
      return JSON.stringify(macros);
    },
    setMacroRange: (m: number, g: number, lo: number, hi: number) => { for (const x of macros[m].links) if (x[0] === g) { x[1] = lo; x[2] = hi; } return JSON.stringify(macros); },
    setMacroMode: (m: number, r: number) => { macros[m].rel = r === 1; return JSON.stringify(macros); },
    clearMacro: (m: number) => { macros[m].links = []; return JSON.stringify(macros); },
    newProject: () => tracksJson(),
    loadSample: (t: number, i: number, name: string) => { samples[t][i] = name; return channelJson(t); },
    clearSample: (t: number, i: number) => { samples[t][i] = null; return channelJson(t); },
    autoSlice: (t: number) => channelJson(t),
    recordAction: (t: number, a: number) => {
      if ((tracks[t]?.instrument ?? "").startsWith("voc")) { // v0.33 : prise de son simulée (armée → en cours → gardée)
        vocCap.st = a === 2 ? 0 : vocCap.st === 0 || vocCap.st === 3 ? 1 : 0; if (vocCap.st === 1) vocCap.t0 = Date.now(); if (a === 2) vocCap.has = false; return null;
      }
      recMock = a === 2 ? 0 : a === 1 ? (recMock === 1 ? 2 : recMock) : recMock === 1 ? 2 : 1; return null; },
    undo: () => { const ok = histMock.undo > 0; if (ok) { histMock.undo--; histMock.redo++; } return [ok, histMock.undo > 0, histMock.redo > 0, histMock.photo]; },
    redo: () => { const ok = histMock.redo > 0; if (ok) { histMock.redo--; histMock.undo++; } return [ok, histMock.undo > 0, histMock.redo > 0, histMock.photo]; },
    // v0.32 : enregistrement audio simulé (aperçu dans le navigateur)
    audioRecStart: (mode: number, format: number) => { audioRecMock.active = true; audioRecMock.mode = mode; audioRecMock.format = format ? "mp3" : "wav"; audioRecMock.t0 = Date.now(); audioRecMock.files = []; return JSON.stringify(recState()); },
    audioRecStop: () => {
      audioRecMock.active = false; audioRecMock.secs = (Date.now() - audioRecMock.t0) / 1000;
      const ext = audioRecMock.format; audioRecMock.files = [...(audioRecMock.mode !== 1 ? [`Master.${ext}`] : []), ...(audioRecMock.mode !== 0 ? [`P01 Batterie.${ext}`, `P02 Basse acide.${ext}`] : [])];
      return JSON.stringify(recState());
    },
    audioRecStatus: () => JSON.stringify(recState()),
    audioRecReveal: () => false,
    setSpectrum: (on: number) => { specOn = on > 0; return null; },
    takePhoto: () => { histMock.photo = true; return [true, histMock.undo > 0, histMock.redo > 0, true]; },
    recallPhoto: () => { if (histMock.photo) { histMock.undo++; histMock.redo = 0; } return [histMock.photo, histMock.undo > 0, histMock.redo > 0, histMock.photo]; },
    keepRecording: (t: number, i: number) => { samples[t][i] = `Enregistrement piste ${t + 1}.wav`; return channelJson(t); },
  };

  // --- Morceau simulé (rangées et courbes en mémoire)
  const song = {
    mode: false, loop: true, loopFrom: 0, loopTo: -1, loopCount: 0, read: true, recPlay: false, recMoves: false, version: 1,
    rows: [[2, -1, [0, 0, 0, 0, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2]], [4, 3, [1, 0, 1, 0, -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2]], [2, -1, [0, 1, -1, 0, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2]]] as [number, number, number[]][],
    lanes: [[L.instrumentBase + 3 + L.slotsPerChannel, 1, 12, 30, Array.from({ length: 96 }, (_, k) => 0.5 + 0.4 * Math.sin(k / 9))]] as [number, number, number, number, number[]][],
  };
  const songJson = () => {
    const total = song.rows.reduce((a, r) => a + 4 * r[0], 0);
    return JSON.stringify({ ...song, span: Math.max(16, total, ...song.lanes.map((l) => l[3])) });
  };
  const bump = () => { song.version += 1; return songJson(); };
  const patLanesMock = new Map<string, [number, number, number[]][]>();
  const songPts = new Map<number, number[]>();
  const patLanesJson = (t: number) => {
    const p = trackSeq[t]?.editing ?? 0;
    if (!patLanesMock.has(`0:0`)) patLanesMock.set(`0:0`, [[L.instrumentBase + 3, 1, [0, 0.2, 4, 0.8, 8, 0.4, 12, 0.9, 15.9, 0.3]]]);
    return JSON.stringify({ track: t, pattern: p, len: pats[t]?.[p]?.length ?? 16, lanes: patLanesMock.get(`${t}:${p}`) ?? [] });
  };
  const songRowAt = (ppq: number) => {
    const total = song.rows.reduce((a, r) => a + 4 * r[0], 0);
    if (!song.mode || total <= 0) return -1;
    const n = song.rows.length, first = Math.min(song.loopFrom, n - 1), last = Math.max(first, song.loopTo < 0 || song.loopTo >= n ? n - 1 : song.loopTo);
    const a = song.rows.slice(0, first).reduce((s, r) => s + 4 * r[0], 0), e = song.rows.slice(0, last + 1).reduce((s, r) => s + 4 * r[0], 0);
    let b = song.loop && ppq >= e && e > a ? a + ((ppq - a) % (e - a)) : ppq;
    for (let i = 0; i < song.rows.length; ++i) { b -= 4 * song.rows[i][0]; if (b < 0) return i; }
    return -1;
  };
  // --- v0.46 fix8 : montage audio simulé (deux prises, quelques clips)
  const mont = {
    on: false, bpm: 122, loop: false, loopA: 0, loopB: 16, playhead: 0, pos: 0, status: "",
    takes: [["Prise_1-P01", 0, 48000, 48000 * 24, 1], ["Prise_1-P02", 1, 48000, 48000 * 24, 1], ["Prise_1-P03", 2, 48000, 48000 * 24, 1], ["Prise_1-R1", 16, 48000, 48000 * 24, 1]] as [string, number, number, number, number][],
    clips: [] as [number, string, number, number, number, number, number, number][],
  };
  {
    const spb = 48000 * 60 / 122; // échantillons par noire
    const add = (lane: number, take: string, b0: number, b1: number) => mont.clips.push([lane, take, Math.round(b0 * spb), Math.round((b1 - b0) * spb), b0, 0, 0.004, 0.004]);
    add(0, "Prise_1-P01", 0, 16); add(0, "Prise_1-P01", 24, 40);
    add(1, "Prise_1-P02", 8, 32);
    add(2, "Prise_1-P03", 4, 12); add(2, "Prise_1-P03", 16, 24); add(2, "Prise_1-P03", 32, 44);
    add(16, "Prise_1-R1", 0, 44);
  }
  const montJson = () => JSON.stringify({ ...mont, pos: mont.playhead });
  const peaksMock = (id: string) => {
    const n = Math.ceil(48000 * 24 / 1024);
    const seed = id.length * 7;
    let bin = "";
    for (let k = 0; k < n; ++k) bin += String.fromCharCode(Math.max(0, Math.min(255, Math.round(150 + 90 * Math.sin((k + seed) / 3.1) * Math.cos(k / 17)))));
    return btoa(bin);
  };
  Object.assign(fns, {
    getMontage: montJson,
    montageSetClips: (json: string) => { mont.clips = JSON.parse(json); return montJson(); },
    montageSetFlags: (on: boolean, loop: boolean, a: number, b: number) => { mont.on = on; mont.loop = loop; mont.loopA = a; mont.loopB = b; return montJson(); },
    montagePlayhead: (b: number) => { mont.playhead = Math.max(0, b); return montJson(); },
    montageRecord: (start: boolean) => { mont.status = start ? "Prise en cours…" : "3 clips ajoutés"; return montJson(); },
    montageExport: () => { mont.status = "Export en cours…"; return montJson(); },
    montagePeaks: (id: string) => peaksMock(id),
    getSong: songJson,
    setSongFlags: (m: boolean, l: boolean, rd: boolean) => { song.mode = m; song.loop = l; song.read = rd; return bump(); },
    setSongLoopCount: (n: number) => { song.loopCount = Math.max(0, Math.min(999, Math.round(n))); return bump(); },
    setSongLoopRange: (f: number, t: number) => { song.loopFrom = Math.max(0, f); song.loopTo = Math.max(-1, t); if (song.loopTo >= 0 && song.loopTo < song.loopFrom) [song.loopFrom, song.loopTo] = [song.loopTo, song.loopFrom]; return bump(); },
    setSongRecord: (p: boolean, m: boolean) => { song.recPlay = p; song.recMoves = m; return songJson(); },
    setSongRows: (json: string) => { song.rows = (JSON.parse(json) as [number, number, number[]][]).slice(0, 128); return bump(); },
    songRowFromPlaying: (after: number) => {
      const row: [number, number, number[]] = [4, -1, trackSeq.map((ts) => (ts.playing >= 0 && ts.playing < NP ? ts.playing : -1))];
      song.rows.splice(after < 0 ? song.rows.length : after + 1, 0, row);
      return bump();
    },
    // courbes de pattern (piste : pattern édité) et points des courbes du morceau
    getPatternLanes: (t: number) => patLanesJson(t),
    setPatternLane: (t: number, g: number, pts: string, on: boolean) => {
      const key = `${t}:${trackSeq[t]?.editing ?? 0}`;
      const list = patLanesMock.get(key) ?? [];
      const arr = JSON.parse(pts) as number[];
      const i = list.findIndex((l) => l[0] === g);
      if (!arr.length) { if (i >= 0) list.splice(i, 1); } else if (i >= 0) list[i] = [g, on ? 1 : 0, arr]; else list.push([g, on ? 1 : 0, arr]);
      patLanesMock.set(key, list);
      return patLanesJson(t);
    },
    getSongLane: (g: number) => JSON.stringify(songPts.get(g) ?? Array.from({ length: 24 }, (_, k) => (k % 2 ? 0.5 + 0.4 * Math.sin(k / 3) : k * 1.25))),
    setSongLane: (g: number, pts: string) => {
      const arr = JSON.parse(pts) as number[];
      songPts.set(g, arr);
      const i = song.lanes.findIndex((l) => l[0] === g);
      const last = arr.length >= 2 ? arr[arr.length - 2] : 0;
      if (!arr.length) { if (i >= 0) song.lanes.splice(i, 1); }
      else if (i >= 0) song.lanes[i] = [g, song.lanes[i][1], arr.length / 2, last, song.lanes[i][4]];
      else song.lanes.push([g, 1, arr.length / 2, last, new Array(96).fill(0.5)]);
      return bump();
    },
    songLane: (g: number, action: number) => {
      if (action === 2) song.lanes = [];
      else { const i = song.lanes.findIndex((l) => l[0] === g); if (i >= 0) { if (action === 0) song.lanes[i][1] = song.lanes[i][1] ? 0 : 1; else song.lanes.splice(i, 1); } }
      return bump();
    },
  });

  // --- Horloge simulée et événement « tick »
  let recMock = 0;
  const vocCap = { st: 0, t0: 0, has: false }; // v0.33
  const audioRecMock = { active: false, mode: 0, format: "wav" as "wav" | "mp3", t0: 0, secs: 0, files: [] as string[] };
const recState = () => ({ active: audioRecMock.active, encoding: false, progress: 1, seconds: audioRecMock.active ? (Date.now() - audioRecMock.t0) / 1000 : audioRecMock.secs, dropped: 0,
  mode: audioRecMock.mode, format: audioRecMock.format, folder: "Groovebox/Enregistrements/2026-10-30 18h52m03", files: audioRecMock.files, error: "", canReveal: false });
const histMock = { undo: 3, redo: 0, photo: false };
  const vocCapTick = () => {
    const el = (Date.now() - vocCap.t0) / 1000;
    if (vocCap.st === 1 && el > 1.2) vocCap.st = 2;
    if (vocCap.st === 2 && el > 3.4) { vocCap.st = 0; vocCap.has = true; }
    return [vocCap.st, vocCap.st === 2 ? el - 1.2 : vocCap.has ? 2.2 : 0, vocCap.has ? ((Date.now() / 2450) % 1) : 0, vocCap.has ? 1 : 0, Math.floor(Date.now() / 600) % 6, vocCap.st >= 1 ? 0.3 : 0];
  };
  const baseTick = () => ({
    // v0.42 : Pieuvre 8 — piste jouée, puis 5 valeurs par piste interne
    inst: tracks[focus]?.instrument === "octa" ? [0, ...Array.from({ length: 8 }, (_, k) => [k === 0 ? recMock : 0, k === 0 && recMock ? 1.5 : 0, transport.playing ? (transport.ppq % 4) / 4 : 0, 0, Math.round(toPlainLocal(focus, `t${k + 1}_machine`))]).flat()]
      // vocodeurs : 16 bandes qui bougent comme une voix
      : (tracks[focus]?.instrument ?? "").startsWith("voc") ? [16, (Date.now() / 900) % 1 > 0.8 ? 0.7 : 0.1, ...Array.from({ length: 16 }, (_, k) => Math.max(0, 0.75 - Math.abs(Math.sin(Date.now() / 300 + k * 0.5)) * 0.5 - k * 0.02)), ...vocCapTick()]
      : [],
    t: { ...transport, bpm: globals.bpm, lq: transport.playing ? 32 : 0, lp: transport.playing ? (Math.max(0, transport.ppq) % 32) / 32 : 0 }, g: { ...globals }, hits: [] as number[][],
    peaks: Array.from({ length: CH }, (_, i) => (transport.playing && i < 4 ? 0.2 + Math.random() * 0.4 : transport.playing && i === L.masterChannel ? 0.6 : 0)),
    outlr: transport.playing ? [0.5 + Math.random() * 0.1, 0.4 + Math.random() * 0.1] : [0, 0], och: 2,
    pos: new Array(16).fill(-1), tpos: new Array(16).fill(-1), eff: [] as number[][], mac: macros.map((m) => m.v), mods: Array.from({ length: 6 }, (_, m) => [m === 0 ? Math.sin(performance.now() / 300) : (performance.now() / 500 % 1), Math.floor(performance.now() / 250) % 16]), fxm: [[3 + Math.random() * 2, 0.5, 0.4], [2, 0.5, 0.4], [0, 0, 0], [0, 0, 0]],
    pat: trackSeq.map((ts) => [ts.playing, ts.queued]), learn: midiLearn, midi: false,
    hist: [histMock.undo > 0 ? 1 : 0, histMock.redo > 0 ? 1 : 0, histMock.photo ? 1 : 0],
    // aperçu : un spectre de mix plausible (grave fort, pente vers l'aigu, bosses qui bougent)
    spec: specOn ? [...Array.from({ length: 96 }, (_, b) => Math.round(10 * (-18 - b * 0.42 + 6 * Math.sin(b / 7 + transport.ppq) + (b < 12 ? 8 : 0) + Math.random() * 3 - (transport.playing ? 0 : 50)))),
      ...Array.from({ length: 96 }, (_, b) => Math.round(10 * (-40 - b * 0.3 + Math.random() * 4 - (transport.playing ? 0 : 50))))] : undefined,
    loud: transport.playing ? [-11 + Math.random(), -11.5, -12.1, -1.4 + Math.random() * 0.4, -0.9, -14 + Math.random()] : [-100, -100, -100, -100, -100, -100],
    perf: [transport.playing ? 14 + Math.random() * 3 : 3, transport.playing ? 22 + Math.random() * 4 : 5, 286, 41, 48000, 192, 4, 8192],
    project: false, seq: false, structure: false,
    song: [songRowAt(transport.ppq), transport.playing ? transport.ppq : -1, song.version, song.recPlay ? 1 : 0, song.recMoves ? 1 : 0],
  });
  let last = performance.now();
  window.setInterval(() => {
    const now = performance.now();
    const tick = baseTick();
    if (transport.playing) {
      const prev = transport.ppq;
      transport.ppq += ((now - last) / 1000) * (globals.bpm / 60);
      const spb = [8, 12, 16, 24, 32, 4, 48, 64][divisionOf(Math.min(focus, 15))];
      const stepQ = 4 / spb;
      const t = Math.min(focus, 15);
      const barCrossed = Math.floor(prev / 4) !== Math.floor(transport.ppq / 4);
      for (let tr = 0; tr < L.numTracks; ++tr) {
        const ts = trackSeq[tr];
        if (ts.queued >= 0 && (barCrossed || launchQuant === 0)) { ts.playing = ts.queued >= NP ? -1 : ts.queued; ts.queued = -1; tick.seq = true; }
        if (ts.playing < 0) continue;
        const p = { ...pats[tr][ts.playing], mute: ts.mute };
        const k0 = Math.floor(prev / stepQ), k1 = Math.floor(transport.ppq / stepQ);
        const step = k1 % p.length;
        tick.tpos[tr] = step;
        if (tr === t) for (let l = 0; l < 16; ++l) tick.pos[l] = step;
        if (k1 !== k0) p.lanes.forEach((ln, l) => { if (ln.vel[step] > 0 && !p.mute && !ln.mute) tick.hits.push([tr, instDesc(tr)?.drumKit ? l : ln.note[step] % 16]); });
      }
      tick.t.ppq = transport.ppq;
      // valeur jouée : le LFO simulé fait bouger la coupure de la basse
      if (focus === 1) tick.eff.push([L.instrumentBase + 3, Math.min(1, Math.max(0, values[1][L.instrumentBase + 3] + 0.3 * Math.sin(now / 300)))]);
    }
    last = now;
    emit("tick", JSON.stringify(tick));
  }, 33);

  const handleFromFrontend = (id: string, payload: any) => {
    if (id !== "__juce__invoke") return;
    const result = fns[payload.name]?.(...payload.params) ?? null;
    window.setTimeout(() => emit("__juce__complete", { promiseId: payload.resultId, result }), 0);
  };

  window.__JUCE__ = {
    backend: {
      addEventListener(id: string, fn: Listener) {
        if (!listeners.has(id)) listeners.set(id, new Map());
        const n = nextId++;
        listeners.get(id)!.set(n, fn);
        return [id, n];
      },
      removeEventListener([id, n]: [string, number]) { listeners.get(id)?.delete(n); },
      emitEvent(id: string, payload: unknown) { handleFromFrontend(id, payload); },
    },
    initialisationData: { __juce__functions: Object.keys(fns), __juce__sliders: [], __juce__toggles: [], __juce__comboBoxes: [], __juce__platform: [] },
  } as unknown as Window["__JUCE__"];
}
