#!/usr/bin/env python3
"""
OFAD GOLEM for MPC & Force — capture des façades GOLEM pour l'habillage des greffons.

Ouvre chaque façade dans l'aperçu de l'interface (moteur simulé, sans son), page par page, et enregistre :
  - chaque panneau (section) de la façade en image, les parties qui bougent cachées (aiguilles, curseurs,
    voyants, valeurs) ;
  - la place de chaque réglage dans le panneau ;
  - les images animées (« filmstrips ») des réglages : bouton rotatif à toutes ses positions, tirette à toutes
    ses hauteurs, bouton à bascule éteint / allumé, sélecteur sur chacun de ses choix ;
  - les couleurs du thème (fond, texte des valeurs).
Les panneaux sans réglage (séquenceur à pas, écrans particuliers) sont laissés de côté.

Prérequis : « npm run build » dans ui/ (dossier ui/dist), Playwright pour Python.
Usage : python3 mpc/face_capture.py <dossier de sortie> [type…]
"""
import functools
import http.server
import json
import os
import re
import shutil
import sys
import tempfile
import threading

from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
DIST = os.path.join(ROOT, "ui", "dist")
CATALOG = os.path.join(ROOT, "ui", "src", "engine", "catalog.json")
INST_BASE = 16          # adresse du premier réglage d'instrument de la piste 1 (moteur simulé)
FRAMES = 49             # images des boutons rotatifs et des tirettes
SCALE = 2               # capture en double définition, réduite ensuite

HIDE_MOVING = """
.cap-hide .encoder-ring, .cap-hide .encoder-cap, .cap-hide .knob-value,
.cap-hide .fc-fader-cap, .cap-hide .fc-fader-value, .cap-hide .fc-toggle,
.cap-hide .fc-switch-opts, .cap-hide .fc-menu-value, .cap-hide .fc-menu-box button { visibility: hidden !important; }
.fp-copy-wrap { display: none !important; }
.cap-frame .knob-label, .cap-frame .knob-value, .cap-frame .fc-fader-value { visibility: hidden !important; }
"""

ISOLATE = """() => {
  const page = document.querySelector('.fp-page');
  if (!page) return null;
  let e = page;
  while (e && e !== document.body) {
    const par = e.parentElement; if (!par) break;
    for (const sib of Array.from(par.children)) if (sib !== e && sib.tagName !== 'STYLE') sib.style.display = 'none';
    e.style.overflow = 'visible'; e.style.maxHeight = 'none'; e.style.height = 'auto'; e.style.flex = 'none';
    e = par;
  }
  page.querySelectorAll('*').forEach(x => { if (getComputedStyle(x).overflow !== 'visible') x.style.overflow = 'visible'; });
  for (const x of [document.documentElement, document.body, document.getElementById('root')]) { if (x) { x.style.height = 'auto'; x.style.overflow = 'visible'; } }
  return true;
}"""

