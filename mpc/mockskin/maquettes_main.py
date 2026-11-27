#!/usr/bin/env python3
"""OFAD GOLEM — écrans de tête dessinés à la main au canvas (Pieuvre 8, Orbiteur, Glutamake),
repris tels quels avec les réglages existants des instruments (valeurs par défaut du catalogue).
Machine électronique et Fatsub passent par les gabarits A et C (maquettes.py)."""
import json
import math
import os

import maquettes_gen as G
from maquettes_gen import P, fmt, ang, kv, esc, HEAD, TAIL, FIRM, TABS, tabs

BODIES = {}


def side(out, key, t, tpl, D):
    json.dump({"key": key, "type": t, "tpl": tpl, "D": D}, open(os.path.join(out, key + ".json"), "w", encoding="utf-8"), ensure_ascii=False)


def board_html(out, key, title, fonts, body, D):
    html = HEAD % (title, fonts) + body + TAIL % esc(json.dumps(D, ensure_ascii=False))
    open(os.path.join(out, key + ".dc.html"), "w", encoding="utf-8").write(html)


# ============================================================================== Pieuvre 8 (A)
BODIES["pieuvre"] = """<div style="width:1280px;height:800px;background:#10161a;font-family:Inter,sans-serif;color:#e6eef0;display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;flex-direction:column;padding:12px 14px 8px;gap:10px">
    <div style="display:flex;align-items:center;gap:14px">
      <div style="font-family:'Space Grotesk',sans-serif;font-weight:700;font-size:26px;color:#3fd0e0">Pieuvre 8</div>
      <div style="font-size:13px;color:#93a3a9">Gros bouton = boutons 1 à 8 · petit bouton = 9 à 16</div>
      <div style="margin-left:auto;display:flex;gap:6px">
        <sc-for list="{{D.banks}}" as="b" hint-placeholder-count="3">
          <button style="height:36px;padding:0 16px;border-radius:18px;border:{{b.bd}};background:{{b.bg}};color:{{b.fg}};font-weight:{{b.fw}};font-size:14px">{{b.n}}</button>
        </sc-for>
      </div>
    </div>
    <div style="flex:1;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:7px">
      <sc-for list="{{D.tracks}}" as="t" hint-placeholder-count="8">
        <div style="background:#172025;border:1px solid {{t.border}};border-radius:12px;display:flex;flex-direction:column;align-items:center;padding:10px 8px;gap:6px">
          <div style="width:100%;display:flex;align-items:center;justify-content:space-between">
            <div style="font-family:'Space Grotesk',sans-serif;font-weight:700;font-size:24px;color:{{t.col}}">T{{t.n}}</div>
            <div data-k="led" data-p="{{t.machid}}" data-on="{{t.col}}" data-off="#2c3a40" style="width:12px;height:12px;border-radius:50%;background:{{t.led}}"></div>
          </div>
          <div data-k="chip" data-p="{{t.slotid}}" style="width:100%;height:32px;border-radius:8px;border:1px solid #2c3a40;background:#10161a;color:#e6eef0;font-size:12px;display:flex;align-items:center;padding:0 8px;white-space:nowrap;overflow:hidden"><span data-v="{{t.slotid}}" data-j="left" style="flex:1;overflow:hidden">{{t.slot}}</span></div>
          <div style="width:100%;height:46px;border-radius:6px;background:#0c1114;display:flex;align-items:center;justify-content:center;position:relative;overflow:hidden">
            <svg width="130" height="40" viewBox="0 0 130 40" aria-hidden="true"><path d="{{t.wave}}" fill="none" stroke="{{t.col}}" stroke-width="1.6"></path></svg>
            <div style="position:absolute;top:0;bottom:0;left:{{t.start}}%;width:{{t.len}}%;background:{{t.col}};opacity:.16"></div>
          </div>
          <div style="width:100%;display:flex;justify-content:space-between;font-size:12px;color:#93a3a9;white-space:nowrap"><span style="display:flex;gap:4px"><span>Boucle</span><span data-v="{{t.loopid}}" data-j="left">{{t.loop}}</span></span><span data-v="{{t.strid}}" data-j="right">{{t.str}}</span></div>
          <div style="font-size:12px;color:#93a3a9;margin-top:2px">{{t.l1}}</div>
          <div data-k="knob" data-p="{{t.big.id}}" data-col="{{t.col}}" data-track="#26333a" style="width:96px;height:96px;border-radius:50%;background:conic-gradient(from 225deg,{{t.col}} 0 {{t.big.a}}deg,#26333a {{t.big.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
            <div data-v="{{t.big.id}}" data-fw="Bold" style="width:80px;height:80px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#34434a,#141b1f 70%);display:flex;align-items:center;justify-content:center;font-family:'Space Grotesk',sans-serif;font-size:16px;font-weight:700">{{t.big.v}}</div>
          </div>
          <div style="font-size:12px;color:#93a3a9">{{t.l2}}</div>
          <div data-k="knob" data-p="{{t.small.id}}" data-col="{{t.col}}" data-track="#26333a" style="width:62px;height:62px;border-radius:50%;background:conic-gradient(from 225deg,{{t.col}} 0 {{t.small.a}}deg,#26333a {{t.small.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
            <div data-v="{{t.small.id}}" data-fw="SemiBold" style="width:50px;height:50px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#2e3c42,#141b1f 70%);display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:600">{{t.small.v}}</div>
          </div>
          <div style="flex:1;width:100%;display:flex;gap:8px;align-items:stretch;justify-content:center;margin-top:4px">
            <div data-k="vbar" data-p="{{t.volid}}" style="width:14px;border-radius:7px;background:#26333a;position:relative;overflow:hidden"><div style="position:absolute;left:0;right:0;bottom:0;height:{{t.lvl}}%;background:{{t.col}}"></div></div>
            <div style="display:flex;flex-direction:column;justify-content:space-between;font-size:11px;color:#93a3a9"><div>Volume</div><div data-v="{{t.volid}}" data-j="left" style="font-size:13px;color:#e6eef0">{{t.db}}</div><div data-v="{{t.machid}}" data-j="left">{{t.mach}}</div></div>
          </div>
        </div>
      </sc-for>
    </div>
  </div>
""" + TABS + """</div>
"""

W1 = "M0 20 L6 6 L10 32 L16 12 L22 28 L28 4 L34 36 L40 16 L48 24 L56 8 L62 30 L70 14 L78 26 L86 10 L94 30 L102 18 L110 22 L118 12 L130 20"
W2 = "M0 20 L10 2 L14 38 L20 20 L40 20 L50 4 L54 36 L60 20 L80 20 L90 2 L94 38 L100 20 L130 20"
W3 = "M0 20 C10 4 20 36 30 20 S50 6 60 20 S80 34 90 20 S110 8 130 20"
PCOL = ["#3fd0e0", "#3fd0e0", "#6fe0a8", "#6fe0a8", "#f0c25a", "#f0c25a", "#f08a9a", "#f08a9a"]
PBANKS = [("Hauteur · Coupure", ("tune", "Hauteur"), ("fcut", "Coupure")),
          ("Début · Longueur", ("start", "Début"), ("length", "Longueur")),
          ("Lo-fi", ("bits", "Bits"), ("srr", "Réduction"))]


