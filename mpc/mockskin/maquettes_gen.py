#!/usr/bin/env python3
"""Génère les maquettes A / B / C de tous les instruments (canvas Design) à partir du catalogue GOLEM.
Valeurs affichées = valeurs par défaut du catalogue. Aucun réglage inventé."""
import json, math, os, sys

CAT = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "ui", "src", "engine", "catalog.json"), encoding="utf-8"))
_X = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "catalog_mpc.json")  # greffons propres à la MPC
if os.path.isfile(_X):
    _x = json.load(open(_X, encoding="utf-8"))
    for _g in ("instruments", "effects"):
        _have = {_m["type"] for _m in CAT[_g]}
        CAT[_g] += [_m for _m in _x.get(_g, []) if _m["type"] not in _have]
INST = {i["type"]: i for i in CAT["instruments"]}
OUT = os.environ.get("MAQUETTES_OUT") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")


def side(key, t, tpl, D):
    """Fiche de la maquette pour l'habillage MPC (mockskin/render.py)."""
    os.makedirs(OUT, exist_ok=True)
    json.dump({"key": key, "type": t, "tpl": tpl, "D": D}, open(os.path.join(OUT, key + ".json"), "w", encoding="utf-8"), ensure_ascii=False)

NOTE = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"]


def P(t, pid):
    for p in INST[t]["params"]:
        if p["id"] == pid:
            return p
    raise KeyError("%s:%s" % (t, pid))