# description des panneaux et des réglages, coordonnées de la page
DESCRIBE = """() => {
  const R = (el) => { if (!el) return null; const r = el.getBoundingClientRect(); return [r.x, r.y, r.width, r.height]; };
  const U = (rs) => { rs = rs.filter(Boolean); if (!rs.length) return null;
    const x0 = Math.min(...rs.map(r => r[0])), y0 = Math.min(...rs.map(r => r[1]));
    const x1 = Math.max(...rs.map(r => r[0] + r[2])), y1 = Math.max(...rs.map(r => r[1] + r[3]));
    return [x0, y0, x1 - x0, y1 - y0]; };
  const col = (el) => el ? getComputedStyle(el).color : null;
  const fs = (el) => el ? parseFloat(getComputedStyle(el).fontSize) : null;
  const out = [];
  document.querySelectorAll('.fp-page .fp').forEach((fp, pi) => {
    fp.setAttribute('data-cap-panel', pi);
    const title = fp.querySelector('.fp-title')?.textContent?.trim() || '';
    const ctls = [];
    fp.querySelectorAll('[data-ctl]').forEach((z, ci) => {
      const addr = Number(z.getAttribute('data-ctl'));
      const tag = 'p' + pi + 'c' + ci; z.setAttribute('data-cap', tag);
      const enc = z.classList.contains('encoder') ? z : z.querySelector('.encoder');
      const fad = z.querySelector('.fc-fader');
      const tog = z.querySelector('.fc-toggle');
      const sw = z.querySelector('.fc-switch');
      const menu = z.querySelector('.fc-menu-box');
      let c = { tag, addr, label: z.getAttribute('data-ctl-label') || '' };
      if (fad) {
        const tr = fad.querySelector('.fc-fader-track');
        c.kind = 'fader'; c.rect = U([R(tr), [R(fad)[0], R(tr)[1], R(fad)[2], R(tr)[3]]]);
        const v = fad.querySelector('.fc-fader-value'); c.value = R(v); c.vcol = col(v); c.vfs = fs(v);
      } else if (sw) {
        const opts = sw.querySelector('.fc-switch-opts');
        c.kind = 'switch'; c.rect = R(opts); c.n = opts.querySelectorAll('button').length;
      } else if (tog) {
        c.kind = 'toggle'; c.rect = U([R(tog), R(z)].concat(Array.from(tog.querySelectorAll('*')).map(R)));
      } else if (menu) {
        c.kind = 'menu'; c.rect = R(menu); const v = menu.querySelector('.fc-menu-value'); c.value = R(v); c.vcol = col(v); c.vfs = fs(v);
      } else if (enc) {
        c.kind = 'knob'; c.rect = U([R(enc.querySelector('.encoder-ring')), R(enc.querySelector('.encoder-cap'))]);
        const v = enc.querySelector('.knob-value'); c.value = R(v); c.vcol = col(v); c.vfs = fs(v);
      } else return;
      if (!c.rect || c.rect[2] < 2 || c.rect[3] < 2 || c.rect[0] < 0 || c.rect[1] < 0) return;
      ctls.push(c);
    });
    out.push({ index: pi, title, rect: R(fp), ctls });
  });
  return out;
}"""

SET_STATE = """([tag, kind, v, n]) => {
  const z = document.querySelector('[data-cap="' + tag + '"]');
  if (kind === 'knob') { const cap = z.querySelector('.encoder-cap'); const a = -135 + 270 * v;
    cap.style.transform = 'rotate(' + a + 'deg)'; cap.style.setProperty('--rot', a + 'deg');
    const arc = z.querySelector('.knob-value-arc, .encoder-ring .knob-fill'); if (arc) arc.style.display = 'none'; }
  else if (kind === 'fader') { const cap = z.querySelector('.fc-fader-cap'); cap.style.bottom = 'calc(' + v + ' * (100% - 14px))'; }
  else if (kind === 'toggle') { const b = z.querySelector('.fc-toggle'); b.classList.toggle('on', v > 0.5); b.setAttribute('aria-pressed', v > 0.5 ? 'true' : 'false'); }
  else if (kind === 'switch') { z.querySelectorAll('.fc-switch-opts button').forEach((b, i) => { b.classList.toggle('on', i === n); b.setAttribute('aria-checked', i === n ? 'true' : 'false'); }); }
}"""


def serve(site):
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *a):
            pass
    handler = functools.partial(Quiet, directory=site)
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd


def demo_name(t):
    return "c" + "".join(ch for ch in t.lower() if ch.isalnum())


