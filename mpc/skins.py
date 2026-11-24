"""
OFAD GOLEM — habillage graphique des greffons MPC (écran tactile des MPC et de la Force) — AGPL-3.0-or-later

Écrit Plugin Skins/TUI.json, Q-Links.json et les images, d'après une description de pages faite à
la main pour chaque instrument (fonctions `skin_<clé>` en bas de ce fichier). Format : référence du
kit MPC (docs/ui-reference.md) ; les briques JSON (objets ordonnés à clés répétées, superpositions)
viennent de tools/gen_ui.py du kit.

Principes :
  - onglets = touches de fonction sous l'écran ; un même onglet peut avoir jusqu'à 8 sous-pages
    (on appuie de nouveau sur la touche pour passer à la suivante : couches 1 à 8 de Kick Forge) ;
  - le fond de chaque page est une image calculée (bandeau, panneaux, noms des réglages) ;
    les valeurs sont écrites en direct par la MPC ;
  - toucher un réglage le sélectionne : la grosse molette le modifie, et les Q-Links passent sur
    son groupe de quatre ;
  - Q-Links : 4 groupes de 4 par page, rangés de haut en bas (la Force en montre 8 à la fois).
"""
import copy as _copy
import json
import os
import sys
import unicodedata

import skin_kit as K

GEN_UI = None  # module tools/gen_ui.py du kit, chargé par load_gen_ui()


def load_gen_ui(sdk):
    global GEN_UI
    sys.path.insert(0, os.path.join(sdk, "tools"))
    import gen_ui
    GEN_UI = gen_ui
    return gen_ui


def fold(s):
    """Même repli ASCII que le greffon (GolemMpc.h) pour les textes de choix."""
    rep = {"œ": "oe", "Œ": "OE", "æ": "ae", "’": "'", "‘": "'", "«": '"', "»": '"', "–": "-", "—": "-", "×": "x",
           "°": "deg", "µ": "u", "·": "-", "…": "...", "→": ">", "←": "<", "½": "1/2", "♯": "#", "♭": "b"}
    out = []
    for ch in s:
        if ch in rep:
            out.append(rep[ch])
            continue
        n = unicodedata.normalize("NFD", ch)
        base = "".join(c for c in n if unicodedata.category(c) != "Mn")
        out.append(base if all(ord(c) < 128 for c in base) else "")
    t = " ".join("".join(out).split())
    return t


# ---------------------------------------------------------------------------------------------
# Réglages du greffon
# ---------------------------------------------------------------------------------------------
class P:
    def __init__(self, index, pid, name, lo, hi, choices):
        self.index, self.id, self.name = index, pid, name
        self.lo, self.hi = lo, hi
        self.raw_choices = list(choices) if choices else []
        self.choices = [fold(c)[:28] for c in choices] if choices else []
        if len(self.choices) == 1:
            self.choices.append(self.choices[0])
        self.bipolar = lo is not None and hi is not None and lo < 0 < hi

    @property
    def handle(self):
        return "Parameter %d" % self.index


def load_catalog(catalog_path):
    """Catalogue de GOLEM + fiches des greffons propres à la MPC (mpc/catalog_mpc.json)."""
    cat = json.load(open(catalog_path, encoding="utf-8"))
    extra = os.path.join(os.path.dirname(os.path.abspath(__file__)), "catalog_mpc.json")
    if os.path.isfile(extra):
        x = json.load(open(extra, encoding="utf-8"))
        for grp in ("instruments", "effects"):
            have = {m["type"] for m in cat[grp]}
            cat[grp] = cat[grp] + [m for m in x.get(grp, []) if m["type"] not in have]
    return cat


def load_params(catalog_path, kind, key, params_json):
    """Réglages du module (catalogue GOLEM) → index du greffon (0 = Volume du greffon, puis l'ordre GOLEM)."""
    cat = load_catalog(catalog_path)
    mod = [m for m in cat["instruments" if kind == "instrument" else "effects"] + cat["instruments"] + cat["effects"] if m["type"] == key][0]
    built = json.load(open(params_json, encoding="utf-8"))["params"]
    if len(built) < len(mod["params"]) + 1:
        raise SystemExit("skins: %s : %d réglages dans le greffon, %d dans le catalogue" % (key, len(built), len(mod["params"]) + 1))
    out = {"__volume__": P(0, "__volume__", "Volume", -60, 6, None)}
    for i, p in enumerate(mod["params"]):
        b = built[i + 1]
        if p.get("choices") and b["kind"] == "choice" and len(b["choices"]) > max(2, len(p["choices"])):
            raise SystemExit("skins: %s : choix différents pour %s" % (key, p["id"]))
        out[p["id"]] = P(i + 1, p["id"], b["name"], p.get("min"), p.get("max"), p.get("choices") or [])
    # réglages propres au greffon MPC (fichiers, phrase, piste au clavier), après ceux de GOLEM
    extras = []
    for b in built[len(mod["params"]) + 1:]:
        q = P(b["index"], "__extra_%d" % b["index"], b["name"], None, None, b["choices"] if b["kind"] == "choice" and not b["name"].startswith(("Sample", "Firmware")) else [])
        q.extra_kind = "file" if b["name"].startswith(("Sample", "Firmware", "Dossier")) else ("choice" if q.choices else "int")
        out[q.id] = q
        if b["name"] in ("Bille X", "Bille Y"):
            # Glutamake : position de la bille, écrite par le moteur (pavé XY de l'écran de tête seulement)
            out["mpc_ball" + b["name"][-1].lower()] = q
            continue
        extras.append(q)
    # emplacements de sample : Dossier N puis Sample N, emplacement par emplacement
    def slot_order(q):
        m = _re.search(r"(\d+)$", q.name)
        return (0 if q.extra_kind == "file" else 1, int(m.group(1)) if m else 0, 0 if q.name.startswith("Dossier") else 1)
    if any(q.name.startswith("Dossier") for q in extras):
        extras = sorted(extras, key=slot_order)
    out["__extras__"] = extras
    return out


# ---------------------------------------------------------------------------------------------
# Composants
# ---------------------------------------------------------------------------------------------
SMALL = (108, 132, 76)   # largeur, hauteur de la case, taille du bouton
COMPACT = (98, 118, 64)  # habillage automatique (plus de réglages par écran)
BIG = (140, 178, 108)


def O(*pairs):
    return GEN_UI.obj(*pairs)


def textstyle(height, colour, just="horizontallyCentred verticallyCentred", style="SemiBold"):
    return O(("version", 1),
             ("font", O(("version", 1), ("name", "Titillium Web"), ("style", style), ("height", float(height)))),
             ("colour", colour), ("justification", just), ("case", "Original"))


def label_value(height, colour, just="horizontallyCentred verticallyCentred", style="SemiBold"):
    return O(("version", 1), ("textStyle", textstyle(height, colour, just, style)), ("type", "Value"), ("handleName", "Data"))


def ent(name, type_name, data, bounds, focus="No", invalid="Show", when="Always", remap=(), invalidating=None):
    return GEN_UI.entry(name, type_name, data, bounds, focus, invalid, when, remap=remap, invalidating=invalidating)


def rect(x, y, w, h):
    return "%d %d %d %d" % (round(x), round(y), round(w), round(h))


