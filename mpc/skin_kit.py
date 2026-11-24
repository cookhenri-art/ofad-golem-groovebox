"""
OFAD GOLEM — habillage graphique des greffons MPC : dessin des images (PNG) — AGPL-3.0-or-later

Toutes les images de l'habillage sont CALCULÉES ici (aucune image reprise d'ailleurs) :
  - boutons rotatifs en bande d'images (film strip vertical) : corps sombre, piste, arc de valeur
    dans la couleur de la section, trait blanc ;
  - boutons à bascule et boutons de choix avec leur mot (police Poppins, licence OFL) ;
  - case de menu ;
  - fond de chaque page : dégradé, bandeau de titre, panneaux arrondis, titres et noms des réglages
    (les valeurs, elles, sont écrites par la MPC en direct).
"""
import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT_DIR = "/usr/share/fonts/truetype/google-fonts"
SS = 3  # suréchantillonnage du dessin (lissage)


GLYPHS = {"→": ">", "←": "<", "↔": "<>", "⇄": "<>", "↑": "^", "↓": "v", "×": "x", "≈": "~", "♯": "#", "♭": "b", "∞": "inf", "—": "-", "–": "-"}


def safe(s):
    """Caractères absents de la police remplacés."""
    for a, b in GLYPHS.items():
        s = s.replace(a, b)
    return s


def font(size, weight="Bold"):
    path = os.path.join(FONT_DIR, "Poppins-%s.ttf" % weight)
    return ImageFont.truetype(path, int(size))


# ---------------------------------------------------------------------------------------------
# Couleurs
# ---------------------------------------------------------------------------------------------
def oklch(l, c, h, a=1.0):
    """oklch → (r, g, b, a) 0..255 (sRGB)."""
    hr = math.radians(h)
    A, B = c * math.cos(hr), c * math.sin(hr)
    l_ = l + 0.3963377774 * A + 0.2158037573 * B
    m_ = l - 0.1055613458 * A - 0.0638541728 * B
    s_ = l - 0.0894841775 * A - 1.2914855480 * B
    L, M, S = l_ ** 3, m_ ** 3, s_ ** 3
    r = 4.0767416621 * L - 3.3077115913 * M + 0.2309699292 * S
    g = -1.2684380046 * L + 2.6097574011 * M - 0.3413193965 * S
    b = -0.0041960863 * L - 0.7034186147 * M + 1.7076147010 * S

    def enc(x):
        x = max(0.0, min(1.0, x))
        return round(255 * (12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055))
    return (enc(r), enc(g), enc(b), round(255 * a))


def argb(rgba):
    r, g, b, a = rgba
    return "%02x%02x%02x%02x" % (a, r, g, b)


def mix(c1, c2, t):
    return tuple(round(c1[i] + (c2[i] - c1[i]) * t) for i in range(4))


# Palette GOLEM : fond anthracite, jaune ambré de l'application
GOLD = oklch(0.82, 0.15, 78)
BG_TOP = (30, 31, 35, 255)
BG_BOT = (17, 17, 20, 255)
PANEL = (35, 36, 41, 255)
PANEL_EDGE = (62, 64, 72, 255)
TEXT = (238, 238, 242, 255)
MUTED = (150, 153, 163, 255)
DARK = (14, 14, 16, 255)


# ---------------------------------------------------------------------------------------------
# Boutons rotatifs
# ---------------------------------------------------------------------------------------------
KNOB_FRAMES = 61
ARC_SPAN = 270.0  # degrés


