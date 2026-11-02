#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
#
# Matières des façades (v0.34, maquettes) : bois, métal brossé, peinture, plastique, boîtier de
# pédale, boutons, vis, interrupteurs au pied. Tout est CALCULÉ ICI, une fois, et rangé en WebP :
# l'interface n'affiche que des images fixes (ombres, reflets et grain sont « cuits » dans l'image).
# En jeu, seul le repère d'un bouton tourne (une petite couche transparente) : aucun calcul
# d'ombre, de flou ou de dégradé pendant le jeu.
#
# Éclairage : carte de hauteur → normales → diffus (Lambert) + reflet (Blinn) + reflet
# d'environnement pour les métaux ; lumière en haut à gauche, la même pour toutes les pièces.
#
# Usage : python3 scripts/make-materials.py <dossier de sortie>

import sys, os
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, map_coordinates

OUT = sys.argv[1] if len(sys.argv) > 1 else "ui/public/mat"
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)
LIGHT = np.array([-0.45, -0.62, 0.64]); LIGHT /= np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0, 0, 1.0]); HALF /= np.linalg.norm(HALF)


def save(img, name, q=90, lossless=False):
    path = os.path.join(OUT, name)
    img.save(path, "WEBP", quality=q, method=6, lossless=lossless)
    print(f"{name:28s} {img.size[0]}x{img.size[1]}  {os.path.getsize(path)/1024:6.1f} ko")


def to_img(rgb, alpha=None):
    rgb = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if alpha is None:
        return Image.fromarray(rgb, "RGB")
    a = np.clip(alpha * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, a]), "RGBA")


def fbm(h, w, octaves=5, base=4, seed=0):
    r = np.random.default_rng(seed)
    out = np.zeros((h, w))
    amp = 1.0
    for o in range(octaves):
        n = base * 2 ** o
        g = r.standard_normal((n + 3, n + 3))
        ys = np.linspace(1, n + 1, h); xs = np.linspace(1, n + 1, w)
        yy, xx = np.meshgrid(ys, xs, indexing="ij")
        out += amp * map_coordinates(g, [yy, xx], order=3, mode="wrap")
        amp *= 0.5
    return (out - out.mean()) / (out.std() + 1e-9)