def pieuvre_D(bank):
    t = "octa"
    tracks = []
    for n in range(1, 9):
        q = lambda s: "t%d_%s" % (n, s)
        vol = P(t, q("volume"))
        flex = int(round(P(t, q("machine"))["def"] or 0)) == 0
        start, length = P(t, q("start"))["def"] or 0, P(t, q("length"))["def"] or 100
        b, s = PBANKS[bank][1], PBANKS[bank][2]
        tracks.append({"n": n, "col": PCOL[n - 1], "led": PCOL[n - 1] if flex else "#2c3a40",
                       "border": (PCOL[n - 1] + "66") if flex else "#24313a",
                       "machid": q("machine"), "mach": fmt(P(t, q("machine"))),
                       "slotid": q("slot"), "slot": fmt(P(t, q("slot"))),
                       "wave": [W2, W1, W3, W1, W2, W3, W1, W2][n - 1], "start": start, "len": max(0, min(100 - start, length)),
                       "loopid": q("loop"), "loop": fmt(P(t, q("loop"))), "strid": q("stretch"), "str": fmt(P(t, q("stretch"))),
                       "l1": b[1], "big": kv(t, q(b[0])), "l2": s[1], "small": kv(t, q(s[0])),
                       "volid": q("volume"), "db": fmt(vol), "lvl": round(100 * ((vol["def"] or 0) - vol["min"]) / (vol["max"] - vol["min"]))})
    banks = []
    for i, (nm, _, _) in enumerate(PBANKS):
        on = i == bank
        banks.append({"n": nm, "bg": "#3fd0e0" if on else "transparent", "fg": "#10161a" if on else "#c6d3d7",
                      "bd": "0" if on else "1px solid #2c3a40", "fw": 700 if on else 400})
    return {"plugin": "Sampler - Pieuvre 8", "banks": banks, "tracks": tracks, "firm": "#182025", "text": "#e6eef0", "muted": "#86959b",
            "tabs": tabs(["Pistes", "Samples", "T1", "T2", "T3", "T4  ›"], "#3fd0e0", "#86959b")}


# ============================================================================== Orbiteur (B)
BODIES["orbit"] = """<div style="width:1280px;height:800px;background:#160f20;font-family:Archivo,sans-serif;color:#efe8fb;display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;gap:12px;padding:12px">
    <nav aria-label="Modules" style="width:118px;display:flex;flex-direction:column;gap:6px">
      <sc-for list="{{D.nav}}" as="n" hint-placeholder-count="7">
        <button data-k="nav" data-i="{{$index}}" data-on="#c4a7ff|#160f20|#c4a7ff" data-off="#1a1127|#efe8fb|#33254a" style="flex:1;border-radius:12px;border:1px solid {{n.border}};background:{{n.bg}};color:{{n.fg}};font-family:Archivo,sans-serif;font-stretch:80%;font-weight:700;font-size:18px;text-align:left;padding:0 14px">{{n.n}}</button>
      </sc-for>
    </nav>
    <div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0">
      <div style="flex:1;border-radius:16px;background:radial-gradient(80% 80% at 40% 50%,#2a1c40,#1a1127 75%);border:1px solid #33254a;display:flex;gap:18px;padding:14px 18px;overflow:hidden">
        <div data-k="hero" style="width:440px;height:440px;flex:none;position:relative">
          <svg width="440" height="440" viewBox="0 0 440 440" role="img" aria-label="Orbite entre les quatre coins A, B, C et D" style="position:absolute;left:0;top:0">
            <rect x="40" y="40" width="360" height="360" rx="18" fill="none" stroke="#3d2d58" stroke-width="1.5"></rect>
            <line x1="220" y1="40" x2="220" y2="400" stroke="#2f2244"></line><line x1="40" y1="220" x2="400" y2="220" stroke="#2f2244"></line>
            <g font-family="Archivo" font-weight="800" font-size="22" fill="#efe8fb"><text x="14" y="30">A</text><text x="410" y="30">B</text><text x="14" y="430">C</text><text x="410" y="430">D</text></g>
          </svg>
          <div data-k="xy" data-px="cx" data-py="cy" style="position:absolute;left:40px;top:40px;width:360px;height:360px"></div>
          <svg data-k="puck" data-cx="{{D.pcx}}" data-cy="{{D.pcy}}" width="440" height="440" viewBox="0 0 440 440" aria-hidden="true" style="position:absolute;left:0;top:0;overflow:visible">
            <ellipse cx="{{D.pcx}}" cy="{{D.pcy}}" rx="{{D.prx}}" ry="{{D.pry}}" transform="rotate({{D.prot}} {{D.pcx}} {{D.pcy}})" fill="#c4a7ff" fill-opacity=".08" stroke="#c4a7ff" stroke-width="2.5" stroke-dasharray="7 6"></ellipse>
            <circle cx="{{D.pcx}}" cy="{{D.pcy}}" r="5" fill="#c4a7ff"></circle>
            <circle cx="{{D.sx}}" cy="{{D.sy}}" r="13" fill="#160f20" stroke="#ffcf6b" stroke-width="4"></circle>
            <circle cx="{{D.sx}}" cy="{{D.sy}}" r="26" fill="none" stroke="#ffcf6b" stroke-opacity=".3" stroke-width="2"></circle>
          </svg>
        </div>
        <div style="flex:1;display:flex;flex-direction:column;gap:10px;min-width:0">
          <div style="display:flex;align-items:baseline;gap:12px">
            <div style="font-weight:800;font-stretch:75%;font-size:34px">Orbite</div>
            <div style="font-size:14px;color:#b6a8cc">le son est le mélange des 4 coins, selon où passe le point</div>
          </div>
          <div style="display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px">
            <sc-for list="{{D.corners}}" as="c" hint-placeholder-count="4">
              <div data-k="chip" data-p="{{c.bl}}" style="border-radius:10px;background:#160f20cc;border:1px solid {{c.border}};padding:8px 10px;display:flex;align-items:center;gap:10px">
                <div style="font-weight:800;font-size:22px;color:{{c.col}};width:20px">{{c.k}}</div>
                <svg width="64" height="30" viewBox="0 0 64 30" aria-hidden="true"><path d="{{c.w}}" fill="none" stroke="{{c.col}}" stroke-width="2"></path></svg>
                <div style="flex:1;min-width:0;font-size:13px;line-height:1.3;color:#d8cdea">
                  <div style="display:flex;gap:4px;white-space:nowrap"><span>Gén.</span><span data-v="{{c.g1}}" data-j="left" style="width:22px">{{c.v1}}</span><span>et</span><span data-v="{{c.g2}}" data-j="left" style="width:22px">{{c.v2}}</span><span data-v="{{c.ra}}" data-j="left" style="flex:1">{{c.vr}}</span></div>
                  <div style="display:flex;gap:4px;white-space:nowrap;color:#9a8db0"><span>mélange</span><span data-v="{{c.bl}}" data-j="left" style="width:40px">{{c.vb}}</span><span>·</span><span data-v="{{c.md}}" data-j="left" style="flex:1">{{c.vm}}</span></div>
                </div>
              </div>
            </sc-for>
          </div>
          <div style="display:flex;gap:8px;flex-wrap:wrap">
            <sc-for list="{{D.traj}}" as="o" hint-placeholder-count="4">
              <button data-k="radio" data-p="shape" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ffcf6b|#160f20|#ffcf6b" data-off="transparent|#efe8fb|#4a3866" style="height:40px;padding:0 14px;border-radius:20px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="display:flex;gap:8px">
            <div data-k="chip" data-p="speed" style="flex:1;border-radius:10px;background:#160f20cc;border:1px solid #33254a;padding:8px 12px;font-size:14px;display:flex;justify-content:space-between"><span style="color:#b6a8cc">Vitesse</span><b data-v="speed" data-j="right" data-fw="Bold">{{D.speed}}</b></div>
            <div data-k="chip" data-p="retrig" style="flex:1;border-radius:10px;background:#160f20cc;border:1px solid #33254a;padding:8px 12px;font-size:14px;display:flex;justify-content:space-between"><span style="color:#b6a8cc">Repart à chaque note</span><b data-v="retrig" data-j="right" data-fw="Bold">{{D.retrig}}</b></div>
          </div>
        </div>
      </div>
      <div data-k="macrorow" style="height:182px;border-radius:16px;background:#1a1127;border:1px solid #33254a;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));align-items:center;padding:6px 10px">
        <sc-for list="{{D.macros}}" as="m" hint-placeholder-count="8">
          <div style="display:flex;flex-direction:column;align-items:center;gap:6px">
            <div data-mn="{{$index}}" style="font-size:15px;font-weight:600;white-space:nowrap;overflow:hidden;max-width:130px;text-overflow:ellipsis">{{m.l}}</div>
            <div data-k="knob" data-p="{{m.id}}" data-col="{{m.col}}" data-track="#2e2240" data-slot="{{$index}}" style="width:100px;height:100px;border-radius:50%;background:conic-gradient(from 225deg,{{m.col}} 0 {{m.a}}deg,#2e2240 {{m.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
              <div data-v="{{m.id}}" data-fw="Bold" style="width:84px;height:84px;border-radius:50%;background:#160f20;display:flex;align-items:center;justify-content:center;font-weight:800;font-stretch:80%;font-size:21px;font-variant-numeric:tabular-nums">{{m.v}}</div>
            </div>
            <div style="font-size:12px;color:#9a8db0">{{m.foot}}</div>
          </div>
        </sc-for>
      </div>
    </div>
  </div>
""" + TABS + """</div>
"""

