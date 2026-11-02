# Komodo Fuel (FM à 6 opérateurs, matrice libre 6 × 6, 12 formes calculées, enveloppes en boucle,
# filtre multimode) : 10 sons par catégorie. Réglages originaux (aucun son d'origine recopié).
# Formes : 0 sinus, 1 demi-sinus, 2 redressé, 3 quarts, 4 doublé, 5 carré doux, 6 scie, 7 triangle,
#          8 impulsion, 9 octave, 10 impair, 11 marches.
# Matrice : m{j}{k} = l'opérateur j module k (m{j}{j} = rétroaction).
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

RATIOS = [0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 20, 24, 32]


def op(k, r=1, wave=0, out=0, att=2, dec=1500, sus=70, rel=400, vel=30, fine=0, loop=0, dly=0, hld=0):
    o = f"o{k}_"
    return {o + "coarse": RATIOS.index(r), o + "wave": wave, o + "out": out, o + "att": att, o + "dec": dec, o + "sus": sus,
            o + "rel": rel, o + "vel": vel, o + "fine": fine, o + "loop": loop, o + "dly": dly, o + "hld": hld}


def mat(*links):
    """links : (j, k, quantité). Efface le lien 2 → 1 d'origine s'il n'est pas redonné."""
    d = {"m21": 0}
    for j, k, a in links:
        d[f"m{j}{k}"] = a
    return d


def flt(kind=1, cut=3000, res=20, env=0, a=2, d=500, s=30, r=400, key=50):
    return dict(ftype=kind, fcut=cut, fres=res, fenv=env, fa=a, fd=d, fs=s, fr=r, fkey=key)


MONO = dict(vmode=1, voices=1)
LEG = dict(vmode=2, voices=1)
NOF = dict(ftype=0)


def pair(car, mod, link, **kw):
    return mix(car, mod, mat(link), kw)


