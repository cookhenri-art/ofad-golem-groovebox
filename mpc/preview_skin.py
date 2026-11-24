"""OFAD GOLEM — aperçu d'un habillage MPC (TUI.json) : une image 1280 × 800 par page, comme l'écran
de la machine (bandeau du haut, contenu de l'onglet, touches de fonction en bas). Les valeurs
affichées sont celles des réglages par défaut du greffon (build/params.json) quand il est fourni.
Usage : python3 preview_skin.py <Plugin Skins> <dossier de sortie> [params.json] [nom du greffon]"""
import json
import os
import sys
from PIL import Image, ImageDraw, ImageFont

FONT_DIR = "/usr/share/fonts/truetype/google-fonts"


def F(size, w="Medium"):
    return ImageFont.truetype(os.path.join(FONT_DIR, "Poppins-%s.ttf" % w), int(size))


def col(s):
    if not s or s == "0":
        return (0, 0, 0, 0)
    v = int(s, 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255, (v >> 24) & 255)


def rect(s):
    return [float(x) for x in s.split()]


def render(skin_dir, out_dir, params_path=None, plugin_name=""):
    tui = json.load(open(os.path.join(skin_dir, "TUI.json")))["pageData"]
    defs = {d["key"]: d["value"] for d in tui["componentDefinitions"]["localComponentDefinitions"]}
    repl = {r["key"]: r["value"] for r in tui.get("replacementStrings", [])}
    params = {}
    if params_path and os.path.isfile(params_path):
        for p in json.load(open(params_path))["params"]:
            params["Parameter %d" % p["index"]] = p
    os.makedirs(out_dir, exist_ok=True)
    cache = {}

    def image(name):
        if name not in cache:
            cache[name] = Image.open(os.path.join(skin_dir, name)).convert("RGBA")
        return cache[name]

    def norm(h):
        if h.startswith("Local/"):
            return 0.0
        p = params.get(h)
        return p["default"] if p else 0.66

    def visible(e):
        for inv in e["bounds"].get("additionalInvalidatingHandles", []):
            if inv.startswith("IndexedEnabling/"):
                _, i, n, h = inv.split("/", 3)
                if round(norm(h) * (int(n) - 1)) != int(i):
                    return False
        return True

    def TF(ts):
        f = ts["font"]
        if f.get("name") == "Titillium Web":
            st = f.get("style", "Regular")
            st = st if st in ("SemiBold", "Bold") else "SemiBold"
            return ImageFont.truetype(os.path.join(os.path.dirname(os.path.abspath(__file__)), "mockskin", "fonts",
                                                   "TitilliumWeb-%s.ttf" % st), max(8, int(f["height"] / 1.521)))
        return F(f["height"] * 0.86, "Medium")

    def display(h):
        p = params.get(h)
        if not p:
            return "120 Hz"
        if h in repl and p["kind"] == "choice":
            n = len(repl[h])
            return repl[h][min(n - 1, round(p["default"] * (n - 1)))]
        return p["display"]

    def draw_entry(img, e, ox, oy, handle):
        cd = e["componentData"]
        if not visible(e):
            return
        for kv in e.get("handle remapping", {}).get("map", []):
            if kv["key"] == "Data":
                handle = kv["value"]
        b = rect(e["bounds"]["bounds"])
        x, y, w, h = ox + b[0], oy + b[1], b[2], b[3]
        t, data = cd["type"], cd["data"]
        d = ImageDraw.Draw(img)
        if t in defs:
            for ch in defs[t]["componentsData"]:
                draw_entry(img, ch, x, y, handle)
        elif t == "Knob" and data.get("knobType") == "FilmStrip":
            st = image(data["filmStrip"])
            n = data["numFrames"]
            fh = st.height // n
            k = max(0, min(n - 1, round(norm(handle) * (n - 1))))
            img.alpha_composite(st.crop((0, fh * k, st.width, fh * k + fh)).resize((int(w), int(h))), (int(x), int(y)))
        elif t == "Knob" and data.get("knobType") == "ValueSlider":
            ts = data["textStyle"]
            d.text((x + (w) / 2, y + h / 2), display(handle), font=F(ts["font"]["height"] * 0.9, "Bold"), fill=col(ts["colour"]), anchor="mm")
        elif t == "Button":
            n = max(2, data["numButtonsInGroup"])
            hn = data.get("handleName", "Data")
            step = round(norm(handle if hn == "Data" else hn) * (n - 1))
            lit = step == data["buttonId"]
            im = image(data["onImage"] if lit else data["offImage"])
            img.alpha_composite(im, (int(x + (w - im.width) / 2), int(y + (h - im.height) / 2)))
        elif t == "Image" and data.get("imageType") == "FilmStrip":
            st = image(data["image"])
            n = data["numFrames"]
            fh = st.height // n
            k = max(0, min(n - 1, round(norm(handle) * (n - 1))))
            img.alpha_composite(st.crop((0, fh * k, st.width, fh * k + fh)).resize((int(w), int(h))), (int(x), int(y)))
        elif t == "Image":
            img.alpha_composite(image(data["image"]).resize((int(w), int(h))), (int(x), int(y)))
        elif t == "XYPad":
            pk = image(data["image"])
            vx, vy = norm(data["xPositionHandle"]), norm(data["yPositionHandle"])
            img.alpha_composite(pk, (int(x + w * vx - pk.width / 2), int(y + h * (1 - vy) - pk.height / 2)))
        elif t == "Label" and data["type"] == "Value":
            ts = data["textStyle"]
            f = TF(ts)
            txt = display(handle)
            while len(txt) > 3 and d.textlength(txt, font=f) > w:
                txt = txt[:-2] + "."
            if "left" in ts["justification"]:
                d.text((x, y + h / 2), txt, font=f, fill=col(ts["colour"]), anchor="lm")
            elif "right" in ts["justification"]:
                d.text((x + w, y + h / 2), txt, font=f, fill=col(ts["colour"]), anchor="rm")
            else:
                d.text((x + w / 2, y + h / 2), txt, font=f, fill=col(ts["colour"]), anchor="mm")
        elif t == "Slider":
            v = norm(handle)
            d.rounded_rectangle([x, y, x + w, y + h], radius=4, fill=col(data["backgroundColour"]), outline=col(data["outlineColour"]))
            d.rounded_rectangle([x + 2, y + 2 + (h - 4) * (1 - v), x + w - 2, y + h - 2], radius=3, fill=col(data["valueColour"]))
        elif t == "Envelope":
            d.rectangle([x, y, x + w, y + h], fill=col(data["backgroundColour"]))
            for i in range(1, 4):
                d.line([(x, y + h * i / 4), (x + w, y + h * i / 4)], fill=col(data["scaleColour"]))
            a = norm(data["attackHandle"]) if data["attackHandle"] in params else 0.1
            dd = norm(data["decayHandle"]) if data["decayHandle"] in params else 0.4
            sh = data["sustainHandle"]
            su = norm(sh) if sh in params else 1.0
            r = norm(data["releaseHandle"]) if data["releaseHandle"] in params else 0.3
            seg = (w - 30) / 3.6
            x0, y0, ytop = x + 12, y + h - 14, y + 18
            pa = (x0 + seg * max(0.05, a), ytop)
            ys = ytop + (y0 - ytop) * (1 - su)
            pd = (pa[0] + seg * max(0.08, dd), ys)
            ps = (pd[0] + seg * 0.6, ys)
            pr = (min(x + w - 12, ps[0] + seg * max(0.08, r)), y0)
            pts = [(x0, y0), pa, pd, ps, pr]
            fc = col(data["fillColour"])
            ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
            ImageDraw.Draw(ov).polygon(pts, fill=fc)
            img.alpha_composite(ov)
            d.line(pts, fill=col(data["lineColour"]), width=3)
            for px, py in pts[1:]:
                rr = data["handleWidth"] / 2.6
                d.ellipse([px - rr, py - rr, px + rr, py + rr], fill=col(data["handleFillColour"]), outline=col(data["handleLineColour"]), width=2)

    tabs = tui["tabs"]
    keys = []
    for t in tabs:
        if t["fnKeyIndex"] >= len(keys):
            keys.extend([None] * (t["fnKeyIndex"] + 1 - len(keys)))
        if keys[t["fnKeyIndex"]] is None:
            keys[t["fnKeyIndex"]] = [t["tabName"], 0]
        keys[t["fnKeyIndex"]][1] += 1
    for tab in tabs:
        root = tab["componentDefinition"]
        img = Image.new("RGBA", (1280, 628), (0, 0, 0, 255))
        bg = root["backgroundData"]["unfocussed"]["image"]
        if bg:
            img.alpha_composite(image(bg))
        for e in root["componentsData"]:
            draw_entry(img, e, 0, 0, "Data")
        full = Image.new("RGB", (1280, 800), (18, 18, 20))
        full.paste(img.convert("RGB"), (0, 110))
        d = ImageDraw.Draw(full)
        # bandeau du haut (piste, greffon, préréglage)
        d.rectangle([0, 0, 1280, 108], fill=(30, 30, 33))
        for x0, lab, val in ((24, "TRACK", "Plugin 001"), (440, "PLUGIN", plugin_name), (860, "PRESET", "Init")):
            d.text((x0, 26), lab, font=F(17), fill=(150, 150, 156))
            d.text((x0, 50), val, font=F(34), fill=(240, 240, 244))
            d.line([(x0, 96), (x0 + 380, 96)], fill=(70, 70, 76), width=2)
        # touches de fonction
        d.rectangle([0, 738, 1280, 800], fill=(24, 24, 27))
        n = max(5, len(keys))
        kw = 1280 / min(6, n)
        for i, k in enumerate(keys[:6]):
            if k is None:
                continue
            x0 = i * kw
            on = i == tab["fnKeyIndex"]
            d.line([(x0 + 18, 742), (x0 + kw - 18, 742)], fill=(232, 31, 64) if on else (80, 80, 86), width=4)
            d.text((x0 + kw / 2, 770), k[0], font=F(24, "Bold" if on else "Medium"), fill=(245, 245, 248) if on else (170, 170, 176), anchor="mm")
            if k[1] > 1:
                for s in range(k[1]):
                    cx = x0 + kw / 2 + (s - (k[1] - 1) / 2) * 12
                    lit = on and s == tab["fnKeySubIndex"]
                    d.ellipse([cx - 3, 788, cx + 3, 794], fill=(245, 245, 248) if lit else (100, 100, 106))
        full.save(os.path.join(out_dir, "page_%d_%d.png" % (tab["fnKeyIndex"], tab["fnKeySubIndex"])))


if __name__ == "__main__":
    render(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None, sys.argv[4] if len(sys.argv) > 4 else "")
