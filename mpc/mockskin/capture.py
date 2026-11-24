#!/usr/bin/env python3
"""OFAD GOLEM — capture des maquettes en images d'habillage MPC — AGPL-3.0-or-later

Pour chaque écran de maquette (render.py) :
  - fond : la page entière, les éléments vivants masqués (boutons rotatifs, curseurs, choix, valeurs) ;
  - bandes d'images (film strips, 61 vues) des boutons rotatifs, curseurs et barres, découpées dans
    le rendu même de la maquette (mêmes dégradés, mêmes ombres) en faisant varier la position ;
  - images allumé / éteint de chaque bouton de choix ;
  - emplacements et styles des valeurs que la MPC écrit en direct.
Écrit <dossier>/<écran>.json (description lue par skins.py) et les PNG à côté.
"""
import hashlib
import math
import json
import os
import re
import sys

from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import render as RD  # noqa: E402

FRAMES = 61
LIVE = "[data-k=knob],[data-k=cknob],[data-k=cfader],[data-k=vbar],[data-k=radio],[data-k=nav],[data-k=xy],[data-k=led],[data-k=puck]"

JS_INFO = r"""
() => {
  const R = e => { const r = e.getBoundingClientRect(); return [r.left, r.top, r.width, r.height]; };
  const items = [...document.querySelectorAll('[data-k]')].map((e, i) => {
    e.dataset.mkid = i;
    const cs = getComputedStyle(e);
    return {i, k: e.dataset.k, d: {...e.dataset}, r: R(e)};
  });
  const vals = [...document.querySelectorAll('[data-v]')].filter(e => e.dataset.v).map(e => {
    const cs = getComputedStyle(e);
    let r = R(e);
    const p = e.parentElement.getBoundingClientRect();
    const j = e.dataset.j || 'center';
    if (e.dataset.abs) { /* position donnée par la maquette */ }
    else if (j == 'right') { const x0 = p.left + p.width * 0.42; r = [x0, r[1], r[0] + r[2] - x0, r[3]]; }
    else if (j == 'left') { r = [r[0], r[1], Math.max(r[2], p.right - r[0] - 4), r[3]]; }
    else if (r[2] < 92 && !e.closest('[data-k]')) { const c = r[0] + r[2] / 2; r = [c - 48, r[1], 96, r[3]]; }
    const host = e.closest('[data-k=knob],[data-k=cknob],[data-k=cfader],[data-k=vbar],[data-k=chip]');
    const zz = e.closest('[data-zoom]');
    return {p: e.dataset.v, j, r, fs: parseFloat(cs.fontSize) * (zz ? parseFloat(zz.dataset.zoom) : 1), col: cs.color, fw: parseInt(cs.fontWeight),
            style: e.dataset.fw || '', host: host ? parseInt(host.dataset.mkid) : -1};
  });
  return {items, vals};
}
"""

JS_HIDE = r"""
(sel) => {
  document.querySelectorAll(sel).forEach(e => e.style.visibility = 'hidden');
  document.querySelectorAll('[data-v]').forEach(e => { if (e.dataset.v) e.style.color = 'transparent'; });
}
"""

JS_GRAB = r"""
(id) => {
  const e = document.querySelector(`[data-mkid="${id}"]`);
  const cs = getComputedStyle(e);
  return {html: e.outerHTML, ff: cs.fontFamily, fs: cs.fontSize, fw: cs.fontWeight, fst: cs.fontStretch, col: cs.color};
}
"""