CWAVES = ["M0 15 C8 0 16 0 24 15 S40 30 48 15 S60 4 64 10", "M0 28 L16 2 L16 28 L32 2 L32 28 L48 2 L48 28 L64 2",
          "M0 26 L0 4 L16 4 L16 26 L32 26 L32 4 L48 4 L48 26 L64 26", "M0 15 L8 4 L16 26 L24 6 L32 22 L40 8 L48 24 L56 10 L64 18"]


def orbit_D():
    t = "orbit"
    nav = ["Orbite", "Coins", "Filtres", "Ampli", "Effets", "Matrice", "Arpège"]
    corners = []
    for i, (k, col) in enumerate(zip("ABCD", ["#ff9ec4", "#ffcf6b", "#8fe0d0", "#9fb4ff"])):
        c = k.lower()
        corners.append({"k": k, "col": col, "border": "#33254a", "w": CWAVES[i],
                        "g1": c + "g1", "v1": fmt(P(t, c + "g1")), "g2": c + "g2", "v2": fmt(P(t, c + "g2")),
                        "ra": c + "ratio", "vr": fmt(P(t, c + "ratio")), "bl": c + "blend", "vb": fmt(P(t, c + "blend")),
                        "md": c + "bmode", "vm": fmt(P(t, c + "bmode"))})
    sh = P(t, "shape")
    traj = [{"n": o, "i": i, "cnt": len(sh["choices"]), "bg": "#ffcf6b" if i == sh["def"] else "transparent",
             "fg": "#160f20" if i == sh["def"] else "#efe8fb", "border": "#ffcf6b" if i == sh["def"] else "#4a3866"}
            for i, o in enumerate(sh["choices"][:4])]
    mcol = ["#c4a7ff"] * 4 + ["#ffcf6b", "#8fe0d0", "#ff9ec4", "#efe8fb"]
    mids = [("cx", "Centre X"), ("cy", "Centre Y"), ("size", "Taille"), ("rotate", "Rotation"), ("warpx", "Déform. X"),
            ("mixcut", "Coupure"), ("chorus", "Chorus"), ("level", "Volume")]
    macros = [dict(kv(t, pid, short), k=i + 1, col=mcol[i], foot="bouton %d" % (i + 1)) for i, (pid, short) in enumerate(mids)]
    # orbite par défaut : centre, taille et rotation du catalogue (le centre suit Centre X / Y en direct)
    cx = 40 + 360 * (P(t, "cx")["def"] / 100.0)
    cy = 40 + 360 * (1 - P(t, "cy")["def"] / 100.0)
    size = P(t, "size")["def"] / 100.0
    rot = -(P(t, "rotate")["def"] or 0)
    rx, ry = 180 * size * 1.1, 180 * size * 0.65
    a = math.radians(-30)
    r = math.radians(rot)
    ex, ey = rx * math.cos(a), ry * math.sin(a)
    sx, sy = cx + ex * math.cos(r) - ey * math.sin(r), cy + ex * math.sin(r) + ey * math.cos(r)
    return {"plugin": "Nappe - Orbiteur", "firm": "#110b19", "text": "#efe8fb", "muted": "#9a8db0",
            "accent": "#c4a7ff", "bg": "#160f20", "hero": "#1a1127", "line": "#33254a",
            "nav": [{"n": n, "bg": "#c4a7ff" if i == 0 else "#1a1127", "fg": "#160f20" if i == 0 else "#efe8fb",
                     "border": "#c4a7ff" if i == 0 else "#33254a"} for i, n in enumerate(nav)],
            "corners": corners, "traj": traj, "speed": fmt(P(t, "speed")), "retrig": fmt(P(t, "retrig")), "macros": macros,
            "pcx": round(cx, 1), "pcy": round(cy, 1), "prx": round(rx, 1), "pry": round(ry, 1), "prot": round(rot, 1),
            "sx": round(sx, 1), "sy": round(sy, 1),
            "tabs": tabs(["Jouer", "Éditer", "Matrice", "Effets", "Arpège", "Global"], "#ffcf6b", "#9a8db0")}