def knob_strip(path, size, colour, bipolar=False, frames=KNOB_FRAMES):
    """Bande verticale de `frames` images size × size."""
    if os.path.isfile(path):
        return
    S = size * SS
    strip = Image.new("RGBA", (size, size * frames), (0, 0, 0, 0))
    # corps commun
    body = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(body)
    pad = S * 0.20
    # ombre
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([pad, pad + S * 0.03, S - pad, S - pad + S * 0.03], fill=(0, 0, 0, 150))
    sh = sh.filter(ImageFilter.GaussianBlur(S * 0.03))
    body.alpha_composite(sh)
    # corps : dégradé radial simple (anneaux)
    steps = 24
    for i in range(steps):
        t = i / (steps - 1)
        col = mix((70, 72, 80, 255), (24, 25, 29, 255), t)
        inset = pad + t * (S * 0.30 - pad) * 0.0 + i * 0.4
        d.ellipse([inset, inset, S - inset, S - inset], fill=col)
    d.ellipse([pad, pad, S - pad, S - pad], outline=(92, 95, 105, 255), width=max(1, SS))
    track_w = max(2, int(S * 0.055))
    r_arc = S * 0.44
    box = [S / 2 - r_arc, S / 2 - r_arc, S / 2 + r_arc, S / 2 + r_arc]
    start = 90 + (360 - ARC_SPAN) / 2  # PIL : 0° = 3 h, sens horaire
    end = start + ARC_SPAN
    d.arc(box, start, end, fill=(52, 54, 61, 255), width=track_w)
    for f in range(frames):
        v = f / (frames - 1)
        img = body.copy()
        dd = ImageDraw.Draw(img)
        ang = start + ARC_SPAN * v
        if bipolar:
            mid = start + ARC_SPAN / 2
            a0, a1 = (ang, mid) if ang < mid else (mid, ang)
        else:
            a0, a1 = start, ang
        if a1 - a0 > 0.5:
            # halo
            glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
            ImageDraw.Draw(glow).arc(box, a0, a1, fill=colour[:3] + (120,), width=track_w * 2)
            glow = glow.filter(ImageFilter.GaussianBlur(S * 0.02))
            img.alpha_composite(glow)
            dd.arc(box, a0, a1, fill=colour, width=track_w)
        # trait
        rad = math.radians(ang)
        r0, r1 = S * 0.10, S * 0.27
        cx = cy = S / 2
        dd.line([cx + r0 * math.cos(rad), cy + r0 * math.sin(rad), cx + r1 * math.cos(rad), cy + r1 * math.sin(rad)],
                fill=(245, 245, 248, 255), width=max(2, int(S * 0.035)))
        small = img.resize((size, size), Image.LANCZOS)
        strip.paste(small, (0, f * size))
    strip.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).save(path, optimize=True)


# ---------------------------------------------------------------------------------------------
# Boutons à texte (bascule, choix), case de menu
# ---------------------------------------------------------------------------------------------
_MEASURE = ImageDraw.Draw(Image.new("RGB", (8, 8)))


def text_px(s, size, weight="Bold"):
    return _MEASURE.textlength(s, font=font(size, weight))


