#!/usr/bin/env python3
"""Aperçu d'un écran capturé (fond + bandes d'images à la valeur par défaut + boutons + valeurs),
pour comparer à la maquette. Les valeurs sont écrites avec la police de la MPC (Titillium Web)."""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
CAT = json.load(open(os.path.join(HERE, "..", "..", "ui", "src", "engine", "catalog.json"), encoding="utf-8"))
_X = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "catalog_mpc.json")  # greffons propres à la MPC
if os.path.isfile(_X):
    _x = json.load(open(_X, encoding="utf-8"))
    for _g in ("instruments", "effects"):
        _have = {_m["type"] for _m in CAT[_g]}
        CAT[_g] += [_m for _m in _x.get(_g, []) if _m["type"] not in _have]
MODS = {m["type"]: m for m in CAT["instruments"] + CAT["effects"]}
sys.path.insert(0, HERE)
import maquettes_gen as G  # noqa: E402

LABEL_K = 1.62


def norm(p):
    ch = p.get("choices")
    d = p.get("def", 0) or 0
    if ch:
        return int(round(d)) / max(1, len(ch) - 1)
    lo, hi = p.get("min", 0), p.get("max", 1)
    return 0.5 if hi == lo else max(0.0, min(1.0, (d - lo) / (hi - lo)))


def compose(spec, t, img_dir, state_imgs=()):
    params = {p["id"]: p for p in MODS[t]["params"]}
    bg = Image.open(os.path.join(img_dir, spec["bg"])).convert("RGBA")
    for w in spec["widgets"]:
        x, y, bw, bh = w["box"]
        p = params.get(w.get("p") or "")
        if w["t"] in ("knob", "cknob", "cfader", "vbar") and p:
            st = Image.open(os.path.join(img_dir, w["strip"])).convert("RGBA")
            fh = st.height // w["frames"]
            f = round(norm(p) * (w["frames"] - 1))
            fr = st.crop((0, f * fh, st.width, (f + 1) * fh))
            bg.alpha_composite(fr, (x, y))
        elif w["t"] == "radio" and p:
            cur = int(round(p.get("def", 0) or 0))
            im = Image.open(os.path.join(img_dir, w["on"] if cur == w["i"] else w["off"])).convert("RGBA")
            bg.alpha_composite(im, (x, y))
        elif w["t"] == "nav":
            im = Image.open(os.path.join(img_dir, w["on"] if w["i"] == spec.get("nav", 0) else w["off"])).convert("RGBA")
            bg.alpha_composite(im, (x, y))
    d = ImageDraw.Draw(bg)
    for lb in spec["labels"]:
        p = params.get(lb["p"])
        if not p:
            continue
        txt = G.fmt(p)
        st = {"Bold": "Bold", "SemiBold": "SemiBold"}.get(lb["style"], "SemiBold")
        f = ImageFont.truetype(os.path.join(HERE, "fonts", "TitilliumWeb-%s.ttf" % st), max(8, round(lb["fs"] * LABEL_K / 1.521)))
        x, y, w, h = lb["box"]
        anchor = {"left": "lm", "right": "rm"}.get(lb["j"], "mm")
        px = x if lb["j"] == "left" else (x + w if lb["j"] == "right" else x + w / 2)
        d.text((px, y + h / 2), txt, font=f, fill=tuple(lb["col"]), anchor=anchor)
    return bg


if __name__ == "__main__":
    spec = json.load(open(sys.argv[1], encoding="utf-8"))
    compose(spec, sys.argv[2], os.path.dirname(sys.argv[1])).save(sys.argv[3])
