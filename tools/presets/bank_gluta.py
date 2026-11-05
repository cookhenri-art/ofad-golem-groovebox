# Glutamake (v0.47 (maj. 5 nov.), tables d'ondes à morphs, bille sur une surface, séquence de mouvement) : 78 sons de plus.
# Tables : 0 analogique, 1 voyelles, 2 cloches FM, 3 plis, 4 harmoniques paires, 5 granuleux, 6 cordes frottées, 7 numérique 8 bits.
# Morphs : 1 étirer, 2 comprimer, 3 refléter, 4 miroir, 5 synchro, 6 plier, 7 pulsation, 8 quantifier, 9 bits, 10 FM, 11 AM, 12 anneau.
# Filtres : 0 multi, 1 PB 4 pôles, 2 PB 2 pôles, 3 PH 4, 4 PH 2, 5 bande, 6 réjection, 7 agressif PB, 8 agressif PH, 9 doux PB.
# Sources de la matrice : 6 LFO 1, 7 LFO 2, 18 mod 1, 26 bille X, 27 Y, 28 distance, 29 angle, 34 choc, 35..38 séquences A à D.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

X, Y, DIST, ANG, HIT, SA, SB, SC, SD, L1, L2 = 26, 27, 28, 29, 34, 35, 36, 37, 38, 6, 7


def route(n, src, dst, amt):
    return {f"mx{n}_src": src, f"mx{n}_dst@{dst}": 0, f"mx{n}_amt": amt}


BASSE = mix(dict(vmode=1, o1oct=-1, sub=45, cutoff=600, reso=25, fenv=50, fd=260, fs=10, ad=500, as_=75, ar=120, lsrc=2, lforce=40),
            route(1, DIST, "o1pos", 35))
LEAD_ = mix(dict(vmode=1, glide=40, cutoff=3000, reso=20, fenv=30, fd=600, fs=50, ad=800, as_=90, ar=250), route(1, Y, "o1morph", 30), route(2, L2, "tune", 1))
NAPPE = mix(dict(aa=700, ad=2000, as_=85, ar=2200, cutoff=2600, reso=12, fenv=15, fric=10), route(1, X, "o1pos", 40), route(2, Y, "o1ab", 40))
PLK = mix(dict(cutoff=900, reso=30, fenv=60, fa=1, fd=200, fs=0, aa=1, ad=400, as_=0, ar=300, lsrc=1), route(1, X, "o1pos", 40))
CLAV = mix(dict(cutoff=2600, reso=10, fenv=25, fd=900, fs=25, aa=1, ad=1400, as_=30, ar=450), route(1, DIST, "o1pos", 25))