# ============================================================================== Glutamake (B)
BODIES["gluta"] = """<div style="width:1280px;height:800px;background:#121213;font-family:Archivo,sans-serif;color:#f1ece6;display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;gap:12px;padding:12px">
    <nav aria-label="Modules" style="width:118px;display:flex;flex-direction:column;gap:6px">
      <sc-for list="{{D.nav}}" as="n" hint-placeholder-count="7">
        <button data-k="nav" data-i="{{$index}}" data-on="#ff8a4c|#121213|#ff8a4c" data-off="#1a1a1c|#f1ece6|#2c2b2e" style="flex:1;border-radius:12px;border:1px solid {{n.border}};background:{{n.bg}};color:{{n.fg}};font-family:Archivo,sans-serif;font-stretch:80%;font-weight:700;font-size:18px;text-align:left;padding:0 14px">{{n.n}}</button>
      </sc-for>
    </nav>
    <div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0">
      <div style="flex:1;border-radius:16px;background:#1a1a1c;border:1px solid #2c2b2e;display:flex;gap:18px;padding:14px 18px;overflow:hidden">
        <div data-k="hero" style="width:440px;height:440px;flex:none;position:relative">
          <svg width="440" height="440" viewBox="0 0 440 440" role="img" aria-label="Pavé de la physique : bille, bosse, inclinaison et bords" style="position:absolute;left:0;top:0">
            <defs>
              <radialGradient id="bump" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#ff8a4c" stop-opacity=".55"></stop><stop offset="1" stop-color="#ff8a4c" stop-opacity="0"></stop></radialGradient>
            </defs>
            <rect x="30" y="30" width="380" height="380" rx="14" fill="#141415"></rect>
            <rect x="30" y="24" width="380" height="7" rx="3" fill="{{D.et}}"></rect>
            <rect x="30" y="409" width="380" height="7" rx="3" fill="{{D.eb}}"></rect>
            <rect x="24" y="30" width="7" height="380" rx="3" fill="{{D.el}}"></rect>
            <rect x="409" y="30" width="7" height="380" rx="3" fill="{{D.er}}"></rect>
            <circle cx="{{D.bx}}" cy="{{D.by}}" r="{{D.br}}" fill="url(#bump)"></circle>
            <g fill="none" stroke="#ff8a4c" stroke-opacity=".5" stroke-width="1.5"><sc-for list="{{D.rings}}" as="r" hint-placeholder-count="4"><circle cx="{{D.bx}}" cy="{{D.by}}" r="{{r}}"></circle></sc-for></g>
            <path d="{{D.path}}" fill="none" stroke="#f1ece6" stroke-opacity=".35" stroke-width="3" stroke-dasharray="2 7" stroke-linecap="round"></path>
            <circle cx="{{D.stx}}" cy="{{D.sty}}" r="7" fill="none" stroke="#9fd2ff" stroke-width="3"></circle>
            <line x1="{{D.stx}}" y1="{{D.sty}}" x2="{{D.ax}}" y2="{{D.ay}}" stroke="#9fd2ff" stroke-width="3" stroke-linecap="round"></line>
            <path d="{{D.ahead}}" fill="#9fd2ff"></path>
            <g transform="translate(70 70)"><circle r="26" fill="#121213" stroke="#3a3836"></circle><line x1="0" y1="0" x2="{{D.tx}}" y2="{{D.ty}}" stroke="#ff8a4c" stroke-width="3" stroke-linecap="round"></line><circle r="3" fill="#ff8a4c"></circle></g>
          </svg>
          <div data-k="xy" data-px="mpc_ballx" data-py="mpc_bally" style="position:absolute;left:30px;top:30px;width:380px;height:380px"></div>
          <svg data-k="puck" data-cx="{{D.ballx}}" data-cy="{{D.bally}}" width="440" height="440" viewBox="0 0 440 440" aria-hidden="true" style="position:absolute;left:0;top:0;overflow:visible">
            <defs><radialGradient id="ball" cx="0.35" cy="0.3" r="0.7"><stop offset="0" stop-color="#fff4e8"></stop><stop offset="1" stop-color="#d2c3b0"></stop></radialGradient></defs>
            <circle cx="{{D.ballx}}" cy="{{D.bally}}" r="15" fill="url(#ball)"></circle>
          </svg>
        </div>
        <div style="flex:1;display:flex;flex-direction:column;gap:10px;min-width:0">
          <div style="display:flex;align-items:center;gap:12px">
            <div style="font-weight:800;font-stretch:75%;font-size:34px">Physique</div>
            <button data-k="radio" data-p="phon" data-i="1" data-n="1" data-on="#ff8a4c|#121213|#ff8a4c" data-off="transparent|#9a928a|#3a3836" style="height:36px;padding:0 16px;border-radius:18px;border:1px solid {{D.phonB}};background:{{D.phonBg}};color:{{D.phonFg}};font-weight:700;font-size:14px">En marche</button>
          </div>
          <div style="font-size:14px;line-height:1.45;color:#c9bfb4">La bille roule sur la surface penchée, la bosse l'attire (creux) ou la repousse (colline). Bords clairs : rebond qui accélère, bords sombres : rebond qui absorbe.</div>
          <div style="font-size:13px;color:#9a928a">Forme de la bosse</div>
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            <sc-for list="{{D.shapes}}" as="o" hint-placeholder-count="4">
              <button data-k="radio" data-p="bumpshape" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ff8a4c|#121213|#ff8a4c" data-off="transparent|#f1ece6|#3a3836" style="height:40px;padding:0 14px;border-radius:20px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="font-size:13px;color:#9a928a">Lancement</div>
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            <sc-for list="{{D.launch}}" as="o" hint-placeholder-count="4">
              <button data-k="radio" data-p="lsrc" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ff8a4c|#121213|#ff8a4c" data-off="transparent|#f1ece6|#3a3836" style="height:40px;padding:0 14px;border-radius:20px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;margin-top:2px">
            <sc-for list="{{D.readouts}}" as="r" hint-placeholder-count="6">
              <div data-k="chip" data-p="{{r.id}}" style="border-radius:10px;background:#121213;border:1px solid #2c2b2e;padding:8px 12px;font-size:14px;display:flex;justify-content:space-between"><span style="color:#c9bfb4;white-space:nowrap">{{r.k}}</span><b data-v="{{r.id}}" data-j="right" data-fw="Bold" style="font-variant-numeric:tabular-nums;white-space:nowrap">{{r.v}}</b></div>
            </sc-for>
          </div>
        </div>
      </div>
      <div data-k="macrorow" style="height:182px;border-radius:16px;background:#1a1a1c;border:1px solid #2c2b2e;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));align-items:center;padding:6px 10px">
        <sc-for list="{{D.macros}}" as="m" hint-placeholder-count="8">
          <div style="display:flex;flex-direction:column;align-items:center;gap:6px">
            <div data-mn="{{$index}}" style="font-size:15px;font-weight:600;white-space:nowrap;overflow:hidden;max-width:130px;text-overflow:ellipsis">{{m.l}}</div>
            <div data-k="knob" data-p="{{m.id}}" data-col="#ff8a4c" data-track="#2e2c2a" data-slot="{{$index}}" style="width:100px;height:100px;border-radius:50%;background:conic-gradient(from 225deg,#ff8a4c 0 {{m.a}}deg,#2e2c2a {{m.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
              <div data-v="{{m.id}}" data-fw="Bold" style="width:84px;height:84px;border-radius:50%;background:radial-gradient(circle at 38% 30%,#2a2a2c,#0f0f10 72%);display:flex;align-items:center;justify-content:center;font-weight:800;font-stretch:80%;font-size:21px;font-variant-numeric:tabular-nums">{{m.v}}</div>
            </div>
            <div style="font-size:12px;color:#9a928a">{{m.foot}}</div>
          </div>
        </sc-for>
      </div>
    </div>
  </div>
""" + TABS + """</div>
"""