def fmt(p):
    d = p.get("def", 0) or 0
    ch = p.get("choices")
    if ch:
        k = int(round(d))
        return ch[k] if 0 <= k < len(ch) else ch[0]
    u = p.get("unit") or ""
    if u == "Hz":
        return ("%.1f kHz" % (d / 1000)).replace(".", ",") if d >= 1000 else "%d Hz" % round(d)
    if u == "ms":
        return ("%.2f s" % (d / 1000)).replace(".", ",") if d >= 1000 else "%d ms" % round(d)
    if u == "s":
        return ("%.2f s" % d).replace(".", ",")
    if u == "dB":
        return ("%+.1f dB" % d).replace(".", ",")
    if u == "%":
        return "%d %%" % round(d)
    if u in ("dt", "st"):
        return "%+d" % round(d)
    if u == "ct":
        return "%+d ct" % round(d)
    if u == "note":
        n = int(round(d))
        return "%s%d" % (NOTE[n % 12], n // 12 - 1)
    if abs(d - round(d)) < 1e-6:
        return "%d" % round(d)
    return ("%.2f" % d).replace(".", ",")


def ang(p):
    lo, hi, d = p.get("min"), p.get("max"), p.get("def", 0) or 0
    ch = p.get("choices")
    if ch:
        return round(270 * (int(round(d)) / max(1, len(ch) - 1)))
    if lo is None or hi is None or hi == lo:
        return 135
    return round(270 * max(0.0, min(1.0, (d - lo) / (hi - lo))))


def name(t, pid, short=None):
    return short or P(t, pid)["name"]


def kv(t, pid, short=None):
    p = P(t, pid)
    return {"l": short or p["name"], "v": fmt(p), "a": ang(p), "id": pid}


def esc(s):
    return s.replace("</", "<\\/")


HEAD = """<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<title>%s</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
<link href="https://fonts.googleapis.com/css2?family=%s&amp;display=swap" rel="stylesheet">
<style>
body{margin:0}
a{color:#aaa}a:hover{color:#fff}
</style>
</helmet>
"""
FIRM = """  <div style="height:54px;background:{{D.firm}};display:flex;align-items:center;gap:60px;padding:0 20px;font-size:11px;color:{{D.muted}}">
    <div>Track<div style="font-size:19px;color:{{D.text}}">Plugin 001</div></div>
    <div>Plugin<div style="font-size:19px;color:{{D.text}}">{{D.plugin}}</div></div>
    <div>Preset<div style="font-size:19px;color:{{D.text}}">Init</div></div>
  </div>
"""
TABS = """  <div style="height:52px;background:{{D.firm}};display:grid;grid-template-columns:repeat(6,minmax(0,1fr));align-items:center;text-align:center;font-size:15px;color:{{D.muted}};font-weight:600">
    <sc-for list="{{D.tabs}}" as="t" hint-placeholder-count="6">
      <div style="height:100%;display:flex;align-items:center;justify-content:center;color:{{t.fg}};box-shadow:inset 0 3px 0 {{t.line}}">{{t.n}}</div>
    </sc-for>
  </div>
"""
TAIL = """</x-dc>
<script type="text/x-dc" data-dc-script data-props='{"$preview":{"width":1280,"height":800}}'>
class Component extends DCLogic {
  renderVals() {
    const D = %s;
    return { D };
  }
}
</script>
</body>
</html>
"""


def tabs(names, accent, muted):
    return [{"n": n, "fg": "#ffffff" if i == 0 else muted, "line": accent if i == 0 else "transparent"} for i, n in enumerate(names[:6])]


# ------------------------------------------------------------------------------------------- A
A_BODY = """<div style="width:1280px;height:800px;background:{{D.bg}};font-family:Inter,sans-serif;color:{{D.text}};display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;flex-direction:column;padding:12px 14px 8px;gap:10px">
    <div style="display:flex;align-items:center;gap:14px">
      <div style="font-family:'Space Grotesk',sans-serif;font-weight:700;font-size:26px;color:{{D.accent}}">{{D.title}}</div>
      <div style="font-size:13px;color:{{D.muted}}">{{D.sub}}</div>
      <div style="margin-left:auto;display:flex;gap:6px">
        <sc-for list="{{D.banks}}" as="b" hint-placeholder-count="2">
          <button style="height:36px;padding:0 16px;border-radius:18px;border:1px solid {{b.border}};background:{{b.bg}};color:{{b.fg}};font-weight:600;font-size:14px">{{b.n}}</button>
        </sc-for>
      </div>
    </div>
    <div style="flex:1;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:7px">
      <sc-for list="{{D.strips}}" as="s" hint-placeholder-count="8">
        <div style="background:{{D.panel}};border:1px solid {{D.line}};border-radius:12px;display:flex;flex-direction:column;align-items:center;padding:10px 8px;gap:6px">
          <sc-if value="{{D.bar}}"><div style="width:100%;height:4px;border-radius:2px;background:{{s.col}}"></div></sc-if>
          <div style="width:100%;display:flex;align-items:center;justify-content:space-between">
            <div style="font-family:'Space Grotesk',sans-serif;font-weight:700;font-size:{{D.abfs}}px;color:{{s.col}}">{{s.ab}}</div>
            <div style="font-size:12px;color:{{D.muted}}">{{s.n}}</div>
          </div>
          <div style="width:100%;font-size:13px;color:{{D.namecol}};white-space:nowrap;overflow:hidden;text-overflow:ellipsis">{{s.name}}</div>
          <div data-k="chip" data-p="{{s.chipid}}" style="width:100%;height:32px;border-radius:8px;border:1px solid {{D.line}};background:{{D.bg}};color:{{D.text}};font-size:12px;display:flex;align-items:center;padding:0 8px;white-space:nowrap;overflow:hidden"><sc-if value="{{s.chipname}}"><span style="white-space:pre">{{s.chipname}} · </span></sc-if><span data-v="{{s.chipid}}" data-j="left" style="flex:1;overflow:hidden">{{s.chipval}}</span></div>
          <div style="font-size:12px;color:{{D.muted}};margin-top:2px">{{s.big.l}}</div>
          <div data-k="knob" data-p="{{s.big.id}}" data-col="{{s.col}}" data-track="{{D.track}}" style="width:96px;height:96px;border-radius:50%;background:conic-gradient(from 225deg,{{s.col}} 0 {{s.big.a}}deg,{{D.track}} {{s.big.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
            <div data-v="{{s.big.id}}" data-fw="Bold" style="width:80px;height:80px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#3a3d46,#18191d 70%);display:flex;align-items:center;justify-content:center;font-family:'Space Grotesk',sans-serif;font-size:{{D.bigfs}}px;font-weight:700;color:#eef0f4">{{s.big.v}}</div>
          </div>
          <div style="font-size:12px;color:{{D.muted}}">{{s.small.l}}</div>
          <div data-k="knob" data-p="{{s.small.id}}" data-col="{{s.col}}" data-track="{{D.track}}" style="width:62px;height:62px;border-radius:50%;background:conic-gradient(from 225deg,{{s.col}} 0 {{s.small.a}}deg,{{D.track}} {{s.small.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
            <div data-v="{{s.small.id}}" data-fw="SemiBold" style="width:50px;height:50px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#33363e,#18191d 70%);display:flex;align-items:center;justify-content:center;font-size:{{D.smallfs}}px;font-weight:600;color:#eef0f4">{{s.small.v}}</div>
          </div>
          <div style="flex:1;width:100%;display:flex;gap:8px;align-items:stretch;justify-content:center;margin-top:4px">
            <div data-k="vbar" data-p="{{s.lid}}" style="width:14px;border-radius:7px;background:{{D.track}};position:relative;overflow:hidden"><div style="position:absolute;left:0;right:0;bottom:0;height:{{s.lvl}}%;background:{{s.col}}"></div></div>
            <div style="display:flex;flex-direction:column;justify-content:space-between;font-size:11px;color:{{D.muted}}"><div>{{s.ll}}</div><div data-v="{{s.lid}}" data-j="left" style="font-size:13px;color:{{D.text}}">{{s.lv}}</div><div style="display:flex;gap:4px"><span>{{s.xl}}</span><span data-v="{{s.xid}}" data-j="left">{{s.xv}}</span></div></div>
          </div>
        </div>
      </sc-for>
    </div>
  </div>
""" + TABS + """</div>
"""


def make_a(key, t, title, plugin, accent, strips, banks, sub, tabnames, bg="#131418", panel="#1b1c21", line="#2a2c33",
           bar=True, abfs=22, bigfs=15, smallfs=12, namecol=None, pages=None):
    D = {"title": title, "plugin": plugin, "accent": accent, "sub": sub, "bg": bg, "panel": panel, "line": line,
         "bar": bar, "abfs": abfs, "bigfs": bigfs, "smallfs": smallfs, "namecol": namecol or "#e8e9ee",
         "track": "#2a2c33", "text": "#e8e9ee", "muted": "#9a9da6", "firm": "#1c1d22",
         "banks": [{"n": b, "bg": accent if i == 0 else "transparent", "fg": "#131418" if i == 0 else "#c9cbd2",
                    "border": accent if i == 0 else "#33353d"} for i, b in enumerate(banks)],
         "strips": strips, "tabs": tabs(tabnames, accent, "#8a8d96")}
    if getattr(strips, "args", None):
        D["_kit"] = strips.args
    if pages:
        D["_pages"] = pages
    html = HEAD % ("A · " + title, "Space+Grotesk:wght@500;700&family=Inter:wght@400;500;600") + A_BODY + TAIL % esc(json.dumps(D, ensure_ascii=False))
    open(os.path.join(OUT, key + ".dc.html"), "w", encoding="utf-8").write(html)
    side(key, t, "A", D)


PALETTE = ["#ff8a3d", "#ffc04d", "#57d0c3", "#9b8cff", "#ff7aa8", "#7fb3ff", "#9ad86a", "#e8a0ff"]
ABBR = [("grosse caisse", "BD"), ("caisse claire", "SD"), ("clap", "CP"), ("rimshot", "RS"), ("charley ferm", "CH"),
        ("charley ouvert", "OH"), ("tom grave", "LT"), ("tom médium", "MT"), ("tom aigu", "HT"), ("tom", "TM"), ("conga", "CG"),
        ("cloche", "CB"), ("cymbale", "CY"), ("crash", "CR"), ("ride", "RD"), ("maracas", "MA"), ("claves", "CL"),
        ("basse", "BASS"), ("tambour fm", "FM"), ("fm", "FM"), ("shaker", "SH"), ("percussion", "PC"), ("échantillon", "SMP"),
        ("zone", "Z"), ("pad", "PAD")]


def abbr(n):
    f = n.lower()
    for k, a in ABBR:
        if f.startswith(k):
            rest = f[len(k):].strip()
            return a + (rest.replace(" ", "") if a in ("Z", "PAD", "SMP") or rest.isdigit() else "")
    return n[:3].upper()


def kit_strips(t, pads, big, small, chip, lvl="level", x=("pan", "Pan"), chipname=True):
    names = INST[t].get("padNames") or []
    out = []
    for n in pads:
        q = lambda s: "p%02d_%s" % (n, s) if ("p%02d_%s" % (n, s)) in [p["id"] for p in INST[t]["params"]] else "z%02d_%s" % (n, s)
        nm = names[n - 1]
        lv = P(t, q(lvl))
        lo, hi = lv.get("min", -48), lv.get("max", 6)
        out.append({"ab": abbr(nm), "n": str(n), "name": nm, "col": PALETTE[(n - 1) // 2 % len(PALETTE)],
                    "chip": (P(t, q(chip[0]))["name"] + " · " + fmt(P(t, q(chip[0])))) if chip else "",
                    "chipid": q(chip[0]) if chip else "", "chipname": P(t, q(chip[0]))["name"] if (chip and chipname) else "", "chipval": fmt(P(t, q(chip[0]))) if chip else "", "lid": q(lvl), "xid": q(x[0]),
                    "big": kv(t, q(big[0]), big[1]), "small": kv(t, q(small[0]), small[1]),
                    "lvl": round(100 * ((lv.get("def", 0) or 0) - lo) / ((hi - lo) or 1)), "ll": "Niveau", "lv": fmt(lv),
                    "xl": x[1], "xv": fmt(P(t, q(x[0])))})
    out = KitStrips(out)
    out.args = [t, list(big), list(small), list(chip), lvl, list(x), chipname]
    return out


class KitStrips(list):
    """Tranches d'une batterie, avec de quoi refaire la banque suivante (sons 9 à 16) à l'habillage."""
    args = None


# ------------------------------------------------------------------------------------------- B
B_BODY = """<div style="width:1280px;height:800px;background:{{D.bg}};font-family:Archivo,sans-serif;color:{{D.text}};display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex;gap:12px;padding:12px">
    <nav aria-label="Modules" style="width:118px;display:flex;flex-direction:column;gap:6px">
      <sc-for list="{{D.nav}}" as="n" hint-placeholder-count="7">
        <button data-k="nav" data-i="{{$index}}" data-on="{{D.accent}}|{{D.bg}}|{{D.accent}}" data-off="{{D.hero}}|{{D.text}}|{{D.line}}" style="flex:1;border-radius:12px;border:1px solid {{n.border}};background:{{n.bg}};color:{{n.fg}};font-family:Archivo,sans-serif;font-stretch:80%;font-weight:700;font-size:18px;text-align:left;padding:0 14px">{{n.n}}</button>
      </sc-for>
    </nav>
    <div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0">
      <div style="flex:1;border-radius:16px;background:{{D.hero}};border:1px solid {{D.line}};display:flex;gap:18px;padding:14px 18px;overflow:hidden">
        <div data-k="hero" style="width:440px;height:440px;flex:none;position:relative">
          <svg width="440" height="440" viewBox="0 0 440 440" role="img" aria-label="{{D.heroAlt}}">
            <sc-for list="{{D.rects}}" as="r" hint-placeholder-count="2"><rect x="{{r.x}}" y="{{r.y}}" width="{{r.w}}" height="{{r.h}}" rx="{{r.rx}}" fill="{{r.fill}}" fill-opacity="{{r.fo}}" stroke="{{r.stroke}}" stroke-width="{{r.sw}}"></rect></sc-for>
            <sc-for list="{{D.paths}}" as="p" hint-placeholder-count="3"><path d="{{p.d}}" fill="{{p.fill}}" fill-opacity="{{p.fo}}" stroke="{{p.stroke}}" stroke-width="{{p.w}}" stroke-dasharray="{{p.dash}}" stroke-opacity="{{p.so}}" stroke-linecap="round" stroke-linejoin="round"></path></sc-for>
            <sc-for list="{{D.circles}}" as="c" hint-placeholder-count="3"><circle cx="{{c.x}}" cy="{{c.y}}" r="{{c.r}}" fill="{{c.fill}}" fill-opacity="{{c.fo}}" stroke="{{c.stroke}}" stroke-width="{{c.w}}"></circle></sc-for>
            <sc-for list="{{D.labels}}" as="l" hint-placeholder-count="3"><text x="{{l.x}}" y="{{l.y}}" fill="{{l.col}}" font-size="{{l.s}}" font-weight="{{l.fw}}" font-family="Archivo" text-anchor="{{l.anchor}}">{{l.t}}</text></sc-for>
          </svg>
          <sc-for list="{{D.heroVals}}" as="hv" hint-placeholder-count="1"><div data-v="{{hv.id}}" data-j="{{hv.j}}" data-abs="1" style="position:absolute;left:{{hv.x}}px;top:{{hv.y}}px;width:{{hv.w}}px;height:{{hv.h}}px;font-size:{{hv.fs}}px;font-weight:{{hv.fw}};color:{{hv.col}};display:flex;align-items:center;justify-content:{{hv.jc}};white-space:nowrap">{{hv.v}}</div></sc-for>
        </div>
        <div style="flex:1;display:flex;flex-direction:column;gap:10px;min-width:0">
          <div style="display:flex;align-items:baseline;gap:12px">
            <div style="font-weight:800;font-stretch:75%;font-size:32px;white-space:nowrap">{{D.heroTitle}}</div>
          </div>
          <div style="font-size:14px;line-height:1.45;color:{{D.sub}}">{{D.heroSub}}</div>
          <sc-for list="{{D.chips}}" as="g" hint-placeholder-count="2">
            <div style="display:flex;flex-direction:column;gap:6px">
              <div style="font-size:13px;color:{{D.muted}}">{{g.label}}</div>
              <div style="display:flex;gap:6px;flex-wrap:wrap">
                <sc-for list="{{g.opts}}" as="o" hint-placeholder-count="4">
                  <button data-k="radio" data-p="{{o.id}}" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="{{D.accent}}|{{D.bg}}|{{D.accent}}" data-off="transparent|{{D.text}}|{{D.line}}" style="height:36px;padding:0 13px;border-radius:18px;border:1px solid {{o.border}};background:{{o.bg}};color:{{o.fg}};font-family:Archivo,sans-serif;font-size:14px;font-weight:600">{{o.n}}</button>
                </sc-for>
              </div>
            </div>
          </sc-for>
          <div style="display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;margin-top:2px">
            <sc-for list="{{D.readouts}}" as="r" hint-placeholder-count="6">
              <div style="border-radius:10px;background:{{D.bg}};border:1px solid {{D.line}};padding:8px 12px;font-size:14px;display:flex;justify-content:space-between;gap:8px"><span style="color:{{D.sub}};white-space:nowrap;overflow:hidden;text-overflow:ellipsis">{{r.k}}</span><b data-v="{{r.id}}" data-j="right" data-fw="Bold" style="font-variant-numeric:tabular-nums;white-space:nowrap">{{r.v}}</b></div>
            </sc-for>
          </div>
        </div>
      </div>
      <div data-k="macrorow" style="height:182px;border-radius:16px;background:{{D.hero}};border:1px solid {{D.line}};display:grid;grid-template-columns:repeat(8,minmax(0,1fr));align-items:center;padding:6px 10px">
        <sc-for list="{{D.macros}}" as="m" hint-placeholder-count="8">
          <div style="display:flex;flex-direction:column;align-items:center;gap:6px">
            <div data-mn="{{$index}}" style="font-size:14px;font-weight:600;white-space:nowrap;overflow:hidden;max-width:130px;text-overflow:ellipsis">{{m.l}}</div>
            <div data-k="knob" data-p="{{m.id}}" data-col="{{D.accent}}" data-track="{{D.track}}" data-slot="{{$index}}" style="width:100px;height:100px;border-radius:50%;background:conic-gradient(from 225deg,{{D.accent}} 0 {{m.a}}deg,{{D.track}} {{m.a}}deg 270deg,transparent 270deg);display:flex;align-items:center;justify-content:center">
              <div data-v="{{m.id}}" data-fw="Bold" style="width:84px;height:84px;border-radius:50%;background:{{D.bg}};display:flex;align-items:center;justify-content:center;font-weight:800;font-stretch:80%;font-size:19px;font-variant-numeric:tabular-nums">{{m.v}}</div>
            </div>
            <div style="font-size:12px;color:{{D.muted}}">{{m.foot}}</div>
          </div>
        </sc-for>
      </div>
    </div>
  </div>
""" + TABS + """</div>
"""


def make_b(key, t, title, plugin, th, nav, hero, heroTitle, heroSub, chips, readouts, macros, tabnames):
    acc = th["accent"]
    D = {"title": title, "plugin": plugin, "accent": acc, "bg": th["bg"], "hero": th["hero"], "line": th["line"], "track": th["track"],
         "text": th["text"], "sub": th["sub"], "muted": th["muted"], "firm": th["firm"],
         "nav": [{"n": n, "bg": acc if i == 0 else th["hero"], "fg": th["bg"] if i == 0 else th["text"], "border": acc if i == 0 else th["line"]} for i, n in enumerate(nav)],
         "heroTitle": heroTitle, "heroSub": heroSub, "heroAlt": heroTitle,
         "chips": [{"label": P(t, c)["name"], "opts": [{"n": o, "i": i, "cnt": len(P(t, c)["choices"]), "id": c,
                                                     "bg": acc if o == fmt(P(t, c)) else "transparent",
                                                     "fg": th["bg"] if o == fmt(P(t, c)) else th["text"],
                                                     "border": acc if o == fmt(P(t, c)) else th["line"]} for i, o in enumerate((P(t, c)["choices"] or [])[:5])]} for c in chips],
         "readouts": [{"k": P(t, r)["name"], "v": fmt(P(t, r)), "id": r} for r in readouts],
         "macros": [dict(kv(t, m), k=i + 1, foot="bouton %d" % (i + 1)) for i, m in enumerate(macros)], "tabs": tabs(tabnames, th["accent2"], th["muted"])}
    D.update({"rects": [], "paths": [], "circles": [], "labels": []})
    for k in ("rects", "paths", "circles", "labels"):
        D[k] = hero.get(k, [])
    D["heroVals"] = hero.get("vals", [])
    if hero.get("film"):
        D["_film"] = hero["film"]
    html = HEAD % ("B · " + title, "Archivo:wdth,wght@75..100,400..800") + B_BODY + TAIL % esc(json.dumps(D, ensure_ascii=False))
    open(os.path.join(OUT, key + ".dc.html"), "w", encoding="utf-8").write(html)
    side(key, t, "B", D)


def R(x, y, w, h, rx=12, fill="none", fo=1, stroke="none", sw=1.5):
    return {"x": x, "y": y, "w": w, "h": h, "rx": rx, "fill": fill, "fo": fo, "stroke": stroke, "sw": sw}


def PA(d, stroke="#fff", w=2.5, fill="none", fo=0, dash="none", so=1):
    return {"d": d, "stroke": stroke, "w": w, "fill": fill, "fo": fo, "dash": dash, "so": so}


def C(x, y, r, fill="none", stroke="none", w=2, fo=1):
    return {"x": x, "y": y, "r": r, "fill": fill, "stroke": stroke, "w": w, "fo": fo}


def L(x, y, t, col="#fff", s=14, fw=600, anchor="start"):
    return {"x": x, "y": y, "t": t, "col": col, "s": s, "fw": fw, "anchor": anchor}


def wave(kind, x0, y0, w, h, cycles=2):
    pts = []
    for i in range(0, 121):
        u = i / 120 * cycles
        f = u % 1
        if kind == "saw":
            v = 1 - 2 * f
        elif kind == "sq":
            v = 1 if f < 0.5 else -1
        elif kind == "tri":
            v = 1 - 4 * abs(f - 0.5)
        elif kind == "sin":
            v = math.sin(2 * math.pi * u)
        elif kind == "fold":
            v = math.sin(2 * math.pi * u + 2.2 * math.sin(2 * math.pi * u))
        elif kind == "pulse":
            v = 1 if f < 0.25 else -1
        else:
            v = math.sin(2 * math.pi * u) * 0.6 + math.sin(6 * math.pi * u) * 0.4
        pts.append((x0 + w * i / 120, y0 + h / 2 - v * h / 2))
    return "M" + " L".join("%.1f %.1f" % p for p in pts)


def hero_filter(acc, a2, title="Filtre"):
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22")],
            "paths": [PA("M20 150 L240 150 C290 150 300 110 320 130 C345 160 360 380 420 400 L420 420 L20 420 Z", acc, 0, acc, .14),
                      PA("M20 150 L240 150 C290 150 300 110 320 130 C345 160 360 380 420 400", acc, 3),
                      PA("M20 285 L420 285", "#ffffff", 1, so=.12), PA("M220 20 L220 420", "#ffffff", 1, so=.12)],
            "circles": [C(318, 128, 10, "#00000099", a2, 4)], "labels": [L(30, 410, "20 Hz", "#ffffff88", 12), L(410, 410, "20 kHz", "#ffffff88", 12, anchor="end")]}


def hero_waves(acc, a2, kinds, labels):
    rects, paths, labels_ = [R(20, 20, 400, 400, 16, stroke="#ffffff22")], [], []
    n = len(kinds)
    for i, k in enumerate(kinds):
        y = 40 + i * (380 / n)
        h = 380 / n - 30
        rects.append(R(34, y, 372, h + 14, 10, "#000000", .25))
        paths.append(PA(wave(k, 50, y + 7, 340, h, 2 + i), [acc, a2, "#9fd2ff", "#ff9ec4"][i % 4], 2.5))
        labels_.append(L(44, y + 22, labels[i], "#ffffffcc", 13, 700))
    return {"rects": rects, "paths": paths, "circles": [], "labels": labels_}


def hero_table(acc, a2, frames=9):
    paths = []
    for i in range(frames):
        off = i * 22
        k = i / (frames - 1)
        pts = []
        for j in range(0, 81):
            u = j / 80
            v = math.sin(2 * math.pi * u) * (1 - k) + (1 - 2 * ((u * 2) % 1)) * k * 0.9
            pts.append((40 + off * 0.9 + u * 280, 330 - off * 1.0 - v * 50))
        d = "M" + " L".join("%.1f %.1f" % p for p in pts)
        paths.append(PA(d, a2 if i == 4 else acc, 3.5 if i == 4 else 1.6, so=1 if i == 4 else 0.45))
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22")], "paths": paths, "circles": [],
            "labels": [L(40, 400, "position dans la table →", "#ffffff99", 13)]}


