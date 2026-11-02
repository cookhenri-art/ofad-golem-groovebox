# Old Metal (FM à 6 opérateurs, principe des claviers FM des années 80) : 10 sons par catégorie.
# Réglages originaux construits sur les recettes de chaque catégorie (aucun son d'origine recopié).
# Écriture : patch() place les 6 opérateurs ; les enveloppes suivent la convention 4 vitesses / 4 niveaux.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT


def op(k, **kw):
    return {f"o{k}_{(n[:-1] if n.endswith('_') else n)}": v for n, v in kw.items()}


def eg(k, r, l):
    d = {}
    for s in range(4):
        d[f"o{k}_r{s + 1}"] = r[s]
        d[f"o{k}_l{s + 1}"] = l[s]
    return d


# enveloppes (vitesses, niveaux)
E_PLK = ((99, 40, 30, 62), (99, 72, 0, 0))
E_PLK_FAST = ((99, 60, 40, 70), (99, 40, 0, 0))
E_SUS = ((99, 50, 40, 72), (99, 95, 92, 0))
E_PAD = ((30, 30, 25, 30), (99, 95, 92, 0))
E_PAD_SLOW = ((24, 26, 22, 26), (99, 95, 92, 0))
E_BRASS = ((62, 42, 35, 66), (99, 92, 88, 0))
E_STR = ((42, 36, 30, 40), (99, 96, 94, 0))
E_KEY = ((99, 34, 24, 60), (99, 82, 0, 0))
E_BELL = ((99, 26, 18, 30), (99, 50, 0, 0))
E_ORG = ((99, 99, 99, 85), (99, 99, 99, 0))


def scale(e, lev):
    """Enveloppe de modulateur : mêmes vitesses, niveaux multipliés (0..1)."""
    r, l = e
    return (r, tuple(int(round(x * lev)) for x in l))


def pairs(algo=5, fb=0, car=((1, 0), (1, 0), (1, 0)), mods=((1, 60), (1, 50), (1, 40)), ce=E_SUS, me=None, mlev=1.0, **extra):
    """Trois paires porteur ← modulateur (algorithmes 5 ou 6) : car = (rapport grossier, désaccord),
    mods = (rapport grossier, niveau). Les niveaux des porteurs sont pleins."""
    me = me or ce
    d = dict(algo=algo, feedback=fb)
    for i in range(3):
        c, m = 2 * i + 1, 2 * i + 2
        cr, cd = car[i]
        mr, ml = mods[i]
        d.update(op(c, coarse=cr, detune=cd, level=99 if i == 0 else 92))
        d.update(eg(c, *ce))
        d.update(op(m, coarse=mr, level=int(ml)))
        d.update(eg(m, *scale(me, mlev)))
    d.update(extra)
    return d


def stack(algo=1, fb=5, c1=(1, 0), m2=(1, 70), c3=(1, 0), m4=(1, 60), m5=(2, 50), m6=(1, 40), ce=E_SUS, me=None, **extra):
    """Algorithme 1 / 2 : 1 ← 2, et 3 ← 4 ← 5 ← 6 (pile de 4)."""
    me = me or ce
    d = dict(algo=algo, feedback=fb)
    d.update(op(1, coarse=c1[0], detune=c1[1], level=99)); d.update(eg(1, *ce))
    d.update(op(2, coarse=m2[0], level=m2[1])); d.update(eg(2, *me))
    d.update(op(3, coarse=c3[0], detune=c3[1], level=90)); d.update(eg(3, *ce))
    for k, (r, lv) in ((4, m4), (5, m5), (6, m6)):
        d.update(op(k, coarse=r, level=lv)); d.update(eg(k, *me))
    d.update(extra)
    return d


def organ(ratios=(0, 1, 2, 3, 4, 6), levels=(90, 99, 70, 50, 40, 30), detunes=(0, 0, 0, 0, 0, 0), ce=E_ORG, **extra):
    """Algorithme 32 : six porteurs (tirettes)."""
    d = dict(algo=32, feedback=0)
    for k in range(6):
        d.update(op(k + 1, coarse=ratios[k], level=levels[k], detune=detunes[k]))
        d.update(eg(k + 1, *ce))
    d.update(extra)
    return d