# une vue par position : t de 0 à 1 (bande d'images) ; style allumé / éteint (boutons)
JS_FRAMES = r"""
([kind, n, on, off]) => {
  const cells = [...document.querySelectorAll('.mkcell > *')];
  cells.forEach((c, i) => {
    c.querySelectorAll('[data-v]').forEach(v => v.style.color = 'transparent');
    const t = n > 1 ? i / (n - 1) : 0;
    if (kind == 'knob') {
      const a = (270 * t).toFixed(2);
      c.style.background = `conic-gradient(from 225deg,${c.dataset.col} 0 ${a}deg,${c.dataset.track} ${a}deg 270deg,transparent 270deg)`;
    } else if (kind == 'cknob') {
      c.firstElementChild.style.transform = `rotate(${(270 * t - 135).toFixed(2)}deg)`;
    } else if (kind == 'cfader') {
      const h = parseFloat(c.dataset.h);
      c.children[1].style.bottom = `${(t * (h - 30)).toFixed(2)}px`;
    } else if (kind == 'vbar') {
      c.firstElementChild.style.height = `${(100 * t).toFixed(2)}%`;
    } else if (kind == 'led') {
      c.style.background = i == 0 ? on : off;
    } else if (kind == 'btn') {
      const s = (i == 0 ? on : off).split('|');
      c.style.background = s[0]; c.style.color = s[1]; c.style.borderColor = s[2];
      if (s[0] != 'transparent' && c.style.border && c.style.border.startsWith('0')) c.style.border = '0';
    }
  });
}
"""


def colour(css):
    m = re.match(r"rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)(?:,\s*([\d.]+))?\)", css)
    if not m:
        return [238, 238, 242, 255]
    a = float(m.group(4)) if m.group(4) is not None else 1.0
    return [int(float(m.group(1))), int(float(m.group(2))), int(float(m.group(3))), int(round(a * 255))]


def canon(htm):
    s = re.sub(r'\sdata-[\w-]+="[^"]*"', "", htm)
    s = re.sub(r"0 \d+(\.\d+)?deg,([^ ]+) \d+(\.\d+)?deg", "0 Xdeg,\\2 Xdeg", s)
    s = re.sub(r"rotate\(-?\d+(\.\d+)?deg\)", "rotate(X)", s)
    s = re.sub(r"bottom:\s*-?\d+(\.\d+)?px", "bottom:X", s)
    s = re.sub(r"height:\s*\d+(\.\d+)?%", "height:X", s)
    s = re.sub(r">[^<]*<", "><", s)
    return s