def build():
    b = Bank("gluta")
    add = b.add

    for nm, kw in [
        ("Basse analogique roulante", dict(o1taba=0, o1pos=50)), ("Basse voyelle", dict(o1taba=1, o1pos=40, ftype=0, fmulti=20)),
        ("Basse cloche grave", dict(o1taba=2, o1pos=15, cutoff=900)), ("Basse harmoniques", dict(o1taba=4, o1pos=50, o1lvl=100, o2lvl=60, o2taba=0, o2oct=-1, o2semi=0, o2fine=4)), ("Basse granuleuse", dict(o1taba=5, o1pos=30, o1lvl=100, o2lvl=60, o2taba=0, o2oct=-1, o2semi=0, o2fine=4)),
        ("Basse cordes graves", dict(o1taba=6, o1pos=30)), ("Basse 8 bits", dict(o1taba=7, o1pos=50, o1modif=4)), ("Basse synchro roulante", dict(o1mtype=5, o1morph=35)),
        ("Basse pliée", dict(o1mtype=6, o1morph=40)), ("Basse FM au choc", mix(dict(o1mtype=10, o1morph=20, o2lvl=40, o2oct=-1, o2semi=0, o2fine=0), route(2, HIT, "o1morph", 50))),
        ("Basse agressive", dict(ftype=7, reso=55, fdrive=10)), ("Basse douce", dict(ftype=9, cutoff=400, reso=10)), ("Basse à la séquence", route(1, SA, "cutoff", 30)),
        ("Basse qui rebondit", dict(edget=60, edgeb=60, edgel=60, edger=60, fric=5, tilty=-40)), ("Basse en creux", dict(bumph=-100, bumpw=30)),
        ("Basse unisson", dict(vmode=2, univoices=5, unidetune=15)), ("Basse saturée", dict(o1modif=1, fdrive=14)), ("Basse glissée", dict(glide=110)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead voyelles qui roulent", dict(o1taba=1, o1pos=50, ftype=0, fmulti=30)), ("Lead cloches", dict(o1taba=2, o1pos=40)), ("Lead plis", dict(o1taba=3, o1pos=40)),
        ("Lead granuleux", dict(o1taba=5, o1pos=50)), ("Lead 8 bits", dict(o1taba=7, o1modif=4)), ("Lead étiré", dict(o1mtype=1, o1morph=40)),
        ("Lead pulsation", dict(o1mtype=7, o1morph=40)), ("Lead anneau", dict(o1mtype=12, o1morph=50, o2lvl=30, o2semi=7)), ("Lead au flipper", mix(dict(edget=70, edgeb=70, edgel=70, edger=70, fric=3), route(1, HIT, "cutoff", 40))),
        ("Lead unisson", dict(vmode=2, univoices=6, unidetune=18)), ("Lead à pas", route(3, SB, "o1pos", 40)), ("Lead passe-bande", dict(ftype=5, cutoff=1800, reso=40)),
        ("Lead harmoniques paires", dict(o1taba=4, o1pos=70, o1lvl=100, o2lvl=50, o2taba=0, o2semi=0)), ("Lead cordes", dict(o1taba=6, o1pos=50)), ("Lead quantifié", dict(o1mtype=8, o1morph=50)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe voyelles", dict(o1taba=1, o1tabb=1, o1pos=30)), ("Nappe cloches", dict(o1taba=2, o1tabb=4)), ("Nappe plis lente", dict(o1taba=3, o1tabb=6)),
        ("Nappe harmoniques", dict(o1taba=4, o1tabb=0)), ("Nappe granuleuse", dict(o1taba=5, o1tabb=1)), ("Nappe cordes frottées", dict(o1taba=6, o1tabb=6, o2lvl=50, o2taba=6, o2fine=8)),
        ("Nappe 8 bits", dict(o1taba=7, o1tabb=0)), ("Nappe planète", dict(fric=0, bumph=-100, bumpw=70)), ("Nappe anneau de bosse", dict(bumpshape=3, bumph=-80, bumpw=55)),
        ("Nappe qui traverse", dict(wrap=1, tiltx=20, fric=2)), ("Nappe à quatre couloirs", mix(route(3, SC, "cutoff", 20), route(4, SD, "spread", 30))),
        ("Nappe multi", dict(ftype=0, fmulti=40)), ("Nappe miroir", dict(o1mtype=4, o1morph=50)), ("Nappe AM", dict(o1mtype=11, o1morph=40, o2lvl=30, o2semi=12)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck analogique", dict(o1taba=0, o1pos=50)), ("Pluck voyelle", dict(o1taba=1, o1pos=60)), ("Pluck cloche", dict(o1taba=2, o1pos=50, ad=900, ar=800)),
        ("Pluck plié", dict(o1taba=3)), ("Pluck 8 bits", dict(o1taba=7)), ("Pluck synchro", dict(o1mtype=5, o1morph=50)), ("Pluck au choc", route(2, HIT, "o1pos", 60)),
        ("Pluck résonant", dict(reso=65)), ("Pluck harmoniques", dict(o1taba=4, o1pos=60)), ("Pluck granuleux", dict(o1taba=5, o1pos=50)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Piano de tables", dict(o1taba=4, o1pos=30)), ("Cloches FM", dict(o1taba=2, o1pos=70, as_=0, ad=2000, ar=1800)), ("Orgue harmoniques", dict(o1taba=4, o1pos=80, as_=100, ad=100, ar=120, fenv=0)),
        ("Clavier voyelle", dict(o1taba=1, o1pos=20)), ("Clavier 8 bits", dict(o1taba=7, o1pos=30)), ("Accords granuleux", dict(o1taba=5, o1pos=40, ad=600, as_=20)),
    ]:
        add(nm, KEYS, mix(CLAV, **kw))
    for nm, kw in [("Cordes frottées qui roulent", dict(o1taba=6, o1tabb=6, aa=400, ar=1300)), ("Cordes lentes", dict(o1taba=6, o1tabb=0, aa=900, ar=2200)),
                   ("Cordes de verre", dict(o1taba=6, o1tabb=2, aa=350, ar=1300))]:
        add(nm, STR, mix(NAPPE, kw))
    for nm, kw in [("Cuivres harmoniques", dict(o1taba=4, o1pos=60)), ("Cuivres analogiques", dict(o1taba=0, o1pos=55)), ("Cuivres voyelle", dict(o1taba=1, o1pos=50))]:
        add(nm, BRASS, mix(dict(cutoff=900, fenv=55, fa=60, fd=600, fs=45, aa=35, ad=800, as_=85, ar=300), kw))
    for nm, kw in [("Arpège qui roule", dict(arp_mode=1, arp_oct=2)), ("Arpège cloches", dict(o1taba=2, arp_mode=3, arp_oct=2)),
                   ("Arpège au flipper", dict(edget=70, edgeb=70, edgel=70, edger=70, fric=3, arp_mode=6, arp_oct=2)), ("Arpège voyelles", dict(o1taba=1, arp_mode=1, arp_oct=3))]:
        add(nm, ARP, mix(PLK, kw))
    for nm, kw in [
        ("Tempête de billes", mix(dict(o1taba=5, wrap=1, tiltx=60, tilty=40, fric=0), route(3, ANG, "cutoff", 50))),
        ("Bille folle", dict(edget=90, edgeb=90, edgel=90, edger=90, fric=0, ptime=300)), ("Gouttière", dict(o1taba=2, tilty=-80, fric=30)),
        ("Machine à couloirs", mix(dict(o1taba=7), route(1, SA, "cutoff", 50), route(2, SB, "o1pos", 60), route(3, SC, "o1morph", 50), route(4, SD, "tune", 5))),
    ]:
        add(nm, FX, mix(NAPPE, kw))
    return b
