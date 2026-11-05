# Ondes Bretzel (v0.47 (maj. 5 nov.), poly à tables d'ondes et deux filtres, 4 enveloppes, 3 LFO) : 76 sons de plus.
# Formes 1 et 2 : 0 impulsion, 1 scie, 2 triangle, 3 sinus, 4 résonances, 5 voyelles, 6 orgues, 7 impulsions, 8 plis, 9 synchros,
# 10 numérique, 11 cloches, 12 doux → vif, 13 bosses, 14 chœurs, 15 escaliers (tables : position o1pos).
# Octaves : 0 64', 1 32', 2 16', 3 8', 4 4', 5 2'. Filtres : 0 arrêt, 1 PB24, 2 PB12, 3 bande 24, 4 bande 12, 5 PH24, 6 PH12,
# 7 réjection 24, 8 réjection 12, 9 peigne +, 10 peigne −. Enveloppes : e1 filtre, e2 ampli, e3 et e4 libres.
# Sources de la matrice : 1 env. filtre, 2 env. ampli, 3 env. 3, 4 env. 4, 6 LFO 1, 7 LFO 2, 8 LFO 3, 11 vélocité, 13 molette.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

BASE = dict(o1shape=1, o1oct=3, o1level=80, o2shape=1, o2oct=3, o2detune=8, o2level=60, f1type=1, f1cut=2500, f1reso=15, f1env=35,
            e1a=2, e1d=600, e1s=30, e1r=500, e2a=2, e2d=900, e2s=80, e2r=600)
BASSE = mix(BASE, vmode=1, voices=1, o1oct=2, o2oct=2, o2detune=5, o3shape=1, o3oct=1, o3level=40, f1cut=500, f1reso=20, f1env=50,
            e1d=280, e1s=10, e1r=150, e2d=600, e2s=85, e2r=150)
LEAD_ = mix(BASE, vmode=1, voices=1, glide=40, f1cut=2800, f1reso=25, f1env=30, e1d=600, e1s=60, e2s=100, e2r=250, vibrato=10, l1rate=5.5)
NAPPE = mix(BASE, o1shape=12, o1pos=40, o2shape=14, o2pos=30, f1type=2, f1cut=1800, f1reso=10, f1env=20, e1a=1200, e1d=2500, e1s=60, e1r=2500,
            e2a=900, e2d=2000, e2s=90, e2r=2500, spread=70, l2rate=0.15, mx1_src=7, mx1_amt=30, **{"mx1_dst@o1pos": 0})
PLK = mix(BASE, f1cut=800, f1reso=30, f1env=60, e1a=0.5, e1d=230, e1s=0, e1r=250, e2a=0.5, e2d=500, e2s=0, e2r=380)
CLAV = mix(BASE, f1cut=2600, f1reso=10, f1env=30, e1a=1, e1d=900, e1s=20, e1r=400, e2a=1, e2d=1500, e2s=35, e2r=450, velamp=50)