def hero_fm(acc, a2, ops, links, carriers):
    pos = {}
    n = len(ops)
    for i, o in enumerate(ops):
        col = i % 3
        row = i // 3
        pos[o] = (100 + col * 120, 110 + row * 150)
    paths = [PA("M%d %d L%d %d" % (pos[a][0], pos[a][1] + 36, pos[b][0], pos[b][1] - 36), "#ffffff", 2.5, so=.55) for a, b in links]
    circles = [C(x, y, 36, a2 if o in carriers else "#00000066", acc, 3) for o, (x, y) in pos.items()]
    labels = [L(x, y + 8, str(o), "#111" if o in carriers else "#fff", 24, 800, "middle") for o, (x, y) in pos.items()]
    labels.append(L(220, 405, "porteuses en plein, modulateurs en creux", "#ffffff99", 13, 500, "middle"))
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22")], "paths": paths, "circles": circles, "labels": labels}


def hero_two(acc, a2, la, lb, ka, kb):
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22"), R(40, 60, 170, 260, 14, "#000000", .3, acc, 2), R(230, 60, 170, 260, 14, "#000000", .3, a2, 2)],
            "paths": [PA(wave(ka, 55, 150, 140, 80, 2), acc, 3), PA(wave(kb, 245, 150, 140, 80, 3), a2, 3),
                      PA("M60 370 L380 370", "#ffffff", 6, so=.2), PA("M60 370 L250 370", acc, 6)],
            "circles": [C(250, 370, 12, "#111", "#fff", 3)], "labels": [L(125, 95, la, acc, 22, 800, "middle"), L(315, 95, lb, a2, 22, 800, "middle"), L(220, 350, "mélange A ↔ B", "#ffffffaa", 13, 500, "middle")]}


