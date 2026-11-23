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


def load_params(catalog_path, kind, key, params_json):
    """Réglages du module (catalogue GOLEM) → index du greffon (0 = Volume du greffon, puis l'ordre GOLEM)."""
    cat = json.load(open(catalog_path, encoding="utf-8"))
    mod = [m for m in cat["instruments" if kind == "instrument" else "effects"] if m["type"] == key][0]
    built = json.load(open(params_json, encoding="utf-8"))["params"]
    if len(built) != len(mod["params"]) + 1:
        raise SystemExit("skins: %s : %d réglages dans le greffon, %d dans le catalogue" % (key, len(built), len(mod["params"]) + 1))
    out = {"__volume__": P(0, "__volume__", "Volume", -60, 6, None)}
    for i, p in enumerate(mod["params"]):
        b = built[i + 1]
        if p.get("choices") and b["kind"] == "choice" and len(b["choices"]) > max(2, len(p["choices"])):
            raise SystemExit("skins: %s : choix différents pour %s" % (key, p["id"]))
        out[p["id"]] = P(i + 1, p["id"], b["name"], p.get("min"), p.get("max"), p.get("choices") or [])
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
        if tag not in self.defs:
            strip = "knob%d_%s_%s.png" % (k, self.colour_tag(colour), "bi" if bipolar else "uni")
            K.knob_strip(os.path.join(self.out, strip), k, colour, bipolar)
            ky = 26 if size == "b" else (20 if size == "c" else 23)
            vy = ky + k + 2
            children = [self.focus(w, h),
                        ent("knob", "Knob", GEN_UI.knob_filmstrip_data(strip, K.KNOB_FRAMES), rect((w - k) / 2, ky, k, k)),
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
                        ent("value", "Label", label_value(19, K.argb(K.TEXT), "left verticallyCentred"), rect(12, top, w - 40, 40))]
            acts = [GEN_UI.action("Mouse Down", "Q-Link", "Data", ""),
                    GEN_UI.action("Double Click", "Show Overlay", "Data", "menu overlay"),
                    GEN_UI.action("Enter Pressed", "Show Overlay", "Data", "menu overlay")]
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
                base = "btn_%s_%dx%d_%s" % (fold(word).replace(" ", "").replace("'", "").replace("/", ""), w, h, self.colour_tag(colour))
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
        tag = "nb_%dx%d_%s" % (w, h, self.colour_tag(colour))
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

    def _choice(self, p):
        if p.choices:
            self.skin.replacements[p.index] = p.choices

    def _add(self, p, tag, x, y, w, h):
        self.children.append(ent(p.id, tag, O(("version", 1), ("handleName", "Data")), rect(x, y, w, h),
                                 focus="Yes", invalid="Hide", remap=[("Data", p.handle)]))
        self.bounds_of[p.index] = (x, y, w, h)

    def knob(self, p, x, y, label, size="s", colour=None):
        colour = colour or self.skin.accent
        self._choice(p)
        overlay = "menu overlay" if len(p.choices) > 4 else ("knob overlay bipolar" if p.bipolar else "knob overlay")
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

    def number(self, p, x, y, w, h, label=None, colour=None):
        colour = colour or self.skin.accent
        tag = self.skin.def_number(w, h, colour)
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


def pack_screens(panels):
    """Panneaux rangés en étagères, écran par écran (ordre conservé)."""
    W, H = RIGHT - LEFT, BOTTOM - TOP
    items = []
    for pn in panels:
        items.extend(split_panel(pn, H, W))
    screens, shelves, shelf, sx, y, sh = [], [], [], 0, 0, 0
    for pn in items:
        w, h, rows = layout_panel(pn, W)
        if shelf and sx + w > W:
            shelves.append((shelf, sh))
            y += sh + GAP
            shelf, sx, sh = [], 0, 0
        if y + h > H and (shelves or shelf):
            if shelf:
                shelves.append((shelf, sh))
            screens.append(shelves)
            shelves, shelf, sx, y, sh = [], [], 0, 0, 0
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
            c.panel(x, y, pw, sh, pn.title, pn.tone)
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


def qlink_groups_for(pg, shelves):
    """Groupes de 4 Q-Links pris rangée par rangée (le cadre lumineux entoure une seule rangée)."""
    groups = []
    for shelf, _ in shelves:
        for pn, _, _, rows in shelf:
            for r in rows:
                ps = []
                for kind, c, _, _ in r:
                    if kind == "env":
                        ps.extend(pn.env[k].p for k in "adsr")
                    elif c.kind != "menu":
                        ps.append(c.p)
                for i in range(0, len(ps), 4):
                    groups.append(ps[i:i + 4])
    for g in groups[:4]:
        pg.qlink_group(g)
    return [p.index for g in groups for p in g]


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