def build():
    b = Bank("fm6")
    add = b.add

    # --- Basses (10)
    BAS = dict(vmode=1, voices=1)
    add("Basse FM ronde", BASS, mix(pairs(fb=2, car=((0, 0), (0, 2), (1, 0)), mods=((0, 55), (1, 40), (0, 30)), ce=E_SUS, me=E_PLK, mlev=0.8), BAS))
    add("Basse claquante", BASS, mix(pairs(fb=5, car=((0, 0), (1, 0), (0, 3)), mods=((1, 78), (3, 55), (0, 40)), ce=E_SUS, me=E_PLK_FAST), BAS))
    add("Basse lourde FM", BASS, mix(stack(fb=6, c1=(0, 0), m2=(0, 75), c3=(0, 3), m4=(1, 70), m5=(1, 55), m6=(2, 40), ce=E_SUS, me=E_PLK), BAS))
    add("Basse sous-marine", BASS, mix(pairs(fb=0, car=((0, 0), (0, -3), (0, 3)), mods=((0, 35), (1, 20), (1, 20)), ce=E_SUS, me=E_SUS, mlev=0.7), BAS, lfowave=4, lfospeed=20, lfoamd=15))
    add("Basse électrique FM", BASS, mix(pairs(fb=3, car=((0, 0), (1, 0), (0, 0)), mods=((1, 70), (1, 45), (5, 30)), ce=E_KEY, me=E_PLK), BAS))
    add("Basse bois FM", BASS, mix(pairs(fb=0, car=((0, 0), (0, 0), (1, 0)), mods=((3, 60), (2, 50), (1, 30)), ce=E_PLK, me=E_PLK_FAST), BAS))
    add("Basse métal", BASS, mix(stack(fb=7, c1=(0, 0), m2=(3, 70), c3=(0, 2), m4=(5, 60), m5=(1, 55), m6=(7, 40), ce=E_SUS, me=E_PLK), BAS))
    add("Basse synthé 84", BASS, mix(stack(fb=7, c1=(0, 0), m2=(0, 80), c3=(1, 0), m4=(0, 75), m5=(1, 45), m6=(1, 30), ce=E_SUS, me=E_PLK), BAS))
    add("Basse fretless FM", BASS, mix(pairs(fb=1, car=((0, 0), (1, 1), (0, 0)), mods=((1, 55), (2, 35), (1, 30)), ce=E_SUS, me=E_SUS, mlev=0.85), vmode=2, voices=1, glide=80))
    add("Basse saturée FM", BASS, mix(stack(fb=7, c1=(0, 0), m2=(1, 88), c3=(0, 4), m4=(1, 82), m5=(2, 60), m6=(1, 50), ce=E_SUS, me=E_SUS), BAS, character=1))

    # --- Leads (10)
    LD = dict(vmode=2, voices=1, glide=40, lfowave=0, lfospeed=34, lfodelay=40, lfopmd=12, pms=3)
    add("Lead cuivré FM", LEAD, mix(stack(fb=6, m2=(1, 80), m4=(1, 70), m5=(1, 50), m6=(1, 40), ce=E_SUS, me=E_SUS), LD))
    add("Lead flûte FM", LEAD, mix(pairs(fb=0, car=((1, 0), (2, 0), (1, 3)), mods=((1, 45), (1, 25), (3, 20)), ce=E_SUS, me=E_SUS), LD))
    add("Lead carré FM", LEAD, mix(pairs(fb=0, car=((1, 0), (1, 2), (1, -2)), mods=((2, 75), (2, 70), (2, 60)), ce=E_SUS, me=E_SUS), LD))
    add("Lead synchro FM", LEAD, mix(stack(fb=7, m2=(3, 70), m4=(5, 65), m5=(2, 55), m6=(1, 50), ce=E_SUS, me=E_BRASS), LD))
    add("Lead hurlant FM", LEAD, mix(stack(fb=7, m2=(1, 90), m4=(1, 85), m5=(1, 75), m6=(1, 70), ce=E_SUS, me=E_SUS), LD, character=1))
    add("Lead harmonica", LEAD, mix(pairs(fb=3, car=((1, 0), (2, 0), (3, 0)), mods=((1, 65), (2, 45), (1, 30)), ce=E_BRASS, me=E_BRASS), LD, lfopmd=18))
    add("Lead verre", LEAD, mix(pairs(fb=0, car=((1, 0), (4, 0), (2, 3)), mods=((3, 40), (1, 30), (5, 25)), ce=E_SUS, me=E_KEY), LD))
    add("Lead sifflet", LEAD, mix(pairs(fb=0, car=((2, 0), (4, 0), (2, 2)), mods=((1, 20), (1, 10), (1, 10)), ce=E_SUS, me=E_SUS), LD, lfopmd=20))
    add("Lead pluck FM", LEAD, mix(pairs(fb=4, car=((1, 0), (1, 3), (2, 0)), mods=((1, 75), (3, 60), (1, 40)), ce=E_SUS, me=E_PLK), LD))
    add("Lead octave FM", LEAD, mix(pairs(fb=2, car=((1, 0), (2, 0), (0, 0)), mods=((1, 60), (1, 50), (1, 40)), ce=E_SUS, me=E_SUS), LD))

    # --- Nappes (10)
    NP = dict(ensemble=55, lfowave=4, lfospeed=28, lfopmd=5, pms=2)
    add("Nappe FM douce", PAD, mix(pairs(car=((1, -4), (1, 0), (1, 4)), mods=((1, 45), (1, 40), (2, 30)), ce=E_PAD, me=E_PAD_SLOW, mlev=0.85), NP))
    add("Nappe cristalline", PAD, mix(pairs(car=((1, -3), (2, 0), (1, 3)), mods=((3, 40), (5, 30), (7, 25)), ce=E_PAD, me=E_PAD), NP))
    add("Nappe de cordes FM", PAD, mix(pairs(fb=4, car=((1, -5), (1, 0), (1, 5)), mods=((1, 55), (1, 50), (1, 50)), ce=E_PAD, me=E_PAD), NP))
    add("Nappe du soir", PAD, mix(pairs(car=((0, 0), (1, -4), (1, 4)), mods=((1, 30), (2, 25), (1, 35)), ce=E_PAD_SLOW, me=E_PAD_SLOW), NP, ensemble=70))
    add("Nappe en verre soufflé", PAD, mix(pairs(car=((1, 0), (3, 0), (1, 5)), mods=((1, 38), (1, 28), (4, 22)), ce=E_PAD, me=E_PAD), NP, lfoamd=15))
    add("Nappe qui monte", PAD, mix(pairs(fb=2, car=((1, -2), (1, 2), (2, 0)), mods=((1, 70), (1, 60), (1, 50)), ce=E_PAD, me=((20, 22, 20, 30), (99, 99, 95, 0))), NP))
    add("Nappe chorale FM", PAD, mix(pairs(car=((1, -4), (2, 0), (1, 4)), mods=((1, 55), (1, 35), (2, 45)), ce=E_PAD, me=E_PAD), NP, ensemble=75))
    add("Nappe sombre FM", PAD, mix(organ(ratios=(0, 1, 1, 2, 3, 1), levels=(92, 88, 88, 60, 40, 55), detunes=(0, -5, 5, 0, 0, 3), ce=E_PAD_SLOW), NP))
    add("Nappe numérique 2", PAD, mix(pairs(fb=3, car=((1, 0), (1, 6), (2, -6)), mods=((2, 50), (3, 40), (1, 45)), ce=E_PAD, me=E_PAD), NP))
    add("Nappe métallique", PAD, mix(pairs(car=((1, -3), (1, 3), (3, 0)), mods=((7, 35), (9, 30), (5, 30)), ce=E_PAD, me=E_PAD), NP))

    # --- Plucks (10)
    add("Pluck FM", PLUCK, pairs(fb=3, car=((1, 0), (1, 3), (2, 0)), mods=((1, 75), (3, 55), (1, 45)), ce=E_PLK, me=E_PLK_FAST))
    add("Pizz numérique", PLUCK, pairs(car=((1, 0), (2, 0), (1, 2)), mods=((1, 60), (1, 40), (3, 35)), ce=((99, 45, 32, 60), (99, 75, 0, 0)), me=E_PLK_FAST))
    add("Koto FM", PLUCK, pairs(fb=5, car=((1, 0), (3, 0), (1, 3)), mods=((3, 70), (1, 50), (5, 45)), ce=E_PLK, me=E_PLK_FAST))
    add("Harpe FM", PLUCK, pairs(car=((1, 0), (2, 0), (1, -2)), mods=((1, 55), (2, 40), (4, 30)), ce=((99, 30, 22, 50), (99, 70, 0, 0)), me=E_PLK))
    add("Pluck de verre", PLUCK, pairs(car=((1, 0), (4, 0), (2, 3)), mods=((5, 50), (3, 40), (7, 35)), ce=E_PLK, me=E_PLK_FAST))
    add("Steel drum", PLUCK, pairs(fb=2, car=((1, 0), (3, 0), (1, 4)), mods=((2, 60), (1, 40), (3, 45)), ce=E_PLK, me=E_PLK))
    add("Pluck creux", PLUCK, pairs(car=((1, 0), (1, 5), (3, 0)), mods=((2, 70), (2, 60), (1, 30)), ce=((99, 45, 32, 60), (99, 75, 0, 0)), me=E_PLK_FAST))
    add("Balafon", PLUCK, pairs(car=((1, 0), (4, 0), (1, -3)), mods=((3, 55), (1, 35), (7, 40)), ce=((99, 45, 32, 60), (99, 75, 0, 0)), me=E_PLK_FAST))
    add("Pluck mat", PLUCK, pairs(car=((1, 0), (1, 0), (2, 0)), mods=((1, 45), (1, 35), (1, 25)), ce=E_PLK, me=E_PLK_FAST))
    add("Guitare FM", PLUCK, stack(fb=4, m2=(3, 70), m4=(1, 60), m5=(2, 45), m6=(5, 35), ce=((99, 34, 24, 55), (99, 75, 0, 0)), me=E_PLK))

    # --- Claviers (10)
    add("Piano FM brillant", KEYS, pairs(fb=2, car=((1, 0), (1, 2), (2, 0)), mods=((1, 72), (14, 55), (1, 50)), ce=E_KEY, me=E_PLK))
    add("Piano électrique doux", KEYS, pairs(fb=1, car=((1, 0), (1, 3), (1, -3)), mods=((1, 62), (14, 45), (1, 50)), ce=E_KEY, me=E_PLK, mlev=0.85, ensemble=25))
    add("Piano électrique dur", KEYS, pairs(fb=4, car=((1, 0), (1, 3), (1, -3)), mods=((1, 80), (14, 65), (1, 65)), ce=E_KEY, me=E_PLK_FAST))
    add("Clavinette FM", KEYS, pairs(fb=5, car=((1, 0), (3, 0), (1, 2)), mods=((2, 70), (1, 55), (5, 45)), ce=((99, 42, 32, 70), (99, 80, 30, 0)), me=E_PLK_FAST))
    add("Orgue rond FM", KEYS, organ(ratios=(0, 1, 2, 3, 4, 8), levels=(90, 99, 75, 50, 40, 25), ce=E_ORG))
    add("Orgue percussif", KEYS, organ(ratios=(0, 1, 2, 3, 4, 6), levels=(85, 99, 80, 60, 50, 35), ce=E_ORG, o5_r2=60, o5_l2=0, o5_l3=0))
    add("Célesta FM", KEYS, pairs(car=((2, 0), (4, 0), (2, 3)), mods=((7, 45), (3, 35), (9, 30)), ce=E_BELL, me=E_BELL))
    add("Vibraphone FM", KEYS, pairs(car=((1, 0), (4, 0), (1, 2)), mods=((4, 40), (1, 30), (10, 25)), ce=E_BELL, me=E_PLK, lfowave=4, lfospeed=45, lfoamd=25, o1_ams=2, o3_ams=2))
    add("Clavecin FM", KEYS, stack(fb=5, m2=(3, 72), m4=(5, 60), m5=(1, 50), m6=(7, 40), ce=((99, 40, 30, 65), (99, 70, 0, 0)), me=E_PLK_FAST))
    add("Accordéon FM", KEYS, pairs(fb=3, car=((1, 0), (1, 6), (2, -6)), mods=((1, 60), (1, 55), (3, 40)), ce=E_SUS, me=E_SUS, lfowave=4, lfospeed=40, lfoamd=12))

    # --- Cordes (10)
    ST = dict(ensemble=60, lfowave=4, lfospeed=38, lfodelay=35, lfopmd=6, pms=3)
    add("Cordes FM", STR, mix(pairs(fb=4, car=((1, -4), (1, 0), (1, 4)), mods=((1, 55), (1, 50), (1, 50)), ce=E_STR, me=E_STR), ST))
    add("Cordes lentes FM", STR, mix(pairs(fb=4, car=((1, -4), (1, 0), (1, 4)), mods=((1, 50), (1, 45), (1, 45)), ce=E_PAD_SLOW, me=E_PAD_SLOW), ST))
    add("Violons FM", STR, mix(pairs(fb=5, car=((1, -3), (2, 0), (1, 3)), mods=((1, 60), (1, 50), (1, 55)), ce=E_STR, me=E_STR), ST))
    add("Violoncelles FM", STR, mix(pairs(fb=5, car=((0, 0), (1, -3), (1, 3)), mods=((1, 55), (1, 50), (1, 50)), ce=E_STR, me=E_STR), ST))
    add("Cordes pizzicato FM", STR, pairs(fb=2, car=((1, 0), (1, 3), (2, 0)), mods=((1, 55), (1, 45), (1, 35)), ce=((99, 45, 32, 60), (99, 75, 0, 0)), me=E_PLK_FAST))
    add("Ensemble à cordes", STR, mix(organ(ratios=(1, 1, 1, 2, 2, 0), levels=(90, 88, 88, 60, 60, 50), detunes=(0, -6, 6, -4, 4, 0), ce=E_STR), ST))
    add("Cordes brillantes", STR, mix(pairs(fb=6, car=((1, -4), (1, 0), (1, 4)), mods=((1, 70), (1, 65), (1, 60)), ce=E_STR, me=E_STR), ST))
    add("Cordes sombres FM", STR, mix(pairs(fb=2, car=((1, -4), (0, 0), (1, 4)), mods=((1, 40), (1, 35), (1, 35)), ce=E_STR, me=E_STR), ST))
    add("Trémolo de cordes", STR, mix(pairs(fb=4, car=((1, -4), (1, 0), (1, 4)), mods=((1, 55), (1, 50), (1, 50)), ce=E_STR, me=E_STR), ST, lfowave=0, lfospeed=70, lfoamd=40, o1_ams=2, o3_ams=2, o5_ams=2))
    add("Cordes en octaves", STR, mix(pairs(fb=4, car=((1, -4), (2, 0), (0, 4)), mods=((1, 55), (1, 45), (1, 45)), ce=E_STR, me=E_STR), ST))

    # --- Cuivres (10)
    BR = dict(lfowave=4, lfospeed=36, lfodelay=50, lfopmd=6, pms=3)
    add("Cuivres FM ronds", BRASS, mix(stack(fb=6, m2=(1, 75), m4=(1, 68), m5=(1, 50), m6=(1, 40), ce=E_BRASS, me=E_BRASS), BR))
    add("Section de cuivres", BRASS, mix(pairs(fb=6, car=((1, -3), (1, 0), (1, 3)), mods=((1, 72), (1, 70), (1, 68)), ce=E_BRASS, me=E_BRASS), BR, ensemble=30))
    add("Trompette FM", BRASS, mix(stack(fb=7, m2=(1, 78), m4=(1, 70), m5=(2, 50), m6=(1, 45), ce=E_BRASS, me=((60, 40, 35, 66), (99, 85, 80, 0))), BR, vmode=2, voices=1))
    add("Trombone FM", BRASS, mix(stack(fb=6, c1=(0, 0), m2=(0, 74), c3=(0, 0), m4=(1, 65), m5=(1, 50), m6=(1, 40), ce=E_BRASS, me=E_BRASS), BR))
    add("Cor FM", BRASS, mix(pairs(fb=3, car=((1, 0), (1, 2), (0, 0)), mods=((1, 55), (1, 50), (1, 45)), ce=((50, 40, 35, 60), (99, 92, 88, 0)), me=E_BRASS), BR))
    add("Cuivres staccato", BRASS, mix(pairs(fb=6, car=((1, -3), (1, 0), (1, 3)), mods=((1, 75), (1, 70), (1, 65)), ce=((80, 55, 40, 70), (99, 60, 0, 0)), me=E_PLK), BR))
    add("Cuivres synthé 84", BRASS, mix(stack(fb=7, m2=(1, 82), m4=(1, 78), m5=(1, 60), m6=(1, 55), ce=E_BRASS, me=E_BRASS), BR, character=1, ensemble=35))
    add("Saxophone FM", BRASS, mix(stack(fb=5, m2=(1, 70), m4=(3, 55), m5=(1, 45), m6=(2, 40), ce=E_BRASS, me=E_BRASS), BR, vmode=2, voices=1, glide=30))
    add("Cuivres doux FM", BRASS, mix(pairs(fb=3, car=((1, -3), (1, 0), (1, 3)), mods=((1, 55), (1, 50), (1, 45)), ce=E_BRASS, me=E_BRASS), BR))
    add("Fanfare FM", BRASS, mix(pairs(fb=7, car=((1, -4), (2, 0), (1, 4)), mods=((1, 80), (1, 70), (1, 75)), ce=E_BRASS, me=E_BRASS), BR))

    # --- Arpèges (10)
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège FM cristal", ARP, mix(pairs(car=((2, 0), (4, 0), (2, 3)), mods=((5, 45), (3, 35), (7, 30)), ce=E_PLK_FAST, me=E_PLK_FAST), AR))
    add("Arpège FM rond", ARP, mix(pairs(fb=2, car=((1, 0), (1, 3), (2, 0)), mods=((1, 60), (1, 45), (1, 35)), ce=E_PLK, me=E_PLK_FAST), AR, arp_mode=3))
    add("Arpège métallique", ARP, mix(pairs(car=((1, 0), (3, 0), (1, 4)), mods=((7, 50), (5, 40), (9, 35)), ce=E_PLK_FAST, me=E_PLK_FAST), AR, arp_mode=2))
    add("Arpège de cloches", ARP, mix(pairs(car=((2, 0), (5, 0), (2, 2)), mods=((7, 40), (3, 30), (11, 30)), ce=E_BELL, me=E_BELL), AR, arp_rate=2))
    add("Arpège claquant", ARP, mix(pairs(fb=5, car=((1, 0), (2, 0), (1, 3)), mods=((3, 70), (1, 55), (5, 45)), ce=E_PLK_FAST, me=E_PLK_FAST), AR, arp_rate=4))
    add("Arpège basse FM", ARP, mix(pairs(fb=4, car=((0, 0), (1, 0), (0, 3)), mods=((1, 70), (1, 50), (1, 40)), ce=E_PLK, me=E_PLK_FAST), AR, arp_oct=1, arp_mode=5))
    add("Arpège aléatoire FM", ARP, mix(pairs(car=((1, 0), (2, 0), (3, 0)), mods=((2, 55), (3, 45), (1, 35)), ce=E_PLK, me=E_PLK_FAST), AR, arp_mode=6))
    add("Arpège en verre", ARP, mix(pairs(car=((2, 0), (2, 4), (4, 0)), mods=((3, 40), (5, 30), (1, 25)), ce=E_PLK, me=E_PLK), AR, ensemble=40))
    add("Arpège court FM", ARP, mix(pairs(fb=3, car=((1, 0), (1, 2), (2, 0)), mods=((1, 65), (2, 50), (1, 40)), ce=E_PLK_FAST, me=E_PLK_FAST), AR, arp_gate=25, arp_rate=4))
    add("Arpège octaves FM", ARP, mix(pairs(car=((1, 0), (2, 0), (4, 0)), mods=((1, 50), (1, 40), (1, 30)), ce=E_PLK, me=E_PLK_FAST), AR, arp_oct=3))

    # --- Effets (10)
    add("Gong lointain", FX, pairs(car=((1, 0), (2, 0), (3, 0)), mods=((5, 55), (7, 45), (11, 40)), ce=((99, 18, 12, 20), (99, 60, 0, 0)), me=E_BELL, ensemble=40))
    add("Tôle froissée", FX, stack(fb=7, m2=(9, 85), m4=(13, 80), m5=(7, 70), m6=(17, 60), ce=((99, 30, 20, 30), (99, 70, 0, 0)), me=E_PLK))
    add("Sirène FM", FX, pairs(car=((1, 0), (1, 0), (1, 0)), mods=((1, 60), (2, 40), (1, 30)), ce=E_SUS, me=E_SUS, lfowave=0, lfospeed=25, lfopmd=70, pms=7))
    add("Laser FM", FX, pairs(fb=6, car=((2, 0), (1, 0), (3, 0)), mods=((3, 80), (1, 60), (5, 50)), ce=E_PLK, me=E_PLK, pr1=99, pr2=40, pl1=99, pl2=30, pl3=50, pl4=50))
    add("Goutte d'eau", FX, pairs(car=((2, 0), (3, 0), (4, 0)), mods=((1, 65), (2, 45), (1, 35)), ce=E_PLK, me=E_PLK_FAST, pr1=99, pr2=70, pl1=70, pl2=50, pl3=50, pl4=50))
    add("Vent métallique", FX, pairs(fb=7, car=((1, 0), (2, 3), (3, -3)), mods=((1, 99), (9, 80), (13, 70)), ce=E_PAD, me=E_PAD, lfowave=5, lfospeed=50, lfoamd=40, o1_ams=3, o3_ams=3, o5_ams=3))
    add("Robot FM", FX, pairs(fb=5, car=((1, 0), (1, 0), (2, 0)), mods=((1, 70), (3, 60), (1, 50)), ce=E_SUS, me=E_SUS, lfowave=3, lfospeed=55, lfoamd=50, o1_ams=3))
    add("Cloche d'église", FX, pairs(car=((1, 0), (2, 0), (1, 3)), mods=((3, 60), (7, 45), (5, 45)), ce=((99, 20, 14, 22), (99, 55, 0, 0)), me=E_BELL))
    add("Impact numérique", FX, stack(fb=7, c1=(0, 0), m2=(0, 90), m4=(1, 85), m5=(3, 70), m6=(5, 60), ce=((99, 40, 30, 50), (99, 40, 0, 0)), me=E_PLK_FAST, pr1=99, pr2=50, pl1=90, pl2=50, pl3=50, pl4=50))
    add("Ondes inquiètes", FX, pairs(car=((1, 0), (1, 7), (1, -7)), mods=((1, 40), (1, 40), (1, 40)), ce=E_PAD_SLOW, me=E_PAD_SLOW, lfowave=5, lfospeed=30, lfopmd=25, pms=5, ensemble=60))
    return b
