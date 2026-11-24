#!/usr/bin/env python3
"""OFAD GOLEM — rendu des maquettes A / B / C en pages d'écran MPC (1280 × 628) — AGPL-3.0-or-later

Les maquettes validées (maquettes.py → maquettes_gen.py) décrivent chaque écran par un gabarit HTML
(A_BODY, B_BODY, C_BODY) et une fiche de données D. Ce module :
  - applique le gabarit à D (mini-moteur des balises {{…}}, <sc-for>, <sc-if> du canvas) ;
  - retire le bandeau et les onglets simulés de la MPC (la vraie MPC les dessine elle-même) et
    ramène la page à la zone de contenu d'un onglet, 1280 × 628 ;
  - charge les polices de la maquette depuis mockskin/fonts (aucun accès réseau).
Les éléments vivants portent data-k (bouton, curseur, choix), data-v (valeur écrite par la MPC).
"""
import html as _html
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import maquettes_gen as G  # noqa: E402

FONTS = os.path.join(HERE, "fonts")
FACES = [
    ("Space Grotesk", "SpaceGrotesk[wght].ttf", "300 700", "normal"),
    ("Archivo", "Archivo[wdth,wght].ttf", "100 900", "62% 125%"),
    ("Barlow Condensed", "BarlowCondensed-Medium.ttf", "500", "normal"),
    ("Barlow Condensed", "BarlowCondensed-SemiBold.ttf", "600", "normal"),
    ("Barlow Condensed", "BarlowCondensed-Bold.ttf", "700", "normal"),
    ("Barlow", "Barlow-Medium.ttf", "400 500", "normal"),
    ("Barlow", "Barlow-SemiBold.ttf", "600 800", "normal"),
    ("Inter", "Inter[opsz,wght].ttf", "100 900", "normal"),
    ("Silkscreen", "Silkscreen-Regular.woff", "400", "normal"),   # écran à points (Gamer Lover)
    ("VT323", "VT323-Regular.woff", "400", "normal"),             # écran de terminal (Gamer Lover)
]


_CSS = []


def font_css():
    """Polices embarquées (data:) : la page de travail n'a pas accès aux fichiers locaux."""
    if not _CSS:
        import base64
        out = []
        for fam, f, w, st in FACES:
            b64 = base64.b64encode(open(os.path.join(FONTS, f), "rb").read()).decode()
            woff = f.endswith(".woff")
            out.append("@font-face{font-family:'%s';src:url(data:font/%s;base64,%s) format('%s');font-weight:%s;font-stretch:%s}"
                       % (fam, "woff" if woff else "ttf", b64, "woff" if woff else "truetype", w, st))
        _CSS.append("\n".join(out))
    return _CSS[0]


# ------------------------------------------------------------------------------- mini-moteur
TOK = re.compile(r'<sc-for list="\{\{([^}]+)\}\}" as="(\w+)"[^>]*>|</sc-for>|<sc-if value="\{\{([^}]+)\}\}"[^>]*>|</sc-if>')
VAR = re.compile(r"\{\{([^}]+)\}\}")


def parse(src):
    root = []
    stack = [root]
    pos = 0
    for m in TOK.finditer(src):
        if m.start() > pos:
            stack[-1].append(("t", src[pos:m.start()]))
        tok = m.group(0)
        if tok.startswith("<sc-for"):
            node = ("for", m.group(1).strip(), m.group(2), [])
            stack[-1].append(node)
            stack.append(node[3])
        elif tok.startswith("<sc-if"):
            node = ("if", m.group(3).strip(), None, [])
            stack[-1].append(node)
            stack.append(node[3])
        else:
            stack.pop()
        pos = m.end()
    stack[-1].append(("t", src[pos:]))
    return root


def ev(expr, scope):
    expr = expr.strip()
    if expr in ("true", "false"):
        return expr == "true"
    parts = expr.split(".")
    v = scope.get(parts[0])
    for p in parts[1:]:
        if isinstance(v, dict):
            v = v.get(p)
        else:
            return None
    return v


def sub(text, scope):
    def rep(m):
        v = ev(m.group(1), scope)
        if v is None:
            return ""
        if isinstance(v, bool):
            return "true" if v else "false"
        if isinstance(v, float) and v.is_integer():
            v = int(v)
        return _html.escape(str(v), quote=True)
    return VAR.sub(rep, text)


def run(nodes, scope):
    out = []
    for n in nodes:
        if n[0] == "t":
            out.append(sub(n[1], scope))
        elif n[0] == "for":
            lst = ev(n[1], scope) or []
            for i, item in enumerate(lst):
                sc = dict(scope)
                sc[n[2]] = item
                sc["$index"] = i
                out.append(run(n[3], sc))
        else:
            if ev(n[1], scope):
                out.append(run(n[3], scope))
    return "".join(out)


# ------------------------------------------------------------------------------- gabarits écran
HERO_ZOOM = "0.8773"  # schéma B : 440 px au canvas, 386 px dans les 628 px de l'écran MPC


def screen_body(tpl):
    """Gabarit de la maquette sans le bandeau ni les onglets simulés, ramené à 1280 × 628.
    tpl : "A", "B", "C" ou "B:orbit" (gabarit dessiné à la main, maquettes_main.BODIES)."""
    if ":" in tpl:
        import maquettes_main as MM
        body = MM.BODIES[tpl.split(":", 1)[1]]
        tpl = tpl[0]
    else:
        body = {"A": G.A_BODY, "B": G.B_BODY, "C": G.C_BODY}[tpl]
    body = body.replace(G.FIRM, "").replace(G.TABS, "")
    body = body.replace("width:1280px;height:800px", "width:1280px;height:628px", 1)
    if tpl == "B":
        # 66 px de moins qu'au canvas : rangée des 8 boutons et schéma resserrés, mêmes proportions
        body = body.replace('data-k="macrorow" style="height:182px;border-radius:16px', 'data-k="macrorow" style="height:176px;flex:none;border-radius:16px')
        body = body.replace('<div style="flex:1;display:flex;gap:12px;padding:12px">', '<div style="flex:1;min-height:0;display:flex;gap:12px;padding:12px">')
        body = body.replace('<div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0">',
                            '<div style="flex:1;display:flex;flex-direction:column;gap:12px;min-width:0;min-height:0">')
        body = body.replace('<div data-k="hero" style="width:440px;height:440px;flex:none',
                            '<div data-k="hero" data-zoom="%s" style="width:440px;height:440px;zoom:%s;flex:none' % (HERO_ZOOM, HERO_ZOOM))
    return body


def page_html(body_tpl, D, extra_css=""):
    body = run(parse(body_tpl), {"D": D})
    return ("<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\"><style>%s\n"
            "html,body{margin:0;padding:0;background:transparent}*,*::before,*::after{box-sizing:border-box}"
            "button{font-family:inherit;cursor:default}%s</style></head><body>%s</body></html>"
            % (font_css(), extra_css, body))