def hero_mask(acc, a2):
    rects = [R(20, 20, 400, 400, 16, stroke="#ffffff22")]
    import random
    random.seed(7)
    for r in range(8):
        for c in range(16):
            on = random.random() < 0.45
            rects.append(R(38 + c * 23, 60 + r * 23, 19, 19, 3, acc if on else "#ffffff", 1 if on else .07))
    return {"rects": rects, "paths": [PA(wave("sq", 40, 290, 360, 90, 4), a2, 3)], "circles": [],
            "labels": [L(40, 48, "masques des bits (osc. 1 et 2)", "#ffffffaa", 13), L(40, 410, "onde obtenue", "#ffffffaa", 13)]}


def hero_sample(acc, a2):
    import random
    random.seed(3)
    d = "M40 150"
    for i in range(0, 361, 3):
        a = math.exp(-i / 160) * (55 + random.random() * 25)
        d += " L%d %.1f L%d %.1f" % (40 + i, 150 - a, 41 + i, 150 + a)
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22"), R(40, 70, 220, 160, 4, acc, .12)],
            "paths": [PA(d, acc, 1.2), PA("M40 380 L90 270 L150 300 L330 300 L400 380", a2, 3), PA("M40 380 L90 270 L150 300 L330 300 L400 380 Z", a2, 0, a2, .1)],
            "circles": [C(90, 270, 8, "#111", a2, 3), C(150, 300, 8, "#111", a2, 3), C(330, 300, 8, "#111", a2, 3)],
            "labels": [L(40, 60, "début → fin de lecture", "#ffffffaa", 13), L(40, 255, "enveloppe ampli", "#ffffffaa", 13)]}