def _edge(v):
    # bord clair : rebond qui accélère (> 0) ; sombre : qui absorbe (< 0) ; neutre entre les deux
    c0, cm, c1 = (0x5a, 0x3a, 0x2a), (0xa5, 0x64, 0x3d), (0xff, 0xd2, 0xa8)
    u = max(-1.0, min(1.0, v / 100.0))
    a, b, k = (cm, c1, u) if u >= 0 else (cm, c0, -u)
    return "#%02x%02x%02x" % tuple(round(a[i] + (b[i] - a[i]) * k) for i in range(3))


def gluta_path(t):
    """Circuit calculé de la bille (réglages par défaut) : pente, bosse en cloche, frottement, lancement."""
    d = lambda pid: P(t, pid)["def"] or 0
    tx, ty = d("tiltx") / 100.0, d("tilty") / 100.0
    h, bx, by, bw = d("bumph") / 100.0, d("bumpx") / 100.0, d("bumpy") / 100.0, max(0.05, d("bumpw") / 100.0)
    fr = d("fric") / 100.0
    x, y = d("startx") / 100.0, d("starty") / 100.0
    a = math.radians(d("langle"))
    f = d("lforce") / 100.0 * 2.2
    vx, vy = f * math.cos(a), f * math.sin(a)
    dt = 1 / 120.0
    pts = []
    for i in range(240):
        rx, ry = x - bx, y - by
        r2 = (rx * rx + ry * ry) / (bw * bw)
        g = math.exp(-r2)
        # force = - gradient du relief (h < 0 : creux qui attire)
        fx = 2.0 * h * g * rx / (bw * bw) * 1.5 + tx * 1.5
        fy = 2.0 * h * g * ry / (bw * bw) * 1.5 + ty * 1.5
        vx += fx * dt * 4
        vy += fy * dt * 4
        vx *= 1 - fr * 0.02
        vy *= 1 - fr * 0.02
        x += vx * dt
        y += vy * dt
        if abs(x) > 1:
            x, vx = math.copysign(1, x), -vx
        if abs(y) > 1:
            y, vy = math.copysign(1, y), -vy
        if i % 3 == 0:
            pts.append((x, y))
    S = lambda u, v: (220 + u * 175, 220 - v * 175)
    pp = [S(u, v) for u, v in pts]
    return "M" + " L".join("%.1f %.1f" % p for p in pp), pp[-1]


def gluta_D():
    t = "gluta"
    d = lambda pid: P(t, pid)["def"] or 0
    nav = ["Physique", "Osc 1", "Osc 2", "Filtre", "Env", "LFO", "Séq."]
    pick = lambda pid, n=4: [{"n": o, "i": i, "cnt": len(P(t, pid)["choices"]), "bg": "#ff8a4c" if i == d(pid) else "transparent",
                              "fg": "#121213" if i == d(pid) else "#f1ece6", "border": "#ff8a4c" if i == d(pid) else "#3a3836"}
                             for i, o in enumerate(P(t, pid)["choices"][:n])]
    ro = [("bumpx", "Bosse X"), ("bumpy", "Bosse Y"), ("startx", "Départ X"), ("starty", "Départ Y"), ("lrand", "Hasard"), ("wrap", "Traverser les bords")]
    mids = [("tiltx", "Inclin. X"), ("tilty", "Inclin. Y"), ("fric", "Frottement"), ("ptime", "Temps"), ("bumph", "Bosse haut."),
            ("bumpw", "Bosse larg."), ("lforce", "Force"), ("langle", "Angle")]
    # pavé : dessin de la maquette validée (bosse, trajet d'exemple, départ et lancement) ; la bille,
    # elle, suit le moteur en direct (réglages Bille X / Y, pavé XY de l'écran)
    illu = {"et": "#ffd2a8", "eb": "#5a3a2a", "el": "#a5643d", "er": "#ffd2a8", "bx": 250, "by": 200, "br": 120,
            "rings": [30, 60, 90, 118], "path": "M120 330 C150 250 210 290 238 230 C262 178 300 160 330 190 C352 212 330 250 300 252",
            "ballx": 300, "bally": 252, "stx": 120, "sty": 330, "ax": 160, "ay": 282, "ahead": "M160 282 L148 284 L156 274 Z",
            "tx": 14, "ty": 12}
    S = lambda u, v: (round(220 + u * 175, 1), round(220 - v * 175, 1))
    bxp, byp = S(d("bumpx") / 100.0, d("bumpy") / 100.0)
    br = round(max(20, d("bumpw") / 100.0 * 300), 1)
    stx, sty = S(d("startx") / 100.0, d("starty") / 100.0)
    a = math.radians(d("langle"))
    L = 30 + 40 * d("lforce") / 100.0
    ax, ay = stx + L * math.cos(a), sty - L * math.sin(a)
    ux, uy = math.cos(a), -math.sin(a)
    head = "M%.1f %.1f L%.1f %.1f L%.1f %.1f Z" % (ax + ux * 10, ay + uy * 10, ax - uy * 6, ay + ux * 6, ax + uy * 6, ay - ux * 6)
    path, end = gluta_path(t)
    on = int(d("phon")) == 1
    return {"plugin": "Glutamake", "firm": "#0c0c0d", "text": "#f1ece6", "muted": "#9a928a",
            "accent": "#ff8a4c", "bg": "#121213", "hero": "#1a1a1c", "line": "#2c2b2e",
            "nav": [{"n": n, "bg": "#ff8a4c" if i == 0 else "#1a1a1c", "fg": "#121213" if i == 0 else "#f1ece6",
                     "border": "#ff8a4c" if i == 0 else "#2c2b2e"} for i, n in enumerate(nav)],
            "et": _edge(d("edget")), "eb": _edge(d("edgeb")), "el": _edge(d("edgel")), "er": _edge(d("edger")),
            "bx": bxp, "by": byp, "br": br, "rings": [round(br * k, 1) for k in (0.25, 0.5, 0.75, 0.98)],
            "path": path, "ballx": round(end[0], 1), "bally": round(end[1], 1),
            "stx": stx, "sty": sty, "ax": round(ax, 1), "ay": round(ay, 1), "ahead": head,
            "tx": round(20 * d("tiltx") / 100.0, 1), "ty": round(-20 * d("tilty") / 100.0, 1),
            "phonB": "#ff8a4c" if on else "#3a3836", "phonBg": "#ff8a4c" if on else "transparent", "phonFg": "#121213" if on else "#9a928a",
            "shapes": pick("bumpshape"), "launch": pick("lsrc"),
            "readouts": [{"k": k, "v": fmt(P(t, pid)), "id": pid} for pid, k in ro],
            "macros": [dict(kv(t, pid, short), k=i + 1, foot="bouton %d" % (i + 1)) for i, (pid, short) in enumerate(mids)],
            "tabs": tabs(["Physique", "Bords", "Osc", "Filtre", "Séquences", "Matrice"], "#ff8a4c", "#9a928a"), **illu}