def make_site():
    site = tempfile.mkdtemp(prefix="golem-face-")
    shutil.rmtree(site)
    shutil.copytree(DIST, site)
    cat = json.load(open(CATALOG, encoding="utf-8"))
    for i in cat["instruments"]:
        with open(os.path.join(site, "demo-%s.gbx" % demo_name(i["type"])), "w", encoding="utf-8") as f:
            f.write('<?xml version="1.0" encoding="UTF-8"?>\n<GROOVEBOX><PROJECT><TRACK i="0" name="%s" color="0" '
                    'instrument="%s" preset=""/></PROJECT></GROOVEBOX>\n' % (i["name"].replace('"', ""), i["type"]))
    return site, cat


def crop(img_path, rect, origin, out_path):
    from PIL import Image
    im = Image.open(img_path)
    x, y, w, h = [v * SCALE for v in (rect[0] - origin[0], rect[1] - origin[1], rect[2], rect[3])]
    im.crop((int(round(x)), int(round(y)), int(round(x + w)), int(round(y + h)))).save(out_path)


def capture(p, url, t, faces, cat, out):
    inst = [i for i in cat["instruments"] if i["type"] == t][0]
    ids = [q["id"] for q in inst["params"]]
    face = faces.get(t)
    if not face:
        return None
    d = os.path.join(out, t)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    result = {"type": t, "name": inst["name"], "theme": face.get("theme", ""), "pages": []}
    for pg_i, page in enumerate(face.get("pages", [])):
        p.goto(url + "/?demo=" + demo_name(t)); p.wait_for_timeout(1500)
        p.get_by_role("button", name=re.compile("ouvrir (le )?synth", re.I)).first.click(); p.wait_for_timeout(900)
        tabs = p.locator(".face-pages [role=tab], .face-pages button").filter(has_text=re.compile("^%s$" % re.escape(page["label"])))
        if tabs.count():
            tabs.first.click(); p.wait_for_timeout(700)
        elif pg_i > 0:
            continue
        p.add_style_tag(content=HIDE_MOVING)
        if not p.evaluate(ISOLATE):
            continue
        p.wait_for_timeout(500)
        panels = p.evaluate(DESCRIBE)
        panels = [x for x in panels if x["ctls"]]
        if not panels:
            continue
        page_el = p.locator(".fp-page").first
        pr = page_el.bounding_box()
        origin = (pr["x"], pr["y"])
        # couleur de fond de la façade (coin de la page)
        full = os.path.join(d, "_page%d.png" % pg_i)
        page_el.screenshot(path=full)
        # images animées, prises avant de cacher les parties mobiles
        strips = {}
        p.evaluate("() => document.querySelector('.fp-page').classList.add('cap-frame')")
        for pn in panels:
            for c in pn["ctls"]:
                kind = c["kind"]
                if kind == "menu":
                    continue
                w, h = c["rect"][2], c["rect"][3]
                key = None
                if kind in ("knob", "fader"):
                    key = "%s_%d_%d" % (kind, round(w), round(h))     # mêmes dimensions : même image
                if key and key in strips:
                    c["strip"] = strips[key]
                    continue
                n = FRAMES if kind in ("knob", "fader") else (2 if kind == "toggle" else c["n"])
                frames = []
                for f in range(n):
                    v = f / (n - 1) if n > 1 else 0
                    p.evaluate(SET_STATE, [c["tag"], kind, v, f])
                    clip = {"x": c["rect"][0], "y": c["rect"][1], "width": w, "height": h}
                    fp = os.path.join(d, "_f.png")
                    try:
                        p.screenshot(path=fp, clip=clip, full_page=True)
                    except Exception:
                        frames = []
                        break
                    from PIL import Image
                    frames.append(Image.open(fp).copy())
                from PIL import Image
                if not frames:
                    continue
                fw, fh = frames[0].size
                sheet = Image.new("RGBA", (fw, fh * n))
                for i, im in enumerate(frames):
                    sheet.paste(im, (0, i * fh))
                name = "strip_%s.png" % (key or "g%d%s" % (pg_i, c["tag"]))
                sheet.save(os.path.join(d, name))
                info = {"png": name, "frames": n, "w": fw, "h": fh}
                if key:
                    strips[key] = info
                c["strip"] = info
        p.evaluate("() => document.querySelector('.fp-page').classList.remove('cap-frame')")
        # fond de chaque panneau, parties mobiles cachées
        p.evaluate("() => document.querySelector('.fp-page').classList.add('cap-hide')")
        p.wait_for_timeout(200)
        bgfull = os.path.join(d, "_bg%d.png" % pg_i)
        page_el.screenshot(path=bgfull)
        p.evaluate("() => document.querySelector('.fp-page').classList.remove('cap-hide')")
        for pn in panels:
            name = "panel_%d_%d.png" % (pg_i, pn["index"])
            crop(bgfull, pn["rect"], origin, os.path.join(d, name))
            pn["png"] = name
            for c in pn["ctls"]:
                k = c["addr"] - INST_BASE
                c["id"] = ids[k] if 0 <= k < len(ids) else None
                # coordonnées relatives au panneau
                c["rect"] = [c["rect"][0] - pn["rect"][0], c["rect"][1] - pn["rect"][1], c["rect"][2], c["rect"][3]]
                if c.get("value"):
                    c["value"] = [c["value"][0] - pn["rect"][0], c["value"][1] - pn["rect"][1], c["value"][2], c["value"][3]]
        from PIL import Image
        im = Image.open(full).convert("RGB").resize((200, 100))
        bgc = max(im.getcolors(20000), key=lambda t: t[0])[1]   # couleur la plus présente : le fond
        result["pages"].append({"label": page["label"], "bg": "#%02x%02x%02x" % bgc, "panels": panels})
    json.dump(result, open(os.path.join(d, "face.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    return result


def capture_fx(p, url, fx, out):
    """Effet : sa pédale (ou son rack) du pédalier GOLEM → thème (couleur du boîtier, bouton, image)."""
    d = os.path.join(out, fx["type"])
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    p.goto(url + "/"); p.wait_for_timeout(1500)
    p.get_by_role("button", name=re.compile("^effets$", re.I)).first.click(); p.wait_for_timeout(400)
    p.locator(".fx-chain-btns button", has_text="Pédalier").first.click(); p.wait_for_timeout(500)
    first = p.locator(".pb-board").first.locator(":scope > .pd, :scope > .rk").first
    first.locator(".pd-swap, .pd-add").first.click(force=True); p.wait_for_timeout(400)
    p.locator(".fxpick .ipick-card", has_text=fx["name"]).first.click(); p.wait_for_timeout(900)
    unit = p.locator(".pb-board").first.locator(":scope > .pd, :scope > .rk").first
    p.add_style_tag(content=".pd-swap,.pd-presets,.rk-swap,.rk-presets,.rk-open{display:none!important}")
    unit.screenshot(path=os.path.join(d, "unit.png"))
    # un bouton rotatif du boîtier, à toutes ses positions
    enc = unit.locator(".pd-knob .encoder, .rk-big .encoder, .rk-small .encoder, .encoder").first
    knob = None
    if enc.count():
        enc.evaluate("e => { e.setAttribute('data-cap', 'k0'); e.closest('.pd-knob, .rk-big, .rk-small, div')?.classList.add('cap-frame'); }")
        p.add_style_tag(content=HIDE_MOVING)
        r = enc.evaluate("""e => { const U = (rs) => { const x0 = Math.min(...rs.map(r => r.x)), y0 = Math.min(...rs.map(r => r.y));
            const x1 = Math.max(...rs.map(r => r.right)), y1 = Math.max(...rs.map(r => r.bottom)); return [x0, y0, x1 - x0, y1 - y0]; };
            window.scrollTo(0, 0);
            const pk = e.closest('.pd-knob');
            if (pk) { const r = pk.getBoundingClientRect(); const lab = pk.querySelector('.pd-lab');
              const bottom = lab ? lab.getBoundingClientRect().top : r.bottom;
              pk.querySelectorAll('.pd-lab, .pd-pos').forEach(x => x.style.visibility = 'hidden');
              const cap = e.querySelector('.encoder-cap'); const cr = cap ? cap.getBoundingClientRect() : r;
              const top = Math.min(r.y, cr.y) - 10;
              return [r.x, top, r.width, Math.max(bottom, cr.bottom) - top]; }
            const parts = [e.querySelector('.encoder-cap'), e.querySelector('.encoder-ring')].filter(x => x && x.getBoundingClientRect().width > 4 && x.getBoundingClientRect().width < 400).map(x => x.getBoundingClientRect());
            e.querySelectorAll('.knob-label, .knob-value').forEach(x => x.style.visibility = 'hidden');
            return parts.length ? U(parts) : null; }""")

        if r:
            from PIL import Image
            frames = []
            for f in range(FRAMES):
                p.evaluate(SET_STATE, ["k0", "knob", f / (FRAMES - 1), f])
                fp = os.path.join(d, "_f.png")
                p.screenshot(path=fp, clip={"x": r[0], "y": r[1], "width": r[2], "height": r[3]}, full_page=True)
                frames.append(Image.open(fp).copy())
            fw, fh = frames[0].size
            sheet = Image.new("RGBA", (fw, fh * FRAMES))
            for i, im in enumerate(frames):
                sheet.paste(im, (0, i * fh))
            sheet.save(os.path.join(d, "strip_knob.png"))
            knob = {"png": "strip_knob.png", "frames": FRAMES, "w": fw, "h": fh}
    from PIL import Image
    im = Image.open(os.path.join(d, "unit.png")).convert("RGB")
    w, h = im.size
    body = im.crop((w // 8, h // 3, w - w // 8, h - h // 4)).resize((60, 60))
    bg = max(body.getcolors(5000), key=lambda t: t[0])[1]
    res = {"type": fx["type"], "name": fx["name"], "fx": True, "pages": [], "unit": "unit.png",
           "bg": "#%02x%02x%02x" % bg, "knob": knob}
    json.dump(res, open(os.path.join(d, "face.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    return res


def main():
    out = sys.argv[1]
    only = sys.argv[2:]
    faces = json.load(open(os.path.join(HERE, "faces.json"), encoding="utf-8"))
    site, cat = make_site()
    httpd = serve(site)
    url = "http://127.0.0.1:%d" % httpd.server_address[1]
    if only and only[0] == "--fx":
        fxs = [e for e in cat["effects"] if len(only) == 1 or e["type"] in only[1:]]
        with sync_playwright() as pw:
            b = pw.chromium.launch()
            p = b.new_page(viewport={"width": 1920, "height": 1080}, device_scale_factor=SCALE)
            for fx in fxs:
                try:
                    r = capture_fx(p, url, fx, out)
                    print(fx["type"], r["bg"], "bouton" if r["knob"] else "-")
                except Exception as e:
                    print(fx["type"], "ÉCHEC", str(e)[:200])
            b.close()
        httpd.shutdown()
        shutil.rmtree(site, ignore_errors=True)
        return
    types = only or [i["type"] for i in cat["instruments"]]
    with sync_playwright() as pw:
        b = pw.chromium.launch()
        p = b.new_page(viewport={"width": 1700, "height": 3200}, device_scale_factor=SCALE)
        for t in types:
            try:
                r = capture(p, url, t, faces, cat, out)
                print(t, "pages", [(pg["label"], len(pg["panels"]), sum(len(x["ctls"]) for x in pg["panels"])) for pg in r["pages"]] if r else "-")
            except Exception as e:  # une façade ratée ne bloque pas les autres
                print(t, "ÉCHEC", str(e)[:200])
        b.close()
    httpd.shutdown()
    shutil.rmtree(site, ignore_errors=True)


if __name__ == "__main__":
    main()