def hero_mod(acc, a2):
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22")],
            "paths": [PA(wave("sin", 40, 60, 360, 70, 1), a2, 3), PA(wave("fold", 40, 190, 360, 120, 3), acc, 3),
                      PA("M220 140 L220 180", "#ffffff", 2, so=.5)],
            "circles": [], "labels": [L(40, 52, "oscillateur de modulation", "#ffffffaa", 13), L(40, 350, "osc. 1 modulé : forme, hauteur, filtres", "#ffffffaa", 13)]}


def hero_quad(acc, a2):
    cols = [acc, a2, "#9fd2ff", "#ff9ec4"]
    names = ["A1", "A2", "B1", "B2"]
    kinds = ["saw", "pulse", "tri", "sin"]
    paths = [PA(wave(k, 60, 120 + i * 10, 320, 160, 2 + i * 0.5), cols[i], 2.2, so=.85) for i, k in enumerate(kinds)]
    labels = [L(60 + i * 90, 360, names[i], cols[i], 20, 800) for i in range(4)]
    return {"rects": [R(20, 20, 400, 400, 16, stroke="#ffffff22")], "paths": paths, "circles": [], "labels": labels + [L(40, 60, "4 oscillateurs superposés", "#ffffffaa", 13)]}


THEMES = {
    "ink": dict(bg="#0e1724", hero="#111c2c", line="#233651", track="#22324a", text="#e6eef8", sub="#9fb2cc", muted="#7d8ea6", firm="#0a111b", accent="#7fd3ff", a2="#ffc873", accent2="#ffc873"),
    "plum": dict(bg="#160f20", hero="#1a1127", line="#33254a", track="#2e2240", text="#efe8fb", sub="#b6a8cc", muted="#9a8db0", firm="#110b19", accent="#c4a7ff", a2="#ffcf6b", accent2="#ffcf6b"),
    "forest": dict(bg="#0f1a16", hero="#13221d", line="#25402f", track="#21382c", text="#e8f3ec", sub="#a6c2b2", muted="#86a394", firm="#0b1411", accent="#6fe0a8", a2="#f0c25a", accent2="#f0c25a"),
    "rust": dict(bg="#1b120e", hero="#221712", line="#41291f", track="#3a261c", text="#f6ebe4", sub="#cbb3a5", muted="#a38e81", firm="#140d0a", accent="#ff8a4c", a2="#9fd2ff", accent2="#ff8a4c"),
    "steel": dict(bg="#121518", hero="#181c20", line="#2b3238", track="#262c32", text="#eef1f4", sub="#aab4bd", muted="#87919a", firm="#0d1012", accent="#9fd2ff", a2="#ff9ec4", accent2="#9fd2ff"),
    "wine": dict(bg="#1a0e14", hero="#22121a", line="#41222f", track="#3a1f2b", text="#f8e9ef", sub="#d0aebd", muted="#a8889a", firm="#13090e", accent="#ff7aa8", a2="#ffd27a", accent2="#ff7aa8"),
    "teal": dict(bg="#0d1b1d", hero="#112426", line="#22434a", track="#1f3b40", text="#e6f5f6", sub="#a2c6ca", muted="#80a6ab", firm="#0a1416", accent="#4fd1c5", a2="#ffb36b", accent2="#4fd1c5"),
}