# ============================================================================== Gamer Lover (B)
# Nuit de cartouche : indigo profond, jaune « pièce », écran cathodique qui change avec la puce choisie.
BODIES["gamer"] = """<div style="width:1280px;height:800px;background:#13112a;font-family:Archivo,sans-serif;color:#f3efff;display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;gap:12px;padding:12px">
    <nav aria-label="Modules" style="width:118px;display:flex;flex-direction:column;gap:6px">
      <sc-for list="{{D.nav}}" as="n" hint-placeholder-count="8">
        <button data-k="nav" data-i="{{$index}}" data-on="#ffd23f|#13112a|#ffd23f" data-off="#1c1940|#f3efff|#2f2b5c" style="flex:1;border-radius:12px;border:1px solid {{n.border}};background:{{n.bg}};color:{{n.fg}};font-family:Archivo,sans-serif;font-stretch:80%;font-weight:700;font-size:18px;text-align:left;padding:0 14px;white-space:nowrap">{{n.n}}</button>
      </sc-for>
    </nav>
    <div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0">
      <div style="flex:1;border-radius:16px;background:#1c1940;border:1px solid #2f2b5c;display:flex;gap:18px;padding:14px 18px;overflow:hidden">
        <div data-k="hero" style="width:440px;height:440px;flex:none;position:relative">
          <svg width="440" height="440" viewBox="0 0 440 440" role="img" aria-label="Écran de télévision : la puce choisie" style="position:absolute;left:0;top:0">
            <defs>
              <pattern id="scan" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="4" height="2" fill="#000" fill-opacity=".28"></rect></pattern>
              <radialGradient id="vign" cx="0.5" cy="0.45" r="0.72"><stop offset="0.55" stop-color="#000" stop-opacity="0"></stop><stop offset="1" stop-color="#000" stop-opacity=".7"></stop></radialGradient>
              <radialGradient id="glare" cx="0.3" cy="0.18" r="0.5"><stop offset="0" stop-color="#fff" stop-opacity=".13"></stop><stop offset="1" stop-color="#fff" stop-opacity="0"></stop></radialGradient>
              <clipPath id="tube"><rect x="40" y="34" width="360" height="292" rx="30"></rect></clipPath>
            </defs>
            <rect x="6" y="6" width="428" height="428" rx="36" fill="#0d0b1f" stroke="#2f2b5c" stroke-width="2"></rect>
            <rect x="28" y="22" width="384" height="316" rx="40" fill="#07061a"></rect>
            <g clip-path="url(#tube)">
              <rect x="40" y="34" width="360" height="292" fill="{{D.scrBg}}"></rect>
              <sc-for list="{{D.scrRects}}" as="r" hint-placeholder-count="40"><rect x="{{r.x}}" y="{{r.y}}" width="{{r.w}}" height="{{r.h}}" rx="{{r.rx}}" fill="{{r.c}}" fill-opacity="{{r.o}}" stroke="{{r.s}}" stroke-width="{{r.sw}}"></rect></sc-for>
              <sc-for list="{{D.scrPaths}}" as="p" hint-placeholder-count="6"><path d="{{p.d}}" fill="none" stroke="{{p.c}}" stroke-width="{{p.w}}" stroke-linejoin="miter" stroke-linecap="square"></path></sc-for>
              <sc-for list="{{D.scrTexts}}" as="t" hint-placeholder-count="20"><text x="{{t.x}}" y="{{t.y}}" font-family="{{t.f}}" font-size="{{t.s}}" fill="{{t.c}}" text-anchor="{{t.a}}">{{t.t}}</text></sc-for>
              <rect x="40" y="34" width="360" height="292" fill="url(#scan)"></rect>
              <rect x="40" y="34" width="360" height="292" fill="url(#vign)"></rect>
              <rect x="40" y="34" width="360" height="292" fill="url(#glare)"></rect>
            </g>
            <text x="40" y="384" font-family="Silkscreen" font-size="17" fill="#a39fcb">GAMER LOVER</text>
            <g fill="#2f2b5c"><rect x="230" y="368" width="70" height="4" rx="2"></rect><rect x="230" y="378" width="70" height="4" rx="2"></rect><rect x="230" y="388" width="70" height="4" rx="2"></rect><rect x="230" y="398" width="70" height="4" rx="2"></rect></g>
            <text x="356" y="384" font-family="Silkscreen" font-size="12" fill="#a39fcb" text-anchor="end">{{D.tvCh}}</text>
            <circle cx="388" cy="379" r="7" fill="#ff4d6d"></circle><circle cx="386" cy="377" r="2.5" fill="#ffd0d8"></circle>
          </svg>
        </div>
        <div style="flex:1;display:flex;flex-direction:column;gap:10px;min-width:0">
          <div style="display:flex;align-items:baseline;gap:12px">
            <div style="font-weight:800;font-stretch:75%;font-size:34px">Puce</div>
            <div style="font-size:14px;color:#a39fcb">le son et l'écran changent avec elle</div>
          </div>
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            <sc-for list="{{D.chips}}" as="o" hint-placeholder-count="5">
              <button data-k="radio" data-p="chip" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ffd23f|#13112a|#ffd23f" data-off="transparent|#f3efff|#3d3870" style="height:42px;padding:0 14px;border-radius:21px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:15px;font-weight:700;white-space:nowrap">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="font-size:14px;line-height:1.45;color:#cfcae8">Les tables tournent à la cadence de l'image : faux accords rapides, volume en marches, vibrato retardé.</div>
          <div style="font-size:13px;color:#a39fcb">Onde (8 bits et portable)</div>
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            <sc-for list="{{D.waves}}" as="o" hint-placeholder-count="3">
              <button data-k="radio" data-p="wave8" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ffd23f|#13112a|#ffd23f" data-off="transparent|#f3efff|#3d3870" style="height:38px;padding:0 14px;border-radius:19px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600;white-space:nowrap">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="font-size:13px;color:#a39fcb">Largeur du carré</div>
          <div style="display:flex;gap:6px;flex-wrap:wrap">
            <sc-for list="{{D.duties}}" as="o" hint-placeholder-count="4">
              <button data-k="radio" data-p="duty" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="#ffd23f|#13112a|#ffd23f" data-off="transparent|#f3efff|#3d3870" style="height:38px;padding:0 14px;border-radius:19px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600;white-space:nowrap">{{o.n}}</button>
            </sc-for>
          </div>
          <div style="display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin-top:2px">
            <sc-for list="{{D.readouts}}" as="r" hint-placeholder-count="6">
              <div data-k="chip" data-p="{{r.id}}" style="border-radius:10px;background:#13112a;border:1px solid #2f2b5c;padding:8px 10px;font-size:14px;display:flex;justify-content:space-between;gap:6px"><span style="color:#cfcae8;white-space:nowrap">{{r.k}}</span><b data-v="{{r.id}}" data-j="right" data-fw="Bold" style="font-variant-numeric:tabular-nums;white-space:nowrap">{{r.v}}</b></div>
            </sc-for>
          </div>
        </div>
      </div>
      <div data-k="macrorow" style="height:182px;border-radius:16px;background:#1c1940;border:1px solid #2f2b5c;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));align-items:center;padding:6px 10px">
        <sc-for list="{{D.macros}}" as="m" hint-placeholder-count="8">
          <div style="display:flex;flex-direction:column;align-items:center;gap:6px">
            <div data-mn="{{$index}}" style="font-size:15px;font-weight:600;white-space:nowrap;overflow:hidden;max-width:130px;text-overflow:ellipsis">{{m.l}}</div>
            <div data-k="knob" data-p="{{m.id}}" data-col="#ffd23f" data-track="#2f2b5c" data-slot="{{$index}}" style="width:100px;height:100px;border-radius:50%;background:conic-gradient(from 225deg,#ffd23f 0 {{m.a}}deg,#2f2b5c {{m.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
              <div data-v="{{m.id}}" data-fw="Bold" style="width:84px;height:84px;border-radius:50%;background:radial-gradient(circle at 38% 30%,#2a2650,#0d0b1f 72%);display:flex;align-items:center;justify-content:center;font-weight:800;font-stretch:80%;font-size:21px;font-variant-numeric:tabular-nums">{{m.v}}</div>
            </div>
            <div style="font-size:12px;color:#a39fcb">{{m.foot}}</div>
          </div>
        </sc-for>
      </div>
    </div>
  </div>
""" + TABS + """</div>
"""