def build():
    b = Bank("fmx")
    add = b.add

    # --- Basses
    add("Basse carburant", BASS, mix(op(1, 0.5, out=100, dec=1500, sus=70, rel=150), op(2, 0.5, dec=300, sus=25), mat((2, 1, 55), (2, 2, 20)), flt(1, 1400, 15, 35, d=250), MONO))
    add("Basse redressée", BASS, mix(op(1, 0.5, wave=2, out=100, dec=1200, sus=70, rel=120), op(2, 1, dec=200, sus=10), mat((2, 1, 40)), flt(1, 1800, 20, 30, d=200), MONO))
    add("Basse en scie FM", BASS, mix(op(1, 0.5, wave=6, out=100, sus=80, rel=120), op(2, 1, wave=0, dec=250, sus=20), mat((2, 1, 35)), flt(1, 700, 45, 50, d=220), LEG, glide=40))
    add("Basse sub et clic", BASS, mix(op(1, 0.5, out=100, dec=2000, sus=80, rel=150), op(2, 4, dec=40, sus=0), op(3, 0.5, wave=0, out=0), mat((2, 1, 45)), NOF, MONO))
    add("Basse marches", BASS, mix(op(1, 0.5, wave=11, out=100, sus=75, rel=120), op(2, 0.5, dec=300, sus=30), mat((2, 1, 30)), flt(1, 1200, 30, 30, d=250), MONO))
    add("Basse grondante FM", BASS, mix(op(1, 0.5, out=100, sus=80, rel=150), op(2, 0.5, dec=500, sus=50), op(3, 1, dec=200, sus=20), mat((2, 1, 50), (3, 2, 40), (3, 3, 30)),
                                        flt(1, 1100, 25, 35, d=300), LEG, glide=30))
    add("Basse carrée douce", BASS, mix(op(1, 0.5, wave=5, out=100, sus=80, rel=120), op(2, 1, dec=150, sus=0), mat((2, 1, 25)), flt(1, 900, 20, 40, d=200), MONO))
    add("Basse en boucle", BASS, mix(op(1, 0.5, out=100, sus=80, rel=150), op(2, 1, att=10, dec=180, sus=0, loop=1), mat((2, 1, 40)), flt(1, 1500, 30, 20), MONO))
    add("Basse impulsion", BASS, mix(op(1, 0.5, wave=8, out=100, dec=800, sus=60, rel=120), op(2, 0.5, dec=200, sus=10), mat((2, 1, 30)), flt(1, 1300, 25, 40, d=200), MONO))
    add("Basse double", BASS, mix(op(1, 0.5, out=80, sus=80, rel=150), op(2, 1, dec=250, sus=20), op(3, 0.5, out=60, fine=0.4, sus=80, rel=150), op(4, 2, dec=200, sus=10),
                                  mat((2, 1, 45), (4, 3, 35)), flt(1, 1600, 20, 30, d=250), MONO, spread=50))

    # --- Leads
    LD = mix(LEG, glide=45)
    add("Lead carburant", LEAD, mix(op(1, 1, out=100, sus=90), op(2, 1, wave=5, sus=70), op(3, 2, sus=40), mat((2, 1, 50), (3, 2, 30), (1, 1, 15)), NOF, LD))
    add("Lead scie FM 2", LEAD, mix(op(1, 1, wave=6, out=100, sus=90), op(2, 2, sus=50), mat((2, 1, 20)), flt(1, 4000, 25, 20), LD))
    add("Lead verre libre", LEAD, mix(op(1, 2, out=100, sus=85), op(2, 7, sus=40, dec=600), op(3, 1, out=60, fine=0.3, sus=85), mat((2, 1, 35), (2, 3, 20)), NOF, LD))
    add("Lead hurlant libre", LEAD, mix(op(1, 1, out=100, sus=90), op(2, 1, sus=90), op(3, 1, sus=80), mat((2, 1, 70), (3, 2, 55), (3, 3, 60)), flt(1, 5000, 30, 15), LD))
    add("Lead flûte libre", LEAD, mix(op(1, 1, out=100, att=60, sus=90), op(2, 1, att=40, sus=40), op(6, 1, wave=0, out=0), mat((2, 1, 15)), NOF, LD))
    add("Lead demi-sinus", LEAD, mix(op(1, 1, wave=1, out=100, sus=90), op(2, 2, wave=1, sus=60), mat((2, 1, 40)), NOF, LD))
    add("Lead octave libre", LEAD, mix(op(1, 1, wave=9, out=100, sus=90), op(2, 2, sus=50), mat((2, 1, 25)), NOF, LD))
    add("Lead impair", LEAD, mix(op(1, 1, wave=10, out=100, sus=90), op(2, 3, sus=40), mat((2, 1, 30)), flt(1, 3500, 30, 20), LD))
    add("Lead mordant libre", LEAD, mix(op(1, 1, out=100, sus=85), op(2, 1, dec=300, sus=60), mat((2, 1, 60), (2, 2, 45)), flt(1, 2500, 45, 35, d=300), LD))
    add("Lead sifflet libre", LEAD, mix(op(1, 4, out=100, att=30, sus=90), op(2, 1, sus=10), mat((2, 1, 8)), NOF, LD, glide=90))

    # --- Nappes
    NP = dict(spread=80)
    add("Nappe carburant", PAD, mix(op(1, 1, wave=7, out=100, att=900, dec=3000, sus=85, rel=2500), op(2, 1, att=1500, sus=60, rel=2500), op(3, 1, wave=7, out=80, fine=0.6, att=900, sus=85, rel=2500),
                                    op(4, 2, att=1800, sus=50, rel=2500), mat((2, 1, 25), (4, 3, 25)), flt(1, 3500, 15), NP))
    add("Nappe à formes 2", PAD, mix(op(1, 1, wave=9, out=100, att=1200, sus=85, rel=3000), op(2, 2, wave=3, att=2000, sus=60, rel=3000, loop=1, dec=4000), mat((2, 1, 25)), flt(1, 4000, 20), NP))
    add("Nappe de verre 2", PAD, mix(op(1, 2, out=100, att=1000, sus=80, rel=3000), op(2, 5, att=1500, sus=40, rel=3000), op(3, 1, out=70, fine=0.5, att=1000, sus=80, rel=3000),
                                     op(4, 3, att=1500, sus=40, rel=3000), mat((2, 1, 25), (4, 3, 25)), NOF, NP))
    add("Nappe sombre libre", PAD, mix(op(1, 0.5, wave=7, out=100, att=1500, sus=90, rel=3000), op(2, 1, att=2000, sus=60, rel=3000), op(3, 1, wave=7, out=70, fine=-0.4, att=1500, sus=90, rel=3000),
                                       mat((2, 1, 20), (2, 3, 15)), flt(1, 1500, 15), NP))
    add("Nappe qui boucle", PAD, mix(op(1, 1, out=100, att=900, sus=85, rel=2500), op(2, 3, att=600, dec=1200, sus=0, rel=2500, loop=1), op(3, 1, out=70, fine=0.4, att=900, sus=85, rel=2500),
                                     mat((2, 1, 30), (2, 3, 20)), flt(1, 3000, 20), NP))
    add("Nappe redressée", PAD, mix(op(1, 1, wave=2, out=90, att=1200, sus=85, rel=3000), op(3, 2, wave=2, out=60, fine=0.3, att=1500, sus=85, rel=3000), op(2, 1, att=1800, sus=40, rel=3000),
                                    mat((2, 1, 15), (2, 3, 15)), flt(1, 2500, 20), NP))
    add("Nappe chorale libre", PAD, mix(op(1, 1, wave=7, out=90, att=1000, sus=85, rel=2500), op(3, 1, wave=7, out=90, fine=0.8, att=1000, sus=85, rel=2500),
                                        op(5, 2, wave=7, out=60, fine=-0.6, att=1000, sus=85, rel=2500), op(2, 1, att=1200, sus=50, rel=2500),
                                        mat((2, 1, 20), (2, 3, 20), (2, 5, 15)), flt(2, 1200, 30), NP, spread=100))
    add("Nappe métallique libre", PAD, mix(op(1, 1, out=100, att=900, sus=80, rel=3000), op(2, 7, fine=0.2, att=1500, sus=40, rel=3000), op(3, 1, out=70, fine=0.5, att=900, sus=80, rel=3000),
                                           op(4, 11, att=1500, sus=40, rel=3000), mat((2, 1, 30), (4, 3, 30)), NOF, NP))
    add("Nappe respirante 2", PAD, mix(op(1, 1, wave=7, out=100, att=1200, sus=85, rel=3000), op(2, 1, att=800, dec=2500, sus=10, rel=3000, loop=1), mat((2, 1, 30)), flt(1, 2000, 25, 20, a=1500, d=2500), NP))
    add("Nappe de marches", PAD, mix(op(1, 1, wave=11, out=90, att=1000, sus=85, rel=2500), op(3, 2, wave=11, out=50, fine=0.4, att=1000, sus=85, rel=2500), mat(), flt(1, 2500, 25), NP))

    # --- Plucks
    add("Pluck carburant", PLUCK, mix(op(1, 1, out=100, dec=500, sus=0, rel=300), op(2, 3, dec=200, sus=0), mat((2, 1, 60)), NOF))
    add("Pluck en verre 2", PLUCK, mix(op(1, 2, out=100, dec=600, sus=0, rel=400), op(2, 7, dec=150, sus=0), op(3, 1, out=60, dec=700, sus=0, rel=400), op(4, 5, dec=200, sus=0),
                                       mat((2, 1, 45), (4, 3, 40)), NOF))
    add("Pluck demi-sinus", PLUCK, mix(op(1, 1, wave=1, out=100, dec=400, sus=0, rel=300), op(2, 2, dec=150, sus=0), mat((2, 1, 50)), NOF))
    add("Pluck sourd libre", PLUCK, mix(op(1, 1, out=100, dec=400, sus=0, rel=250), op(2, 1, dec=150, sus=0), mat((2, 1, 50)), flt(1, 1500, 25, 50, d=200)))
    add("Koto libre", PLUCK, mix(op(1, 1, out=100, dec=1200, sus=0, rel=500), op(2, 3, dec=300, sus=0), op(3, 5, dec=150, sus=0), mat((2, 1, 50), (3, 2, 30), (1, 1, 20)), NOF))
    add("Pizz libre", PLUCK, mix(op(1, 1, wave=7, out=100, dec=300, sus=0, rel=200), op(2, 1, dec=100, sus=0), mat((2, 1, 35)), flt(1, 3000, 15, 30, d=150)))
    add("Pluck impair", PLUCK, mix(op(1, 1, wave=10, out=100, dec=450, sus=0, rel=300), op(2, 2, dec=200, sus=0), mat((2, 1, 35)), NOF))
    add("Pluck en boucle", PLUCK, mix(op(1, 1, out=100, dec=700, sus=0, rel=400), op(2, 4, att=5, dec=90, sus=0, loop=1), mat((2, 1, 35)), NOF))
    add("Steel libre", PLUCK, mix(op(1, 1, out=100, dec=1500, sus=0, rel=800), op(2, 3, dec=600, sus=0), op(3, 2, out=40, dec=1200, sus=0, rel=800), mat((2, 1, 40), (2, 3, 20)), NOF))
    add("Pluck sec libre", PLUCK, mix(op(1, 1, wave=6, out=100, dec=250, sus=0, rel=150), mat(), flt(1, 600, 40, 70, d=150)))

    # --- Claviers
    add("Piano électrique libre", KEYS, mix(op(1, 1, out=100, dec=2500, sus=0, rel=600), op(2, 1, dec=1200, sus=10, vel=70), op(3, 1, out=60, fine=0.4, dec=2800, sus=0, rel=600),
                                            op(4, 14, dec=250, sus=0, vel=80), mat((2, 1, 40), (4, 3, 30)), NOF))
    add("Piano de verre libre", KEYS, mix(op(1, 1, out=100, dec=3000, sus=0, rel=700), op(2, 14, dec=600, sus=0, vel=60), op(3, 2, out=40, dec=2000, sus=0, rel=700), op(4, 1, dec=1000, sus=0),
                                          mat((2, 1, 30), (4, 3, 30)), NOF))
    add("Orgue libre", KEYS, mix(op(1, 1, wave=0, out=100, sus=100, rel=80), op(2, 2, out=70, sus=100, rel=80), op(3, 3, out=45, sus=100, rel=80), op(4, 4, out=35, sus=100, rel=80),
                                 op(5, 8, out=20, sus=100, rel=80), mat(), NOF))
    add("Orgue à percussion", KEYS, mix(op(1, 1, out=100, sus=100, rel=80), op(2, 2, out=70, sus=100, rel=80), op(3, 3, out=60, dec=250, sus=0, rel=80), mat(), NOF))
    add("Clavinette libre", KEYS, mix(op(1, 1, wave=8, out=100, dec=900, sus=30, rel=120), op(2, 3, dec=300, sus=10), mat((2, 1, 30)), flt(1, 3500, 30, 30, d=200)))
    add("Célesta libre", KEYS, mix(op(1, 2, out=100, dec=2000, sus=0, rel=1000), op(2, 7, dec=400, sus=0), op(3, 4, out=40, dec=1500, sus=0, rel=1000), mat((2, 1, 30)), NOF))
    add("Vibraphone libre", KEYS, mix(op(1, 1, out=100, dec=3500, sus=0, rel=1200), op(2, 4, dec=300, sus=0), op(3, 1, out=30, dec=3500, sus=0, rel=1200, fine=1.2), mat((2, 1, 25)), NOF))
    add("Clavecin libre", KEYS, mix(op(1, 1, wave=6, out=100, dec=1500, sus=0, rel=300), op(2, 3, dec=400, sus=0), mat((2, 1, 25)), flt(1, 6000, 10)))
    add("Marimba libre", KEYS, mix(op(1, 1, out=100, dec=600, sus=0, rel=300), op(2, 4, dec=80, sus=0), mat((2, 1, 45)), NOF))
    add("Accords numériques", KEYS, mix(op(1, 1, wave=5, out=100, dec=800, sus=30, rel=300), op(2, 2, dec=300, sus=10), mat((2, 1, 30)), flt(1, 2000, 30, 35, d=300)))

    # --- Cordes
    ST = dict(spread=80)
    add("Cordes libres", STR, mix(op(1, 1, wave=6, out=90, att=500, sus=90, rel=1500), op(3, 1, wave=6, out=90, fine=0.7, att=550, sus=90, rel=1500), mat(), flt(1, 4500, 10), ST))
    add("Cordes lentes libres", STR, mix(op(1, 1, wave=6, out=90, att=1500, sus=90, rel=2500), op(3, 1, wave=6, out=80, fine=-0.6, att=1500, sus=90, rel=2500), mat(), flt(1, 3000, 10), ST))
    add("Violons libres", STR, mix(op(1, 2, wave=6, out=90, att=350, sus=90, rel=900), op(3, 2, wave=6, out=80, fine=0.5, att=350, sus=90, rel=900), mat(), flt(1, 6000, 10), ST))
    add("Violoncelles libres", STR, mix(op(1, 0.5, wave=6, out=90, att=400, sus=90, rel=900), op(3, 0.5, wave=6, out=80, fine=0.5, att=400, sus=90, rel=900), mat(), flt(1, 2500, 10), ST))
    add("Cordes en FM", STR, mix(op(1, 1, out=90, att=500, sus=90, rel=1500), op(2, 1, att=600, sus=80), op(3, 1, out=90, fine=0.7, att=550, sus=90, rel=1500), op(4, 1, fine=0.7, att=600, sus=80),
                                 mat((2, 1, 30), (2, 2, 35), (4, 3, 30), (4, 4, 35)), flt(1, 5000, 10), ST))
    add("Pizzicato libre", STR, mix(op(1, 1, wave=6, out=100, dec=300, sus=0, rel=250), op(3, 1, wave=6, out=70, fine=0.5, dec=300, sus=0, rel=250), mat(), flt(1, 2500, 10, 30, d=150), ST))
    add("Cordes brillantes libres", STR, mix(op(1, 1, wave=6, out=90, att=400, sus=90, rel=1200), op(3, 2, wave=6, out=60, fine=0.5, att=400, sus=90, rel=1200), mat(), flt(1, 8000, 10), ST))
    add("Cordes sombres libres", STR, mix(op(1, 1, wave=7, out=90, att=600, sus=90, rel=1500), op(3, 1, wave=6, out=70, fine=0.6, att=600, sus=90, rel=1500), mat(), flt(1, 1500, 10), ST))
    add("Trémolo libre", STR, mix(op(1, 1, wave=6, out=90, att=300, sus=90, rel=1000), op(3, 1, wave=6, out=80, fine=0.6, att=300, sus=90, rel=1000), op(5, 0.5, out=0),
                                  mat(), flt(1, 4000, 10), ST))
    add("Cordes en octaves libres", STR, mix(op(1, 1, wave=6, out=90, att=600, sus=90, rel=1500), op(3, 2, wave=6, out=60, fine=0.4, att=600, sus=90, rel=1500),
                                             op(5, 0.5, wave=6, out=60, fine=-0.4, att=600, sus=90, rel=1500), mat(), flt(1, 4000, 10), ST))

    # --- Cuivres
    add("Cuivres carburant", BRASS, mix(op(1, 1, out=100, att=60, dec=1500, sus=80, rel=300), op(2, 1, att=90, dec=900, sus=60), mat((2, 1, 55), (2, 2, 30)),
                                        flt(1, 3500, 10, 25, a=80, d=500, s=60)))
    add("Trompette libre", BRASS, mix(op(1, 1, out=100, att=40, sus=85, rel=200), op(2, 1, att=60, dec=600, sus=60), mat((2, 1, 60), (2, 2, 35)), flt(1, 4500, 10, 25, a=50, d=400, s=60), LEG, glide=25))
    add("Section libre", BRASS, mix(op(1, 1, out=90, att=60, sus=85, rel=300), op(2, 1, att=80, sus=60), op(3, 1, out=90, fine=0.6, att=60, sus=85, rel=300), op(4, 1, fine=0.6, att=80, sus=60),
                                    mat((2, 1, 50), (2, 2, 30), (4, 3, 50), (4, 4, 30)), flt(1, 4000, 10, 25, a=60, d=500, s=60), spread=70))
    add("Trombone libre", BRASS, mix(op(1, 0.5, out=100, att=70, sus=85, rel=300), op(2, 0.5, att=100, sus=60), mat((2, 1, 55), (2, 2, 25)), flt(1, 2500, 10, 25, a=90, d=500, s=60)))
    add("Cuivres staccato libres", BRASS, mix(op(1, 1, out=100, att=20, dec=300, sus=20, rel=150), op(2, 1, att=30, dec=250, sus=20), mat((2, 1, 55), (2, 2, 30)), flt(1, 4000, 10, 30, d=250)))
    add("Cor libre", BRASS, mix(op(1, 1, out=100, att=120, sus=85, rel=400), op(2, 1, att=150, sus=40), mat((2, 1, 35)), flt(1, 1800, 10, 20, a=150, d=600, s=50)))
    add("Saxo libre", BRASS, mix(op(1, 1, out=100, att=40, sus=90, rel=200), op(2, 3, att=50, sus=50), op(3, 1, att=50, sus=60), mat((2, 1, 35), (3, 1, 35), (3, 3, 25)),
                                 flt(1, 3000, 20, 15), LEG, glide=30))
    add("Cuivres synthé libres", BRASS, mix(op(1, 1, wave=6, out=90, att=40, sus=90, rel=300), op(3, 1, wave=6, out=90, fine=0.8, att=40, sus=90, rel=300), mat(),
                                            flt(1, 900, 30, 55, a=80, d=600, s=40), spread=70))
    add("Cuivres doux libres", BRASS, mix(op(1, 1, out=100, att=150, sus=85, rel=500), op(2, 1, att=200, sus=50), mat((2, 1, 40), (2, 2, 20)), flt(1, 2200, 10, 20, a=150, d=700, s=60)))
    add("Fanfare libre", BRASS, mix(op(1, 1, out=90, att=50, sus=85, rel=300), op(2, 1, att=70, sus=60), op(3, 2, out=60, att=50, sus=85, rel=300), op(4, 1, att=70, sus=60),
                                    mat((2, 1, 55), (2, 2, 35), (4, 3, 45)), flt(1, 5000, 10, 20, a=50, d=500, s=60)))

    # --- Arpèges
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège libre", ARP, mix(op(1, 1, out=100, dec=350, sus=0, rel=200), op(2, 2, wave=8, dec=200, sus=0), mat((2, 1, 50)), NOF, AR))
    add("Arpège verre libre", ARP, mix(op(1, 2, out=100, dec=400, sus=0, rel=300), op(2, 7, dec=150, sus=0), mat((2, 1, 40)), NOF, AR, arp_mode=3))
    add("Arpège rond libre", ARP, mix(op(1, 1, wave=7, out=100, dec=350, sus=0, rel=250), op(2, 1, dec=150, sus=0), mat((2, 1, 25)), NOF, AR, arp_mode=2))
    add("Arpège claquant libre", ARP, mix(op(1, 1, out=100, dec=250, sus=0, rel=150), op(2, 3, dec=100, sus=0), mat((2, 1, 60), (2, 2, 30)), NOF, AR, arp_rate=4))
    add("Arpège cloches libres", ARP, mix(op(1, 1, out=100, dec=1500, sus=0, rel=800), op(2, 5, fine=2, dec=500, sus=0), mat((2, 1, 40)), NOF, AR, arp_rate=1, arp_oct=3))
    add("Arpège filtré libre", ARP, mix(op(1, 1, wave=6, out=100, dec=300, sus=20, rel=150), mat(), flt(1, 900, 55, 55, d=180), AR, arp_mode=5))
    add("Arpège aléatoire libre", ARP, mix(op(1, 1, out=100, dec=300, sus=0, rel=200), op(2, 3, dec=200, sus=0), op(3, 2, out=50, dec=300, sus=0, rel=200), mat((2, 1, 40), (2, 3, 25)),
                                           NOF, AR, arp_mode=6, arp_oct=3))
    add("Arpège basse libre", ARP, mix(op(1, 0.5, out=100, dec=300, sus=30, rel=100), op(2, 1, dec=150, sus=0), mat((2, 1, 45)), flt(1, 1500, 30, 40, d=200), AR, arp_oct=1))
    add("Arpège court libre", ARP, mix(op(1, 1, wave=5, out=100, dec=200, sus=0, rel=120), op(2, 2, dec=100, sus=0), mat((2, 1, 30)), NOF, AR, arp_gate=25, arp_rate=5))
    add("Arpège en marches", ARP, mix(op(1, 1, wave=11, out=100, dec=400, sus=0, rel=250), mat(), flt(1, 2500, 30, 30, d=200), AR, arp_oct=3))

    # --- Effets
    add("Cloche du réservoir", FX, mix(op(1, 1, out=100, dec=6000, sus=0, rel=4000), op(2, 3, fine=1.5, dec=3000, sus=0), op(3, 1, out=60, fine=0.7, dec=5000, sus=0, rel=4000),
                                       op(4, 7, fine=-2, dec=2500, sus=0), mat((2, 1, 50), (4, 3, 45)), NOF, spread=60))
    add("Tôle libre", FX, mix(op(1, 1, out=100, dec=3000, sus=0, rel=2000), op(2, 7, fine=3, dec=2000, sus=0), op(3, 11, dec=1500, sus=0), mat((2, 1, 70), (3, 2, 60), (3, 3, 70)), NOF))
    add("Sirène libre", FX, mix(op(1, 1, out=100, sus=100, rel=500), op(2, 0.5, att=1500, dec=1500, sus=0, loop=1), mat((2, 1, 40)), NOF))
    add("Laser libre", FX, mix(op(1, 2, out=100, dec=500, sus=0, rel=300), op(2, 3, dec=200, sus=0), mat((2, 1, 70), (1, 1, 30)), flt(1, 9000, 20, -30, d=400)))
    add("Goutte libre", FX, mix(op(1, 4, out=100, att=1, dec=900, sus=0, rel=500), op(2, 1, dec=120, sus=0), op(3, 2, out=60, dec=600, sus=0, rel=400), mat((2, 1, 45), (2, 3, 30)), NOF, spread=60))
    add("Vent libre", FX, mix(op(1, 1, out=100, att=2000, sus=90, rel=3000), op(2, 32, att=2000, sus=90), op(3, 24, att=2000, sus=90), mat((2, 1, 90), (3, 2, 80), (3, 3, 90)),
                              flt(2, 1500, 60, 30, a=2000, d=3000, s=50), spread=100))
    add("Robot libre", FX, mix(op(1, 1, wave=11, out=100, sus=90, rel=200), op(2, 1, att=5, dec=120, sus=0, loop=1), mat((2, 1, 50)), flt(2, 1500, 60)))
    add("Impact libre", FX, mix(op(1, 0.5, out=100, dec=800, sus=0, rel=400), op(2, 1, dec=200, sus=0), op(3, 7, dec=100, sus=0), mat((2, 1, 80), (3, 2, 70), (3, 3, 60)), NOF))
    add("Ondes libres", FX, mix(op(1, 1, wave=7, out=90, att=1500, sus=90, rel=3000), op(3, 1, wave=9, out=70, fine=3, att=1500, sus=90, rel=3000), op(2, 1, att=3000, dec=4000, sus=0, loop=1),
                                mat((2, 1, 25), (2, 3, 25)), flt(1, 3000, 30), spread=100))
    add("Moteur libre", FX, mix(op(1, 0.5, wave=5, out=100, sus=90, rel=300), op(2, 0.5, att=5, dec=60, sus=0, loop=1), mat((2, 1, 60), (1, 1, 40)), flt(1, 1200, 50)))
    return b