# ------------------------------------------------------------------------------------------- C
C_BODY = """<div style="width:1280px;height:800px;background:#1a1b1e;font-family:Barlow,sans-serif;display:flex;flex-direction:column;overflow:hidden">
""" + FIRM + """  <div style="flex:1;display:flex">
    <div style="width:{{D.cheekW}}px;background:{{D.cheekL}};box-shadow:inset -3px 0 6px #0008"></div>
    <div style="flex:1;background:{{D.face}};display:flex;flex-direction:column;padding:10px 16px 10px;gap:10px;color:{{D.ink}}">
      <div style="display:flex;align-items:center;gap:16px;height:64px">
        <div style="font-family:'Barlow Condensed',sans-serif;font-weight:700;font-size:38px;letter-spacing:.08em;color:{{D.logo}}">{{D.title}}</div>
        <div style="font-size:14px;color:{{D.sub}}">{{D.subtitle}}</div>
        <div style="margin-left:auto;width:{{D.scrW}}px;height:56px;border-radius:6px;background:#0d1712;border:4px solid #111;padding:4px 10px;display:flex;flex-direction:column;justify-content:center">
          <div style="font-size:12px;color:#5fa878">{{D.screenL}}</div>
          <div data-v="{{D.screenId}}" data-j="left" data-fw="SemiBold" style="font-family:'Barlow Condensed',sans-serif;font-size:24px;font-weight:600;color:#c9f5d6;line-height:1">{{D.screenV}}</div>
        </div>
        <div style="width:{{D.vuW}}px;height:56px;border-radius:6px;background:linear-gradient(#f3ead0,#e2d3a8);border:4px solid #111;position:relative;overflow:hidden">
          <div style="position:absolute;left:{{D.vuX}}px;bottom:-24px;width:3px;height:64px;background:#1d1d1d;transform-origin:50% 100%;transform:rotate(22deg)"></div>
          <div style="position:absolute;left:8px;top:4px;font-family:'Barlow Condensed',sans-serif;font-size:11px;color:#5a4d2c">Sortie</div>
        </div>
      </div>
      <div style="flex:1;display:flex;gap:10px">
        <sc-for list="{{D.secs}}" as="s" hint-placeholder-count="4">
          <div style="flex:{{s.flex}};min-width:0;border:1.5px solid {{D.border}};border-radius:8px;display:flex;flex-direction:column;padding:6px 8px 8px;gap:4px">
            <div style="align-self:center;font-family:'Barlow Condensed',sans-serif;font-weight:700;font-size:17px;letter-spacing:.12em;color:{{D.logo}};background:{{D.tagBg}};padding:0 10px;margin-top:-17px;white-space:nowrap">{{s.name}}</div>
            <div style="flex:1;display:grid;grid-template-columns:repeat({{s.cols}},minmax(0,1fr));align-items:center;justify-items:center;gap:4px">
              <sc-for list="{{s.items}}" as="k" hint-placeholder-count="6">
                <div style="display:flex;flex-direction:column;align-items:center;gap:3px;grid-column:span {{k.span}}">
                  <div style="font-size:13px;font-weight:600;color:{{D.label}};white-space:nowrap">{{k.l}}</div>
                  <sc-if value="{{k.knob}}" hint-placeholder-val="{{ true }}">
                    <div data-k="cknob" data-p="{{k.id}}" data-q="{{k.q}}" style="width:{{k.s}}px;height:{{k.s}}px;border-radius:50%;background:radial-gradient(circle,{{D.ring}} 0 60%,transparent 61%),repeating-conic-gradient(from 225deg,{{D.tick}} 0 1.6deg,transparent 1.6deg 27deg);display:flex;align-items:center;justify-content:center">
                      <div style="width:{{k.i}}px;height:{{k.i}}px;border-radius:50%;background:{{D.knob}};box-shadow:0 5px 9px #0009,inset 0 1px 1px #fff2;border:3px solid {{k.cap}};position:relative;transform:rotate({{k.rot}}deg)">
                        <div style="position:absolute;left:calc(50% - 1.5px);top:3px;width:3px;height:{{k.ptr}}px;border-radius:2px;background:{{D.pointer}}"></div>
                      </div>
                    </div>
                  </sc-if>
                  <sc-if value="{{k.fader}}" hint-placeholder-val="{{ false }}">
                    <div data-k="cfader" data-p="{{k.id}}" data-q="{{k.q}}" data-h="{{k.h}}" style="width:30px;height:{{k.h}}px;border-radius:5px;background:{{D.slot}};border:1px solid {{D.border}};position:relative">
                      <div style="position:absolute;left:13px;top:8px;bottom:8px;width:4px;background:#000"></div>
                      <div style="position:absolute;left:-4px;bottom:{{k.pos}}px;width:38px;height:22px;border-radius:3px;background:linear-gradient(#4c4f56,#202124);border-top:3px solid {{k.cap}};box-shadow:0 4px 6px #0008"></div>
                    </div>
                  </sc-if>
                  <sc-if value="{{k.btns}}" hint-placeholder-val="{{ false }}">
                    <div style="display:flex;gap:4px;flex-wrap:wrap;justify-content:center">
                      <sc-for list="{{k.opts}}" as="o" hint-placeholder-count="3">
                        <button data-k="radio" data-p="{{o.id}}" data-i="{{o.i}}" data-n="{{o.cnt}}" data-on="{{o.onbg}}|{{o.onfg}}|{{D.border}}" data-off="transparent|{{D.ink}}|{{D.border}}" style="min-width:{{D.btnW}}px;height:{{D.btnH}}px;padding:0 8px;border-radius:5px;border:1px solid {{D.border}};background:{{o.bg}};color:{{o.fg}};font-family:'Barlow Condensed',sans-serif;font-weight:700;font-size:{{D.btnF}}px">{{o.n}}</button>
                      </sc-for>
                    </div>
                  </sc-if>
                  <div data-v="{{k.vid}}" style="font-size:12px;color:{{D.sub}};font-variant-numeric:tabular-nums;white-space:nowrap;min-width:70px;text-align:center">{{k.v}}</div>
                </div>
              </sc-for>
            </div>
          </div>
        </sc-for>
      </div>
      <div style="display:flex;gap:16px;align-items:center;font-size:13px;color:{{D.sub}}">Q-Links 1-16
        <sc-for list="{{D.legend}}" as="g" hint-placeholder-count="4">
          <span style="display:flex;align-items:center;gap:6px"><span style="width:14px;height:14px;border-radius:3px;background:{{g.c}}"></span>{{g.t}}</span>
        </sc-for>
        <span style="margin-left:auto">sans couleur : au toucher seulement</span>
      </div>
    </div>
    <div style="width:{{D.cheekW}}px;background:{{D.cheekR}};box-shadow:inset 3px 0 6px #0008"></div>
  </div>
""" + TABS + """</div>
"""
QC = ["#e2702a", "#a8b84a", "#3aa6c8", "#d8507a"]
CTHEMES = {
    "walnut_dark": dict(face="linear-gradient(180deg,#2c2e33,#232428)", cheek="#6e4122", ink="#e9dcc0", logo="#f1e6cc", sub="#a99f88", label="#d9ccb0", border="#5b5446", tagBg="#262729", ring="#2c2e33", tick="#8f866f", knob="radial-gradient(circle at 38% 30%,#4c4f56,#121315 72%)", pointer="#f1e6cc", slot="#141517"),
    "silver": dict(face="repeating-linear-gradient(90deg,#d5d8dc 0 2px,#cfd2d6 2px 3px,#d9dce0 3px 5px)", cheek=None, ink="#24262a", logo="#1d1f23", sub="#4b4f56", label="#33363c", border="#9fa4ab", tagBg="#d3d6da", ring="#c6c9ce", tick="#55595f", knob="radial-gradient(circle at 38% 30%,#4a4d53,#16171a 72%)", pointer="#f2f2f2", slot="#8c9096"),
    "black": dict(face="linear-gradient(180deg,#1f2023,#18191b)", cheek=None, ink="#eceae6", logo="#ffffff", sub="#a3a09a", label="#dad7d1", border="#4a4946", tagBg="#1c1d1f", ring="#1f2023", tick="#77736c", knob="radial-gradient(circle at 38% 30%,#3e4045,#0c0d0e 72%)", pointer="#ffffff", slot="#0e0f10"),
    "walnut_black": dict(face="linear-gradient(180deg,#1d1e21,#151618)", cheek="#7a4a28", ink="#efe7da", logo="#ffffff", sub="#a9a194", label="#ddd4c6", border="#4c4740", tagBg="#1a1b1d", ring="#1d1e21", tick="#857c6e", knob="radial-gradient(circle at 38% 30%,#45474c,#0f1011 72%)", pointer="#ffffff", slot="#0e0f10"),
    "grey_light": dict(face="linear-gradient(180deg,#bfc2c5,#a9adb1)", cheek=None, ink="#1c1e21", logo="#15171a", sub="#3d4146", label="#25282c", border="#7d8287", tagBg="#b5b8bc", ring="#b0b4b8", tick="#4a4e53", knob="radial-gradient(circle at 38% 30%,#4a4d53,#16171a 72%)", pointer="#f2f2f2", slot="#7d8287"),
    "navy": dict(face="linear-gradient(180deg,#1a2130,#141a26)", cheek="#5b361d", ink="#e6ebf3", logo="#ffffff", sub="#9aa7bb", label="#d3dbe8", border="#3b465c", tagBg="#171e2b", ring="#1a2130", tick="#6f7d95", knob="radial-gradient(circle at 38% 30%,#444b58,#0d1017 72%)", pointer="#ffffff", slot="#0c1018"),
}


def ck(t, pid, q=None, big=False, short=None, small=False):
    p = P(t, pid)
    s = 112 if big else (60 if (q is None or small) else 74)
    return {"l": short or p["name"], "v": fmt(p), "rot": round(ang(p) - 135), "cap": QC[q] if q is not None else "#55524a", "span": 1,
            "id": pid, "vid": pid, "q": -1 if q is None else q, "knob": True, "s": s, "i": round(s * 0.7), "ptr": round(s * 0.2)}


def cf(t, pid, q=None, h=200, short=None):
    p = P(t, pid)
    return {"l": short or p["name"], "v": fmt(p), "pos": round(ang(p) / 270 * (h - 30)), "cap": QC[q] if q is not None else "#77736c",
            "span": 1, "fader": True, "h": h, "id": pid, "vid": pid, "q": -1 if q is None else q}