def build():
    b = Bank("blo")
    add = b.add

    for nm, kw in [
        ("Basse table résonances", dict(o1shape=4, o1pos=30)), ("Basse scie et sous", {}), ("Basse impulsion", dict(o1shape=0, o1pwm=30, o2shape=0)),
        ("Basse orgue", dict(o1shape=6, o1pos=20, f1env=20)), ("Basse plis", dict(o1shape=8, o1pos=35, mx1_src=1, mx1_amt=40, **{"mx1_dst@o1pos": 0})),
        ("Basse synchro sur 3", dict(o2sync=1, o2shape=1, o2semi=7, o1level=0, o2level=80, mx1_src=1, mx1_amt=35, **{"mx1_dst@o2semi": 0})),
        ("Basse numérique", dict(o1shape=10, o1pos=50)), ("Basse peigne", dict(f2type=9, f2cut=110, f2reso=60, routing=0)),
        ("Basse lampe", dict(f1drive=60, f1curve=1)), ("Basse repliée", dict(f1drive=70, f1curve=3, f1cut=800)), ("Basse FM sinus", dict(o1shape=3, o1fm=40, o1fmsrc=0, o3shape=3)),
        ("Basse anneau", dict(ring=40, o2semi=7)), ("Basse bosses", dict(o1shape=13, o1pos=60)), ("Basse escaliers", dict(o1shape=15, o1pos=40)),
        ("Basse glissée", dict(glide=100)), ("Basse unisson", dict(vmode=2, univoices=4, unidetune=15)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead table vive", dict(o1shape=12, o1pos=80)), ("Lead voyelles", dict(o1shape=5, o1pos=30, mx1_src=6, mx1_amt=40, l1rate=0.8, **{"mx1_dst@o1pos": 0})),
        ("Lead synchro sur 3", dict(o2sync=1, o2semi=12, o1level=0, o2level=80)), ("Lead impulsions", dict(o1shape=7, o1pos=50)), ("Lead numérique", dict(o1shape=10, o1pos=70)),
        ("Lead plis", dict(o1shape=8, o1pos=50)), ("Lead FM", dict(o1shape=3, o1fm=35, o3shape=3, o3oct=4)), ("Lead bande", dict(f1type=3, f1cut=1500, f1reso=40)),
        ("Lead unisson large", dict(vmode=2, univoices=6, unidetune=20)), ("Lead escaliers", dict(o1shape=15, o1pos=70)), ("Lead doux", dict(o1shape=2, o2shape=3, f1cut=2000, f1reso=5)),
        ("Lead chœur", dict(o1shape=14, o1pos=50)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe en tables", {}), ("Nappe voyelles", dict(o1shape=5, o2shape=5, o2pos=70)), ("Nappe orgues", dict(o1shape=6, o2shape=6, o2pos=60)),
        ("Nappe cloches", dict(o1shape=11, o2shape=12)), ("Nappe chœurs", dict(o1shape=14, o2shape=14, o2pos=80)), ("Nappe plis", dict(o1shape=8, o2shape=4)),
        ("Nappe numérique", dict(o1shape=10, o2shape=15)), ("Nappe résonances", dict(o1shape=4, o2shape=4, o2pos=60)), ("Nappe peigne", dict(f2type=9, f2cut=300, f2reso=50, routing=1)),
        ("Nappe deux filtres", dict(f2type=3, f2cut=1200, f2reso=30, routing=1)), ("Nappe en boucle", dict(e3mode=4, e3a=800, e3d=1200, mx2_src=3, mx2_amt=30, **{"mx2_dst@f1cut": 0})),
        ("Nappe sombre", dict(f1cut=700)), ("Nappe bosses", dict(o1shape=13, o2shape=12)), ("Nappe escaliers lents", dict(o1shape=15, o2shape=14, l2rate=0.08)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck table", dict(o1shape=12, o1pos=60)), ("Pluck cloche", dict(o1shape=11, o1pos=40, f1cut=3000, e2d=1200, e2r=900)), ("Pluck orgue", dict(o1shape=6, o1pos=50)),
        ("Pluck peigne", dict(f1type=9, f1cut=260, f1reso=70, f1key=100)), ("Pluck synchro", dict(o2sync=1, o2semi=12, o1level=0, o2level=80)), ("Pluck bosses", dict(o1shape=13, o1pos=80)),
        ("Pluck numérique sec", dict(o1shape=10, o1pos=30)), ("Pluck résonant", dict(f1reso=65)), ("Pluck voyelle", dict(o1shape=5, o1pos=60)),
        ("Pluck chœur", dict(o1shape=14, o1pos=30)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Orgue de tables", dict(o1shape=6, o1pos=70, o2shape=6, o2pos=30, f1env=0, e2s=100, e2d=100, e2r=150)), ("Piano de tables", dict(o1shape=12, o1pos=30)),
        ("Cloches de verre", dict(o1shape=11, o1pos=70, o2shape=11, o2oct=4, e2s=0, e2d=2200, e2r=2000)), ("Clavier numérique", dict(o1shape=10, o1pos=40)),
        ("Clavier doux", dict(o1shape=2, o2shape=3)), ("Accords chœur", dict(o1shape=14, o1pos=40, e2d=700, e2s=20)),
    ]:
        add(nm, KEYS, mix(CLAV, **kw))
    for nm, kw in [("Cordes de tables", dict(o1shape=4, o1pos=50, o2shape=1, e2a=400, e2r=1300, f1cut=3500)),
                   ("Cordes chœur", dict(o1shape=14, o2shape=1, e2a=500, e2r=1500)), ("Cordes pincées", dict(o1shape=1, e2a=1, e2s=0, e2d=600, e2r=500, f1env=40))]:
        add(nm, STR, mix(NAPPE, **kw))
    for nm, kw in [("Cuivres de tables", dict(o1shape=12, o1pos=50, f1cut=900, f1env=55, e1a=60, e2a=30)), ("Cuivres scies", dict(f1cut=1000, f1env=55, e1a=50, e2a=25)),
                   ("Cuivres voyelles", dict(o1shape=5, o1pos=60, f1cut=1100, f1env=50, e1a=60, e2a=30))]:
        add(nm, BRASS, mix(BASE, **kw))
    for nm, kw in [("Arpège de tables", dict(o1shape=12, o1pos=50, arp_mode=1, arp_oct=2)), ("Arpège cloches", dict(o1shape=11, arp_mode=3, arp_oct=2)),
                   ("Arpège numérique", dict(o1shape=10, arp_mode=6, arp_oct=2, arp_rate=4)), ("Arpège voyelles", dict(o1shape=5, arp_mode=1, arp_oct=3))]:
        add(nm, ARP, mix(PLK, **kw))
    for nm, kw in [
        ("Balayage de synchros", dict(o1shape=9, mx1_src=6, l1rate=0.2, mx1_amt=90)), ("Bruit de vent", dict(o1level=0, o2level=0, noise=100, f1type=3, f1cut=900, f1reso=50)),
        ("Escaliers fous", dict(o1shape=15, mx1_src=6, l1shape=4, l1rate=6, mx1_amt=80)), ("Cloches qui tombent", dict(o1shape=11, o1fm=40, e2s=0, e2d=3000, e2r=3000)),
        ("Peigne qui tourne", dict(f2type=10, f2cut=200, f2reso=70, routing=0, mx2_src=7, mx2_amt=40, **{"mx2_dst@f2cut": 0})),
        ("Machine", dict(o1shape=7, ring=70, o2semi=-11, mx1_src=6, l1rate=7, mx1_amt=50)),
    ]:
        add(nm, FX, mix(NAPPE, **kw))
    return b