def shade(height, albedo, spec=0.35, gloss=40.0, metal=0.0, env=None, scale=1.0):
    """height : carte de hauteur (pixels) ; albedo : HxWx3 ; renvoie HxWx3 éclairé."""
    gy, gx = np.gradient(height * scale)
    n = np.dstack([-gx, -gy, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    diff = np.clip((n * LIGHT).sum(2), 0, 1)
    sp = np.clip((n * HALF).sum(2), 0, 1) ** gloss
    out = albedo * (0.30 + 0.78 * diff)[..., None]
    if metal > 0:
        # reflet d'environnement : ciel clair en haut, sol sombre en bas (selon la normale)
        e = 0.55 + 0.45 * (-n[..., 1]) if env is None else env(n)
        out = out * (1 - metal) + metal * albedo * e[..., None] * 1.25
    out += spec * sp[..., None]
    # lustre large (matières sombres : le noir satiné garde un reflet gris côté lumière)
    out += 0.07 * (np.clip((n * HALF).sum(2), 0, 1) ** 4)[..., None]
    return np.clip(out, 0, 1)


# ---------------------------------------------------------------------------------------------
# Textures de fond
# ---------------------------------------------------------------------------------------------

def wood(w=120, h=1000, seed=1, tone=(0.44, 0.25, 0.12), name="bois-noyer.webp"):
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    warp = fbm(h, w, 4, 2, seed) * 9 + fbm(h, w, 3, 1, seed + 1) * 25
    rings = np.sin((xx + warp) / w * 2 * np.pi * 7.0)
    fine = fbm(h, w, 6, 24, seed + 2)
    streak = gaussian_filter(rng.standard_normal((h, w)), (14, 0.6))
    streak = (streak - streak.mean()) / streak.std()
    t = 0.58 + 0.22 * rings + 0.08 * fine + 0.14 * streak
    pores = (rng.random((h, w)) < 0.012) * gaussian_filter(rng.random((h, w)), (3, 0.5)) * 6
    t -= np.clip(pores, 0, 0.25)
    base = np.array(tone)
    rgb = base[None, None, :] * (0.60 + 0.80 * t[..., None])
    rgb[..., 0] += 0.04 * t
    # vernis : léger relief du grain, reflet satiné
    rgb = shade(t * 1.6, rgb, spec=0.10, gloss=18)
    # chants arrondis (bords gauche / droit plus sombres, liseré clair côté lumière)
    edge = np.clip(np.minimum(xx, w - 1 - xx) / 10.0, 0, 1)
    rgb *= (0.55 + 0.45 * edge)[..., None]
    rgb[:, 2:5] += 0.06
    save(to_img(rgb), name, q=88)


def brushed(w=1600, h=480, tint=(0.80, 0.81, 0.82), name="alu-brosse.webp", strength=1.0):
    # stries horizontales (flou très allongé, raccord parfait gauche / droite)
    n = rng.standard_normal((h, w))
    s = gaussian_filter(n, (0.5, 80), mode="wrap") * 10 + gaussian_filter(n, (0.35, 14), mode="wrap") * 2.5
    s = (s - s.mean()) / s.std()
    t = 0.80 + 0.028 * strength * s
    yy = np.linspace(0, 1, h)[:, None]
    t = t + 0.05 * (0.5 - yy)              # reflet : plus clair en haut
    rgb = np.array(tint)[None, None, :] * t[..., None]
    save(to_img(np.clip(rgb, 0, 1)), name, q=88)


def paint(w=512, h=512, color=(0.10, 0.10, 0.11), grain=0.02, name="peinture-noire.webp"):
    n = gaussian_filter(rng.standard_normal((h, w)), 0.7)
    n = (n - n.mean()) / n.std()
    rgb = np.array(color)[None, None, :] * (1 + grain * 6 * n[..., None]) + grain * 0.3 * n[..., None]
    save(to_img(np.clip(rgb, 0, 1)), name, q=84)


def hammertone(w=420, h=680, color=(0.62, 0.10, 0.07), name="martele-rouge.webp", seed=3):
    # peinture martelée des boîtiers de pédales : petites cellules en relief, reflets doux ; rendue
    # à la taille d'un boîtier (pas de raccord), bombé du capot et arrondi des arêtes compris
    cells = fbm(h, w, 2, 46, seed)
    bumps = gaussian_filter(np.abs(cells), 0.8) * 0.75
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    ex = np.clip(np.minimum(xx, w - 1 - xx) / 22.0, 0, 1); ey = np.clip(np.minimum(yy, h - 1 - yy) / 22.0, 0, 1)
    crown = 26 * np.sqrt(ex) * np.sqrt(ey) + 6 * (1 - ((xx - w / 2) / (w / 2)) ** 2)
    alb = np.array(color)[None, None, :] * (0.96 + 0.05 * fbm(h, w, 2, 5, seed + 4)[..., None])
    rgb = shade(bumps + crown, alb, spec=0.22, gloss=26, scale=1.0)
    save(to_img(rgb), name, q=86)


# ---------------------------------------------------------------------------------------------
# Pièces (images RGBA avec ombre portée cuite)
# ---------------------------------------------------------------------------------------------

def disc_coords(S):
    yy, xx = np.mgrid[0:S, 0:S].astype(float)
    c = (S - 1) / 2.0
    dx, dy = xx - c, yy - c
    return np.hypot(dx, dy), np.arctan2(dy, dx), dx, dy


def shift(a, dy, dx):
    out = np.zeros_like(a)
    h, w = a.shape
    out[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)] = a[max(0, -dy):h - max(0, dy), max(0, -dx):w - max(0, dx)]
    return out


def with_shadow(rgb, alpha, S, off=0.045, blur=0.03, strength=0.55):
    sh = gaussian_filter(shift(alpha, int(S * off), int(S * off * 0.55)), S * blur)
    a_out = alpha + (1 - alpha) * sh * strength
    rgb_out = rgb * alpha[..., None] / np.maximum(a_out, 1e-6)[..., None]
    return rgb_out, a_out


def knob(name, S=256, R=0.34, wall=0.10, cap_col=(0.07, 0.07, 0.08), wall_col=None, knurl=0,
         skirt=0.0, skirt_col=(0.80, 0.80, 0.82), skirt_wall=0.035, dome=0.05, top_ring=0.0,
         ring_col=(0.78, 0.78, 0.80), cap_metal=0.0, gloss=60, spec=0.45, top_col=None):
    """Bouton en légère perspective (caméra un peu en avant) : flanc visible et cannelé, dessus
    bombé et chanfreiné, jupe métal éventuelle. Le centre du dessus est à (S/2, S/2 - wall·S/2) :
    c'est l'axe de rotation de la couche du repère (voir pointer)."""
    yy, xx = np.mgrid[0:S, 0:S].astype(float)
    c = (S - 1) / 2.0
    ct = c - wall * S / 2          # centre du dessus
    cb = c + wall * S / 2          # centre de la base
    Rp = R * S
    rgb = np.zeros((S, S, 3)); alpha = np.zeros((S, S))

    def capsule(r, y0, y1):
        yc = np.clip(yy, y0, y1)
        return np.clip((r - np.hypot(xx - c, yy - yc)) * 1.2, 0, 1)

    def wall_shade(r, y0, y1, col, n_knurl, metal=0.0):
        m = capsule(r, y0, y1)
        xn = np.clip((xx - c) / r, -1, 1)
        phi = np.arcsin(xn)
        # normale du cylindre : (sin φ, 0, cos φ) inclinée vers le bas (on voit le flanc avant)
        n = np.dstack([np.sin(phi), np.full_like(phi, 0.55), np.cos(phi)])
        n /= np.linalg.norm(n, axis=2, keepdims=True)
        diff = np.clip((n * LIGHT).sum(2), 0, 1)
        sp = np.clip((n * HALF).sum(2), 0, 1) ** 18
        k = 1.0
        if n_knurl:
            k = 0.78 + 0.22 * (np.sin(phi * n_knurl) > 0)   # cannelures
        base = np.array(col)[None, None, :]
        out = base * (0.35 + 0.85 * diff)[..., None] * (k if np.isscalar(k) else k[..., None])
        if metal:
            out = out * (1 - metal) + metal * base * (0.4 + 0.9 * np.clip(0.6 - 0.8 * np.sin(phi) * 0.5 + 0.4 * np.cos(phi * 3), 0, 1.3))[..., None]
        out += 0.18 * sp[..., None] + 0.06 * (diff ** 2)[..., None]
        # occlusion au pied du flanc
        ao = np.clip((y1 + r - yy) / (0.6 * (y1 - y0) + 1e-6), 0.55, 1)
        out *= ao[..., None]
        return out, m

    def top_shade(r, cy, col, dome_amt, metal=0.0, extra=None):
        rr = np.hypot(xx - c, yy - cy)
        m = np.clip((r - rr) * 1.2, 0, 1)
        bev = np.clip((r - rr) / (0.07 * r), 0, 1)
        h = m * (r * 0.10 * np.sin(bev * np.pi / 2) + dome_amt * r * (1 - (rr / r) ** 2))
        alb = np.zeros((S, S, 3)) + np.array(col)
        if extra is not None:
            alb = extra(rr, alb, r)
        env = lambda n: 0.45 + 0.7 * np.clip(0.55 - 0.55 * n[..., 1] + 0.30 * np.cos(np.arctan2(n[..., 1], n[..., 0]) * 2), 0, 1.4)
        out = shade(h, alb, spec=spec, gloss=gloss, metal=metal, env=env if metal else None)
        return out, m

    def over(src, a):
        nonlocal rgb, alpha
        rgb = src * a[..., None] + rgb * (1 - a[..., None])
        alpha = a + alpha * (1 - a)

    if skirt > 0:
        Rs = skirt * S
        ys0 = cb - wall * S * 0.25
        w_rgb, w_m = wall_shade(Rs, ys0, ys0 + skirt_wall * S, (0.40, 0.40, 0.42), 0, metal=0.3)
        over(w_rgb, w_m)
        def turned(rr, alb, r):
            conc = 0.03 * np.sin(rr * 2.4) + 0.02 * gaussian_filter(rng.standard_normal((S, S)), 0.6)
            return alb * (1 + conc)[..., None]
        t_rgb, t_m = top_shade(Rs, ys0, skirt_col, 0.0, metal=0.6, extra=turned)
        over(t_rgb, t_m)
        # ombre de contact du corps sur la jupe
        body_m = capsule(Rp, ct, cb)
        cont = gaussian_filter(body_m, S * 0.025)
        rgb *= (1 - 0.55 * np.clip(cont - body_m, 0, 1))[..., None]

    w_rgb, w_m = wall_shade(Rp, ct, cb, wall_col or cap_col, knurl, metal=cap_metal)
    over(w_rgb, w_m)

    def ring(rr, alb, r):
        if top_ring <= 0:
            return alb
        rr0 = top_ring * S
        m = np.clip((rr0 - rr) * 1.2, 0, 1) * np.clip((rr - (rr0 - 0.045 * S)) * 1.2, 0, 1)
        return alb * (1 - m[..., None]) + m[..., None] * np.array(ring_col)
    t_rgb, t_m = top_shade(Rp * 0.97, ct, top_col or cap_col, dome, metal=cap_metal, extra=ring)
    over(t_rgb, t_m)

    rgb, alpha = with_shadow(rgb, alpha, S, off=0.05, blur=0.035, strength=0.6)
    save(to_img(rgb, alpha), name, q=92)
    return ct / S   # position verticale de l'axe (fraction)


def pointer(name, S=256, kind="line", r0=0.10, r1=0.37, width=0.022, col=(0.96, 0.96, 0.94)):
    """Repère seul (couche qui tourne autour du centre de l'image) : trait ou point, à midi.
    L'interface le place sur l'axe du bouton (centre du dessus) et le fait tourner."""
    r, th, dx, dy = disc_coords(S)
    rr = r / S
    if kind == "line":
        a = np.clip((width * S / 2 - np.abs(dx)) * 1.2, 0, 1) * (dy < 0) * np.clip((rr - r0) * S, 0, 1) * np.clip((r1 - rr) * S, 0, 1)
    else:
        d = np.hypot(dx, dy + r1 * S)
        a = np.clip((width * S - d) * 1.2, 0, 1)
    rgb = np.zeros((S, S, 3)) + np.array(col)
    save(to_img(rgb, a), name, q=95)


def screw(name="vis.webp", S=64):
    r, th, dx, dy = disc_coords(S)
    rr = r / S
    body = np.clip((0.42 - rr) * S * 1.4, 0, 1)
    h = body * (8 * np.sqrt(np.clip((0.42 - rr) / 0.12, 0, 1)))
    slot = (np.abs(dx) < S * 0.045) | (np.abs(dy) < S * 0.045)
    h -= body * slot * 4
    alb = np.zeros((S, S, 3)) + 0.72
    rgb = shade(h, alb, spec=0.5, gloss=25, metal=0.7)
    rgb, a = with_shadow(rgb, body, S, off=0.05, blur=0.04, strength=0.5)
    save(to_img(rgb, a), name, q=92)


def footswitch(name="interrupteur-pied.webp", S=200):
    # interrupteur au pied : écrou rond moleté (métal sombre), tête chromée bombée
    r, th, dx, dy = disc_coords(S)
    rr = r / S
    nutR = 0.36 + 0.006 * np.cos(th * 40)
    nut = np.clip((nutR - rr) * S * 1.3, 0, 1)
    head = np.clip((0.25 - rr) * S * 1.3, 0, 1)
    h = nut * (10 * np.sqrt(np.clip((nutR - rr) / 0.05, 0, 1))) + head * (14 + 26 * np.sqrt(np.clip(1 - (rr / 0.25) ** 2, 0, 1)))
    alb = np.zeros((S, S, 3)) + 0.42
    alb[head > 0.5] = 0.86
    env = lambda n: 0.30 + 0.95 * np.clip(-n[..., 1] * 0.6 + 0.45 + 0.25 * np.sin(n[..., 0] * 5), 0, 1.3)
    rgb = shade(h, alb, spec=0.8, gloss=40, metal=0.9, env=env)
    a = np.maximum(nut, head)
    rgb, a = with_shadow(rgb, a, S, off=0.05, blur=0.035, strength=0.6)
    save(to_img(rgb, a), name, q=92)


def jack_wheel(name="molette.webp", w=72, h=300):
    # molette de hauteur / modulation : cylindre caoutchouté vu de face
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    u = (xx / (w - 1)) * 2 - 1
    ridges = (np.sin(yy / 3.2) > 0.3) * 1.0
    hgt = np.sqrt(np.clip(1 - u ** 2, 0, 1)) * 30 + ridges * 1.5
    alb = np.zeros((h, w, 3)) + 0.11
    rgb = shade(hgt, alb, spec=0.25, gloss=20)
    v = np.clip(np.minimum(yy, h - 1 - yy) / 40.0, 0, 1)
    rgb *= (0.45 + 0.55 * v)[..., None]
    save(to_img(rgb), name, q=90)


if __name__ == "__main__":
    wood()
    wood(name="bois-clair.webp", tone=(0.52, 0.32, 0.16), seed=5)
    brushed()
    brushed(name="alu-bleu.webp", tint=(0.21, 0.29, 0.43), strength=1.2)
    brushed(name="alu-sombre.webp", tint=(0.22, 0.22, 0.23), strength=1.1)
    brushed(name="alu-orange.webp", tint=(0.95, 0.52, 0.11), strength=0.7)
    paint()
    paint(name="plastique-gris.webp", color=(0.70, 0.70, 0.68), grain=0.004)
    paint(name="plastique-vert.webp", color=(0.55, 0.72, 0.22), grain=0.004)
    hammertone()
    hammertone(name="martele-orange.webp", color=(0.92, 0.42, 0.06), seed=4)
    hammertone(name="martele-vert.webp", color=(0.47, 0.56, 0.38), seed=5)
    hammertone(name="martele-bleu.webp", color=(0.14, 0.42, 0.72), seed=6)
    hammertone(name="martele-creme.webp", color=(0.88, 0.84, 0.74), seed=8)
    # boutons (l'axe de rotation du repère est imprimé : il sert dans la feuille de style)
    axes = {}
    axes["moog"] = knob("bouton-moog.webp", R=0.25, wall=0.12, cap_col=(0.05, 0.05, 0.055), knurl=40, skirt=0.39, skirt_col=(0.58, 0.58, 0.60), dome=0.06)
    axes["poly"] = knob("bouton-poly.webp", R=0.36, wall=0.12, cap_col=(0.07, 0.075, 0.08), dome=0.02, spec=0.55, gloss=80)
    axes["pedale"] = knob("bouton-pedale.webp", R=0.36, wall=0.13, cap_col=(0.05, 0.05, 0.05), knurl=36, dome=0.03)
    axes["creme"] = knob("bouton-creme.webp", R=0.36, wall=0.12, cap_col=(0.88, 0.84, 0.74), knurl=30, dome=0.05, spec=0.3, gloss=30)
    axes["echo"] = knob("bouton-echo.webp", R=0.40, wall=0.10, cap_col=(0.05, 0.05, 0.05), knurl=60, dome=0.02, top_ring=0.26)
    axes["plastique"] = knob("bouton-plastique.webp", R=0.33, wall=0.12, cap_col=(0.16, 0.16, 0.17), dome=0.10, spec=0.35, gloss=35)
    axes["chrome"] = knob("bouton-chrome.webp", R=0.36, wall=0.12, cap_col=(0.72, 0.73, 0.75), knurl=44, dome=0.04, cap_metal=0.85, spec=0.6)
    for k, v in axes.items():
        print(f"  axe {k}: {v * 100:.1f} %")
    pointer("repere-trait.webp", r0=0.04, r1=0.30, width=0.03)
    pointer("repere-trait-noir.webp", r0=0.04, r1=0.30, width=0.03, col=(0.08, 0.08, 0.08))
    pointer("repere-point.webp", kind="dot", r1=0.31, width=0.035)
    pointer("repere-jupe.webp", r0=0.34, r1=0.46, width=0.022, col=(0.10, 0.10, 0.10))
    screw()
    footswitch()
    jack_wheel()


def fader_cap(name="curseur.webp", w=96, h=150, col=(0.10, 0.10, 0.11), line=(0.95, 0.95, 0.92)):
    # bouton de curseur : bloc à dessus creusé (prise), ligne blanche, ombre cuite
    S = 200
    yy, xx = np.mgrid[0:S, 0:S].astype(float)
    cx, cy = S / 2, S / 2
    hw, hh = w / 2, h / 2
    dx = np.abs(xx - cx) - hw + 12; dy = np.abs(yy - cy) - hh + 12
    d = np.hypot(np.clip(dx, 0, None), np.clip(dy, 0, None)) + np.minimum(np.maximum(dx, dy), 0) - 12
    m = np.clip(-d * 1.2, 0, 1)
    edge = np.clip(-d / 10, 0, 1)
    groove = np.cos((yy - cy) / hh * np.pi / 2) ** 2   # dessus creusé
    hgt = m * (16 * np.sqrt(edge) - 6 * groove * edge)
    alb = np.zeros((S, S, 3)) + np.array(col)
    ln = (np.abs(yy - cy) < 2.2) & (np.abs(xx - cx) < hw - 10)
    alb[ln] = line
    rgb = shade(hgt, alb, spec=0.35, gloss=30)
    rgb, a = with_shadow(rgb, m, S, off=0.05, blur=0.035, strength=0.65)
    save(to_img(rgb, a), name, q=92)


if __name__ == "__main__":
    fader_cap()
    fader_cap(name="curseur-gris.webp", col=(0.62, 0.62, 0.64), line=(0.1, 0.1, 0.1))