def cb(t, pid, short=None, span=2, th=None):
    p = P(t, pid)
    cur = fmt(p)
    on_bg, on_fg = (th or {}).get("on", ("#e9dcc0", "#1a1b1e"))
    return {"l": short or p["name"], "v": "", "span": span, "btns": True, "id": pid, "vid": "", "q": -1,
            "opts": [{"n": o, "i": i, "cnt": len(p["choices"]), "id": pid, "bg": on_bg if o == cur else "transparent",
                      "fg": on_fg if o == cur else (th or {}).get("offfg", "#e9dcc0")} for i, o in enumerate((p["choices"] or [])[:4])]}


def make_c(key, t, title, plugin, theme, subtitle, screen, secs, legend, tabnames, accent="#e2702a", scrW=230, vuW=140, btn=(40, 34, 14)):
    th = CTHEMES[theme]
    light = theme in ("silver", "grey_light")
    for s in secs:
        for it in s["items"]:
            if it.get("btns"):
                for o in it["opts"]:
                    o["onbg"], o["onfg"] = (th["ink"], "#f2f2f2") if light else ("#e9dcc0", "#1a1b1e")
                    if o["bg"] != "transparent":
                        o["bg"], o["fg"] = o["onbg"], o["onfg"]
                    else:
                        o["fg"] = th["ink"]
    D = dict(th)
    D.update({"scrW": scrW, "vuW": vuW, "vuX": vuW // 2 - 2, "btnW": btn[0], "btnH": btn[1], "btnF": btn[2]})
    D.update({"title": title, "plugin": plugin, "subtitle": subtitle, "screenL": screen[0], "screenV": screen[1], "screenId": screen[2] if len(screen) > 2 else "", "secs": secs,
              "legend": [{"c": QC[i], "t": g} for i, g in enumerate(legend)],
              "cheekW": 34 if th["cheek"] else 0, "cheekL": "linear-gradient(90deg,#00000055,%s 45%%,#00000033)" % (th["cheek"] or "#000"),
              "cheekR": "linear-gradient(270deg,#00000055,%s 45%%,#00000033)" % (th["cheek"] or "#000"),
              "text": "#e8e9ee", "muted": "#8a8d96", "firm": "#16171b", "tabs": tabs(tabnames, accent, "#8a8d96")})
    html = HEAD % ("C · " + title, "Barlow+Condensed:wght@500;600;700&family=Barlow:wght@500;600") + C_BODY + TAIL % esc(json.dumps(D, ensure_ascii=False))
    open(os.path.join(OUT, key + ".dc.html"), "w", encoding="utf-8").write(html)
    side(key, t, "C", D)


def hero_fm_cards(stacks, fb_op, head, retro, sel=None):
    """Schéma d'algorithme FM façon GOLEM : cartes numérotées empilées (modulateurs en haut), porteuses reliées
    au bus SORTIE en bas, boucle de rétroaction en pointillé, en-tête ALG ◀ ▶ et RÉTRO."""
    SCR, CARD, CARD_S, CAR, CAR_S, BUS, SEL, TXT = "#06120f", "#0b1f1b", "#2c5f55", "#0f4a40", "#3fae95", "#58e0c0", "#f2c94c", "#eafff8"
    rects = [R(20, 20, 400, 400, 16, "#0a0b0c", 1, "#ffffff22"), R(40, 92, 360, 300, 10, SCR, 1, "#1f3d37", 2)]
    rects += [R(40, 36, 40, 40, 8, "#111", 1, "#3a3a3a"), R(178, 36, 40, 40, 8, "#111", 1, "#3a3a3a")]
    paths = [PA("M66 46 L54 56 L66 66 Z", "#cfe", 0, "#cfe", 1), PA("M192 46 L204 56 L192 66 Z", "#cfe", 0, "#cfe", 1)]
    labels = [L(129, 64, head, BUS, 24, 800, "middle"), L(400, 64, retro, "#e7a6ff", 15, 600, "end"), L(220, 386, "SORTIE", "#9adbcb", 12, 600, "middle")]
    circles = []
    n = len(stacks)
    bw, bh, gap = (60 if n <= 3 else 52), 40, 18
    span = 280 if n > 2 else 200
    x0 = 220 - span / 2
    xs = [x0 + (span - bw) * (i / max(1, n - 1)) if n > 1 else 220 - bw / 2 for i in range(n)]
    bus_y = 372
    paths.append(PA("M60 %d L380 %d" % (bus_y, bus_y), BUS, 3))
    first_car = True
    for i, st in enumerate(stacks):
        x = xs[i]
        for j, op in enumerate(st):
            y = bus_y - 30 - bh - j * (bh + gap)
            carrier = j == 0
            if j > 0:
                paths.append(PA("M%.0f %.0f L%.0f %.0f" % (x + bw / 2, y + bh, x + bw / 2, y + bh + gap), CAR_S, 2.5))
            else:
                paths.append(PA("M%.0f %.0f L%.0f %d" % (x + bw / 2, y + bh, x + bw / 2, bus_y), BUS, 2.5))
            stroke = SEL if (sel == op or (sel is None and carrier and first_car)) else (CAR_S if carrier else CARD_S)
            if carrier:
                first_car = False
            rects.append(R(x, y, bw, bh, 8, CAR if carrier else CARD, 1, stroke, 3 if stroke == SEL else 2))
            labels.append(L(x + bw / 2, y + bh / 2 + 8, str(op), TXT, 22, 800, "middle"))
            if op == fb_op:
                paths.append(PA("M%.0f %.0f h 12 v %.0f h %.0f v 8" % (x + bw, y + bh / 2, -bh / 2 - 10, -bw / 2 - 12), "#9adbcb", 2, dash="4 4"))
    return {"rects": rects, "paths": paths, "circles": circles, "labels": labels}


def hero_fm_graph(mods, car, fb, names, head, retro_id=None, sel=None):
    """Schéma d'un algorithme FM façon GOLEM (cartes numérotées), pour n'importe quelle table :
    mods[k] = bits des modulateurs de l'opérateur k, car = bits des opérateurs entendus, fb = opérateur
    à rétroaction (indice). Rangée 0 : opérateurs entendus sur le bus SORTIE ; au-dessus, leurs
    modulateurs. Même dessin que hero_fm_cards pour les algorithmes en piles."""
    SCR, CARD, CARD_S, CAR, CAR_S, BUS, SEL, TXT = "#06120f", "#0b1f1b", "#2c5f55", "#0f4a40", "#3fae95", "#58e0c0", "#f2c94c", "#eafff8"
    n = len(mods)
    depth = [0 if (car >> k) & 1 else -1 for k in range(n)]
    for _ in range(n + 1):
        for k in range(n):
            if depth[k] < 0:
                continue
            for j in range(n):
                if j != k and (mods[k] >> j) & 1 and depth[j] < depth[k] + 1:
                    depth[j] = depth[k] + 1
    for k in range(n):
        if depth[k] < 0:
            depth[k] = 0
    col = [-1] * n
    used = set()
    nxt = 0
    for d in range(max(depth) + 1):
        for k in range(n):
            if depth[k] != d:
                continue
            if d == 0:
                c = nxt
            else:
                tg = [col[t] for t in range(n) if (mods[t] >> k) & 1 and col[t] >= 0 and t != k]
                c = min(tg) if tg else 0
            while (d, c) in used:
                c += 1
            used.add((d, c))
            col[k] = c
            nxt = max(nxt, c + 1) if d == 0 else nxt
    ncol = max(col) + 1
    rects = [R(20, 20, 400, 400, 16, "#0a0b0c", 1, "#ffffff22"), R(40, 92, 360, 300, 10, SCR, 1, "#1f3d37", 2)]
    rects += [R(40, 36, 40, 40, 8, "#111", 1, "#3a3a3a"), R(178, 36, 40, 40, 8, "#111", 1, "#3a3a3a")]
    paths = [PA("M66 46 L54 56 L66 66 Z", "#cfe", 0, "#cfe", 1), PA("M192 46 L204 56 L192 66 Z", "#cfe", 0, "#cfe", 1)]
    labels = [L(129, 64, head, BUS, 24, 800, "middle"), L(220, 386, "SORTIE", "#9adbcb", 12, 600, "middle")]
    vals = []
    if retro_id:
        labels.append(L(368, 64, "RÉTRO", "#e7a6ff", 15, 600, "end"))
        vals.append({"id": retro_id, "x": 372, "y": 48, "w": 32, "h": 22, "fs": 15, "fw": 600, "col": "#e7a6ff", "j": "left", "jc": "flex-start", "v": "0"})
    bw = 60 if ncol <= 3 else (52 if ncol <= 5 else 44)
    bh = 40
    rows = max(depth) + 1
    gap = 18 if rows <= 4 else max(6, (372 - 30 - 100 - rows * bh) // max(1, rows - 1))
    span = 280 if ncol > 2 else 200
    if ncol > 5:
        span = 320
    x0 = 220 - span / 2
    xs = [x0 + (span - bw) * (i / max(1, ncol - 1)) if ncol > 1 else 220 - bw / 2 for i in range(ncol)]
    bus_y = 372
    paths.append(PA("M60 %d L380 %d" % (bus_y, bus_y), BUS, 3))
    pos = {}
    for k in range(n):
        pos[k] = (xs[col[k]], bus_y - 30 - bh - depth[k] * (bh + gap))
    for k in range(n):
        x, y = pos[k]
        if (car >> k) & 1:
            paths.append(PA("M%.0f %.0f L%.0f %d" % (x + bw / 2, y + bh, x + bw / 2, bus_y), BUS, 2.5))
        for j in range(n):
            if j != k and (mods[k] >> j) & 1:
                xj, yj = pos[j]
                paths.append(PA("M%.0f %.0f L%.0f %.0f" % (xj + bw / 2, yj + bh, x + bw / 2, y), CAR_S, 2.5))
    first = min([k for k in range(n) if (car >> k) & 1], default=0)
    for k in range(n):
        x, y = pos[k]
        carrier = (car >> k) & 1
        stroke = SEL if (sel == k or (sel is None and k == first)) else (CAR_S if carrier else CARD_S)
        rects.append(R(x, y, bw, bh, 8, CAR if carrier else CARD, 1, stroke, 3 if stroke == SEL else 2))
        labels.append(L(x + bw / 2, y + bh / 2 + 8, str(names[k]), TXT, 22 if len(str(names[k])) < 2 else 18, 800, "middle"))
        if k == fb:
            paths.append(PA("M%.0f %.0f h 12 v %.0f h %.0f v 8" % (x + bw, y + bh / 2, -bh / 2 - 10, -bw / 2 - 12), "#9adbcb", 2, dash="4 4"))
    return {"rects": rects, "paths": paths, "circles": [], "labels": labels, "vals": vals}


# tables des moteurs (mêmes que ui/src/screens/faces/FmPanels.tsx, Fm6.h et Fm4.h)
_O = [1, 2, 4, 8, 16, 32]
O1, O2, O3, O4, O5, O6 = _O
FM6_ALGOS = [
    ([O2, 0, O4, O5, O6, 0], O1 | O3, 5), ([O2, 0, O4, O5, O6, 0], O1 | O3, 1), ([O2, O3, 0, O5, O6, 0], O1 | O4, 5),
    ([O2, O3, 0, O5, O6, 0], O1 | O4, 5), ([O2, 0, O4, 0, O6, 0], O1 | O3 | O5, 5), ([O2, 0, O4, 0, O6, 0], O1 | O3 | O5, 5),
    ([O2, 0, O4 | O5, 0, O6, 0], O1 | O3, 5), ([O2, 0, O4 | O5, 0, O6, 0], O1 | O3, 3), ([O2, 0, O4 | O5, 0, O6, 0], O1 | O3, 1),
    ([O2, O3, 0, O5 | O6, 0, 0], O1 | O4, 2), ([O2, O3, 0, O5 | O6, 0, 0], O1 | O4, 5), ([O2, 0, O4 | O5 | O6, 0, 0, 0], O1 | O3, 1),
    ([O2, 0, O4 | O5 | O6, 0, 0, 0], O1 | O3, 5), ([O2, 0, O4, O5 | O6, 0, 0], O1 | O3, 5), ([O2, 0, O4, O5 | O6, 0, 0], O1 | O3, 1),
    ([O2 | O3 | O5, 0, O4, 0, O6, 0], O1, 5), ([O2 | O3 | O5, 0, O4, 0, O6, 0], O1, 1), ([O2 | O3 | O4, 0, 0, O5, O6, 0], O1, 2),
    ([O2, O3, 0, O6, O6, 0], O1 | O4 | O5, 5), ([O3, O3, 0, O5 | O6, 0, 0], O1 | O2 | O4, 2), ([O3, O3, 0, O6, O6, 0], O1 | O2 | O4 | O5, 2),
    ([O2, 0, O6, O6, O6, 0], O1 | O3 | O4 | O5, 5), ([0, O3, 0, O6, O6, 0], O1 | O2 | O4 | O5, 5),
    ([0, 0, O6, O6, O6, 0], O1 | O2 | O3 | O4 | O5, 5), ([0, 0, 0, O6, O6, 0], O1 | O2 | O3 | O4 | O5, 5),
    ([0, O3, 0, O5 | O6, 0, 0], O1 | O2 | O4, 5), ([0, O3, 0, O5 | O6, 0, 0], O1 | O2 | O4, 2), ([O2, 0, O4, O5, 0, 0], O1 | O3 | O6, 4),
    ([0, 0, O4, 0, O6, 0], O1 | O2 | O3 | O5, 5), ([0, 0, O4, O5, 0, 0], O1 | O2 | O3 | O6, 4), ([0, 0, 0, 0, O6, 0], O1 | O2 | O3 | O4 | O5, 5),
    ([0, 0, 0, 0, 0, 0], O1 | O2 | O3 | O4 | O5 | O6, 5),
]
_A, _B1, _B2 = 2, 4, 8
FM4_ALGOS = [([_A, 0, _B2, 0], 1 | _B1, 1), ([_A, _B1, _B2, 0], 1 | _A, 3), ([_A | _B1, 0, _B2, 0], 1 | _A, 3), ([_A, _B2, _B2, 0], 1 | _B1, 3),
             ([_A | _B1, 0, _B2, 0], 1 | _B1, 1), ([_B2, _B2, _B2, 0], 1 | _A | _B1, 3), ([_B1, _B2, 0, 0], 1 | _A, 3), ([0, 0, _B2, 0], 1 | _A | _B1, 3)]


def hero_fm_film(table, names, pid, retro_id):
    """Schéma de l'algorithme choisi : une vue par algorithme (bande d'images liée au réglage)."""
    frames = [hero_fm_graph(m, c, f, names, "ALG %02d" % (i + 1), retro_id) for i, (m, c, f) in enumerate(table)]
    h = dict(frames[0])
    h["film"] = {"p": pid, "frames": [{k: fr[k] for k in ("rects", "paths", "circles", "labels")} for fr in frames]}
    return h