def _R(x, y, w, h, c, o=1, rx=0, s="none", sw=0):
    return {"x": round(x, 1), "y": round(y, 1), "w": round(w, 1), "h": round(h, 1), "c": c, "o": o, "rx": rx, "s": s, "sw": sw}


def _T(x, y, t, s, c, f="Silkscreen", a="start"):
    return {"x": round(x, 1), "y": round(y, 1), "t": t, "s": s, "c": c, "f": f, "a": a}


def _steps(pts):
    """Tracé en marches (valeurs tenues), comme une sortie de puce."""
    d = "M%.1f %.1f" % pts[0]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        d += " H%.1f V%.1f" % (x1, y1)
    return d


def gamer_screens():
    """Une image d'écran par puce (bande d'images liée au réglage Puce)."""
    X0, Y0, W, H = 40, 34, 360, 292
    fr = []
    # 8 bits : carré, triangle en escalier, bruit ; phosphore chaud
    ph, dim = "#f6ead2", "#5b5446"
    R, P, T = [], [], [_T(62, 74, "8 BITS", 22, ph), _T(378, 74, "SALON", 11, dim, a="end")]
    for name, y in (("CARRE 25 %", 100), ("TRIANGLE 4 BITS", 160), ("BRUIT", 240)):
        T.append(_T(62, y, name, 11, dim))
    pts = []
    for k in range(5):
        x = 62 + k * 64
        pts += [(x, 110), (x + 16, 134)]
    pts.append((62 + 5 * 64, 110))
    P.append({"d": _steps(pts), "c": ph, "w": 3})
    pts = []
    for k in range(64):
        st = k % 32
        lv = 15 - st if st < 16 else st - 16
        pts.append((62 + k * 5, 210 - lv * 2.4))
    pts.append((62 + 64 * 5, 210 - 15 * 2.4))
    P.append({"d": _steps(pts), "c": ph, "w": 3})
    import random
    rnd = random.Random(7)
    for k in range(40):
        if rnd.random() < 0.5:
            R.append(_R(62 + k * 8, 250, 8, 28, ph, 0.9))
    fr.append({"scrBg": "#0b0a10", "scrRects": R, "scrPaths": P, "scrTexts": T, "tvCh": "CH 1"})
    # portable : écran à cristaux verts, 4 teintes, onde libre de 32 pas sur 4 bits
    c0, c1, c2, c3 = "#0f380f", "#306230", "#8bac0f", "#9bbc0f"
    R = [_R(X0, Y0, W, H, c3)]
    T = [_T(62, 74, "PORTABLE", 22, c0), _T(378, 74, "4 TEINTES", 11, c1, a="end")]
    for k in range(32):
        v = math.sin(2 * math.pi * (k + 0.5) / 32)
        q = max(0, min(15, round((v + 1) * 7.5)))
        h = 8 + q * 9
        R.append(_R(64 + k * 10, 252 - h, 8, h, c0 if q > 7 else c1))
    R.append(_R(62, 262, 316, 3, c2))
    T.append(_T(62, 300, "ONDE 4 BITS · 32 PAS", 12, c1))
    fr.append({"scrBg": c3, "scrRects": R, "scrPaths": [], "scrTexts": T, "tvCh": "CH 2"})
    # 16 bits FM : onde modulée, quatre opérateurs
    cy_, mg = "#5de4ff", "#ff5fd2"
    T = [_T(62, 74, "16 BITS FM", 22, cy_), _T(378, 74, "4 OPS", 11, "#3b4a70", a="end")]
    pts = []
    for k in range(161):
        x = k / 160.0
        env = 2.6 * math.exp(-x * 2.2)
        y = math.sin(2 * math.pi * 3 * x + env * math.sin(2 * math.pi * 6 * x))
        pts.append("%.1f %.1f" % (62 + x * 316, 160 - y * 48))
    P = [{"d": "M" + " L".join(pts), "c": cy_, "w": 3}]
    R = []
    for k in range(4):
        x = 62 + k * 82
        on = k in (1, 3)
        R.append(_R(x, 236, 68, 50, mg if on else "#0a0c1c", 1, 8, mg if on else cy_, 2))
        T.append(_T(x + 34, 268, "OP%d" % (k + 1), 13, "#0a0c1c" if on else cy_, a="middle"))
    fr.append({"scrBg": "#070814", "scrRects": R, "scrPaths": P, "scrTexts": T, "tvCh": "CH 3"})
    # 16 bits échantillons : son qui retombe, découpé en blocs de 16
    vi, vd = "#c9a8ff", "#4a3a73"
    T = [_T(62, 74, "16 BITS ECH.", 22, vi), _T(378, 74, "BLOCS DE 16", 11, vd, a="end")]
    R = []
    nb = 20
    for b in range(nb):
        x = 62 + b * 15.8
        R.append(_R(x, 102, 1.5, 150, vd))
        for j in range(4):
            t = (b * 4 + j) / (nb * 4.0)
            a = math.exp(-t * 3.0) * (0.55 + 0.45 * math.sin(2 * math.pi * t * 9))
            q = round(a * 7) / 7.0
            h = max(3, q * 70)
            R.append(_R(x + 2 + j * 3.4, 177 - h, 3, h * 2, vi, 0.95))
    T.append(_T(62, 290, "4 BITS PAR BLOC · LECTURE DOUCE", 12, vi))
    fr.append({"scrBg": "#120d22", "scrRects": R, "scrPaths": [], "scrTexts": T, "tvCh": "CH 4"})
    # batterie : 16 pads (pad 1 en bas à gauche, comme la MPC)
    names = ["GC 8", "GC FM", "CC 8", "CC PORT", "CLAP", "CH FERME", "CH OUV.", "CYMBALE", "TOM G", "TOM M", "TOM A", "TOM FM",
             "CLOCHE", "PIECE", "SAUT", "BOUM"]
    am, ad = "#ffd23f", "#4d4320"
    T = [_T(62, 74, "BATTERIE", 22, am), _T(378, 74, "16 PADS", 11, ad, a="end")]
    R = []
    for i, n in enumerate(names):
        col, row = i % 4, 3 - i // 4
        x, y = 62 + col * 80, 90 + row * 56
        R.append(_R(x, y, 72, 48, "#14120a" if i % 5 else am, 1, 6, am, 2))
        T.append(_T(x + 36, y + 30, n, 11 if len(n) < 7 else 9, "#14120a" if i % 5 == 0 else am, a="middle"))
    fr.append({"scrBg": "#0b0a08", "scrRects": R, "scrPaths": [], "scrTexts": T, "tvCh": "CH 5"})
    return fr