def rounded(draw, box, radius, fill=None, outline=None, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def text_button(path, w, h, word, on, colour, size=None):
    if os.path.isfile(path):
        return
    S = SS
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = min(h, 18) * S * 0.5
    if on:
        rounded(d, [S, S, w * S - S, h * S - S], r, fill=colour, outline=mix(colour, (255, 255, 255, 255), 0.3), width=S)
        tc = DARK
    else:
        rounded(d, [S, S, w * S - S, h * S - S], r, fill=(28, 29, 33, 255), outline=(78, 80, 90, 255), width=S)
        tc = (150, 153, 163, 255)   # fond sombre du bouton : texte clair quel que soit le thème
    word = safe(word)
    fs = size or min(h * 0.46, 20)
    f = font(fs * S, "Bold")
    tw = d.textlength(word, font=f)
    d.text(((w * S - tw) / 2, h * S / 2), word, font=f, fill=tc, anchor="lm")
    img = img.resize((w, h), Image.LANCZOS)
    img.save(path, optimize=True)


def menu_box(path, w, h, colour):
    if os.path.isfile(path):
        return
    S = SS
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rounded(d, [S, S, w * S - S, h * S - S], 8 * S, fill=(22, 23, 27, 255), outline=mix(colour, PANEL_EDGE, 0.55), width=S)
    # flèche
    cx, cy, a = w * S - 18 * S, h * S / 2, 6 * S
    d.polygon([(cx - a, cy - a / 2), (cx + a, cy - a / 2), (cx, cy + a / 2)], fill=colour)
    img = img.resize((w, h), Image.LANCZOS)
    img.save(path, optimize=True)


# ---------------------------------------------------------------------------------------------
# Fonds de page
# ---------------------------------------------------------------------------------------------
class Canvas:
    """Fond d'une page 1280 × 628 (zone de contenu d'un onglet)."""

    W, H = 1280, 628

    def __init__(self):
        S = SS
        self.img = Image.new("RGBA", (self.W * S, self.H * S), BG_BOT)
        d = ImageDraw.Draw(self.img)
        for y in range(0, self.H * S, S):
            t = y / (self.H * S)
            d.line([(0, y), (self.W * S, y)], fill=mix(BG_TOP, BG_BOT, t), width=S)
        self.d = d

    def _s(self, v):
        return v * SS

    def header(self, title, colour, left="OFAD GOLEM", right="", height=64, centred=True, title_size=40):
        S = SS
        d = self.d
        # bande
        for y in range(0, height * S):
            t = y / (height * S)
            d.line([(0, y), (self.W * S, y)], fill=mix((40, 41, 47, 255), (25, 26, 30, 255), t))
        d.line([(0, height * S - S), (self.W * S, height * S - S)], fill=colour, width=2 * S)
        # liseré lumineux sous la bande
        glow = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        ImageDraw.Draw(glow).line([(self.W * S * 0.25, height * S), (self.W * S * 0.75, height * S)],
                                  fill=colour[:3] + (160,), width=6 * S)
        self.img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(10 * S)))
        f = font(title_size * S, "Bold")
        if centred:
            tw = d.textlength(title, font=f)
            d.text(((self.W * S - tw) / 2, height * S / 2), title, font=f, fill=colour, anchor="lm")
        else:
            d.text((24 * S, height * S / 2), title, font=f, fill=colour, anchor="lm")
        if left and centred:
            d.text((24 * S, height * S / 2), left, font=font(17 * S, "Medium"), fill=(150, 153, 163, 255), anchor="lm")
        if right:
            fr = font(17 * S, "Medium")
            d.text((self.W * S - 24 * S - d.textlength(right, font=fr), height * S / 2), right, font=fr, fill=(150, 153, 163, 255), anchor="lm")

    def panel(self, x, y, w, h, title="", colour=GOLD, title_right="", fill=None):
        S = SS
        d = self.d
        sh = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle([x * S, (y + 3) * S, (x + w) * S, (y + h + 3) * S], radius=12 * S, fill=(0, 0, 0, 140))
        self.img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(5 * S)))
        rounded(d, [x * S, y * S, (x + w) * S, (y + h) * S], 12 * S, fill=fill or PANEL, outline=PANEL_EDGE, width=S)
        title = safe(title)
        if title:
            # pastille de couleur + titre
            d.rounded_rectangle([(x + 14) * S, (y + 12) * S, (x + 18) * S, (y + 28) * S], radius=2 * S, fill=colour)
            d.text(((x + 26) * S, (y + 20) * S), title.upper(), font=font(15 * S, "Bold"), fill=colour, anchor="lm")
        if title_right:
            fr = font(13 * S, "Medium")
            d.text(((x + w - 14) * S - d.textlength(title_right, font=fr), (y + 20) * S), title_right, font=fr, fill=MUTED, anchor="lm")

    def text(self, x, y, s, size=15, colour=MUTED, weight="Medium", centre_w=None, anchor="lm"):
        S = SS
        s = safe(s)
        f = font(size * S, weight)
        if centre_w is not None:
            tw = self.d.textlength(s, font=f)
            self.d.text(((x + (centre_w - tw / S) / 2) * S, y * S), s, font=f, fill=colour, anchor="lm")
        else:
            self.d.text((x * S, y * S), s, font=f, fill=colour, anchor=anchor)

    def fit_text(self, x, y, s, size, width, colour=MUTED, weight="Medium"):
        """Texte centré dans `width`, police réduite (jusqu'à 11) puis coupé s'il ne tient pas."""
        S = SS
        s = safe(s)
        sz = size
        while sz > 11 and self.d.textlength(s, font=font(sz * S, weight)) / S > width:
            sz -= 1
        f = font(sz * S, weight)
        t = s
        while len(t) > 3 and self.d.textlength(t, font=f) / S > width:
            t = t[:-2] + "."
        tw = self.d.textlength(t, font=f) / S
        self.d.text(((x + (width - tw) / 2) * S, y * S), t, font=f, fill=colour, anchor="lm")

    def text_width(self, s, size, weight="Bold"):
        return self.d.textlength(s, font=font(size * SS, weight)) / SS

    def line(self, x0, y0, x1, y1, colour=PANEL_EDGE, width=1):
        S = SS
        self.d.line([(x0 * S, y0 * S), (x1 * S, y1 * S)], fill=colour, width=width * S)

    def save(self, path):
        self.img.resize((self.W, self.H), Image.LANCZOS).convert("RGB").save(path, optimize=True)