PAD_RE = _re.compile(r"^p(\d\d)_")


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
            pad = int(m.group(1))
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


def kit_pages(mod, P_, used, accent):
    """Batteries : un écran par pad (tous ses réglages dans un panneau), pads 1-8 et 9-16 sur deux touches."""
    params = mod["params"]
    names = {p["id"]: p["name"] for p in params}
    pads, kitp = {}, []
    padnames = mod.get("padNames") or []
    for p in params:
        pid = p["id"]
        if pid not in P_ or pid in used:
            continue
        m = PAD_RE.match(pid)
        if m:
            pads.setdefault(int(m.group(1)), []).append(pid)
        else:
            kitp.append(pid)
        used.add(pid)
    tabs = []
    if kitp:
        tabs.append(("KIT", [[Panel("Kit", accent, [make_ctl(P_[i], "", names[i]) for i in kitp])]]))
    order = sorted(pads)
    for start, tname in ((1, "PADS 1-8"), (9, "PAD 9-16")):
        screens = []
        for n in order:
            if not (start <= n < start + 8):
                continue
            pname = padnames[n - 1] if n - 1 < len(padnames) else "Pad %d" % n
            ctls = [make_ctl(P_[i], "", names[i]) for i in pads[n]]
            env = detect_adsr(ctls)
            if env:
                ctls = [c for c in ctls if c not in env.values()]
            screens.append([Panel("%d · %s" % (n, pname), TONES[(n - 1) % len(TONES)], ctls, env)])
        if screens:
            tabs.append((tname, screens))
    return tabs


def auto_skin(skin, P_, mod, face, title, accent):
    """tabs : [(nom de touche, [écran = [Panel]] ou [Panel])]"""
    used = set()
    tabs = []          # (nom, liste de panneaux à ranger) ou (nom, [[Panel]...]) déjà par écran
    kit = bool(mod.get("drumKit"))
    if face:
        pages, used = panels_from_face(face, P_, accent)
        tabs.extend((n, pns, False) for n, pns in pages)
    if kit:
        tabs.extend((n, scr, True) for n, scr in kit_pages(mod, P_, used, accent))
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
        if per0:
            for si, scr in enumerate(pns0):
                if len(pack_screens(list(scr) + [vol])) == len(pack_screens(scr)):
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
        if tabs:
            n0, pns0, per0 = tabs[-1]
            tabs[-1] = (n0, (list(pns0) + [[vol]]) if per0 else (list(pns0) + [vol]), per0)
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
    for name, pns, per_screen in tabs:
        if per_screen:
            screens = []
            for scr in pns:
                screens.extend(pack_screens(scr))
        else:
            screens = pack_screens(pns)
        chunks = [screens[i:i + 8] for i in range(0, len(screens), 8)]
        for ci, chunk in enumerate(chunks):
            if total >= 32:
                break
            tab_name = short_name(name) if ci == 0 else short_name(name, 6) + " %d" % (ci + 1)
            for si, shelves in enumerate(chunk):
                if total >= 32:
                    break
                pg = skin.page(tab_name, fn, si)
                label = name if len(chunk) == 1 else "%s · %d/%d" % (name, si + 1, len(chunk))
                draw_screen(skin, pg, shelves, title, accent, label)
                program.extend(qlink_groups_for(pg, shelves))
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


def build(sdk, key, kind, params_json, catalog, out_dir, envelope=True, title=None):
    load_gen_ui(sdk)
    params = load_params(catalog, kind, key, params_json)
    if key in SKINS:
        fn, accent = SKINS[key]
        skin = Skin(out_dir, accent)
        skin.no_envelope = not envelope
        fn(skin, params)
    else:
        cat = json.load(open(catalog, encoding="utf-8"))
        mod = [m for m in cat["instruments" if kind == "instrument" else "effects"] if m["type"] == key][0]
        faces = json.load(open(FACES_JSON, encoding="utf-8")) if FACES_JSON and os.path.isfile(FACES_JSON) else {}
        face = faces.get(key) if kind == "instrument" else None
        accent = theme_accent(face.get("theme", "")) if face else K.GOLD
        skin = Skin(out_dir, accent)
        skin.no_envelope = not envelope
        auto_skin(skin, params, mod, face, title or mod["name"], accent)
    skin.write()
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