GAMER_NAV = [
    ("Jeu", [("level", "Niveau"), ("tune", "Accord"), ("glide", "Glisse"), ("voices", "Voix"), ("vibdepth", "Vibrato"),
             ("sweep", "Chute"), ("echomix", "Écho"), ("bits", "Bits")]),
    ("Ondes", ["wave8", "duty", "dutymove", "gbwave", "noisemode", "smpl", "smplsmooth", "tick"]),
    ("FM", ["fmalgo", "fmfb", "fmmul1", "fmmul2", "fmmul3", "fmmul4", "fmdec", "fmgrain"]),
    ("Niveaux FM", ["fmlev1", "fmlev2", "fmlev3", "fmlev4"]),
    ("Tables", ["arp", "arpspeed", "sweep", "sweeptime", "vibdepth", "vibrate", "vibdelay", "dutymove"]),
    ("Volume", ["vatk", "vdec", "vsus", "vrel", "level"]),
    ("Batterie", ["drtune", "drdecay", "drtone", "level"]),
    ("Console", ["bits", "rate", "tone", "width", "echomix", "echotime", "echofb", "echofilter"]),
]


def gamer_D():
    t = "gamer"
    d = lambda pid: P(t, pid)["def"] or 0
    pick = lambda pid, names=None: [{"n": (names or P(t, pid)["choices"])[i], "i": i, "cnt": len(P(t, pid)["choices"]),
                                     "bg": "#ffd23f" if i == d(pid) else "transparent", "fg": "#13112a" if i == d(pid) else "#f3efff",
                                     "border": "#ffd23f" if i == d(pid) else "#3d3870"} for i in range(len(P(t, pid)["choices"]))]
    nav = [n for n, _ in GAMER_NAV]
    first = GAMER_NAV[0][1]
    frames = gamer_screens()
    D = {"plugin": "Retro - Gamer Lover", "firm": "#0d0b1f", "text": "#f3efff", "muted": "#a39fcb",
         "accent": "#ffd23f", "bg": "#13112a", "hero": "#1c1940", "line": "#2f2b5c",
         "nav": [{"n": n, "bg": "#ffd23f" if i == 0 else "#1c1940", "fg": "#13112a" if i == 0 else "#f3efff",
                  "border": "#ffd23f" if i == 0 else "#2f2b5c"} for i, n in enumerate(nav)],
         "chips": pick("chip", ["8 bits", "Portable", "16 bits FM", "16 bits éch.", "Batterie"]),
         "waves": pick("wave8"), "duties": pick("duty"),
         "readouts": [{"k": k, "v": fmt(P(t, pid)), "id": pid} for pid, k in
                      (("tick", "Cadence"), ("voices", "Voix"), ("arp", "Arpège"), ("tone", "Filtre"), ("rate", "Fréquence"), ("bits", "Résolution"))],
         "macros": [dict(kv(t, pid, short), k=i + 1, foot="bouton %d" % (i + 1)) for i, (pid, short) in enumerate(first)],
         "_navmacros": [[p if isinstance(p, str) else p[0] for p in ps] for _, ps in GAMER_NAV],
         "_film": {"p": "chip", "frames": frames},
         "tabs": tabs(["Jeu", "Puce", "Tables", "Batterie", "Console", "Écho"], "#ffd23f", "#a39fcb")}
    D.update(frames[int(d("chip"))])
    return D


def write_all(out):
    D0 = pieuvre_D(0)
    D0["_pages"] = [pieuvre_D(1), pieuvre_D(2)]
    side(out, "A-Pieuvre", "octa", "A:pieuvre", D0)
    board_html(out, "A-Pieuvre", "A · Pieuvre 8", "Space+Grotesk:wght@500;700&family=Inter:wght@400;500;600", BODIES["pieuvre"], D0)
    D = orbit_D()
    side(out, "B-Orbiteur", "orbit", "B:orbit", D)
    board_html(out, "B-Orbiteur", "B · Orbiteur", "Archivo:wdth,wght@75..100,400..800", BODIES["orbit"], D)
    D = gluta_D()
    side(out, "B-Glutamake", "gluta", "B:gluta", D)
    board_html(out, "B-Glutamake", "B · Glutamake", "Archivo:wdth,wght@75..100,400..800", BODIES["gluta"], D)
    D = gamer_D()
    side(out, "B-Gamer-Lover", "gamer", "B:gamer", D)
    board_html(out, "B-Gamer-Lover", "B · Gamer Lover", "Archivo:wdth,wght@75..100,400..800&family=Silkscreen&family=VT323", BODIES["gamer"], D)


if __name__ == "__main__":
    import sys
    write_all(sys.argv[1] if len(sys.argv) > 1 else G.OUT)