class Skin:
    def __init__(self, out_dir, accent):
        self.out = out_dir
        os.makedirs(out_dir, exist_ok=True)
        self.accent = accent
        self.defs = {}           # nom → définition (ordre d'insertion conservé)
        self.pages = []
        self.replacements = {}   # index → choix
        self.program = []        # 16 index pour le mode Programme
        self.no_envelope = False  # habillage de secours : sans le dessin d'enveloppe

    # -- images -------------------------------------------------------------------------------
    def img(self, name):
        return name  # chemins relatifs au dossier Plugin Skins

    def colour_tag(self, c):
        return K.argb(c)[2:]

    # -- définitions ----------------------------------------------------------------------------
    def focus(self, w, h):
        return ent("focus", "Focus", GEN_UI.focus_data(K.argb(self.accent)), rect(0, 0, w, h), when="WhenFocussed")

    def actions(self, overlay, double=True):
        return GEN_UI.control_actions(overlay, double_click=double)

    def def_knob(self, size, colour, bipolar, overlay):
        w, h, k = BIG if size == "b" else (COMPACT if size == "c" else SMALL)
        tag = "kn_%s_%s_%s_%s" % (size, self.colour_tag(colour), "b" if bipolar else "u", overlay.replace(" ", ""))
        th = getattr(self, "theme", None)
        if th and th.get("knob"):
            tag += "_g"
        if tag not in self.defs:
            frames = K.KNOB_FRAMES
            if th and th.get("knob"):
                strip, frames = theme_strip(self, th["knob"], k, k, round_mask=True)
            else:
                strip = "knob%d_%s_%s.png" % (k, self.colour_tag(colour), "bi" if bipolar else "uni")
                K.knob_strip(os.path.join(self.out, strip), k, colour, bipolar)
            ky = 26 if size == "b" else (20 if size == "c" else 23)
            vy = ky + k + 2
            children = [self.focus(w, h),
                        ent("knob", "Knob", GEN_UI.knob_filmstrip_data(strip, frames), rect((w - k) / 2, ky, k, k)),
                        ent("value", "Label", label_value(22 if size == "b" else (17 if size == "c" else 19), K.argb(K.TEXT)), rect(0, vy, w, h - vy - 2))]
            self.defs[tag] = GEN_UI.component_definition(self.actions(overlay), children, "00000000")
        return tag

    def def_menu(self, w, colour, top=24):
        tag = "mn_%d_%d_%s" % (w, top, self.colour_tag(colour))
        if tag not in self.defs:
            box = "menu_%d_%s.png" % (w, self.colour_tag(colour))
            K.menu_box(os.path.join(self.out, box), w, 40, colour)
            children = [self.focus(w, top + 42),
                        ent("box", "Image", O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", box)), rect(0, top, w, 40)),
                        ent("value", "Label", label_value(19, K.argb((238, 238, 242, 255)), "left verticallyCentred"), rect(12, top, w - 40, 40))]
            acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                    GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
            self.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
        return tag

    def def_buttons(self, words, w, h, colour, vertical=False, gap=6):
        """Bouton à bascule (1 mot) ou groupe de choix (n mots)."""
        n = len(words)
        tag = "bt_%s_%dx%d_%s_%s" % ("_".join(fold(x).replace(" ", "").replace("'", "") for x in words), w, h,
                                     self.colour_tag(colour), "v" if vertical else "h")
        if tag not in self.defs:
            children = []
            tw = w if vertical else (w * n + gap * (n - 1))
            th = (h * n + gap * (n - 1)) if vertical else h
            children.append(self.focus(tw, th))
            for i, word in enumerate(words):
                # nom de fichier valable sur les cartes FAT/exFAT et sous Windows (« 4:1 » → « 4-1 »)
                safe = __import__("re").sub(r"[^A-Za-z0-9._+-]", "", fold(word).replace(":", "-").replace(" ", ""))
                base = "btn_%s_%dx%d_%s" % (safe, w, h, self.colour_tag(colour))
                K.text_button(os.path.join(self.out, base + "_on.png"), w, h, word, True, colour)
                K.text_button(os.path.join(self.out, base + "_off.png"), w, h, word, False, colour)
                bid, group = (1, 1) if n == 1 else (i, n)
                bx, by = (0, i * (h + gap)) if vertical else (i * (w + gap), 0)
                children.append(ent("b%d" % i, "Button", GEN_UI.button_data(base + "_on.png", base + "_off.png", bid, group), rect(bx, by, w, h)))
            self.defs[tag] = GEN_UI.component_definition(self.actions("knob overlay", double=False), children, "00000000")
            self.defs[tag + "#size"] = (tw, th)
        return tag, self.defs[tag + "#size"]

    def def_fader(self, w, h, colour, bipolar=False):
        tag = "fd_%dx%d_%s_%s" % (w, h, self.colour_tag(colour), "b" if bipolar else "u")
        th = getattr(self, "theme", None)
        if th and th.get("fader") and tag + "_g" not in self.defs:
            info = th["fader"]
            fh = h + 8
            fw = max(8, round(info["w"] * fh / info["h"]))
            strip, frames = theme_strip(self, info, fw, fh)
            children = [self.focus(w + 16, h + 34),
                        ent("bar", "Knob", GEN_UI.knob_filmstrip_data(strip, frames, "Vertical"), rect(8 + (w - fw) / 2, 0, fw, fh)),
                        ent("value", "Label", label_value(17, K.argb(K.TEXT)), rect(-14, h + 6, w + 44, 26))]
            self.defs[tag + "_g"] = GEN_UI.component_definition(self.actions("knob overlay bipolar" if bipolar else "knob overlay"), children, "00000000")
        if th and th.get("fader"):
            return tag + "_g"
        if tag not in self.defs:
            bar = O(("version", 2), ("sliderType", "ColourBar"), ("direction", "Vertical"),
                    ("backgroundColour", "ff1a1b1f"), ("outlineColour", K.argb(K.PANEL_EDGE)), ("valueColour", K.argb(colour)),
                    ("isBipolar", bipolar), ("handleName", "Data"))
            children = [self.focus(w + 16, h + 34),
                        ent("bar", "Slider", bar, rect(8, 4, w, h)),
                        ent("value", "Label", label_value(17, K.argb(K.TEXT)), rect(-14, h + 6, w + 44, 26))]
            self.defs[tag] = GEN_UI.component_definition(self.actions("knob overlay bipolar" if bipolar else "knob overlay"), children, "00000000")
        return tag

    def def_number(self, w, h, colour, size=26):
        tag = "nb_%dx%d_%d_%s" % (w, h, size, self.colour_tag(colour))
        if tag not in self.defs:
            box = "menu_%d_%s.png" % (w, self.colour_tag(colour))
            K.menu_box(os.path.join(self.out, box), w, h, colour)
            vs = O(("version", 4), ("knobType", "ValueSlider"), ("textStyle", textstyle(size, K.argb(colour), style="Bold")),
                   ("dragOrientation", "Vertical"), ("handleName", "Data"))
            children = [self.focus(w, h),
                        ent("box", "Image", O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", box)), rect(0, 0, w, h)),
                        ent("num", "Knob", vs, rect(0, 0, w - 30, h))]
            self.defs[tag] = GEN_UI.component_definition(self.actions("knob overlay"), children, "00000000")
        return tag

    def finish_defs(self):
        bg = K.argb(K.BG_BOT)
        self.defs["knob overlay"] = GEN_UI.overlay_component("KnobOverlay", GEN_UI.knob_overlay_data(False), bg)
        self.defs["knob overlay bipolar"] = GEN_UI.overlay_component("KnobOverlay", GEN_UI.knob_overlay_data(True), bg)
        self.defs["menu overlay"] = GEN_UI.overlay_component("MenuOverlay", GEN_UI.menu_overlay_data(), bg)

    # -- pages ----------------------------------------------------------------------------------
    def page(self, tab_name, fn_key, sub=0):
        pg = Page(self, tab_name, fn_key, sub)
        self.pages.append(pg)
        return pg

    def write(self):
        self.finish_defs()
        defs = [O(("key", k), ("value", v)) for k, v in self.defs.items() if not k.endswith("#size")]
        repl = [O(("key", "Parameter %d" % i), ("value", ch)) for i, ch in sorted(self.replacements.items())]
        tabs = []
        for pg in self.pages:
            bgname = "bg_%d_%d.png" % (pg.fn, pg.sub)
            pg.canvas.save(os.path.join(self.out, bgname))
            root = GEN_UI.component_definition([], pg.children, K.argb(K.BG_BOT), bg_image=bgname)
            tabs.append(O(("version", 1), ("tabName", pg.name), ("fnKeyIndex", pg.fn), ("fnKeySubIndex", pg.sub),
                          ("componentDefinition", root), ("qlinkBoundsData", pg.qlink_rects())))
        page_data = O(("version", 1),
                      ("componentDefinitions", O(("version", 2), ("importFiles", []), ("localComponentDefinitions", defs))),
                      ("replacementStrings", repl),
                      ("info", O(("version", 1), ("type", "CompleteDescription"))),
                      ("tabs", tabs))
        with open(os.path.join(self.out, "TUI.json"), "w", encoding="utf-8") as f:
            f.write(GEN_UI.dump_json(O(("pageData", page_data))) + "\n")
        entries = []
        for pg in self.pages:
            qm = GEN_UI.Obj(("Q-Link %d" % q, pg.qlinks.get(q, -1)) for q in range(1, 17))
            entries.append(O(("Tab", pg.fn + 1), ("SubTab", pg.sub + 1), ("Bank Direction", "RowTopDown"), ("Q-Links", qm)))
        prog = (self.program + [-1] * 16)[:16]
        qobj = O(("version", 4), ("info", O(("version", 1), ("type", "CompleteDescription"))),
                 ("Screen Mode Q-Links", O(("version", 4), ("map", entries))),
                 ("Program Mode Q-Links", GEN_UI.Obj(("Q-Link %d" % (q + 1), prog[q]) for q in range(16))))
        with open(os.path.join(self.out, "Q-Links.json"), "w", encoding="utf-8") as f:
            f.write(GEN_UI.dump_json(qobj) + "\n")


class Page:
    def __init__(self, skin, name, fn, sub):
        self.skin, self.name, self.fn, self.sub = skin, name, fn, sub
        self.canvas = K.Canvas()
        self.children = []
        self.bounds_of = {}   # index → (x, y, w, h)
        self.qlinks = {}
        self.groups = []
        self.placed = []      # réglages posés sur la page, dans l'ordre

    def _choice(self, p):
        if p.choices:
            self.skin.replacements[p.index] = p.choices

    def _add(self, p, tag, x, y, w, h):
        self.children.append(ent(p.id, tag, O(("version", 1), ("handleName", "Data")), rect(x, y, w, h),
                                 focus="Yes", invalid="Hide", remap=[("Data", p.handle)]))
        self.bounds_of[p.index] = (x, y, w, h)
        if p not in self.placed:
            self.placed.append(p)
        if isinstance(tag, str) and tag.startswith(("kn_", "fd_")):
            p._turn = True

    def fill_qlinks(self, candidates):
        """Complète les cases vides des 16 Q-Links (2 rangées de 8 sur la Force), dans l'ordre des rangées."""
        used = set(self.qlinks.values())
        cands = [c for c in candidates if c.index not in used]
        for g in range(4):
            base = 13 - 4 * g
            for i in range(4):
                if not cands:
                    return
                if self.qlinks.get(base + i, -1) == -1:
                    c = cands.pop(0)
                    self.qlinks[base + i] = c.index
                    while len(self.groups) <= g:
                        self.groups.append([])
                    self.groups[g].append(c)

    def knob(self, p, x, y, label, size="s", colour=None):
        colour = colour or self.skin.accent
        self._choice(p)
        overlay = "knob overlay" if len(p.choices) > 4 else ("knob overlay bipolar" if p.bipolar else "knob overlay")
        tag = self.skin.def_knob(size, colour, p.bipolar, overlay)
        w, h, _ = BIG if size == "b" else (COMPACT if size == "c" else SMALL)
        self.canvas.fit_text(x, y + (10 if size == "c" else 12), label, {"s": 15, "c": 14}.get(size, 17), w - 2, K.MUTED)
        self._add(p, tag, x, y, w, h)
        return w

    def menu(self, p, x, y, w, label, colour=None):
        """Case de menu ; label vide : la case seule (40 de haut) placée en y."""
        colour = colour or self.skin.accent
        self._choice(p)
        top = 24 if label else 0
        tag = self.skin.def_menu(w, colour, top)
        if label:
            self.canvas.text(x + 4, y + 11, label, size=14, colour=K.MUTED)
        self._add(p, tag, x, y, w, top + 42)

    def buttons(self, p, x, y, words, w=96, h=40, label=None, colour=None, vertical=False):
        colour = colour or self.skin.accent
        self._choice(p)
        tag, (tw, th) = self.skin.def_buttons(words, w, h, colour, vertical)
        if label:
            self.canvas.text(x + 2, y - 12, label, size=14, colour=K.MUTED)
        self._add(p, tag, x, y, tw, th)
        return tw, th

    def fader(self, p, x, y, w, h, label=None, colour=None):
        colour = colour or self.skin.accent
        tag = self.skin.def_fader(w, h, colour, p.bipolar)
        if label:
            self.canvas.fit_text(x - 14, y - 14, label, 14, w + 44, K.MUTED)
        self._add(p, tag, x, y, w + 16, h + 34)

    def number(self, p, x, y, w, h, label=None, colour=None, size=26):
        colour = colour or self.skin.accent
        tag = self.skin.def_number(w, h, colour, size)
        if label:
            self.canvas.text(x + 4, y - 12, label, size=14, colour=K.MUTED)
        self._add(p, tag, x, y, w, h)

    def value_text(self, p, x, y, w, h, size=16, colour=K.MUTED):
        self._choice(p)
        self.children.append(ent(p.id + " txt", "Label", label_value(size, K.argb(colour), style="Regular"), rect(x, y, w, h),
                                 invalid="Hide", remap=[("Data", p.handle)]))

    def envelope(self, x, y, w, h, attack, decay, release, ashape=None, rshape=None, colour=None, name="", sustain=None):
        """Enveloppe ADSR à palier fixe : montée (fondu d'entrée), corps (longueur), descente (fondu de sortie)."""
        if self.skin.no_envelope:
            return
        colour = colour or self.skin.accent
        fixed = ("Fixed/displayRange 0 1 valueRange 0 1 fineIncrement 0.01 coarseIncrement 0.1 defaultValue 1 "
                 "displayString '100%' name 'Corps' value 1")
        c = K.argb
        data = O(("version", 1), ("envelopeType", "ADSR"), ("envelopeStyle", "WithoutHeader"), ("envelopeName", name),
                 ("backgroundColour", "ff17181c"), ("scaleColour", "ff2c2e34"), ("lineColour", c(colour)),
                 ("fillColour", c(colour[:3] + (64,))), ("highlightColour", "ffffffff"),
                 ("lineWidth", 3), ("handleWidth", 30), ("handleHeight", 30), ("handleShape", "Circle"),
                 ("handleLineWidth", 2), ("handleLineColour", c(colour)), ("handleFillColour", "ff17181c"),
                 ("handleTextColour", "ffffffff"), ("handleLineWidthOnPressed", 3), ("handleLineColourOnPressed", "ffffffff"),
                 ("handleFillColourOnPressed", c(colour)), ("handleTextColourOnPressed", "ff111111"),
                 ("handleArrowLineColourOnPressed", "ffffffff"), ("handleArrowFillColourOnPressed", c(colour)),
                 ("attackHandle", attack.handle), ("decayHandle", decay.handle),
                 ("sustainHandle", sustain.handle if sustain else fixed),
                 ("releaseHandle", release.handle),
                 ("attackShapeHandle", ashape.handle if ashape else "Invalid"), ("decayShapeHandle", "Invalid"),
                 ("releaseShapeHandle", rshape.handle if rshape else "Invalid"),
                 ("forceHideShapeHandles", not (ashape or rshape)), ("forceCurveShape", 0.5), ("curveType", "Exponential"))
        self.children.append(ent("envelope", "Envelope", data, rect(x, y, w, h), focus="Yes", invalid="Show"))

    def qlink_group(self, params):
        """Un groupe de 4 Q-Links (de haut en bas : 13-16, 9-12, 5-8, 1-4)."""
        g = len(self.groups)
        base = 13 - 4 * g
        for i, p in enumerate(params[:4]):
            if p is not None:
                self.qlinks[base + i] = p.index
        self.groups.append([p for p in params if p is not None])

    def qlink_rects(self):
        if getattr(self, "face", False):
            return []      # écrans de façade : réglages placés librement, pas de cadres de Q-Links
        out = self._qlink_rects()
        # un cadre qui passe sur un réglage d'un autre groupe : pas de cadres sur cette page
        def box(r):
            x, y, w, h = [float(v) for v in r.split()]
            return x, y, x + w, y + h
        members = [set(p.index for p in g) for g in self.groups]
        for gi, r in enumerate(out):
            x0, y0, x1, y1 = box(r)
            for idx, (bx, by, bw, bh) in self.bounds_of.items():
                if gi < len(members) and idx in members[gi]:
                    continue
                if bx < x1 - 2 and bx + bw > x0 + 2 and by < y1 - 2 and by + bh > y0 + 2:
                    return []
        return out

    def _qlink_rects(self):
        out = []
        for g in self.groups:
            bs = [self.bounds_of[p.index] for p in g if p.index in self.bounds_of]
            if not bs:
                continue
            x0 = min(b[0] for b in bs) - 1
            y0 = min(b[1] for b in bs) - 1
            x1 = max(b[0] + b[2] for b in bs) + 1
            y1 = max(b[1] + b[3] for b in bs) + 1
            x0, y0 = max(2, x0), max(2, y0)
            x1, y1 = min(1278, x1), min(626, y1)
            out.append(rect(x0, y0, x1 - x0, y1 - y0))
        return out


# =============================================================================================
# Kick Forge
# =============================================================================================
FORGE = K.oklch(0.78, 0.17, 55)
T_SRC = K.oklch(0.78, 0.17, 55)
T_FADE = K.oklch(0.82, 0.12, 200)
T_SLICE = K.oklch(0.78, 0.15, 145)
T_DRIVE = K.oklch(0.68, 0.2, 25)


def skin_kickforge(skin, P_):
    def p(i):
        return P_[i]

    # ---- KICK : jeu et table de mixage des 8 couches --------------------------------------
    pg = skin.page("KICK", 0)
    c = pg.canvas
    c.header("KICK FORGE", K.GOLD, right="Constructeur de kicks")
    c.panel(12, 76, 1256, 394, "Couches", FORGE, "toucher le volume d'une couche · molette pour régler")
    sw = 154
    for n in range(1, 9):
        x0 = 20 + (n - 1) * sw
        if n > 1:
            c.line(x0 - 2, 116, x0 - 2, 456)
        c.text(x0, 126, str(n), size=26, colour=FORGE, weight="Bold", centre_w=sw - 8)
        pg.buttons(p("l%d_on" % n), x0 + 31, 148, ["ON"], w=84, h=34)
        tw, _ = pg.buttons(p("l%d_mute" % n), x0 + 31, 190, ["M"], w=39, h=32)
        pg.buttons(p("l%d_solo" % n), x0 + 31 + 45, 190, ["S"], w=39, h=32)
        pg.fader(p("l%d_vol" % n), x0 + 50, 232, 40, 160)
        pg.value_text(p("l%d_fam" % n), x0 + 2, 440, sw - 12, 24, size=15)
    c.panel(12, 478, 1256, 142, "Jeu", K.GOLD)
    pg.menu(p("style"), 30, 520, 220, "Style")
    pg.menu(p("key"), 266, 520, 150, "Tonalité")
    pg.buttons(p("pitchmode"), 434, 556, ["Fixe", "Suivie"], w=86, h=36, label="Note jouée")
    pg.knob(p("velsens"), 640, 480, "Vélocité")
    pg.knob(p("volume"), 760, 480, "Volume")
    pg.buttons(p("autogain"), 890, 556, ["Gain auto"], w=120, h=36, label="Niveau")
    pg.buttons(p("phasealign"), 1022, 556, ["Phase auto"], w=124, h=36, label="Phase")
    pg.qlink_group([p("l%d_vol" % n) for n in (1, 2, 3, 4)])
    pg.qlink_group([p("l%d_vol" % n) for n in (5, 6, 7, 8)])
    pg.qlink_group([p("volume"), p("velsens"), p("style"), p("key")])

    # ---- COUCHES : 8 sous-pages ------------------------------------------------------------
    for n in range(1, 9):
        L = lambda s: p("l%d_%s" % (n, s))
        pg = skin.page("COUCHES", 1, n - 1)
        c = pg.canvas
        c.header("COUCHE %d" % n, T_SRC, centred=False, title_size=34, right="")
        c.text(1256, 32, "KICK FORGE", size=15, colour=K.MUTED, anchor="rm")
        x = 220
        for key, word, w in (("on", "ON", 70), ("mute", "MUET", 80), ("solo", "SOLO", 80), ("phase", "PHASE", 86),
                             ("rev", "ENVERS", 96), ("lock", "VERROU", 100)):
            pg.buttons(L(key), x, 12, [word], w=w, h=40)
            x += w + 8
        pg.menu(L("fam"), 806, 12, 220, "", T_SRC)
        pg.number(L("part"), 1036, 12, 112, 40, colour=T_SRC)
        # SOURCE
        c.panel(12, 72, 548, 304, "Source", T_SRC)
        xs = [24 + i * 132 for i in range(4)]
        pg.knob(L("vol"), xs[0], 98, "Volume", colour=T_SRC)
        pg.knob(L("pitch"), xs[1], 98, "Transpos.", colour=T_SRC)
        pg.knob(L("fine"), xs[2], 98, "Accord fin", colour=T_SRC)
        pg.knob(L("stretch"), xs[3], 98, "Étirement", colour=T_SRC)
        pg.knob(L("offset"), xs[0], 238, "Décalage", colour=T_SRC)
        pg.knob(L("len"), xs[1], 238, "Longueur", colour=T_SRC)
        pg.buttons(L("keytrk"), xs[2] + 20, 296, ["Accordé"], w=120, h=38, label="Sample")
        # ENVELOPPE
        c.panel(572, 72, 696, 304, "Enveloppe · fondus · toucher un point, molette", T_FADE)
        pg.envelope(588, 104, 420, 262, L("filen"), L("len"), L("folen"), L("ficurve"), L("focurve"), T_FADE)
        pg.knob(L("filen"), 1018, 98, "Entrée", colour=T_FADE)
        pg.knob(L("folen"), 1140, 98, "Sortie", colour=T_FADE)
        pg.knob(L("ficurve"), 1018, 238, "Courbe ent.", colour=T_FADE)
        pg.knob(L("focurve"), 1140, 238, "Courbe sort.", colour=T_FADE)
        # TRANCHE
        c.panel(12, 384, 700, 236, "Tranche · filtres et égaliseur", T_SLICE)
        names = (("hp", "Passe-haut"), ("lp", "Passe-bas"), ("eqlo", "Grave"), ("eqmidf", "Fréq. méd."), ("eqmidg", "Médium"), ("eqhi", "Aigu"))
        for i, (k, lab) in enumerate(names):
            pg.knob(L(k), 22 + i * 114, 414, lab, colour=T_SLICE)
        # ATTAQUE · DISTORSION
        c.panel(724, 384, 544, 236, "Attaque · distorsion", T_DRIVE)
        pg.menu(L("drvtype"), 1076, 392, 180, "", T_DRIVE)
        for i, (k, lab) in enumerate((("tratk", "Attaque"), ("trsus", "Maintien"), ("drv", "Poussée"), ("drvmix", "Mélange"))):
            pg.knob(L(k), 734 + i * 132, 446, lab, colour=T_DRIVE)
        pg.qlink_group([L("vol"), L("pitch"), L("fine"), L("stretch")])
        pg.qlink_group([L("filen"), L("folen"), L("ficurve"), L("focurve")])
        pg.qlink_group([L("hp"), L("lp"), L("eqlo"), L("eqhi")])
        pg.qlink_group([L("tratk"), L("trsus"), L("drv"), L("drvmix")])

    # ---- Effets du kick ---------------------------------------------------------------------
    def fx_page(fn, sub, tab, title, prefix, tone, body, groups, extra=None):
        pg = skin.page(tab, fn, sub)
        c = pg.canvas
        c.header(title.upper(), tone, centred=False, title_size=34)
        c.text(1256, 32, "KICK FORGE · EFFETS", size=15, colour=K.MUTED, anchor="rm")
        x = 40 + int(c.d.textlength(title.upper(), font=K.font(34 * K.SS, "Bold")) / K.SS) + 24
        pg.buttons(p(prefix + "_on"), x, 12, ["ON"], w=76, h=40, colour=tone)
        pg.number(p(prefix + "_ord"), x + 92, 12, 112, 40, colour=tone)
        c.text(x + 92 + 118, 32, "rang dans la chaîne", size=14, colour=K.MUTED)
        c.panel(12, 76, 1256, 544, "", tone)
        # rangées centrées : bouton 176 de large, menu 330, curseur 150
        widths = {"k": 176, "m": 330, "f": 150}
        heights = []
        for items in body:
            kinds = {it[0] for it in items if it}
            heights.append(290 if "f" in kinds else 190)
        gap = (544 - sum(heights)) / (len(body) + 1)
        y = 76 + gap
        for items, rh in zip(body, heights):
            items = [it for it in items if it]
            total = sum(widths[it[0]] for it in items)
            x = (1280 - total) / 2
            for kind, pid, lab in items:
                if kind == "k":
                    pg.knob(p(pid), x + 18, y + (rh - BIG[1]) / 2, lab, "b", tone)
                elif kind == "m":
                    pg.menu(p(pid), x + 15, y + (rh - 66) / 2, 300, lab, tone)
                elif kind == "f":
                    pg.fader(p(pid), x + 53, y + 30, 44, rh - 80, lab, tone)
                x += widths[kind]
            y += rh + gap
        if extra:
            extra(pg)
        for g in groups:
            pg.qlink_group([p(i) for i in g])

    fx_page(2, 0, "TIMBRE", "Égaliseur", "eq", K.oklch(0.8, 0.13, 230),
            [[("k", "eq_lsf", "Fréq. grave"), ("k", "eq_lsg", "Plateau grave"), ("k", "eq_b1f", "Fréq. 1"), ("k", "eq_b1g", "Cloche 1"), ("k", "eq_b1q", "Largeur 1"), ("k", "eq_mix", "Mélange")],
             [("k", "eq_hsf", "Fréq. aigu"), ("k", "eq_hsg", "Plateau aigu"), ("k", "eq_b2f", "Fréq. 2"), ("k", "eq_b2g", "Cloche 2"), ("k", "eq_b2q", "Largeur 2"), None]],
            [["eq_lsf", "eq_lsg", "eq_b1f", "eq_b1g"], ["eq_hsf", "eq_hsg", "eq_b2f", "eq_b2g"], ["eq_b1q", "eq_mix"], ["eq_b2q"]])
    fx_page(2, 1, "TIMBRE", "Égaliseur M/S", "ms", K.oklch(0.8, 0.12, 280),
            [[("k", "ms_midlo", "Milieu grave"), ("k", "ms_midhi", "Milieu aigu"), ("k", "ms_width", "Largeur"), ("k", "ms_mix", "Mélange")],
             [("k", "ms_sidehp", "Côtés passe-haut"), ("k", "ms_sideg", "Côtés")]],
            [["ms_midlo", "ms_midhi", "ms_width", "ms_mix"], ["ms_sidehp", "ms_sideg"]])
    fx_page(2, 2, "TIMBRE", "Distorsion", "ds", T_DRIVE,
            [[("m", "ds_type", "Type"), None, ("k", "ds_drive", "Poussée"), ("k", "ds_tone", "Ton"), ("k", "ds_mix", "Mélange")],
             [("m", "ds_pre", "Préfiltre"), None, ("k", "ds_pref", "Fréq. préfiltre")]],
            [["ds_type", "ds_drive", "ds_tone", "ds_mix"], ["ds_pre", "ds_pref"]])
    fx_page(2, 3, "TIMBRE", "Filtres", "fl", T_SLICE,
            [[("k", "fl_hp", "Passe-haut"), ("k", "fl_hpres", "Résonance"), ("k", "fl_lp", "Passe-bas"), ("k", "fl_lpres", "Résonance"), ("k", "fl_mix", "Mélange")]],
            [["fl_hp", "fl_hpres", "fl_lp", "fl_lpres"], ["fl_mix"]])

    def shaper_extra(pg):
        pass
    fx_page(3, 0, "IMPACT", "Façonneur de kick", "ks", K.oklch(0.82, 0.16, 95),
            [[("f", "ks_n%d" % i, "Point %d" % i) for i in range(1, 9)],
             [("k", "ks_amount", "Quantité"), ("k", "ks_mix", "Mélange")]],
            [["ks_n1", "ks_n2", "ks_n3", "ks_n4"], ["ks_n5", "ks_n6", "ks_n7", "ks_n8"], ["ks_amount", "ks_mix"]])
    fx_page(3, 1, "IMPACT", "Transitoires", "tr", K.oklch(0.8, 0.15, 160),
            [[("k", "tr_attack", "Attaque"), ("k", "tr_duck", "Creux"), ("m", "tr_speed", "Vitesse"), None, ("k", "tr_mix", "Mélange")]],
            [["tr_attack", "tr_duck", "tr_speed", "tr_mix"]])
    fx_page(3, 2, "IMPACT", "Multibande", "mb", K.oklch(0.78, 0.14, 300),
            [[("k", "mb_depth", "Profondeur"), ("k", "mb_time", "Temps"), ("k", "mb_up", "Vers le haut"), ("k", "mb_down", "Vers le bas"), ("k", "mb_mix", "Mélange")],
             [("k", "mb_low", "Grave"), ("k", "mb_mid", "Médium"), ("k", "mb_high", "Aigu")]],
            [["mb_depth", "mb_time", "mb_up", "mb_down"], ["mb_low", "mb_mid", "mb_high", "mb_mix"]])
    fx_page(3, 3, "IMPACT", "Finaliseur", "fn", K.GOLD,
            [[("k", "fn_boostf", "Fréq. grave"), ("k", "fn_boost", "Grave relevé"), ("k", "fn_mono", "Grave mono sous"), ("k", "fn_clip", "Écrêtage"), ("k", "fn_mix", "Mélange")]],
            [["fn_boostf", "fn_boost", "fn_mono", "fn_clip"], ["fn_mix"]])
    fx_page(3, 4, "IMPACT", "Convolution", "cv", K.oklch(0.8, 0.12, 200),
            [[("m", "cv_ir", "Réponse"), None, ("k", "cv_wet", "Mouillé"), ("k", "cv_stereo", "Stéréo"), ("k", "cv_tone", "Ton"), ("k", "cv_speed", "Vitesse")],
             [("k", "cv_c1", "Courbe départ"), ("k", "cv_c2t", "Milieu (temps)"), ("k", "cv_c2", "Milieu"), ("k", "cv_c3", "Fin"), ("k", "cv_mix", "Mélange")]],
            [["cv_wet", "cv_stereo", "cv_tone", "cv_speed"], ["cv_c1", "cv_c2t", "cv_c2", "cv_c3"], ["cv_ir"], ["cv_mix"]])

    skin.program = [p("l%d_vol" % n).index for n in range(1, 9)] + [p(i).index for i in ("volume", "velsens", "style", "key")] + \
                   [p("l1_pitch").index, p("l1_len").index, p("l1_lp").index, p("l1_drv").index]


# =============================================================================================
# Acid Lover
# =============================================================================================
ACID = K.oklch(0.86, 0.17, 95)     # jaune acide
ACID2 = K.GOLD


def skin_acid(skin, P_):
    p = lambda i: P_[i]
    pg = skin.page("ACID", 0)
    c = pg.canvas
    c.header("ACID LOVER", ACID, right="Basse acide", height=86, title_size=52)
    # FILTRE ET ENVELOPPE : les cinq grands boutons
    c.panel(12, 98, 900, 248, "Filtre et enveloppe", ACID)
    for i, (k, lab) in enumerate((("cutoff", "Coupure"), ("reso", "Résonance"), ("envmod", "Modulation"), ("decay", "Déclin"), ("accent", "Accent"))):
        pg.knob(p(k), 40 + i * 172, 140, lab, "b", ACID)
    c.panel(924, 98, 344, 248, "Sortie", ACID2)
    pg.knob(p("level"), 944, 140, "Niveau", "b", ACID2)
    pg.knob(P_["__volume__"], 1104, 140, "Volume", "b", ACID2)
    # bas : oscillateur, jeu, saturation
    c.panel(12, 356, 404, 264, "Oscillateur", ACID)
    pg.buttons(p("wave"), 30, 440, ["Scie", "Carré"], w=110, h=48, label="Forme", vertical=True)
    pg.knob(p("tune"), 160, 410, "Accord", colour=ACID)
    pg.knob(p("fine"), 286, 410, "Fin", colour=ACID)
    c.panel(428, 356, 404, 264, "Jeu", ACID)
    for i, (k, lab) in enumerate((("glide", "Glissé"), ("accthr", "Seuil accent"), ("keytrack", "Suivi clavier"))):
        pg.knob(p(k), 446 + i * 128, 410, lab, colour=ACID)
    c.panel(844, 356, 424, 264, "Saturation", ACID2)
    for i, (k, lab) in enumerate((("drive", "Saturation"), ("dist", "Distorsion"), ("ampdecay", "Déclin ampli"))):
        pg.knob(p(k), 862 + i * 134, 410, lab, colour=ACID2)
    pg.qlink_group([p("cutoff"), p("reso"), p("envmod"), p("decay")])
    pg.qlink_group([p("accent"), p("level"), P_["__volume__"], None])
    pg.qlink_group([p("tune"), p("fine"), p("glide"), p("accthr")])
    pg.qlink_group([p("keytrack"), p("drive"), p("dist"), p("ampdecay")])
    skin.program = [p(k).index for k in ("cutoff", "reso", "envmod", "decay", "accent", "level", "drive", "dist",
                                         "tune", "fine", "glide", "accthr", "keytrack", "ampdecay", "wave")] + [0]



# =============================================================================================
# Habillage automatique : façades GOLEM (instruments) ou sections du catalogue (effets)
# =============================================================================================
import re as _re

STYLES_CSS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ui", "src", "styles.css")
TONES = [K.oklch(0.82, 0.15, 78), K.oklch(0.8, 0.13, 230), K.oklch(0.78, 0.15, 145), K.oklch(0.72, 0.18, 30),
         K.oklch(0.8, 0.13, 300), K.oklch(0.82, 0.12, 195), K.oklch(0.84, 0.15, 100), K.oklch(0.75, 0.17, 0)]


def parse_colour(txt):
    txt = (txt or "").strip()
    m = _re.match(r"oklch\(\s*([\d.]+)\s+([\d.]+)\s+([\d.]+)", txt)
    if m:
        return K.oklch(float(m.group(1)), float(m.group(2)), float(m.group(3)))
    m = _re.match(r"#([0-9a-fA-F]{6})", txt)
    if m:
        v = int(m.group(1), 16)
        return ((v >> 16) & 255, (v >> 8) & 255, v & 255, 255)
    return None


def theme_accent(theme):
    try:
        css = open(STYLES_CSS, encoding="utf-8").read()
    except OSError:
        return K.GOLD
    m = _re.search(r"\.face--t-%s\s*\{[^}]*--face-accent:\s*([^;]+);" % _re.escape(theme), css)
    c = parse_colour(m.group(1)) if m else None
    if c is None:
        return K.GOLD
    lum = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
    return c if lum > 110 else K.GOLD   # façades claires (accent sombre) : jaune GOLEM sur l'écran sombre


def short_name(s, n=8):
    s = fold(s).upper()
    for a, b in (("ENVELOPPES", "ENV"), ("ENVELOPPE", "ENV"), ("OSCILLATEURS", "OSC"), ("OSCILLATEUR", "OSC"),
                 ("MODULATION", "MOD"), ("EGALISEUR", "EQ"), ("SEQUENCE", "SEQ"), ("FILTRES", "FILTRE")):
        s = s.replace(a, b)
    if len(s) > n:
        s = s.split(" ")[0]
    return s[:n].strip(" ,.;:-·")


class Ctl:
    def __init__(self, p, kind, label, words=None):
        self.p, self.kind, self.label, self.words = p, kind, label, words


class Panel:
    def __init__(self, title, tone, ctls, env=None):
        self.title, self.tone, self.ctls, self.env = title, tone, ctls, env


def cell_size(c):
    k = c.kind
    if k == "big":
        return 146, 178
    if k == "small":
        return 102, 120
    if k == "fader":
        return 72, 170
    if k == "menu":
        return 204, 64
    if k == "toggle":
        return max(104, int(K.text_px(c.label, 17) + 40)), 64
    if k == "btns":
        return len(c.words) * c.bw + (len(c.words) - 1) * 6 + 12, 64
    if k == "env":
        return 360, 240
    if k == "file":
        return 306, 80
    return 102, 120


ADSR_KEYS = {
    "a": ("attaque", "attack", "att."),
    "d": ("declin", "decay", "dec."),
    "s": ("maintien", "soutien", "sustain", "tenue", "niveau"),
    "r": ("relachement", "relache", "relach", "release", "rel.", "chute"),
}


def detect_adsr(ctls):
    found = {}
    for c in ctls:
        if c.p.choices:
            continue
        lab = fold(c.label).lower()
        for k, words in ADSR_KEYS.items():
            if k not in found and any(lab == w or lab.startswith(w + " ") or lab.startswith(w) and len(lab) <= len(w) + 3 for w in words):
                found[k] = c
                break
    if len(found) != 4:
        return None
    pos = [ctls.index(c) for c in found.values()]
    return found if max(pos) - min(pos) <= 5 else None


def make_ctl(p, kind_hint, label, short=None):
    """Choisit la forme du réglage : bouton rotatif, curseur, boutons à mot, bascule, menu."""
    if p.choices:
        n = len(p.choices)
        words = list(short) if short and len(short) == n else None
        if words is None:
            words = [c for c in p.raw_choices] or list(p.choices)
        low = [fold(w).lower() for w in p.choices]
        if n == 2 and (low in (["non", "oui"], ["off", "on"], ["arret", "marche"]) or kind_hint == "toggle"):
            return Ctl(p, "toggle", label)
        maxlen = max(len(w) for w in words)
        tw = max(K.text_px(w, 17) for w in words)
        bw = max(56, int(tw + 26))
        if n <= 4 and bw <= 150 and n * bw + (n - 1) * 6 <= 470:
            c = Ctl(p, "btns", label, words)
            c.bw = bw
            return c
        return Ctl(p, "menu", label)
    if kind_hint == "big":
        return Ctl(p, "big", label)
    if kind_hint == "fader":
        return Ctl(p, "fader", label)
    return Ctl(p, "small", label)


def layout_panel(panel, max_w):
    """Rangées de cases dans un panneau ; renvoie (largeur, hauteur, rangées [(cases, hauteur)])."""
    cells = []
    if panel.env:
        cells.append(("env", panel.env))
    for c in panel.ctls:
        cells.append(("ctl", c))
    rows, row, rw = [], [], 0
    for kind, c in cells:
        w, h = cell_size(c if kind == "ctl" else Ctl(None, "env", ""))
        if row and rw + w > max_w - 20:
            rows.append(row)
            row, rw = [], 0
        row.append((kind, c, w, h))
        rw += w
    if row:
        rows.append(row)
    width = max(sum(w for _, _, w, _ in r) for r in rows) + 20
    height = 28 + sum(max(h for _, _, _, h in r) for r in rows) + 4
    return width, height, rows


def split_panel(panel, max_h, max_w):
    """Découpe un panneau trop haut pour un écran."""
    w, h, rows = layout_panel(panel, max_w)
    if h <= max_h:
        return [panel]
    out, cur = [], []
    used = 32
    part = 1
    env = panel.env
    for r in rows:
        rh = max(x[3] for x in r)
        if cur and used + rh > max_h:
            out.append(Panel(panel.title + (" (suite)" if part > 1 else ""), panel.tone, [c for k, c, _, _ in cur if k == "ctl"],
                             env if part == 1 else None))
            part += 1
            cur, used = [], 32
        cur.extend(r)
        used += rh
    if cur:
        out.append(Panel(panel.title + (" (suite)" if part > 1 else ""), panel.tone, [c for k, c, _, _ in cur if k == "ctl"],
                         env if part == 1 else None))
    return out


TOP, BOTTOM, LEFT, RIGHT, GAP = 72, 624, 12, 1268, 8


def qcount(pn):
    """Réglages d'un panneau pour les Q-Links (une enveloppe en compte 4)."""
    return sum(1 for c in pn.ctls if c.p is not None) + (4 if pn.env else 0)


def split_q(pn, cap=16):
    """Panneau de plus de 16 réglages : coupé en parties de 16 au plus (l'enveloppe reste dans la première)."""
    if qcount(pn) <= cap:
        return [pn]
    ctls = list(pn.ctls)
    first = cap - (4 if pn.env else 0)
    parts = [ctls[:first]] + [ctls[i:i + cap] for i in range(first, len(ctls), cap)]
    return [Panel(pn.title + (" (suite)" if i else ""), pn.tone, part, pn.env if i == 0 else None)
            for i, part in enumerate(parts) if part or (i == 0 and pn.env)]


def pack_screens(panels, qcap=16):
    """Panneaux rangés en étagères, écran par écran (ordre conservé). Un écran porte 16 réglages au plus,
    autant que de Q-Links (qcap=None : pas de limite, pages des sons de batterie)."""
    W, H = RIGHT - LEFT, BOTTOM - TOP
    items = []
    for pn in panels:
        for part in (split_q(pn, qcap) if qcap else [pn]):
            items.extend(split_panel(part, H, W))
    screens, shelves, shelf, sx, y, sh = [], [], [], 0, 0, 0
    q = 0
    for pn in items:
        w, h, rows = layout_panel(pn, W)
        n = qcount(pn)
        if qcap and (shelves or shelf) and q + n > qcap:
            if shelf:
                shelves.append((shelf, sh))
            screens.append(shelves)
            shelves, shelf, sx, y, sh = [], [], 0, 0, 0
            q = 0
        q += n
        if shelf and sx + w > W:
            shelves.append((shelf, sh))
            y += sh + GAP
            shelf, sx, sh = [], 0, 0
        if y + h > H and (shelves or shelf):
            if shelf:
                shelves.append((shelf, sh))
            screens.append(shelves)
            shelves, shelf, sx, y, sh = [], [], 0, 0, 0
            q = n
        shelf.append((pn, w, h, rows))
        sx += w + GAP
        sh = max(sh, h)
    if shelf:
        shelves.append((shelf, sh))
    if shelves:
        screens.append(shelves)
    return screens


def draw_screen(skin, pg, shelves, title, accent, page_label):
    c = pg.canvas
    th = getattr(skin, "theme", None)
    if th and th.get("bg"):
        bg = tuple(int(th["bg"][i:i + 2], 16) for i in (1, 3, 5)) + (255,)
        c.img.paste(bg, (0, 0, c.img.width, c.img.height))
    if th and th.get("unit") and LEFT > 100:
        from PIL import Image
        u = Image.open(th["unit"]).convert("RGBA")
        hh = BOTTOM - TOP
        ww = round(u.width * hh / u.height)
        if ww > LEFT - 24:
            ww = LEFT - 24
            hh = round(u.height * ww / u.width)
        u = u.resize((ww * K.SS, hh * K.SS), Image.LANCZOS)
        c.img.alpha_composite(u, (round((LEFT - ww) / 2) * K.SS, round(TOP + (BOTTOM - TOP - hh) / 2) * K.SS))
    c.header(title.upper(), accent, right=page_label, title_size=36)
    W = RIGHT - LEFT
    used_h = sum(sh for _, sh in shelves) + GAP * (len(shelves) - 1)
    y = TOP + max(0, (BOTTOM - TOP - used_h) / 2) if len(shelves) == 1 else TOP
    for shelf, sh in shelves:
        used = sum(w for _, w, _, _ in shelf) + GAP * (len(shelf) - 1)
        extra = (W - used) / len(shelf)
        x = LEFT
        for pn, w, h, rows in shelf:
            pw = w + extra
            th = getattr(skin, "theme", None)
            c.panel(x, y, pw, sh, pn.title, pn.tone, fill=(th or {}).get("panel"))
            ry = y + 28
            for r in rows:
                rh = max(cell[3] for cell in r)
                rw = sum(cell[2] for cell in r)
                cx = x + (pw - rw) / 2
                for kind, ctl, cw, ch in r:
                    cy = ry + (rh - ch) / 2
                    if kind == "env":
                        e = pn.env
                        pg.envelope(cx + 4, cy + 4, cw - 8, ch - 8, e["a"].p, e["d"].p, e["r"].p, colour=pn.tone, sustain=e["s"].p)
                        for k in "adsr":
                            pg.bounds_of[e[k].p.index] = (cx + 4, cy + 4, cw - 8, ch - 8)
                    else:
                        place_ctl(pg, ctl, cx, cy, cw, ch, pn.tone)
                    cx += cw
                ry += rh
            x += pw + GAP
        y += sh + GAP


def place_ctl(pg, c, x, y, w, h, tone):
    if c.kind in ("small", "big"):
        sw = BIG[0] if c.kind == "big" else COMPACT[0]
        pg.knob(c.p, x + (w - sw) / 2, y, c.label, "b" if c.kind == "big" else "c", tone)
    elif c.kind == "fader":
        pg.fader(c.p, x + (w - 50) / 2, y + 18, 34, 118, c.label, tone)
    elif c.kind == "menu":
        pg.menu(c.p, x + 7, y, w - 14, c.label, tone)
    elif c.kind == "toggle":
        pg.buttons(c.p, x + 6, y + 18, [c.label], w=w - 12, h=40, colour=tone)
    elif c.kind == "btns":
        pg.buttons(c.p, x + 6, y + 22, c.words, w=c.bw, h=40, label=c.label, colour=tone)
    elif c.kind == "file":
        pg.number(c.p, x + 8, y + 28, w - 16, 44, c.label, tone, size=19)


def qlink_order(shelves):
    """Réglages de l'écran pour les Q-Links : d'abord ceux qui tournent (boutons rotatifs, curseurs,
    enveloppes, fichiers), puis les menus, puis les boutons à enclencher ; dans l'ordre de l'écran."""
    turn, menus, push = [], [], []
    for shelf, _ in shelves:
        for pn, _, _, rows in shelf:
            for r in rows:
                for kind, c, _, _ in r:
                    if kind == "env":
                        for k in "adsr":
                            pn.env[k].p._turn = True
                            turn.append(pn.env[k].p)
                    elif c.kind in ("small", "big", "fader", "file"):
                        c.p._turn = True
                        turn.append(c.p)
                    elif c.kind == "menu":
                        menus.append(c.p)
                    else:
                        push.append(c.p)
    return turn + menus + push


def assign_qlinks(pg, params):
    """16 Q-Links au plus par page : groupes de 4 pleins."""
    for i in range(0, min(16, len(params)), 4):
        pg.qlink_group(params[i:i + 4])
    return [p.index for p in params[:16]]


def panels_from_face(face, P_, accent):
    """Panneaux d'une façade GOLEM, page par page : [(nom de page, [Panel])]."""
    pages = []
    used = set()
    for page in face.get("pages", []):
        panels = []
        for row in page.get("rows", []):
            for grp in row:
                ctl_defs = grp.get("ctl") or []
                if not ctl_defs:
                    continue
                banks = grp.get("banks") or [None]
                tone = parse_colour(grp.get("tone")) or accent
                for b in banks:
                    ctls = []

                    def add(cd):
                        if not isinstance(cd, dict):
                            return
                        if "stack" in cd:
                            for x in cd["stack"]:
                                add(x)
                            return
                        pid = cd.get("id")
                        if not pid:
                            return
                        if b is not None:
                            pid = pid.replace("{b}", str(b.get("value")))
                        if "{" in pid or pid not in P_ or pid in used:
                            return
                        used.add(pid)
                        ctls.append(make_ctl(P_[pid], cd.get("k", ""), cd.get("label") or P_[pid].name, cd.get("short")))
                    for cd in ctl_defs:
                        add(cd)
                    if not ctls:
                        continue
                    title = grp.get("title", "")
                    if b is not None:
                        title = (title + " · " if title else "") + str(b.get("label", ""))
                    env = detect_adsr(ctls)
                    if env:
                        ctls = [c for c in ctls if c not in env.values()]
                    if len(ctls) > 6:
                        for c in ctls:
                            if c.kind == "big":
                                c.kind = "small"
                    panels.append(Panel(title, tone, ctls, env))
        if panels:
            pages.append((page.get("label", "Son"), panels))
    return pages, used


PAD_RE = _re.compile(r"^(?:[pz](\d\d)|t(\d))_")


def panels_from_sections(mod, P_, used, accent, kit=False):
    params = mod["params"]
    secs = []
    for si, sec in enumerate(mod.get("sections", [])):
        ids = [params[i]["id"] for i in sec["params"] if i < len(params)]
        ids = [i for i in ids if i in P_ and i not in used]
        if not ids:
            continue
        for i in ids:
            used.add(i)
        pad = None
        m = PAD_RE.match(ids[0])
        if kit and m:
            pad = int(m.group(1) or m.group(2))
        title = fold(sec["name"]) if False else sec["name"]
        if pad:
            title = "%d · %s" % (pad, title)
        ctls = [make_ctl(P_[i], "", params[[x["id"] for x in params].index(i)]["name"]) for i in ids]
        env = detect_adsr(ctls)
        if env:
            ctls = [c for c in ctls if c not in env.values()]
        tone = TONES[(pad or si) % len(TONES)] if not kit else TONES[(pad or 0) % len(TONES)]
        secs.append((pad or 0, si, sec.get("tab") or sec["name"], Panel(title, tone, ctls, env)))
    rest = [p["id"] for p in params if p["id"] in P_ and p["id"] not in used]
    if rest:
        for i in rest:
            used.add(i)
        secs.append((99, 999, "Plus", Panel("Plus", accent, [make_ctl(P_[i], "", P_[i].name) for i in rest])))
    if kit:
        secs.sort(key=lambda t: (t[0] == 0, t[0], t[1]))
    return secs


DRUM_ABBREV = [   # abréviations des boîtes à rythmes que tout le monde connaît
    ("grosse caisse coloree", "BD C"), ("grosse caisse 2", "BD2"), ("grosse caisse", "BD"),
    ("caisse claire coloree", "SD C"), ("caisse claire 1", "SD1"), ("caisse claire 2", "SD2"), ("caisse claire", "SD"),
    ("clap", "CP"), ("rimshot", "RS"), ("charley ferme", "CH"), ("charley ouvert", "OH"),
    ("tom grave", "LT"), ("tom medium", "MT"), ("tom aigu", "HT"), ("conga", "CG"), ("cloche", "CB"),
    ("cymbale", "CY"), ("crash", "CR"), ("ride", "RD"), ("maracas", "MA"), ("claves", "CL"),
    ("basse longue", "BASS"), ("tambour fm", "FM"), ("fm colore", "FM C"), ("shaker", "SH"),
    ("percussion", "PC"), ("echantillon", "SMP"),
]


def drum_abbrev(name):
    f = fold(name).lower()
    for k, a in DRUM_ABBREV:
        if f.startswith(k):
            rest = f[len(k):].strip()
            return (a + (" " + rest if rest else "")).upper()
    return fold(name).upper()[:10]


def kit_pages(mod, P_, used, accent, extras=(), reserved=0):
    """Batteries, comme les boîtes à rythmes de la machine : une touche par son (1 BD, 2 SD, 3 CP...),
    la barre des touches défile. Chaque son a ses écrans d'environ 16 réglages (le son, puis filtre et
    ampli...) : rappuyer sur la touche passe à l'écran suivant, les 16 Q-Links suivent l'écran.
    Les touches sont limitées à 32 : les réglages communs du kit vont sur le dernier écran de chaque son."""
    params = mod["params"]
    names = {p["id"]: p["name"] for p in params}
    pads, kitp, skipped = {}, [], []
    padnames = mod.get("padNames") or []
    for p in params:
        pid = p["id"]
        if pid not in P_ or pid in used:
            continue
        m = PAD_RE.match(pid)
        if m and (p.get("name") or "").strip() in ("-", "—", "–", ""):
            skipped.append(pid)   # réglage sans rôle pour ce son (nom « - »)
        elif m:
            pads.setdefault(int(m.group(1) or m.group(2)), []).append(pid)
        elif _re.match(r"^e\d+", pid):
            skipped.append(pid)   # pas de séquenceur sur la machine (Pieuvre 8 : verrous des pas)
        else:
            kitp.append(pid)
    if not pads:
        return [], None
    used.update(kitp)
    used.update(skipped)
    for v in pads.values():
        used.update(v)
    common = [make_ctl(P_[i], "", names[i]) for i in kitp] + [make_ctl(q, "", q.name) for q in extras]
    total = 32 - reserved
    base = max(1, min(8, total // max(1, len(pads))))
    extra = max(0, total - base * len(pads)) if base < 8 else 0
    pad_tabs = []
    for pi, n in enumerate(sorted(pads)):
        per_pad = base + (1 if pi < extra else 0)
        pname = padnames[n - 1] if n - 1 < len(padnames) else "Pad %d" % n
        ctls = [make_ctl(P_[i], "", names[i]) for i in pads[n]]   # les plus importants d'abord
        if len(ctls) > per_pad * 40:
            # trop de réglages pour les écrans de ce son (Pieuvre 8 : 155 par piste) : les valeurs des pas
            # des LFO pas à pas restent réglables dans GOLEM, pas sur l'écran de la MPC
            ctls = [c for c in ctls if not _re.search(r"_lf\d_s\d+$", c.p.id)]
        k = max(1, min(per_pad, -(-len(ctls) // 16)))
        size = -(-len(ctls) // k)
        chunks = [ctls[i:i + size] for i in range(0, len(ctls), size)][:k]
        chunks[-1] = chunks[-1] + ctls[size * k:]
        if common:
            chunks[-1] = chunks[-1] + [_copy.copy(c) for c in common]
        # le type de filtre va sur l'écran de la coupure et de la résonance (sinon on ne sait pas ce qu'on
        # règle) : il change de place avec le groupe d'étouffement, un menu de même taille
        def _lab(c):
            return fold(c.label or c.p.name or "").lower()
        def _find(pred):
            for ci, ch in enumerate(chunks):
                for k, c in enumerate(ch):
                    if pred(_lab(c)):
                        return ci, k
            return None
        ft = _find(lambda l: "type" in l and "filtre" in l)
        cut = _find(lambda l: "coupure" in l)
        if ft and cut and ft[0] != cut[0]:
            ch = _find(lambda l: "etouf" in l)
            if ch and ch[0] == cut[0]:
                chunks[ft[0]][ft[1]], chunks[ch[0]][ch[1]] = chunks[ch[0]][ch[1]], chunks[ft[0]][ft[1]]
        ab = drum_abbrev(pname)
        tab = ab if (ab.endswith(" %d" % n) or ab == "T%d" % n) else "%d %s" % (n, ab)
        screens = []
        for ci, ch in enumerate(chunks):
            env = detect_adsr(ch)
            if env:
                ch = [c for c in ch if c not in env.values()]
            title = (pname if ab == "T%d" % n else "%d · %s" % (n, pname)) + (" · %d/%d" % (ci + 1, len(chunks)) if len(chunks) > 1 else "")
            screens.append([Panel(title, TONES[(n - 1) % len(TONES)], ch, env)])
        pad_tabs.append((tab, screens))
    return pad_tabs, None


FACE_CAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", "_facecap")  # face_capture.py
FACE_HEAD = 46


def load_face_cap(key):
    f = os.path.join(FACE_CAP, key, "face.json")
    if not os.path.isfile(f):
        return None
    cap = json.load(open(f, encoding="utf-8"))
    cap["dir"] = os.path.join(FACE_CAP, key)
    return cap


def _css_colour(c, default=(235, 235, 235, 255)):
    m = _re.match(r"rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)", c or "")
    return (int(float(m.group(1))), int(float(m.group(2))), int(float(m.group(3))), 255) if m else default


def split_face_cols(cap, pn, max_w=1000):
    """Panneau très large (une longue rangée) : coupé en morceaux côte à côte, empilés ensuite à l'écran
    pour garder des réglages lisibles."""
    import math
    W0 = pn["rect"][2]
    k = math.ceil(W0 / max_w)
    if k <= 1 or len(pn["ctls"]) < 4:
        return [pn]
    from PIL import Image
    xs = sorted(pn["ctls"], key=lambda c: c["rect"][0])
    cuts = [0.0]
    for j in range(1, k):
        target = W0 * j / k
        # coupe dans l'espace libre le plus proche de la cible, entre deux réglages
        best, bd = None, 1e9
        for a, b in zip(xs, xs[1:]):
            right = max(a["rect"][0] + a["rect"][2], (a["value"][0] + a["value"][2]) if a.get("value") else 0)
            left = min(b["rect"][0], b["value"][0] if b.get("value") else 1e9)
            if left - right < 4:
                continue
            mid = (right + left) / 2
            if abs(mid - target) < bd:
                best, bd = mid, abs(mid - target)
        if best is None or best <= cuts[-1] + 40:
            return [pn]
        cuts.append(best)
    cuts.append(W0)
    im = Image.open(os.path.join(cap["dir"], pn["png"]))
    f = im.width / pn["rect"][2]
    out = []
    for j in range(k):
        x0, x1 = cuts[j], cuts[j + 1]
        cs = []
        for c in pn["ctls"]:
            cx = c["rect"][0] + c["rect"][2] / 2
            if x0 <= cx < x1:
                c2 = dict(c, rect=[c["rect"][0] - x0, c["rect"][1], c["rect"][2], c["rect"][3]])
                if c.get("value"):
                    c2["value"] = [c["value"][0] - x0, c["value"][1], c["value"][2], c["value"][3]]
                cs.append(c2)
        if not cs:
            continue
        name = "%s_col%d.png" % (os.path.splitext(pn["png"])[0], j)
        im.crop((round(x0 * f), 0, round(x1 * f), im.height)).save(os.path.join(cap["dir"], name))
        out.append(dict(pn, png=name, rect=[pn["rect"][0] + x0, pn["rect"][1], x1 - x0, pn["rect"][3]], ctls=cs))
    return out


def split_face_panel(cap, pn, P_, cap_n=16):
    """Groupes de morceaux : une rangée coupée en colonnes reste ensemble sur le même écran."""
    return [split_face_cols(cap, part) for part in split_face_rows(cap, pn, P_, cap_n)]


def split_face_rows(cap, pn, P_, cap_n=16):
    """Panneau de plus de 16 réglages : découpé par rangées de réglages (image recadrée), 16 au plus par part."""
    ctls = [c for c in pn["ctls"] if c.get("id") and c["id"] in P_]
    if len({c["id"] for c in ctls}) <= cap_n:
        return [pn]
    from PIL import Image
    rows = []
    for c in sorted(ctls, key=lambda c: c["rect"][1] + c["rect"][3] / 2):
        cy = c["rect"][1] + c["rect"][3] / 2
        if rows and abs(rows[-1]["cy"] - cy) < 30:
            rows[-1]["ctls"].append(c)
        else:
            rows.append({"cy": cy, "ctls": [c]})
    def ext(r):
        top = min(c["rect"][1] for c in r["ctls"]) - 24
        bot = max(max(c["rect"][1] + c["rect"][3], (c["value"][1] + c["value"][3]) if c.get("value") else 0) for c in r["ctls"]) + 4
        return top, bot
    groups, cur, n = [], [], 0
    for r in rows:
        k = len(r["ctls"])
        if cur and n + k > cap_n:
            groups.append(cur); cur, n = [], 0
        cur.append(r); n += k
    if cur:
        groups.append(cur)
    if len(groups) == 1:
        return [pn]
    im = Image.open(os.path.join(cap["dir"], pn["png"]))
    f = im.width / pn["rect"][2]
    out = []
    for gi, g in enumerate(groups):
        top = 0 if gi == 0 else (ext(groups[gi - 1][-1])[1] + ext(g[0])[0]) / 2
        bot = pn["rect"][3] if gi == len(groups) - 1 else (ext(g[-1])[1] + ext(groups[gi + 1][0])[0]) / 2
        top, bot = max(0, top), min(pn["rect"][3], bot)
        name = "%s_part%d.png" % (os.path.splitext(pn["png"])[0], gi)
        im.crop((0, round(top * f), im.width, round(bot * f))).save(os.path.join(cap["dir"], name))
        cs = []
        for r in g:
            for c in r["ctls"]:
                c2 = dict(c, rect=[c["rect"][0], c["rect"][1] - top, c["rect"][2], c["rect"][3]])
                if c.get("value"):
                    c2["value"] = [c["value"][0], c["value"][1] - top, c["value"][2], c["value"][3]]
                cs.append(c2)
        out.append(dict(pn, png=name, rect=[pn["rect"][0], pn["rect"][1] + top, pn["rect"][2], bot - top], ctls=cs,
                        title=pn["title"] + (" (suite)" if gi else "")))
    return out


def face_theme(cap):
    """Thème tiré de la façade capturée : fond, plus grand bouton rotatif, tirette."""
    if not cap:
        return None
    if cap.get("fx"):
        th = {"bg": cap.get("bg"), "dir": cap["dir"], "unit": os.path.join(cap["dir"], cap["unit"]) if cap.get("unit") else None}
        if cap.get("knob"):
            th["knob"] = dict(cap["knob"], dir=cap["dir"])
            from PIL import Image
            im = Image.open(os.path.join(cap["dir"], cap["knob"]["png"])).convert("RGBA")
            th["panel"] = im.getpixel((2, 2))[:3] + (255,)
        return th
    knobs, faders = [], []
    for pg in cap["pages"]:
        for pn in pg["panels"]:
            for c in pn["ctls"]:
                st = c.get("strip")
                if st and c["kind"] == "knob":
                    knobs.append(st)
                elif st and c["kind"] == "fader":
                    faders.append(st)
    th = {"bg": cap["pages"][0]["bg"] if cap["pages"] else None, "dir": cap["dir"]}
    if knobs:
        th["knob"] = dict(max(knobs, key=lambda x: x["w"] * x["h"]), dir=cap["dir"])
        from PIL import Image
        im = Image.open(os.path.join(cap["dir"], th["knob"]["png"])).convert("RGBA")
        th["panel"] = im.getpixel((1, 1))[:3] + (255,)   # fond des panneaux : celui derrière les boutons
    if faders:
        th["fader"] = dict(max(faders, key=lambda x: x["h"]), dir=cap["dir"])
    return th


def theme_strip(skin, info, w, h, round_mask=False):
    """Image animée capturée, remise à la taille w × h (round_mask : fond fondu autour du bouton)."""
    from PIL import Image, ImageDraw, ImageFilter
    name = "t_%s_%dx%d%s.png" % (os.path.splitext(info["png"])[0], w, h, "m" if round_mask else "")
    path = os.path.join(skin.out, name)
    if not os.path.isfile(path):
        sheet = Image.open(os.path.join(info["dir"], info["png"])).convert("RGBA")
        out = Image.new("RGBA", (w, h * info["frames"]))
        for i in range(info["frames"]):
            fr = sheet.crop((0, i * info["h"], info["w"], (i + 1) * info["h"]))
            if round_mask and fr.width > fr.height * 1.15:
                side = fr.height
                x0 = (fr.width - side) // 2
                fr = fr.crop((x0, 0, x0 + side, side))
            k = min(w / fr.width, h / fr.height)
            fr = fr.resize((max(1, round(fr.width * k)), max(1, round(fr.height * k))), Image.LANCZOS)
            if round_mask:
                m = Image.new("L", fr.size, 0)
                r = min(fr.size) * 0.47
                cx, cy = fr.width / 2, fr.height / 2
                ImageDraw.Draw(m).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
                m = m.filter(ImageFilter.GaussianBlur(max(1, r * 0.06)))
                fr.putalpha(Image.composite(fr.getchannel("A"), Image.new("L", fr.size, 0), m))
            out.paste(fr, ((w - fr.width) // 2, i * h + (h - fr.height) // 2))
        out.save(path, optimize=True)
    return name, info["frames"]


def face_tabs(cap, P_):
    """Pages de la façade GOLEM capturée → écrans de 16 réglages au plus, panneaux à l'échelle commune.
    Retourne [(nom de touche, [écran]), ...] et les identifiants placés."""
    used = set()
    tabs = []
    allp = [pn for pg in cap["pages"] for pn in pg["panels"]]
    if not allp:
        return [], used
    W, H = 1280 - 24, 628 - FACE_HEAD - 16
    mw = max(pn["rect"][2] for pn in allp)
    mh = max(pn["rect"][3] for pn in allp)
    sc = min(0.9, W / mw, H / mh)
    for pg in cap["pages"]:
        screens, cur, q = [], [], 0
        groups = []
        for pn in pg["panels"]:
            groups.extend(split_face_panel(cap, pn, P_))
        for grp in groups:
            parts = []
            for pn in grp:
                ctls = [c for c in pn["ctls"] if c.get("id") and c["id"] in P_]
                if ctls:
                    parts.append(dict(pn, ctls=ctls))
            if not parts:
                continue
            ids_ = {c["id"] for pn in parts for c in pn["ctls"]}
            used |= ids_
            n = len(ids_)
            if cur and q + n > 16:
                screens.append(cur)
                cur, q = [], 0
            cur.extend(parts)
            q += n
        if cur:
            screens.append(cur)
        # rangement : pour chaque écran, la plus grande échelle où les panneaux tiennent en étagères
        out = []
        for scr in screens:
            def shelve(k):
                shelves, shelf, sx, y, sh = [], [], 0, 0, 0
                for pn in scr:
                    w, h = pn["rect"][2] * k, pn["rect"][3] * k
                    if w > W:
                        return None
                    if shelf and sx + w > W:
                        shelves.append((shelf, sh)); y += sh + 10; shelf, sx, sh = [], 0, 0
                    shelf.append(pn); sx += w + 10; sh = max(sh, h)
                if shelf:
                    shelves.append((shelf, sh)); y += sh
                return shelves if y <= H else None
            k, best = 1.4, None
            while k >= 0.3:
                best = shelve(k)
                if best:
                    break
                k -= 0.02
            if best:
                out.append({"shelves": best, "bg": pg["bg"], "label": pg["label"], "sc": round(k, 2), "dir": cap["dir"]})
        if out:
            tabs.append((pg["label"], out))
    return tabs, used


def face_order(scr, P_):
    """Réglages d'un écran de façade pour les Q-Links : ceux qui tournent d'abord."""
    turn, other = [], []
    for shelf, _ in scr["shelves"]:
        for pn in shelf:
            for c in pn["ctls"]:
                p = P_[c["id"]]
                if c["kind"] in ("knob", "fader"):
                    p._turn = True
                    if p not in turn:
                        turn.append(p)
                elif p not in other:
                    other.append(p)
    return turn + [p for p in other if p not in turn]


def _strip(skin, scr, info, sc):
    """Image animée capturée (double définition) remise à l'échelle de l'écran."""
    from PIL import Image
    name = "f_%s_%d.png" % (os.path.splitext(info["png"])[0], round(sc * 100))
    path = os.path.join(skin.out, name)
    fw, fh = max(1, round(info["w"] * sc / 2)), max(1, round(info["h"] * sc / 2))
    if not os.path.isfile(path):
        sheet = Image.open(os.path.join(scr["dir"], info["png"])).convert("RGBA")
        out = Image.new("RGBA", (fw, fh * info["frames"]))
        for i in range(info["frames"]):
            fr = sheet.crop((0, i * info["h"], info["w"], (i + 1) * info["h"])).resize((fw, fh), Image.LANCZOS)
            out.paste(fr, (0, i * fh))
        out.save(path, optimize=True)
    return name, fw, fh


def draw_face_screen(skin, pg, scr, P_, title, accent, page_label):
    """Écran à l'image de la façade GOLEM : panneaux capturés, réglages vivants posés dessus."""
    from PIL import Image
    pg.face = True
    c = pg.canvas
    S = K.SS
    bg = tuple(int(scr["bg"][i:i + 2], 16) for i in (1, 3, 5)) + (255,)
    c.img.paste(bg, (0, 0, c.img.width, c.img.height))
    # en-tête : nom de l'instrument, page
    light = (236, 236, 240, 255) if sum(bg[:3]) < 380 else (25, 25, 28, 255)
    c.d.rectangle((0, (FACE_HEAD - 2) * S, c.img.width, FACE_HEAD * S), fill=accent)
    c.d.text((18 * S, FACE_HEAD * S / 2), title.upper(), font=K.font(26 * S, "Bold"), fill=light, anchor="lm")
    c.d.text((1262 * S, FACE_HEAD * S / 2), page_label, font=K.font(17 * S, "Regular"), fill=light, anchor="rm")
    sc = scr["sc"]
    W = 1280 - 24
    H = 628 - FACE_HEAD - 16
    used_h = sum(sh for _, sh in scr["shelves"]) + 10 * (len(scr["shelves"]) - 1)
    y = FACE_HEAD + 8 + max(0, (H - used_h) / 2)
    for shelf, sh in scr["shelves"]:
        used_w = sum(pn["rect"][2] * sc for pn in shelf) + 10 * (len(shelf) - 1)
        x = 12 + (W - used_w) / 2
        for pn in shelf:
            im = Image.open(os.path.join(scr["dir"], pn["png"])).convert("RGBA")
            pw, ph = pn["rect"][2] * sc, pn["rect"][3] * sc
            im = im.resize((max(1, round(pw * S)), max(1, round(ph * S))), Image.LANCZOS)
            c.img.alpha_composite(im, (round(x * S), round(y + (sh - ph) / 2) * S if False else round((y + (sh - ph) / 2) * S)))
            oy = y + (sh - ph) / 2
            for ctl in pn["ctls"]:
                place_face_ctl(skin, pg, scr, ctl, P_[ctl["id"]], x, oy, sc)
            x += pw + 10
        y += sh + 10


def place_face_ctl(skin, pg, scr, ctl, p, ox, oy, sc):
    kind = ctl["kind"]
    rx, ry, rw, rh = ctl["rect"]
    x, y, w, h = ox + rx * sc, oy + ry * sc, rw * sc, rh * sc
    pg._choice(p)
    if kind == "menu":
        tag = "fm_%d_%d" % (round(w), round(h))
        if tag not in skin.defs:
            col = _css_colour(ctl.get("vcol"))
            size = max(12, (ctl.get("vfs") or 14) * sc * 1.25)
            children = [skin.focus(w, h), ent("value", "Label", label_value(size, K.argb(col)), rect(0, 0, w, h))]
            acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                    GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
            skin.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
        pg._add(p, tag, x, y, w, h)
        return
    info = ctl.get("strip")
    if not info:
        return
    name, fw, fh = _strip(skin, scr, info, sc)
    n = info["frames"]
    tag = "fs_%s_%s" % (os.path.splitext(name)[0], kind)
    if tag not in skin.defs:
        drag = "Vertical" if kind == "fader" else None
        children = [skin.focus(fw, fh), ent("strip", "Knob", GEN_UI.knob_filmstrip_data(name, n, drag), rect(0, 0, fw, fh))]
        if kind in ("toggle",) or (kind == "switch" and n == 2):
            acts = [GEN_UI.action("Mouse Down", "Toggle Switch", "Data", ""), GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Enter Pressed", "Toggle Switch", "Data", "")]
        elif kind == "switch" or len(p.choices) > 4:
            acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                    GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
        else:
            acts = skin.actions("knob overlay bipolar" if p.bipolar else "knob overlay")
        skin.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
    pg._add(p, tag, x, y, fw, fh)
    if ctl.get("value") and kind in ("knob", "fader"):
        vx, vy, vw, vh = ctl["value"]
        col = _css_colour(ctl.get("vcol"))
        size = max(11, (ctl.get("vfs") or 12) * sc * 1.3)
        pg.children.append(ent(p.id + " val", "Label", label_value(size, K.argb(col)),
                               rect(ox + vx * sc - 10, oy + vy * sc - 2, vw * sc + 20, vh * sc + 4),
                               invalid="Hide", remap=[("Data", p.handle)]))



# =============================================================================================
# Écrans « remplis » : panneaux redessinés aux couleurs de la façade GOLEM, réglages capturés
# (boutons, tirettes, sélecteurs) agrandis pour occuper toute la largeur et la hauteur de l'écran.
# Noms au-dessus, valeurs en dessous (jamais par-dessus), texte foncé sur panneau clair.
# =============================================================================================
FLOW_GAP = 8


def _lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def css_any(c):
    """Couleur CSS (rgb(), oklch(), #hex) → (r, g, b, 255) ou None."""
    if not c:
        return None
    m = _re.match(r"oklch\(\s*([\d.]+)(%?)\s+([\d.]+)\s+([\d.]+)", c)
    if m:
        L = float(m.group(1)) / (100 if m.group(2) else 1)
        return K.oklch(L, float(m.group(3)), float(m.group(4)))
    m = _re.match(r"rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)", c)
    if m:
        return (int(float(m.group(1))), int(float(m.group(2))), int(float(m.group(3))), 255)
    if c.startswith("#") and len(c) >= 7:
        return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5)) + (255,)
    return None


class FItem:
    """Un réglage d'écran rempli. cw, ch : taille du visuel à l'échelle 1 (pixels CSS de la façade)."""
    def __init__(self, kind, p, label, cw=0, ch=0, strip=None, vcol=None, words=None, big=False):
        self.kind, self.p, self.label, self.cw, self.ch = kind, p, label, cw, ch
        self.strip, self.vcol, self.words, self.big = strip, vcol, words, big


class FPanel:
    def __init__(self, title, items, fill, tone):
        self.title, self.items, self.fill, self.tone = title, items, fill, tone


def _strip_corner_colour(info):
    from PIL import Image
    try:
        im = Image.open(os.path.join(info["dir"], info["png"])).convert("RGB")
    except OSError:
        return None
    return im.getpixel((1, 1)) + (255,)


def _dominant(path):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    im.thumbnail((160, 160))
    cols = im.quantize(colors=8).convert("RGB").getcolors(100000)
    return max(cols)[1] + (255,) if cols else None


def fpanels_from_face(cap, P_, used, accent):
    """Panneaux capturés de la façade → FPanel (réglages dans l'ordre de la façade)."""
    out = []
    knobs = []
    for pg in cap["pages"]:
        for pn in pg["panels"]:
            items = []
            corners = []
            for c in pn["ctls"]:
                pid = c.get("id")
                if not pid or pid not in P_ or pid in used:
                    continue
                kind = c["kind"]
                st = dict(c["strip"], dir=cap["dir"]) if c.get("strip") else None
                if kind != "menu" and not st:
                    continue
                used.add(pid)
                rw, rh = c["rect"][2], c["rect"][3]
                if st:
                    rw, rh = st["w"] / 2, st["h"] / 2
                    cc = _strip_corner_colour(st)
                    if cc:
                        corners.append(cc[:3])
                if kind == "knob":
                    knobs.append(rw)
                p = P_[pid]
                if kind == "menu" and 2 <= len(p.choices) <= 4 and max(K.text_px(w, 15) for w in p.choices) <= 130:
                    # peu de choix courts : boutons visibles plutôt qu'une liste à ouvrir
                    items.append(FItem("btns", p, c.get("label") or p.name, words=list(p.choices)))
                    continue
                items.append(FItem(kind, p, c.get("label") or p.name, rw, rh, st, css_any(c.get("vcol"))))
            if not items:
                continue
            fill = None
            if corners:
                fill = max(set(corners), key=corners.count) + (255,)
            if fill is None:
                fill = _dominant(os.path.join(cap["dir"], pn["png"])) or K.PANEL
            out.append(FPanel(pn["title"], items, fill, accent))
    kref = sorted(knobs)[len(knobs) // 2] if knobs else 56
    return out, kref


def fpanel_from_generic(pn, kref, theme, fill):
    """Panneau de l'habillage automatique (sections, batterie, extras) → FPanel."""
    items = []
    for c in ([pn.env[k] for k in "adsr"] if pn.env else []) + list(pn.ctls):
        if c.label:
            c.label = _re.sub(r"^T\d+ · ", "", c.label)
    ctls = []
    if pn.env:
        ctls += [pn.env[k] for k in "adsr"]
    ctls += [c for c in pn.ctls if c.p is not None]
    for c in ctls:
        k = c.kind
        if k in ("small", "big"):
            s = kref * (1.25 if k == "big" else 1.0)
            items.append(FItem("gknob", c.p, c.label, s, s, (theme or {}).get("knob"), big=(k == "big")))
        elif k == "fader":
            info = (theme or {}).get("fader")
            if info:
                items.append(FItem("fader", c.p, c.label, info["w"] / 2 * 110 / (info["h"] / 2), 110, info))
            else:
                items.append(FItem("gfader", c.p, c.label, 26, 110))
        elif k == "menu":
            items.append(FItem("menu", c.p, c.label, kref * 2.4, kref * 0.5))
        elif k == "toggle":
            items.append(FItem("gtoggle", c.p, c.label))
        elif k == "btns":
            items.append(FItem("btns", c.p, c.label, words=c.words))
        elif k == "file":
            items.append(FItem("file", c.p, c.label))
        else:
            items.append(FItem("gknob", c.p, c.label, kref, kref, (theme or {}).get("knob")))
    return FPanel(pn.title, items, fill, pn.tone)


def _fonts(f):
    return max(12, min(19, round(10 * f))), max(12, min(18, round(9.5 * f)))


def item_box(it, f, fmax_px=118):
    """Taille de la case d'un réglage à l'échelle f : (w, h, vis_w, vis_h, label_h, value_h)."""
    fs, vfs = _fonts(f)
    k = it.kind
    lh = fs + 6 if (it.label and k not in ("toggle", "gtoggle")) else 0
    vh_val = 0
    if k in ("knob", "gknob"):
        vw = vh = min(it.cw * f, fmax_px * (1.2 if it.big else 1.0))
        vh = min(it.ch * f, vh) if k == "knob" else vw
        vh_val = vfs + 6
    elif k in ("fader", "gfader"):
        vh = min(it.ch * f, 230)
        vw = it.cw * vh / it.ch
        vh_val = vfs + 6
    elif k in ("switch", "toggle"):
        sc = min(f, 2.4)
        vw, vh = it.cw * sc, it.ch * sc
    elif k == "menu":
        vw, vh = max(130, min(it.cw * f, 300)), max(30, min(it.ch * f, 46))
    elif k == "gtoggle":
        bfs = max(13, min(19, round(9.5 * f)))
        vw, vh = K.text_px(it.label, bfs) + 34, max(32, min(46, 21 * f))
    elif k == "btns":
        bfs = max(13, min(19, round(9.5 * f)))
        bw = max(52, max(K.text_px(w, bfs) for w in it.words) + 24)
        vw, vh = len(it.words) * bw + (len(it.words) - 1) * 6, max(32, min(46, 21 * f))
    elif k == "file":
        vw, vh = 300, 46
    else:
        vw, vh = it.cw * f, it.ch * f
    lw = K.text_px(it.label or "", fs, "Medium") if lh else 0
    w = max(vw + 16, min(lw + 12, vw + 70))
    h = lh + vh + vh_val + 6
    return w, h, vw, vh, lh, vh_val


PAN_TITLE, PAN_PAD, ROW_GAP = 30, 10, 4


def panel_rows(pn, f, r):
    """Panneau en r rangées : (w, h, rangées [[(item, box)...]])."""
    boxes = [(it, item_box(it, f)) for it in pn.items]
    n = len(boxes)
    per = -(-n // r)
    rows = [boxes[i:i + per] for i in range(0, n, per)]
    w = max(sum(b[0] for _, b in row) for row in rows) + 2 * PAN_PAD
    w = max(w, min(K.text_px(short_title(pn.title).upper(), 14) + 50, 360))
    h = PAN_TITLE + sum(max(b[1] for _, b in row) for row in rows) + ROW_GAP * (len(rows) - 1) + PAN_PAD
    return w, h, rows


def _partitions(n):
    """Coupures en étagères consécutives des n panneaux."""
    if n <= 1:
        yield [list(range(n))]
        return
    for mask in range(1 << (n - 1)):
        parts, cur = [], [0]
        for i in range(1, n):
            if mask >> (i - 1) & 1:
                parts.append(cur)
                cur = [i]
            else:
                cur.append(i)
        parts.append(cur)
        yield parts


def flow_layout(panels, W, H, fmax=2.0, fmin=0.45):
    """Plus grande échelle où les panneaux tiennent en étagères ; renvoie (f, [[(panneau, (w, h, rangées))]], hauteurs)."""
    n = len(panels)
    f = fmax
    while f >= fmin - 1e-6:
        confs = []
        for pn in panels:
            cs = []
            for r in range(1, min(len(pn.items), 6) + 1):
                cs.append(panel_rows(pn, f, r))
            confs.append(cs)
        best = None
        if n <= 9:
            parts_iter = _partitions(n)
        else:   # beaucoup de panneaux : k étagères de tailles voisines
            parts_iter = []
            for k in range(1, n + 1):
                b = [round(j * n / k) for j in range(k + 1)]
                parts_iter.append([list(range(b[j], b[j + 1])) for j in range(k) if b[j + 1] > b[j]])
        for parts in parts_iter:
            total_h, shelves, ok, score = 0, [], True, 0.0
            for part in parts:
                cands = sorted({c[1] for i in part for c in confs[i]})
                chosen = None
                for hh in cands:
                    pick = []
                    for i in part:
                        fit = [c for c in confs[i] if c[1] <= hh]
                        if not fit:
                            pick = None
                            break
                        pick.append(min(fit, key=lambda c: c[0]))
                    if pick is None:
                        continue
                    wsum = sum(c[0] for c in pick) + FLOW_GAP * (len(pick) - 1)
                    if wsum <= W:
                        chosen = (hh, pick, wsum)
                        break
                if not chosen:
                    ok = False
                    break
                hh, pick, wsum = chosen
                total_h += hh
                shelves.append((part, pick, hh, wsum))
            if not ok:
                continue
            total_h += FLOW_GAP * (len(parts) - 1)
            if total_h > H:
                continue
            score = sum(ws / W * hh for _, _, hh, ws in shelves) / H + total_h / H * 0.5
            if best is None or score > best[0]:
                best = (score, shelves, total_h)
        if best:
            return f, best[1], best[2]
        f -= 0.05
    return None


def _flow_strip(skin, info, w, h, round_mask=False):
    return theme_strip(skin, info, max(4, round(w)), max(4, round(h)), round_mask)


def _flow_def_strip(skin, p, kind, name, frames, w, h):
    tag = "fl_%s_%s_%d" % (os.path.splitext(name)[0], kind, len(p.choices) > 4)
    if tag not in skin.defs:
        drag = "Vertical" if kind in ("fader",) else None
        children = [skin.focus(w, h), ent("strip", "Knob", GEN_UI.knob_filmstrip_data(name, frames, drag), rect(0, 0, w, h))]
        if kind == "toggle" or (kind == "switch" and frames == 2):
            acts = [GEN_UI.action("Mouse Down", "Toggle Switch", "Data", ""), GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Enter Pressed", "Toggle Switch", "Data", "")]
        elif kind == "switch" or len(p.choices) > 4:
            acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                    GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
        else:
            acts = skin.actions("knob overlay bipolar" if p.bipolar else "knob overlay")
        skin.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
    return tag


def _flow_def_menu(skin, w, h, fs, tone):
    w, h = round(w), round(h)
    tag = "flm_%d_%d_%d_%s" % (w, h, fs, skin.colour_tag(tone))
    if tag not in skin.defs:
        box = "menu_%d_%d_%s.png" % (w, h, skin.colour_tag(tone))
        K.menu_box(os.path.join(skin.out, box), w, h, tone)
        children = [skin.focus(w, h),
                    ent("box", "Image", O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", box)), rect(0, 0, w, h)),
                    ent("value", "Label", label_value(fs, K.argb((238, 238, 242, 255)), "left verticallyCentred"), rect(12, 0, w - 40, h))]
        acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
        skin.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
    return tag


def flow_place(skin, pg, it, box, x, y, cw, f, colours):
    """Pose un réglage dans sa case (x, y, largeur cw)."""
    w, h, vw, vh, lh, vval = box
    label_c, value_c = colours[0], colours[1]
    p = it.p
    fs, vfs = _fonts(f)
    pg._choice(p)
    if lh:
        pg.canvas.fit_text(x + 2, y + lh / 2, it.label, fs, cw - 4, label_c)
    vx, vy = x + (cw - vw) / 2, y + lh + 2
    k = it.kind
    if k in ("knob", "gknob", "fader", "switch", "toggle"):
        info = it.strip
        if info is None and k == "gknob":
            strip = "knob%d_%s_%s.png" % (round(vw), skin.colour_tag(it_tone(it, skin)), "bi" if p.bipolar else "uni")
            K.knob_strip(os.path.join(skin.out, strip), round(vw), it_tone(it, skin), p.bipolar)
            name, frames = strip, K.KNOB_FRAMES
        else:
            name, frames = _flow_strip(skin, info, vw, vh, round_mask=(k in ("knob", "gknob")))
        tag = _flow_def_strip(skin, p, "knob" if k == "gknob" else k, name, frames, round(vw), round(vh))
        pg._add(p, tag, vx, vy, round(vw), round(vh))
        if k in ("knob", "gknob", "fader"):
            p._turn = True
            col = it.vcol if it.vcol and abs(_lum(it.vcol) - _lum(colours[2])) > 90 else value_c
            pg.children.append(ent(p.id + " val", "Label", label_value(vfs, K.argb(col)),
                                   rect(x, vy + vh + 2, cw, vval), invalid="Hide", remap=[("Data", p.handle)]))
    elif k == "gfader":
        tag = skin.def_fader(round(vw), round(vh) - 8, it_tone(it, skin), p.bipolar)
        pg._add(p, tag, vx - 8, vy, round(vw) + 16, round(vh) + 34)
        p._turn = True
    elif k == "menu":
        tag = _flow_def_menu(skin, vw, vh, max(13, min(19, round(vh * 0.45))), it_tone(it, skin))
        pg._add(p, tag, vx, vy, round(vw), round(vh))
    elif k in ("gtoggle", "btns"):
        words = [it.label] if k == "gtoggle" else it.words
        bw = round(vw) if k == "gtoggle" else round((vw - 6 * (len(words) - 1)) / len(words))
        tag, (tw, th) = skin.def_buttons(words, bw, round(vh), it_tone(it, skin))
        pg._add(p, tag, vx, vy, tw, th)
    elif k == "file":
        tag = skin.def_number(round(vw), round(vh), it_tone(it, skin), size=19)
        pg._add(p, tag, vx, vy, round(vw), round(vh))
        p._turn = True


def it_tone(it, skin):
    return getattr(it, "tone", None) or skin.accent


def flow_text_colours(fill):
    if _lum(fill) > 140:
        return (34, 34, 40, 255), (18, 18, 22, 255), fill
    return (200, 202, 210, 255), (240, 240, 244, 255), fill


def draw_flow_screen(skin, pg, panels, title, accent, page_label, area, bg, unit=None, fmax=2.6, hint=False):
    """Écran rempli : en-tête GOLEM, panneaux en étagères à la plus grande échelle, étirés à toute la surface."""
    from PIL import Image
    pg.face = True
    c = pg.canvas
    S = K.SS
    c.img.paste(bg, (0, 0, c.img.width, c.img.height))
    light = (236, 236, 240, 255) if _lum(bg) < 140 else (25, 25, 28, 255)
    c.d.rectangle((0, (FACE_HEAD - 2) * S, c.img.width, FACE_HEAD * S), fill=accent)
    c.d.text((18 * S, FACE_HEAD * S / 2), title.upper(), font=K.font(26 * S, "Bold"), fill=light, anchor="lm")
    c.d.text((1262 * S, FACE_HEAD * S / 2), K.safe(page_label), font=K.font(17 * S, "Regular"), fill=light, anchor="rm")
    x0, y0, x1, y1 = area
    if hint:
        y1 -= 18
    if unit:
        u = Image.open(unit).convert("RGBA")
        hh = y1 - y0
        ww = round(u.width * hh / u.height)
        if ww > x0 - 24:
            ww = x0 - 24
            hh = round(u.height * ww / u.width)
        u = u.resize((ww * S, hh * S), Image.LANCZOS)
        c.img.alpha_composite(u, (round((x0 - ww) / 2) * S, round(y0 + (y1 - y0 - hh) / 2) * S))
    W, H = x1 - x0, y1 - y0
    lay = flow_layout(panels, W, H, fmax)
    if not lay:
        lay = flow_layout(panels, W, H, fmax, fmin=0.2)
    if not lay:
        raise SystemExit("skins: écran impossible à ranger (%s, %s)" % (title, page_label))
    f, shelves, total_h = lay
    extra_h = (H - total_h) / len(shelves)
    y = y0
    for part, pick, hh, wsum in shelves:
        sh = hh + extra_h
        extra_w = W - wsum
        x = x0
        for i, conf in zip(part, pick):
            pn = panels[i]
            pw = conf[0] + extra_w * conf[0] / (wsum - FLOW_GAP * (len(pick) - 1))
            label_c, value_c, fill = flow_text_colours(pn.fill)
            edge = K.mix(fill, (0, 0, 0, 255), 0.35) if _lum(fill) > 140 else K.mix(fill, (255, 255, 255, 255), 0.16)
            K.rounded(c.d, [x * S, y * S, (x + pw) * S, (y + sh) * S], 10 * S, fill=fill, outline=edge, width=S)
            tone = pn.tone
            tcol = tone if abs(_lum(tone) - _lum(fill)) > 80 else value_c
            c.d.rounded_rectangle([(x + 12) * S, (y + 9) * S, (x + 16) * S, (y + 23) * S], radius=2 * S, fill=tone)
            ttl = short_title(pn.title).upper()
            while len(ttl) > 4 and K.text_px(ttl, 14) > pw - 40:
                ttl = ttl[:-2].rstrip() + "…"
            c.d.text(((x + 24) * S, (y + 16) * S), K.safe(ttl), font=K.font(14 * S, "Bold"), fill=tcol, anchor="lm")
            rows = conf[2]
            rows_h = sum(max(b[1] for _, b in row) for row in rows) + ROW_GAP * (len(rows) - 1)
            spare = sh - PAN_TITLE - PAN_PAD - rows_h
            gap_y = spare / (len(rows) + 1)
            ry = y + PAN_TITLE + gap_y
            for row in rows:
                rh = max(b[1] for _, b in row)
                rw = sum(b[0] for _, b in row)
                inner = pw - 2 * PAN_PAD
                spread = (inner - rw) / len(row)
                cx = x + PAN_PAD
                for it, b in row:
                    cw = b[0] + spread
                    it.tone = tone
                    flow_place(skin, pg, it, b, cx, ry + (rh - b[1]) / 2, cw, f, (label_c, value_c, fill))
                    cx += cw
                ry += rh + ROW_GAP + gap_y
            x += pw + FLOW_GAP
        y += sh + FLOW_GAP
    return f


def short_title(t):
    """Titre de panneau sans ses explications (après « : », « · » ou entre parenthèses)."""
    t = t or ""
    suite = t.endswith(" (suite)")
    t = t.replace(" (suite)", "")
    for sep in (" (", " : ", " · ", " - "):
        if sep in t and t.index(sep) >= 3:
            t = t[:t.index(sep)]
    return t + (" (suite)" if suite else "")


def flow_order(panels):
    """Q-Links : ce qui tourne d'abord, puis menus, puis boutons, dans l'ordre de l'écran."""
    turn, menus, push = [], [], []
    for pn in panels:
        for it in pn.items:
            if it.kind in ("knob", "gknob", "fader", "gfader", "file"):
                it.p._turn = True
                turn.append(it.p)
            elif it.kind in ("menu", "switch"):
                menus.append(it.p)
            else:
                push.append(it.p)
    return turn + menus + push


FAMILIES = [   # (clé, onglet, débuts de titres repliés)
    ("porteuse", "PORTEUSE", ("porteuse",)),
    ("samples", "SAMPLES", ("samples",)),
    ("osc", "OSC", ("oscill", "osc", "source", "operateur", "onde", "table", "generateur", "sous-osc", "fm", "harmon",
                    "partiel", "cordes", "voix 1", "couche")),
    ("mix", "MIX", ("melangeur", "mix", "niveaux", "bruit")),
    ("filtre", "FILTRE", ("filtre", "vcf", "passe", "formant", "resonateur")),
    ("env", "ENV", ("envelop", "env", "contour", "adsr", "vca", "ampli")),
    ("lfo", "LFO", ("lfo", "vibrato")),
    ("mod", "MATRICE", ("matrice", "routage", "modulation", "mod ", "mod.", "velocite", "aftertouch")),
    ("fx", "FX", ("effet", "chorus", "delay", "retard", "reverb", "echo", "distorsion", "phaser", "saturation", "ensemble")),
    ("jeu", "JEU", ("jeu", "arpege", "arp", "levier", "glisse", "portamento", "voix", "global", "sequence", "poly",
                    "clavier", "accord", "phrase", "paires", "groupes")),
    ("sortie", "SORTIE", ("sortie", "volume", "master", "plus")),
]


def family_of(title):
    t = fold(title or "").lower().strip()
    if t.startswith("en plus"):
        return "plus", "PLUS"
    for key, name, starts in FAMILIES:
        if any(t.startswith(s) for s in starts):
            if key == "mod" and not t.startswith("matrice"):
                name = "MOD"
            return key, name
    w = fold(title or "SON").upper().split(" ")[0] or "SON"
    w = {"TRANSPOSITION": "TRANSPO", "TRANSPOSITIONS": "TRANSPO"}.get(w, w)[:12]
    return "x_" + w, w


def item_rank(it):
    """Réglages principaux d'abord quand il faut en retirer (matrices : source, destination, profondeur)."""
    l = fold(it.label or "").lower()
    if any(w in l for w in ("dest", "source", "prof", "quant", "coupure", "niveau", "vitesse", "forme")):
        return 0
    if any(w in l for w in ("via", "fin", "phase", "retard", "fondu")):
        return 2
    return 1


def split_fpanel(pn, cap=16):
    if len(pn.items) <= cap:
        return [pn]
    k = -(-len(pn.items) // cap)
    size = -(-len(pn.items) // k)
    return [FPanel(pn.title + (" (suite)" if i else ""), pn.items[i * size:(i + 1) * size], pn.fill, pn.tone) for i in range(k)]


def group_tabs(fpanels, max_tabs=12, max_screens=32, per_tab=4, cap=16):
    """Panneaux → onglets par fonction (OSC, FILTRE, ENV...) ; chaque onglet a ses écrans de 16 réglages au plus
    (on rappuie sur la touche pour passer au suivant). 12 onglets au plus, 32 écrans au plus."""
    fams = []
    # panneaux sans famille connue et petits : avec leur voisin (le suivant s'ils sont en tête)
    keys = [family_of(pn.title) for pn in fpanels]
    for i, (k, nm) in enumerate(keys):
        if k.startswith("x_") and len(fpanels[i].items) <= 6:
            nb = [j for j in (list(range(i - 1, -1, -1)) + list(range(i + 1, len(keys)))) if not keys[j][0].startswith("x_")]
            if i == 0 or not any(j < i for j in nb):
                nb = [j for j in range(i + 1, len(keys)) if not keys[j][0].startswith("x_")] or nb
            if nb:
                keys[i] = keys[nb[0]]
    for pn, (key, name) in zip(fpanels, keys):
        for part in split_fpanel(pn, cap):
            if fams and fams[-1][0] == key:
                fams[-1][2].append(part)
                continue
            found = [f for f in fams if f[0] == key]
            if found:
                found[0][2].append(part)
            else:
                fams.append([key, name, [part]])

    def screens_of(pns):
        out, cur, q = [], [], 0
        for pn in pns:
            n = len(pn.items)
            if cur and q + n > cap:
                out.append(cur)
                cur, q = [], 0
            cur.append(pn)
            q += n
        if cur:
            out.append(cur)
        return out
    tabs = [[name, screens_of(pns)] for key, name, pns in fams]
    # petites familles (6 réglages ou moins) : rangées avec la voisine si l'écran reste à 16
    changed = True
    while changed:
        changed = False
        for i, (name, scr) in enumerate(tabs):
            n = sum(len(pn.items) for s in scr for pn in s)
            if len(scr) != 1 or n > 6 or len(tabs) == 1:
                continue
            for j in ([i - 1, i + 1] if i > 0 else [i + 1]):
                if 0 <= j < len(tabs):
                    tgt = tabs[j][1][-1] if j < i else tabs[j][1][0]
                    if sum(len(pn.items) for pn in tgt) + n <= cap and len(tgt) < 4:
                        if tabs[j][0] == "SORTIE":
                            tabs[j][0] = name
                        if j < i:
                            tgt.extend(scr[0])
                            if len(tabs[j][0]) + 1 + len(name) <= 12 and name not in ("SORTIE", tabs[j][0]):
                                tabs[j][0] += "+" + name
                        else:
                            tgt[0:0] = scr[0]
                            if len(tabs[j][0]) + 1 + len(name) <= 12 and name not in ("SORTIE", tabs[j][0]):
                                tabs[j][0] = name + "+" + tabs[j][0]
                        del tabs[i]
                        changed = True
                        break
            if changed:
                break
    # trop d'onglets : les deux voisins les plus légers partagent une touche (écrans à la suite)
    while len(tabs) > max_tabs:
        best, bi = None, 0
        for i in range(len(tabs) - 1):
            s = len(tabs[i][1]) + len(tabs[i + 1][1])
            if best is None or s < best:
                best, bi = s, i
        a, b = tabs[bi], tabs[bi + 1]
        tabs[bi:bi + 2] = [[(a[0] + "+" + b[0]) if len(a[0] + b[0]) < 12 else a[0], a[1] + b[1]]]
    # écrans par touche et en tout : on garde les premiers réglages de chaque panneau (les principaux)
    def trim(tab, keep):
        scr = tab[1]
        if len(scr) <= keep:
            return
        pns = [pn for s in scr for pn in s]
        total = sum(len(pn.items) for pn in pns)
        budget = keep * cap
        for pn in pns:
            keep_n = max(1, int(len(pn.items) * budget / total))
            if keep_n < len(pn.items):
                occ, seen_l = [], {}
                for it in pn.items:          # 1re occurrence d'un nom = 1er emplacement (matrice 1, puis 2)
                    l = fold(it.label or "").lower()
                    occ.append(seen_l.get(l, 0))
                    seen_l[l] = seen_l.get(l, 0) + 1
                slot = sum(1 for i in range(len(pn.items)) if occ[i] == 0 and item_rank(pn.items[i]) < 2)
                if max(occ) > 0 and slot:
                    keep_n = slot          # emplacements répétés (matrices) : le premier, en entier
                rank = sorted(range(len(pn.items)), key=lambda i: (occ[i], item_rank(pn.items[i]), i))[:keep_n]
                pn.items = [pn.items[i] for i in sorted(rank)]
        tab[1] = screens_of(pns)[:keep]
    for t in tabs:
        trim(t, min(8, 3 if t[0].split("+")[0] in ("MATRICE", "MOD") else per_tab))
    while sum(len(t[1]) for t in tabs) > max_screens:
        t = max(tabs, key=lambda t: len(t[1]))
        trim(t, len(t[1]) - 1)
    return [(n, s) for n, s in tabs]


TAB_ABBREV = [("OSCILLATEURS", "OSC"), ("OSCILLATEUR", "OSC"), ("ENVELOPPES", "ENV"), ("ENVELOPPE", "ENV"),
              ("MELANGEUR", "MIX"), ("MODULATIONS", "MOD"), ("MODULATION", "MOD"), ("FILTRE D'ETAT", "FILTRE"),
              ("ARPEGE", "ARP"), ("SEQUENCE", "SEQ"), ("PASSE-HAUT", "HPF"), ("PASSE-BAS", "LPF"), ("MATRICE", "MATRICE"),
              ("AMPLI", "AMP"), ("EGALISEUR", "EQ"), ("REVERBERATION", "REVERB"), ("DISTORSION", "DIST"), ("LEVIER ET JEU", "JEU")]


def _titles(shelves, kind):
    out = []
    for shelf in shelves:
        items = shelf if kind == "face" else shelf[0]
        for it in (shelf[0] if kind == "face" else items):
            t = it["title"] if kind == "face" else it[0].title
            t = (t or "").replace(" (suite)", "")
            if t and t not in out:
                out.append(t)
    return out


def screen_titles(shelves, kind):
    return " · ".join(_titles(shelves["shelves"] if kind == "face" else shelves, kind))


def screen_tab_name(shelves, kind):
    words = []
    for t in _titles(shelves["shelves"] if kind == "face" else shelves, kind):
        f = fold(t).upper().replace(" ET ", "-")
        for a, b in TAB_ABBREV:
            if f.startswith(a):
                f = b + f[len(a):]
                break
        f = f.split(" ")[0] if len(f) > 10 else f
        if f not in words:
            words.append(f)
    name = words[0] if words else "SON"
    for w in words[1:]:
        if len(name) + 1 + len(w) <= 14:
            name += "+" + w
    return name


def console_kit(skin, P_, mod, title, accent, bg, pfill, sounds=16):
    """Piste A « console 8 tranches » (batteries) : une tranche par son, calée sur les 8 boutons de la Force.
    Gros bouton = Accord (Q-Links, rangée 1), petit = Déclin (rangée 2), puis machine, niveau et panoramique.
    Deux écrans : sons 1 à 8 et 9 à 16 (on rappuie sur la touche KIT). Ne montre que des réglages existants."""
    names = mod.get("padNames") or []
    pads = [n for n in range(1, sounds + 1) if "p%02d_tune" % n in P_]
    banks = [pads[i:i + 8] for i in range(0, len(pads), 8)]
    used = []
    S = K.SS
    light = (236, 236, 240, 255) if _lum(bg) < 140 else (25, 25, 28, 255)
    for bi, bank in enumerate(banks):
        pg = skin.page("KIT", 0, bi)
        pg.face = True
        c = pg.canvas
        c.img.paste(bg, (0, 0, c.img.width, c.img.height))
        c.d.rectangle((0, (FACE_HEAD - 2) * S, c.img.width, FACE_HEAD * S), fill=accent)
        c.d.text((18 * S, FACE_HEAD * S / 2), title.upper(), font=K.font(26 * S, "Bold"), fill=light, anchor="lm")
        c.d.text((1262 * S, FACE_HEAD * S / 2), K.safe("Sons %d à %d  ·  Q-Links : Accord puis Déclin" % (bank[0], bank[-1])),
                 font=K.font(17 * S, "Regular"), fill=light, anchor="rm")
        if len(banks) > 1:
            nxt = (bi + 1) % len(banks)
            c.text(1262, 616, "Écran %d/%d  ·  sons %d à %d : rappuyer sur KIT  >" % (bi + 1, len(banks), banks[nxt][0], banks[nxt][-1]),
                   size=15, colour=light, anchor="rm")
        sw = 1256 / 8
        tunes, decays = [], []
        for j, n in enumerate(bank):
            x0 = round(12 + j * sw)
            w = round(sw) - 8
            tone = TONES[(n - 1) % len(TONES)]
            K.rounded(c.d, [x0 * S, (FACE_HEAD + 8) * S, (x0 + w) * S, 600 * S], 12 * S, fill=pfill,
                      outline=K.mix(pfill, (255, 255, 255, 255), 0.14), width=S)
            pname = names[n - 1] if n - 1 < len(names) else "Son %d" % n
            ab = drum_abbrev(pname).split(" ")[0]
            c.d.rounded_rectangle([(x0 + 10) * S, (FACE_HEAD + 16) * S, (x0 + w - 10) * S, (FACE_HEAD + 20) * S], radius=2 * S, fill=tone)
            c.text(x0 + 12, FACE_HEAD + 42, ab, size=26, colour=tone, weight="Bold")
            c.text(x0 + w - 12, FACE_HEAD + 42, str(n), size=15, colour=K.MUTED, anchor="rm")
            c.fit_text(x0 + 12, FACE_HEAD + 66, fold(pname), 14, w - 20, K.TEXT)
            q = lambda s: P_.get("p%02d_%s" % (n, s))
            if q("mach"):
                pg.menu(q("mach"), x0 + 8, FACE_HEAD + 78, w - 16, "", tone)
                used.append(q("mach").id)
            pg.knob(q("tune"), round(x0 + (w - BIG[0]) / 2), FACE_HEAD + 122, "Accord", "b", tone)
            tunes.append(q("tune"))
            used.append(q("tune").id)
            if q("decay"):
                pg.knob(q("decay"), round(x0 + (w - SMALL[0]) / 2), FACE_HEAD + 298, "Déclin", "s", tone)
                decays.append(q("decay"))
                used.append(q("decay").id)
            if q("level"):
                pg.fader(q("level"), x0 + 12, FACE_HEAD + 452, 30, 56, "Niveau", tone)
                used.append(q("level").id)
            if q("pan"):
                pg.knob(q("pan"), x0 + w - COMPACT[0] + 2, FACE_HEAD + 432, "Pan", "c", tone)
                used.append(q("pan").id)
        skin.program = skin.program or [p.index for p in tunes + decays][:16]
        assign_qlinks(pg, (tunes + decays)[:16])
    return used, len(banks)


CONSOLES = {"drumA": console_kit}


# =============================================================================================
# Habillages des maquettes validées (mpc/mockskin) : écrans de tête dessinés d'après le canvas
# =============================================================================================
MOCK_DIR = os.environ.get("GOLEM_MOCK_DIR") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", "mockcache")
MOCK_LABEL_K = 1.62   # hauteur de police MPC (ascendante + descendante) pour une taille CSS donnée


class ImgCanvas:
    """Fond de page déjà rendu (capture de la maquette) : même interface d'écriture que K.Canvas."""

    def __init__(self, path):
        from PIL import Image as _I
        self.src = path
        self.img = _I.open(path).convert("RGBA")

    def save(self, path):
        self.img.save(path)

    def text(self, *a, **k):
        pass

    def fit_text(self, *a, **k):
        pass


def mock_screens(key):
    f = os.path.join(MOCK_DIR, key, "screens.json")
    return json.load(open(f, encoding="utf-8")) if os.path.isfile(f) else None


def _mock_copy(skin, key, name):
    import shutil as _sh
    src = os.path.join(MOCK_DIR, key, name)
    dst = os.path.join(skin.out, name)
    if not os.path.isfile(dst):
        _sh.copyfile(src, dst)
    return name


def _mock_label(lb, ox=0, oy=0):
    fh = round(lb["fs"] * MOCK_LABEL_K, 1)
    x, y, w, h = lb["box"]
    if h < fh + 4:
        y -= (fh + 4 - h) / 2
        h = fh + 4
    just = {"left": "left verticallyCentred", "right": "right verticallyCentred"}.get(lb["j"], "horizontallyCentred verticallyCentred")
    style = lb["style"] if lb["style"] in ("Bold", "SemiBold") else "Regular"
    data = label_value(fh, K.argb(tuple(lb["col"])), just, style)
    return data, (x - ox, y - oy, w, h)


def _mock_overlay(p):
    return "knob overlay" if len(p.choices) > 4 else ("knob overlay bipolar" if p.bipolar else "knob overlay")


def mock_widget_def(skin, key, w, lab, p):
    """Bouton rotatif / curseur / barre de la maquette : bande d'images + valeur écrite par la MPC."""
    x, y, bw, bh = w["box"]
    strip = _mock_copy(skin, key, w["strip"])
    vertical = w["t"] in ("cfader", "vbar")
    ov = _mock_overlay(p)
    lsig = ""
    if lab:
        ld, lr = _mock_label(lab, x, y)
        lsig = "_%d_%d_%d_%d_%s_%s_%s" % (lr[0], lr[1], lr[2], lr[3], lab["j"], lab["style"], "".join("%02x" % c for c in lab["col"]))
    tag = "mk_%s%s_%s" % (strip[:-4], lsig, ov.replace(" ", ""))
    if tag not in skin.defs:
        children = [skin.focus(bw, bh),
                    ent("knob", "Knob", GEN_UI.knob_filmstrip_data(strip, w["frames"], "Vertical" if vertical else None), rect(0, 0, bw, bh))]
        if lab:
            children.append(ent("value", "Label", ld, rect(*lr)))
        skin.defs[tag] = GEN_UI.component_definition(skin.actions(ov), children, "00000000")
    return tag


def mock_place(skin, key, pg, P_, spec, invalid=None, used=None):
    """Pose les éléments vivants d'un écran capturé ; rend les réglages tournants, dans l'ordre de l'écran."""
    labels_by_host = {}
    free = []
    for lb in spec["labels"]:
        if lb["host"] >= 0:
            labels_by_host.setdefault(lb["host"], lb)
        else:
            free.append(lb)
    turn = []
    radios = {}
    for w in spec["widgets"]:
        p = P_.get(w.get("p") or "")
        if w["t"] in ("knob", "cknob", "cfader", "vbar"):
            if not p:
                continue
            pg._choice(p)
            tag = mock_widget_def(skin, key, w, labels_by_host.get(w["mkid"]), p)
            x, y, bw, bh = w["box"]
            pg.children.append(ent(p.id, tag, O(("version", 1), ("handleName", "Data")), rect(x, y, bw, bh),
                                   focus="Yes", invalid="Hide", remap=[("Data", p.handle)], invalidating=invalid))
            pg.bounds_of[p.index] = (x, y, bw, bh)
            p._turn = True
            if p not in pg.placed:
                pg.placed.append(p)
            turn.append((w, p))
        elif w["t"] == "radio" and p:
            radios.setdefault(p.id, []).append(w)
        elif w["t"] in ("img", "film") and p:
            # image qui suit le réglage : voyant (Pieuvre 8), schéma de l'algorithme FM choisi
            pg._choice(p)
            strip = _mock_copy(skin, key, w["strip"])
            x, y, bw, bh = w["box"]
            data = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", strip),
                     ("numFrames", w["frames"]), ("handleName", "Data"))
            pg.children.append(ent(p.id + " img", "Image", data, rect(x, y, bw, bh), invalid="Hide",
                                   remap=[("Data", p.handle)], invalidating=invalid))
        elif w["t"] == "xy":
            # pavé XY : rondelle posée par deux réglages (Orbiteur : centre X / Y ; Glutamake : la bille)
            px, py = P_.get(w.get("px") or ""), P_.get(w.get("py") or "")
            if px and py and w.get("puck"):
                puck = _mock_copy(skin, key, w["puck"])
                x, y, bw, bh = w["box"]
                data = O(("version", 1), ("xPositionHandle", px.handle), ("yPositionHandle", py.handle), ("image", puck))
                pg.children.append(ent("pave xy", "XYPad", data, rect(x, y, bw, bh), focus="Yes", invalid="Show", invalidating=invalid))
        elif w["t"] == "chip" and p:
            pg._choice(p)
            lab = labels_by_host.get(w["mkid"])
            x, y, bw, bh = w["box"]
            tag = "mk_chip_%dx%d" % (bw, bh)
            if lab:
                ld, lr = _mock_label(lab, x, y)
                tag += "_%d_%d_%s" % (lr[0], lr[2], "".join("%02x" % c for c in lab["col"]))
            if tag not in skin.defs:
                children = [skin.focus(bw, bh)]
                if lab:
                    children.append(ent("value", "Label", ld, rect(*lr)))
                acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                        GEN_UI.action("Double Click", "Show Overlay", "Data", "knob overlay"),
                        GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "knob overlay")]
                skin.defs[tag] = GEN_UI.component_definition(acts, children, "00000000")
            pg.children.append(ent(p.id, tag, O(("version", 1), ("handleName", "Data")), rect(x, y, bw, bh),
                                   focus="Yes", invalid="Hide", remap=[("Data", p.handle)], invalidating=invalid))
            pg.bounds_of[p.index] = (x, y, bw, bh)
            if p not in pg.placed:
                pg.placed.append(p)
    for pid, ws in radios.items():
        p = P_[pid]
        pg._choice(p)
        n = max(len(p.choices), 2)
        x0 = min(w["box"][0] for w in ws)
        y0 = min(w["box"][1] for w in ws)
        x1 = max(w["box"][0] + w["box"][2] for w in ws)
        y1 = max(w["box"][1] + w["box"][3] for w in ws)
        tag = "mk_rd_%s" % "_".join(w["on"][6:16] for w in ws)
        if tag not in skin.defs:
            children = [skin.focus(x1 - x0, y1 - y0)]
            for k, w in enumerate(ws):
                on, off = _mock_copy(skin, key, w["on"]), _mock_copy(skin, key, w["off"])
                bx, by, bw, bh = w["box"]
                gsize = n if len(ws) > 1 or n > 2 else 1
                bid = w["i"] if gsize > 1 else 1
                children.append(ent("b%d" % k, "Button", GEN_UI.button_data(on, off, bid, gsize), rect(bx - x0, by - y0, bw, bh)))
            skin.defs[tag] = GEN_UI.component_definition(skin.actions("knob overlay", double=False), children, "00000000")
        pg.children.append(ent(p.id, tag, O(("version", 1), ("handleName", "Data")), rect(x0, y0, x1 - x0, y1 - y0),
                               focus="Yes", invalid="Hide", remap=[("Data", p.handle)], invalidating=invalid))
        pg.bounds_of[p.index] = (x0, y0, x1 - x0, y1 - y0)
        if p not in pg.placed:
            pg.placed.append(p)
    for lb in free:
        p = P_.get(lb["p"])
        if not p:
            continue
        pg._choice(p)
        ld, lr = _mock_label(lb)
        pg.children.append(ent(p.id + " txt", "Label", ld, rect(*lr), invalid="Hide", remap=[("Data", p.handle)], invalidating=invalid))
    if used is not None:
        used.update(p.id for p in pg.placed)
    return turn


def mock_console(key):
    """Console d'écrans de tête tirée des maquettes : rend (réglages montrés, nombre d'écrans, touches)."""
    data = mock_screens(key)
    if not data:
        return None

    def console(skin, P_, mod, title, accent, bg, pfill, **_):
        used = set()
        entries = 0
        fn = 0
        for scr in data["screens"]:
            name = short_name(fold(scr["tab"]), 12).upper()
            for si, spec in enumerate(scr["pages"][:8]):
                pg = skin.page(name, fn, si)
                pg.face = True
                pg.canvas = ImgCanvas(os.path.join(MOCK_DIR, key, spec["bg"]))
                entries += 1
                if scr["kind"] != "B":
                    turn = mock_place(skin, key, pg, P_, spec, used=used)
                else:
                    n = scr["nav_n"]
                    loc = "Local/%d/mknav" % n
                    # rangée des 8 boutons : celle de la maquette sur le premier module, celle de chaque
                    # autre module (image de l'état + ses boutons) au toucher de son nom dans la colonne
                    mac = [w for w in spec["widgets"] if w.get("slot", -1) >= 0]
                    mid = {w["mkid"] for w in mac}
                    rest = {"widgets": [w for w in spec["widgets"] if w.get("slot", -1) < 0],
                            "labels": [lb for lb in spec["labels"] if lb["host"] not in mid]}
                    turn = mock_place(skin, key, pg, P_, rest, used=used)
                    base_inv = ["IndexedEnabling/0/%d/%s" % (n, loc)]
                    turn = mock_place(skin, key, pg, P_, {"widgets": mac, "labels": [lb for lb in spec["labels"] if lb["host"] in mid]},
                                      invalid=base_inv, used=used) + turn
                    for st in scr.get("states") or []:
                        if not st:
                            continue
                        inv = ["IndexedEnabling/%d/%d/%s" % (st["nav"], n, loc)]
                        img = _mock_copy(skin, key, st["region"])
                        x, y, bw, bh = st["box"]
                        pg.children.append(ent("etat %d" % st["nav"], "Image",
                                               O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", img)),
                                               rect(x, y, bw, bh), invalid="Hide", invalidating=inv))
                        mock_place(skin, key, pg, P_, {"widgets": st["widgets"], "labels": st["labels"]}, invalid=inv)
                    for w in spec["widgets"]:
                        if w["t"] != "nav":
                            continue
                        on, off = _mock_copy(skin, key, w["on"]), _mock_copy(skin, key, w["off"])
                        x, y, bw, bh = w["box"]
                        data_b = O(("version", 2), ("onImage", on), ("offImage", off), ("buttonId", w["i"]),
                                   ("numButtonsInGroup", n), ("handleName", loc), ("gestureBehaviour", "Instant"))
                        pg.children.append(ent("module %d" % w["i"], "Button", data_b, rect(x, y, bw, bh), focus="No"))
                # Q-Links : dans l'ordre de la maquette (A : gros boutons puis petits ; B : les 8 boutons
                # puis le reste ; C : groupes de couleur 1-4, 5-8, 9-12, 13-16 puis le reste)
                ws = [(w, p) for w, p in turn]
                if scr["kind"] == "A":
                    big = [p for w, p in ws if w["box"][2] >= 80]
                    small = [p for w, p in ws if w["box"][2] < 80 and w["t"] == "knob"]
                    rest = [p for w, p in ws if p not in big and p not in small]
                    order = big[:8] + small[:8] + rest
                elif scr["kind"] == "B":
                    mac = sorted([(w["slot"], p) for w, p in ws if w.get("slot", -1) >= 0], key=lambda a: a[0])
                    order = [p for _, p in mac] + [p for w, p in ws if w.get("slot", -1) < 0]
                    order += [p for p in pg.placed if p not in order]
                else:
                    groups = [[p for w, p in ws if w.get("q", -1) == g] for g in range(4)]
                    order = []
                    for g in groups:
                        order += g[:4] + [None] * (4 - min(4, len(g)))
                    while order and order[-1] is None:
                        order.pop()
                    rest = [p for w, p in ws if w.get("q", -1) < 0] + [p for p in pg.placed if p not in [q for q in order if q]]
                    # cases vides des groupes : complétées par les réglages sans couleur
                    order = [q if q is not None else (rest.pop(0) if rest else None) for q in order] + rest
                seen, final = set(), []
                for p in order:
                    if p is not None and p.index not in seen:
                        seen.add(p.index)
                        final.append(p)
                for i in range(0, min(16, len(final)), 4):
                    pg.qlink_group(final[i:i + 4])
                if not skin.program:
                    skin.program = [p.index for p in final[:16]]
            fn += 1
        return used, entries, fn

    return console




def auto_skin_flow(skin, P_, mod, face, title, accent, console=None):
    """Habillage v1.1 : écrans remplis, onglets par fonction (12 au plus), volume rangé dans un écran.
    console : écran console dessiné à la main (touche 1), les réglages qu'il montre sortent des écrans par son."""
    cap_all = load_face_cap(mod["type"])
    th = face_theme(cap_all)
    skin.theme = th
    kit = bool(mod.get("drumKit"))
    fx = bool(cap_all and cap_all.get("fx")) or not cap_all
    bg = None
    if th and th.get("bg"):
        bg = tuple(int(th["bg"][i:i + 2], 16) for i in (1, 3, 5)) + (255,)
    bg = bg or K.BG_BOT
    pfill = (th or {}).get("panel") or K.PANEL
    used = set()
    extras = P_.get("__extras__") or []
    kref = 56
    fpanels = []
    face_fp = []
    if cap_all and not cap_all.get("fx") and not kit:
        face_fp, kref = fpanels_from_face(cap_all, P_, used, accent)
    elif th and th.get("knob"):
        kref = max(44, min(64, th["knob"]["w"] / 2))
    tabs = []
    fn_start = 0
    if kit:
        files = [q for q in extras if q.extra_kind == "file"]
        reserved = 2 if files else 0
        if console:
            bg0 = None
            if th and th.get("bg"):
                bg0 = tuple(int(th["bg"][i:i + 2], 16) for i in (1, 3, 5)) + (255,)
            res = console(skin, P_, mod, title, accent, bg0 or K.BG_BOT, (th or {}).get("panel") or K.PANEL)
            used |= set(res[0])
            reserved += res[1]
            fn_start = res[2] if len(res) > 2 else 1
        pad_tabs, _ = kit_pages(mod, P_, used, accent, [q for q in extras if q.extra_kind != "file"],
                                reserved=reserved)
        if pad_tabs:
            for n, scrs in pad_tabs:
                tabs.append([n, [[fpanel_from_generic(pn, kref, th, pfill) for pn in scr] for scr in scrs]])
            extras = files
            # « Disposition pads » (gamme de la Force : les sons sur les touches blanches, lisibles dans le
            # séquenceur pas à pas) : en tête du premier écran du premier son, pour la trouver tout de suite
            got = []
            for _, scrs in tabs:
                for scr in scrs:
                    for pn in scr:
                        got += [it for it in pn.items if it.p.name == "Disposition pads"]
                        pn.items = [it for it in pn.items if it.p.name != "Disposition pads"]
            if got:
                tabs[0][1][0][0].items.insert(0, got[0])
    main_entries = 0
    if console and not kit:
        # écrans de tête des maquettes (piste B / C) : les réglages restent aussi dans leurs onglets
        res = console(skin, P_, mod, title, accent, bg, pfill)
        main_entries, fn_start = res[1], res[2]
    if extras:
        files = [q for q in extras if q.extra_kind == "file"]
        others = [q for q in extras if q.extra_kind != "file"]
        if files:
            dirs = {_re.search(r"(\d*)$", q.name).group(1): q for q in files if q.name.startswith("Dossier")}
            if dirs:
                # un panneau par emplacement : le dossier (carte, clé, disque interne), puis le fichier
                slots = [q for q in files if not q.name.startswith("Dossier")]
                for i in range(0, len(slots), 2):
                    ctls, nums = [], []
                    for q in slots[i:i + 2]:
                        num = _re.search(r"(\d*)$", q.name).group(1)
                        nums.append(num)
                        ctls += ([Ctl(dirs[num], "file", ("Dossier " + num).strip())] if num in dirs else []) + [Ctl(q, "file", ("Fichier " + num).strip())]
                    ttl = ("Samples " + " et ".join(nums)).strip() if nums[0] else "Sample"
                    fpanels.append(fpanel_from_generic(Panel(ttl, accent, ctls), kref, th, pfill))
            else:
                ttl = "Samples · fichiers WAV du dossier Content/Samples"
                fpanels.append(fpanel_from_generic(Panel(ttl, accent, [Ctl(q, "file", fold(q.name)) for q in files]), kref, th, pfill))
        if others:
            ttl = "Porteuse" if any("porteuse" in q.name.lower() for q in others) else "Jeu"
            fpanels.append(fpanel_from_generic(Panel(ttl, accent, [make_ctl(q, "", q.name) for q in others]), kref, th, pfill))
    fpanels += face_fp
    if face and not kit and not face_fp:
        fp, used2 = panels_from_face(face, P_, accent)
        used |= used2
        for _, pns in fp:
            fpanels += [fpanel_from_generic(pn, kref, th, pfill) for pn in pns]
    secs = panels_from_sections(mod, P_, used, accent, False)
    sec_fp = [(tab, fpanel_from_generic(pn, kref, th, pfill)) for _, _, tab, pn in secs]
    sec_fp = [(t, f) for t, f in sec_fp if f.items]
    if kit:
        if fpanels:
            tabs.insert(0, ["SAMPLES" if any(it.kind == "file" for f in fpanels for it in f.items) else "JEU",
                            [s for o in group_like([["x", fpanels]]) for s in o[1]]])
        if sec_fp:
            tabs.append(["PLUS", [[f for _, f in sec_fp]]])
    elif fx and not face_fp:
        # effets : touches de leurs sections, tout sur une touche si tout tient en 16 réglages
        names = {}
        for sec in mod.get("sections", []):
            names.setdefault(sec.get("tab"), sec["name"])
        order = []
        for f in fpanels:
            order.append([short_name(short_title(f.title), 12).upper(), [f], None])
        for t, f in sec_fp:
            if order and order[-1][2] == t:
                order[-1][1].append(f)
            else:
                order.append([short_name(fold(names.get(t, t) or "SON"), 12).upper(), [f], t])
        # touches légères voisines rassemblées tant que l'écran reste à 16 réglages
        merged = []
        for o in order:
            n = sum(len(f.items) for f in o[1])
            if merged and merged[-1][3] + n <= 16 and len(merged[-1][1]) + len(o[1]) <= 3:
                m = merged[-1]
                if len(m[0]) + 1 + len(o[0]) <= 12:
                    m[0] += "+" + o[0]
                m[1] += o[1]
                m[3] += n
            else:
                merged.append([o[0], list(o[1]), o[2], n])
        seen_n = {}
        order = []
        for m in merged:
            nm = m[0]
            if nm in seen_n:
                seen_n[nm] += 1
                nm = "%s %d" % (nm[:9], seen_n[nm])
            else:
                seen_n[nm] = 1
            order.append([nm, m[1]])
        tabs = group_like(order)
    else:
        fpanels += [f for _, f in sec_fp]
        tabs = [list(t) for t in group_tabs(fpanels, max_tabs=12 - fn_start, max_screens=32 - main_entries)]
        # onglet SORTIE seul (volume global...) : rangé dans un écran qui a de la place (JEU d'abord)
        for t in [t for t in tabs if t[0] == "SORTIE"]:
            n = sum(len(pn.items) for scr in t[1] for pn in scr)
            if len(t[1]) != 1 or len(tabs) < 2:
                continue
            cands = [x for x in tabs if x is not t and x[0].startswith("JEU")] + [x for x in reversed(tabs) if x is not t]
            for x in cands:
                scr = x[1][-1]
                if sum(len(pn.items) for pn in scr) + n + 1 <= 16 and len(scr) < 4:
                    scr.extend(t[1][0])
                    tabs.remove(t)
                    break
            else:
                # pas de place : un écran de plus sur la touche JEU (ou la dernière), pas une touche à lui
                x = ([x for x in tabs if x is not t and x[0].startswith("JEU") and len(x[1]) < 8] +
                     [x for x in reversed(tabs) if x is not t and len(x[1]) < 8])
                if x:
                    x[0][1].append(t[1][0])
                    tabs.remove(t)
    # volume : rangé en petit dans le dernier panneau d'un écran qui a de la place (pas d'onglet SORTIE)
    vinfo = (th or {}).get("fader")
    vol = (FItem("fader", P_["__volume__"], "Volume", vinfo["w"] / 2 * 110 / (vinfo["h"] / 2), 110, vinfo) if vinfo
           else FItem("gfader", P_["__volume__"], "Volume", 26, 110))
    placed = False
    order_tabs = [t for t in tabs if t[0] not in ("SAMPLES", "JEU")][:1] if kit else list(reversed(tabs))
    for t in order_tabs:
        for scr in (t[1] if kit else list(reversed(t[1]))):
            if kit or sum(len(pn.items) for pn in scr) < 16:
                scr[-1].items.append(vol)
                placed = True
                break
        if placed:
            break
    if not placed:
        if tabs:
            tabs[-1][1][-1][-1].items.append(vol)
        else:
            tabs = [["SON", [[FPanel("Sortie", [vol], pfill, accent)]]]]
    area = (12, FACE_HEAD + 8, 1268, 620)
    unit = None
    if th and th.get("unit"):
        area = (400, FACE_HEAD + 8, 1268, 620)
        unit = th["unit"]
    all_turn = []
    for _, scrs in tabs:
        for scr in scrs:
            for p in flow_order(scr):
                if getattr(p, "_turn", False) and p not in all_turn:
                    all_turn.append(p)
    program = list(getattr(skin, "program", None) or [])
    fn = fn_start
    for name, scrs in tabs:
        tab_all = []
        for scr in scrs:
            for p in flow_order(scr):
                if p not in tab_all:
                    tab_all.append(p)
        for si, scr in enumerate(scrs[:8]):
            pg = skin.page(name, fn, si)
            ttl = []
            for pn in scr:
                t = short_title(pn.title).replace(" (suite)", "")
                if t not in ttl:
                    ttl.append(t)
            label = " · ".join(ttl) if ttl and fold(ttl[0]).upper().startswith(fold(name).upper()) else name + " · " + " · ".join(ttl)
            if len(label) > 70:
                label = label[:68] + "…"
            draw_flow_screen(skin, pg, scr, title, accent, label, area, bg, unit, hint=len(scrs) > 1)
            if len(scrs) > 1:
                nxt = (si + 1) % len(scrs) + 1
                hc = (236, 236, 240, 255) if _lum(bg) < 140 else (30, 30, 34, 255)
                pg.canvas.text(1262, 616, "Écran %d/%d  ·  écran %d : rappuyer sur %s  >" % (si + 1, len(scrs), nxt, name),
                               size=15, colour=hc, anchor="rm")
            order = flow_order(scr)
            part = order[:16]
            # Q-Links : seulement les réglages de l'écran (plus de réglages d'autres écrans pour compléter :
            # on les retrouvait sur toutes les pages)
            part = [p for p in part if getattr(p, "_turn", False)] + [p for p in part if not getattr(p, "_turn", False)]
            program.extend(assign_qlinks(pg, part))
        fn += 1
    seen = []
    for i in program:
        if i not in seen:
            seen.append(i)
    skin.program = seen[:16]


def group_like(order):
    """Touches d'effets : [nom, panneaux] → écrans de 16 réglages au plus."""
    out = []
    for o in order:
        name, pns = o[0], o[1]
        scr, cur, q = [], [], 0
        for pn in pns:
            for part in split_fpanel(pn):
                n = len(part.items)
                if cur and q + n > 16:
                    scr.append(cur)
                    cur, q = [], 0
                cur.append(part)
                q += n
        if cur:
            scr.append(cur)
        if out and out[-1][0] == name:
            out[-1][1].extend(scr)
        else:
            out.append([name, scr])
    return out


def auto_skin(skin, P_, mod, face, title, accent):
    """tabs : [(nom de touche, [écran = [Panel]] ou [Panel])]"""
    used = set()
    tabs = []          # (nom, liste de panneaux à ranger) ou (nom, [[Panel]...]) déjà par écran
    extras = P_.get("__extras__") or []
    kit = bool(mod.get("drumKit"))
    face_pages = []
    cap_tabs = []
    global LEFT
    LEFT = 12
    cap_all = load_face_cap(mod["type"])
    skin.theme = face_theme(cap_all)
    if skin.theme and skin.theme.get("unit"):
        LEFT = 400       # effets : leur boîtier GOLEM (pédale ou rack) à gauche de chaque écran
    pc = (skin.theme or {}).get("panel")
    if pc and (0.299 * pc[0] + 0.587 * pc[1] + 0.114 * pc[2]) > 150:
        K.TEXT, K.MUTED = (28, 28, 32, 255), (70, 72, 82, 255)   # boîtier clair : textes foncés
    cap = cap_all if not kit else None
    if cap:
        cap_tabs, used2 = face_tabs(cap, P_)
        used |= used2
    if face and not kit and not cap_tabs:
        face_pages, used2 = panels_from_face(face, P_, accent)
        used |= used2
    if kit:
        # batteries : une touche par son d'abord (comme les boîtes à rythmes de la machine), puis KIT
        files = [q for q in extras if q.extra_kind == "file"]
        pad_tabs, kit_tab = kit_pages(mod, P_, used, accent, [q for q in extras if q.extra_kind != "file"],
                                      reserved=2 if files else 0)
        if pad_tabs:
            tabs.extend((n, scr, "pad") for n, scr in pad_tabs)
            extras = files
        else:
            kit = False
    main_entries = 0
    if console and not kit:
        # écrans de tête des maquettes (piste B / C) : les réglages restent aussi dans leurs onglets
        res = console(skin, P_, mod, title, accent, bg, pfill)
        main_entries, fn_start = res[1], res[2]
    if extras:
        files = [q for q in extras if q.extra_kind == "file"]
        others = [q for q in extras if q.extra_kind != "file"]
        pns = []
        if files:
            ttl = "Firmware · fichiers .fwsc du dossier Content/Firmwares" if files[0].name.startswith("Firmware") \
                else "Samples · Dossier (carte, clé, disque interne) puis Sample"
            pns.append(Panel(ttl, accent, [Ctl(q, "file", fold(q.name)) for q in files]))
        if others:
            pns.append(Panel("Jeu", accent, [make_ctl(q, "", q.name) for q in others]))
        name = "FIRMWARE" if files and files[0].name.startswith("Firmware") else ("SAMPLES" if files else
                ("PORTEUSE" if any("porteuse" in q.name.lower() for q in others) else "PHRASE"))
        tabs.insert(0, (name, pns, False))
    tabs.extend((n, scr, "face") for n, scr in cap_tabs)
    tabs.extend((n, pns, False) for n, pns in face_pages if pns)
    secs = panels_from_sections(mod, P_, used, accent, False)
    if secs:
        if face or kit:
            tabs.append(("PLUS", [s[3] for s in secs], False))
        else:
            order = []
            for _, _, tab, pn in secs:
                if not order or order[-1][0] != tab:
                    order.append((tab, []))
                order[-1][1].append(pn)
            names = {}
            for sec in mod.get("sections", []):
                names.setdefault(sec.get("tab"), sec["name"])
            tabs.extend((names.get(t, t), pns, False) for t, pns in order)
    vol = Panel("Sortie", K.GOLD, [Ctl(P_["__volume__"], "fader", "Volume")])
    placed = False
    for ti, (n0, pns0, per0) in enumerate(tabs):
        if per0 == "face":
            continue
        if per0:
            for si, scr in enumerate(pns0):
                qc = None if per0 == "pad" else 16
                if len(pack_screens(list(scr) + [vol], qc)) == len(pack_screens(scr, qc)):
                    pns0 = list(pns0)
                    pns0[si] = list(scr) + [vol]
                    tabs[ti] = (n0, pns0, per0)
                    placed = True
                    break
        elif len(pack_screens(list(pns0) + [vol])) == len(pack_screens(pns0)):
            tabs[ti] = (n0, list(pns0) + [vol], per0)
            placed = True
        if placed:
            break
    if not placed:
        if tabs and tabs[-1][2] != "face":
            n0, pns0, per0 = tabs[-1]
            tabs[-1] = (n0, (list(pns0) + [[vol]]) if per0 else (list(pns0) + [vol]), per0)
        elif tabs:
            tabs.append(("SORTIE", [vol], False))
        else:
            tabs = [("SON", [vol], False)]
    # effets légers : grands boutons
    if not face and not kit:
        n_ctl = sum(len(pn.ctls) for _, pns, _ in tabs for pn in pns)
        if n_ctl <= 12:
            for _, pns, _ in tabs:
                for pn in pns:
                    for c in pn.ctls:
                        if c.kind == "small":
                            c.kind = "big"
    # effets légers : toutes les sections sur une seule touche si tout tient sur un écran
    if not face and not kit and len(tabs) > 1:
        allp = [pn for _, pns, _ in tabs for pn in pns]
        if len(pack_screens(allp)) == 1:
            tabs = [(tabs[0][0], allp, False)]
    fn, total, program = 0, 0, []
    tab_screens = []
    for name, pns, per_screen in tabs:
        if per_screen == "face":
            screens = pns
        elif per_screen:
            screens = []
            for scr in pns:
                screens.extend(pack_screens(scr, None if per_screen == "pad" else 16))
        else:
            screens = pack_screens(pns)
        tab_screens.append((name, screens, per_screen))

    def dedupe(ps):
        seen, out = set(), []
        for p in ps:
            if p.index not in seen:
                seen.add(p.index)
                out.append(p)
        return out

    def is_turn(p):
        return getattr(p, "_turn", False)

    def order_of(sh, kind):
        return face_order(sh, P_) if kind == "face" else qlink_order(sh)

    all_turn = dedupe([p for _, scr, _f in tab_screens for sh in scr for p in order_of(sh, _f) if is_turn(p)])

    def fill(part, tab_all):
        """Complète à 16 (2 rangées de 8 sur la Force) : réglages suivants de la même page de touche,
        boutons qui tournent d'abord, puis les boutons qui tournent des autres pages."""
        if len(part) >= 16:
            return part
        have = {p.index for p in part}
        start = 0
        idx = [p.index for p in tab_all]
        if part and part[-1].index in idx:
            start = idx.index(part[-1].index) + 1
        ring = tab_all[start:] + tab_all[:start]
        extra = [p for p in ring if p.index not in have and is_turn(p)]
        extra += [p for p in ring if p.index not in have and not is_turn(p)]
        extra += [p for p in all_turn if p.index not in have]
        extra = dedupe(extra)[:16 - len(part)]
        # les boutons qui tournent d'abord (ceux de l'écran, puis les ajoutés), les boutons à enclencher ensuite
        return ([p for p in part if is_turn(p)] + [p for p in extra if is_turn(p)]
                + [p for p in part if not is_turn(p)] + [p for p in extra if not is_turn(p)])

    for name, screens, per_screen in tab_screens:
        # vues : un écran, et s'il a plus de 16 réglages, le même écran encore (sous-page suivante,
        # on rappuie sur la touche) avec les Q-Links sur les réglages suivants
        tab_all = dedupe([p for sh in screens for p in order_of(sh, per_screen)])
        views = []
        for shelves in screens:
            order = order_of(shelves, per_screen)
            parts = [order[i:i + 16] for i in range(0, len(order), 16)] or [[]]
            if per_screen in ("pad", "face"):
                parts = parts[:1]     # une touche par son : 16 tabs au plus, pas de sous-pages de Q-Links
            for pi, part in enumerate(parts):
                views.append((shelves, fill(part, tab_all), pi, len(parts)))
        if per_screen != "pad" and len(views) > 1:
            # un écran = une touche, nommée d'après ce qu'on y règle (OSC, FILTRE, ENV...), pas de sous-pages
            for shelves, part, pi, np_ in views:
                if total >= 32:
                    break
                tab_name = screen_tab_name(shelves, per_screen)
                taken = {q.name for q in skin.pages}
                if tab_name in taken:
                    sfx = lambda k: "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[k] if k < 26 else str(k)
                    k = 1
                    while "%s %s" % (tab_name, sfx(k)) in taken:
                        k += 1
                    tab_name = "%s %s" % (tab_name, sfx(k))
                pg = skin.page(tab_name, fn, 0)
                label = "%s · %s" % (name, screen_titles(shelves, per_screen))
                if per_screen == "face":
                    draw_face_screen(skin, pg, shelves, P_, title, accent, label)
                else:
                    draw_screen(skin, pg, shelves, title, accent, label)
                program.extend(assign_qlinks(pg, part))
                total += 1
                fn += 1
            continue
        chunks = [views[i:i + 8] for i in range(0, len(views), 8)]
        for ci, chunk in enumerate(chunks):
            if total >= 32:
                break
            tab_name = short_name(name) if ci == 0 else short_name(name, 6) + " %d" % (ci + 1)
            if per_screen == "pad":
                tab_name = name
            n_screens = len({id(v[0]) for v in chunk})
            screen_no = 0
            last = None
            for si, (shelves, part, pi, np_) in enumerate(chunk):
                if total >= 32:
                    break
                if id(shelves) != last:
                    screen_no += 1
                    last = id(shelves)
                pg = skin.page(tab_name, fn, si)
                label = name if n_screens == 1 else "%s · %d/%d" % (name, screen_no, n_screens)
                if np_ > 1:
                    label += " · Q-Links %d/%d" % (pi + 1, np_)
                if per_screen == "face":
                    draw_face_screen(skin, pg, shelves, P_, title, accent, label)
                else:
                    draw_screen(skin, pg, shelves, title, accent, label)
                if n_screens > 1:
                    # pas de bouton possible pour changer d'écran (le MPC ne le permet pas) : on l'indique
                    nxt = screen_no % n_screens + 1
                    pg.canvas.text(1262, 614, "Écran %d/%d  ·  écran %d : rappuyer sur %s  >"
                                   % (screen_no, n_screens, nxt, tab_name), size=16, colour=K.MUTED, anchor="rm")
                program.extend(assign_qlinks(pg, part))
                total += 1
            fn += 1
    seen = []
    for i in program:
        if i not in seen:
            seen.append(i)
    skin.program = seen[:16]

SKINS = {"kickforge": (skin_kickforge, FORGE), "acid": (skin_acid, ACID)}
WITH_ENVELOPE = {"kickforge"}


FACES_JSON = os.path.join(os.path.dirname(os.path.abspath(__file__)), "faces.json")  # façades GOLEM (faces_to_json.cjs)


def fill_hand_qlinks(skin):
    """Pages dessinées à la main : les Q-Links libres prennent les réglages de la page, ceux qui tournent d'abord."""
    turn = lambda p: getattr(p, "_turn", False)
    all_turn = []
    for pg in skin.pages:
        for p in pg.placed:
            if turn(p) and p not in all_turn:
                all_turn.append(p)
    for pg in skin.pages:
        # seulement les réglages de la page : un Q-Link vide reste vide (pas de réglages venus d'ailleurs)
        cands = [p for p in pg.placed if turn(p)] + [p for p in pg.placed if not turn(p)]
        seen, out = set(), []
        for p in cands:
            if p.index not in seen:
                seen.add(p.index)
                out.append(p)
        pg.fill_qlinks(out)


# Séquenceurs internes : sur la machine on joue avec son propre séquenceur, ces réglages n'ont pas
# leur place sur l'écran (valeurs des pas des LFO pas à pas, couloirs de séquence de Glutamake...).
# Kick Killer garde sa séquence de 8 pas : elle fait partie du son (avance d'un pas à chaque note).
SEQ_KEEP = {"dfam"}


def drop_internal_seq(params, mod):
    if mod["type"] in SEQ_KEEP:
        return params
    pids = [p["id"] for p in mod["params"]]
    drop = set()
    for sec in mod.get("sections", []):
        nm = fold(sec.get("name", "")).lower()
        if _re.search(r"pas \d+", nm) or nm.startswith("sequence"):
            drop.update(pids[i] for i in sec["params"] if 0 <= i < len(pids))
    for p in mod["params"]:
        pid = p["id"]
        if _re.search(r"(^|_)lf\d_(s\d+|steps)$", pid) or pid in ("seqon", "seqretrig") or _re.match(r"^sq[a-d]", pid):
            drop.add(pid)
    return {k: v for k, v in params.items() if k not in drop}


def build(sdk, key, kind, params_json, catalog, out_dir, envelope=True, title=None):
    load_gen_ui(sdk)
    params = load_params(catalog, kind, key, params_json)
    _cat = load_catalog(catalog)
    _mods = [m for m in _cat["instruments"] + _cat["effects"] if m["type"] == key]
    if _mods:
        params = drop_internal_seq(params, _mods[0])
    if key in SKINS and not (kind == "instrument" and mock_screens(key)):
        fn, accent = SKINS[key]
        skin = Skin(out_dir, accent)
        skin.no_envelope = not envelope
        fn(skin, params)
        fill_hand_qlinks(skin)
    else:
        cat = load_catalog(catalog)
        mod = [m for m in cat["instruments" if kind == "instrument" else "effects"] + cat["instruments"] + cat["effects"] if m["type"] == key][0]
        faces = json.load(open(FACES_JSON, encoding="utf-8")) if FACES_JSON and os.path.isfile(FACES_JSON) else {}
        face = faces.get(key)
        if kind == "effect" and mod in cat["instruments"]:
            # vocodeur en effet : le modulateur est le son de la piste, les choix de source ne servent pas
            params = {k: v for k, v in params.items() if k not in ("src", "srcmute", "ploop")}
        accent = theme_accent(face.get("theme", "")) if face else K.GOLD
        skin = Skin(out_dir, accent)
        skin.no_envelope = not envelope
        if os.environ.get("GOLEM_OLD_SKIN"):
            auto_skin(skin, params, mod, face, title or mod["name"], accent)
        else:
            auto_skin_flow(skin, params, mod, face, title or mod["name"], accent, console=mock_console(key) or CONSOLES.get(key))
    try:
        skin.write()
    finally:
        K.TEXT, K.MUTED = (238, 238, 242, 255), (150, 153, 163, 255)
    return True


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", required=True)
    ap.add_argument("--key", required=True)
    ap.add_argument("--kind", default="instrument")
    ap.add_argument("--params", required=True)
    ap.add_argument("--catalog", default=os.path.join(os.path.dirname(__file__), "..", "ui", "src", "engine", "catalog.json"))
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    print("ok" if build(a.sdk, a.key, a.kind, a.params, a.catalog, a.out) else "pas d'habillage pour " + a.key)