class Capture:
    def __init__(self, out_dir, prefix):
        self.out = out_dir
        self.prefix = prefix
        os.makedirs(out_dir, exist_ok=True)
        self.pw = sync_playwright().start()
        self.browser = self.pw.chromium.launch()
        self.page = self.browser.new_page(viewport={"width": 1280, "height": 628}, device_scale_factor=1)
        self.sprites = {}  # signature → nom de fichier

    def close(self):
        self.browser.close()
        self.pw.stop()

    def _load(self, htm):
        self.page.set_viewport_size({"width": 1280, "height": 628})
        self.page.set_content(htm, wait_until="load")
        self.page.evaluate("document.fonts.ready.then(() => true)")
        self.page.wait_for_timeout(60)

    def _stage(self, grabs, w, h, n):
        """Page de travail : n copies de l'élément, l'une sous l'autre, cellules de w × h."""
        g = grabs
        cell = ('<div class="mkcell" style="width:%dpx;height:%dpx;display:flex;align-items:center;justify-content:center;'
                'font-family:%s;font-size:%s;font-weight:%s;font-stretch:%s;color:%s;white-space:nowrap">%s</div>')
        inner = re.sub(r'^(<\w+)', r'\1 data-mkclone="1"', g["html"], count=1)
        body = "".join(cell % (w, h, g["ff"].replace('"', "'"), g["fs"], g["fw"], g["fst"], g["col"], inner) for _ in range(n))
        htm = RD.page_html("", {}, ".mkcell>*{flex:none!important;margin:0!important;width:%dpx!important;height:%dpx!important}" % (w, h))
        htm = htm.replace("<body></body>", '<body><div id="st" style="width:%dpx">%s</div></body>' % (w, body))
        self.page.set_viewport_size({"width": max(1280, w + 20), "height": max(628, h * n + 20)})
        self.page.set_content(htm, wait_until="load")
        self.page.evaluate("document.fonts.ready.then(() => true)")

    def strip(self, src_html, item, kind):
        """Bande de 61 vues d'un bouton rotatif / curseur / barre, partagée entre éléments identiques."""
        x, y, w, h = item["r"]
        W, H = round(w), round(h)
        g = item["grab"]
        sig = hashlib.sha1(("%s|%d|%d|%s" % (kind, W, H, canon(g["html"]))).encode()).hexdigest()[:10]
        if sig in self.sprites:
            return self.sprites[sig]
        name = "%s_%s_%s.png" % (self.prefix, kind, sig)
        self._stage(g, W, H, FRAMES)
        self.page.evaluate(JS_FRAMES, [kind, FRAMES, "", ""])
        self.page.locator("#st").screenshot(path=os.path.join(self.out, name), omit_background=True)
        self.sprites[sig] = name
        return name

    def button(self, item):
        x, y, w, h = item["r"]
        W, H = math.ceil(w - 0.05), math.ceil(h - 0.05)
        g = item["grab"]
        d = item["d"]
        sig = hashlib.sha1(("btn|%d|%d|%s|%s|%s" % (W, H, g["html"], d.get("on"), d.get("off"))).encode()).hexdigest()[:10]
        if sig in self.sprites:
            return self.sprites[sig]
        on, off = "%s_bt_%s_on.png" % (self.prefix, sig), "%s_bt_%s_off.png" % (self.prefix, sig)
        self._stage(g, W, H, 2)
        self.page.evaluate(JS_FRAMES, ["btn", 2, d.get("on", ""), d.get("off", "")])
        cells = self.page.locator(".mkcell")
        cells.nth(0).screenshot(path=os.path.join(self.out, on), omit_background=True)
        cells.nth(1).screenshot(path=os.path.join(self.out, off), omit_background=True)
        self.sprites[sig] = (on, off)
        return on, off

    def puck(self, name):
        """Image de la rondelle d'un pavé XY : l'élément data-k=puck seul, centré sur son point (data-cx/cy)."""
        box = self.page.evaluate(r"""
        () => {
          const p = document.querySelector('[data-k=puck]');
          const r0 = p.getBoundingClientRect();
          const z = p.closest('[data-zoom]'); const k = z ? parseFloat(z.dataset.zoom) : 1;
          const cx = r0.left + parseFloat(p.dataset.cx) * k, cy = r0.top + parseFloat(p.dataset.cy) * k;
          let half = 0;
          p.querySelectorAll('ellipse,circle,path,line,rect').forEach(e => {
            const r = e.getBoundingClientRect();
            half = Math.max(half, Math.abs(r.left - cx), Math.abs(r.right - cx), Math.abs(r.top - cy), Math.abs(r.bottom - cy));
          });
          half = Math.ceil(half + 4);
          document.body.firstElementChild.style.visibility = 'hidden';
          p.style.visibility = 'visible';
          return [cx - half, cy - half, 2 * half];
        }""")
        fn = "%s_%s_puck.png" % (self.prefix, name)
        self.page.screenshot(path=os.path.join(self.out, fn), clip={"x": box[0], "y": box[1], "width": box[2], "height": box[2]},
                             omit_background=True)
        self.page.evaluate("() => { document.body.firstElementChild.style.visibility = ''; document.querySelector('[data-k=puck]').style.visibility = 'hidden'; }")
        return fn

    def region_frames(self, body_tpl, Ds, box, name):
        """Une vue par fiche D (même gabarit), découpée dans la zone box : bande d'images verticale."""
        from PIL import Image
        x, y, w, h = box
        frames = []
        for i, D in enumerate(Ds):
            self._load(RD.page_html(body_tpl, D))
            self.page.evaluate(JS_HIDE, LIVE)
            tmp = os.path.join(self.out, "_f%d.png" % i)
            self.page.screenshot(path=tmp, clip={"x": x, "y": y, "width": w, "height": h})
            frames.append(Image.open(tmp).convert("RGBA"))
            os.remove(tmp)
        strip = Image.new("RGBA", (w, h * len(frames)))
        for i, f in enumerate(frames):
            strip.paste(f, (0, i * h))
        fn = "%s_%s_film.png" % (self.prefix, name)
        strip.save(os.path.join(self.out, fn))
        return fn

    def screen(self, body_tpl, D, name, regions=None):
        """Capture un écran. regions : zones (x, y, w, h) à découper aussi dans le fond (états d'une page)."""
        htm = RD.page_html(body_tpl, D)
        self._load(htm)
        info = self.page.evaluate(JS_INFO)
        for it in info["items"]:
            if it["k"] in ("knob", "cknob", "cfader", "vbar", "radio", "nav", "led"):
                it["grab"] = self.page.evaluate(JS_GRAB, it["i"])
        self.page.evaluate(JS_HIDE, LIVE)
        bg = "%s_%s_bg.png" % (self.prefix, name)
        self.page.screenshot(path=os.path.join(self.out, bg), clip={"x": 0, "y": 0, "width": 1280, "height": 628})
        crops = []
        for k, (rx, ry, rw, rh) in enumerate(regions or []):
            cn = "%s_%s_reg%d.png" % (self.prefix, name, k)
            self.page.screenshot(path=os.path.join(self.out, cn), clip={"x": rx, "y": ry, "width": rw, "height": rh})
            crops.append(cn)
        puck = self.puck(name) if any(it["k"] == "puck" for it in info["items"]) else None
        widgets = []
        for it in info["items"]:
            k = it["k"]
            x, y, w, h = it["r"]
            box = [round(x), round(y), round(w), round(h)]
            d = it["d"]
            if k in ("knob", "cknob", "cfader", "vbar"):
                strip = self.strip(htm, it, k)
                widgets.append({"t": k, "p": d.get("p"), "box": box, "strip": strip, "frames": FRAMES, "q": int(d.get("q", -1) or -1),
                                "slot": int(d.get("slot", -1) or -1), "mkid": it["i"]})
            elif k in ("radio", "nav"):
                on, off = self.button(it)
                widgets.append({"t": k, "p": d.get("p"), "box": box, "on": on, "off": off, "i": int(d.get("i", 0)),
                                "n": int(d.get("n", 0) or 0), "mkid": it["i"]})
            elif k == "led":
                g = it["grab"]
                W, H = math.ceil(w - 0.05), math.ceil(h - 0.05)
                sig = hashlib.sha1(("led|%d|%d|%s|%s" % (W, H, d.get("on"), d.get("off"))).encode()).hexdigest()[:10]
                name = self.sprites.get(sig)
                if not name:
                    name = "%s_led_%s.png" % (self.prefix, sig)
                    self._stage(g, W, H, 2)
                    self.page.evaluate(JS_FRAMES, ["led", 2, d.get("on"), d.get("off")])
                    self.page.locator("#st").screenshot(path=os.path.join(self.out, name), omit_background=True)
                    self.sprites[sig] = name
                widgets.append({"t": "img", "p": d.get("p"), "box": box, "strip": name, "frames": 2, "mkid": it["i"]})
            elif k == "chip":
                widgets.append({"t": "chip", "p": d.get("p"), "box": box, "mkid": it["i"]})
            elif k in ("hero", "macrorow"):
                widgets.append({"t": k, "box": box})
            elif k == "xy":
                widgets.append({"t": "xy", "box": box, "px": d.get("px"), "py": d.get("py"), "mkid": it["i"], "puck": puck})
        labels = []
        for v in info["vals"]:
            x, y, w, h = v["r"]
            st = v["style"] or ("Bold" if v["fw"] >= 700 else ("SemiBold" if v["fw"] >= 600 else "Regular"))
            labels.append({"p": v["p"], "box": [round(x), round(y), round(w), round(h)], "fs": v["fs"], "col": colour(v["col"]),
                           "j": v["j"], "style": st, "host": v["host"]})
        return {"bg": bg, "widgets": widgets, "labels": labels, "regions": crops}


def board(path):
    return json.load(open(path, encoding="utf-8"))
