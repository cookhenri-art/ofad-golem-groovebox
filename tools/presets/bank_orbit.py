# Orbiteur (v0.47 (maj. 5 nov.), synthèse vectorielle : quatre coins A à D de deux générateurs, une orbite qui les parcourt) : 74 sons de plus.
# Générateurs xg1 / xg2 : 0..5 = A..F, 6 = bruit, 7 = W (onde choisie xw1 / xw2 : 0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion,
# 5 orgue, 6 voix A, 7 voix O, 8 cloche, 9 cordes, 10 creux, 11 métal, 12 premiers, 13 bande, 14 grain, 15 doux, 16-18 scies, 19 impulsion 10 %).
# xbmode : 0 AM, 1 PM, 2 PM + AM, 3 PD + AM, 4 PD inv. + AM · xftype : 0 arrêt, 1 PB12, 2 PB24, 3 bande, 4 PH, 5 formant
# shape : 0 manuelle, 1 enveloppe ADSR, 2-4 Lissajous, 5 polygone, 6 impulsion · speed : 0 8 mes. … 3 1 mes., 4 1/2, 5 1/4, 6 1/8, 7 1/16, 8 1/32
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

W = 7  # générateur « onde choisie »
BASE = dict(ag1=W, ag2=W, bg1=W, bg2=W, cg1=W, cg2=W, dg1=W, dg2=W, aw1=2, aw2=3, bw1=2, bw2=9, cw1=5, cw2=15, dw1=11, dw2=8,
            shape=2, size=60, speed=1, aa=20, ad=1200, as_=80, ar=1200, chorus=25)
BASSE = mix(BASE, vmode=1, stack=3, stackdet=10, aw1=2, aw2=3, bw1=17, bw2=4, cw1=0, cw2=2, dw1=19, dw2=3, shape=0, cx=30, cy=30, size=20,
            aa=1, ad=600, as_=85, ar=150, aftype=2, acut=900, bftype=2, bcut=700, cftype=2, ccut=600, dftype=2, dcut=800, chorus=0, keytrack=40)
LEAD_ = mix(BASE, vmode=1, stack=2, stackdet=8, glide=40, shape=0, cx=50, cy=60, size=25, aa=3, ad=800, as_=100, ar=250, vibdepth=8, vibrate=5.5, chorus=15)
NAPPE = mix(BASE, shape=3, speed=0, size=70, aa=900, ad=2000, as_=90, ar=2500, chorus=45, mixtype=1, mixcut=5000)
PLK = mix(BASE, shape=1, orbenv=1, oa=1, od=300, os=0, or_=300, size=70, aa=1, ad=400, as_=0, ar=350, chorus=20)
CLAV = mix(BASE, shape=1, orbenv=1, oa=1, od=900, os=20, or_=500, aa=1, ad=1500, as_=35, ar=500, velamp=50)


def corners(a, b, c, d):
    """Une onde choisie par coin (générateur 1), le générateur 2 reprend la même onde un cran plus loin."""
    return dict(aw1=a, aw2=min(63, a + 1), bw1=b, bw2=min(63, b + 1), cw1=c, cw2=min(63, c + 1), dw1=d, dw2=min(63, d + 1))


def build():
    b = Bank("orbit")
    add = b.add

    for nm, kw in [
        ("Basse en coin A", dict(cx=0, cy=100)), ("Basse qui voyage", dict(shape=1, orbenv=1, oa=1, od=400, os=0, size=60)),
        ("Basse PM double", dict(abmode=1, ablend=60, bbmode=1, bblend=60)), ("Basse PD", dict(abmode=3, ablend=50, bbmode=3, bblend=40)),
        ("Basse sinus et scie", corners(0, 2, 0, 17)), ("Basse carrée", corners(3, 3, 19, 4)), ("Basse cordes", corners(9, 2, 9, 17)),
        ("Basse saturée", dict(drive=60, drivecol=900)), ("Basse empilée large", dict(stack=6, stackdet=20)), ("Basse en polygone", dict(shape=5, ngon=4, speed=6, size=40)),
        ("Basse lissajous", dict(shape=2, speed=5, size=35)), ("Basse formant", dict(aftype=5, acut=800, bftype=5, bcut=600)), ("Basse glissée", dict(glide=110)),
        ("Basse grave pure", corners(0, 0, 15, 15)), ("Basse métal", corners(11, 2, 11, 3)), ("Basse impulsion 10 %", corners(19, 19, 4, 3)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead en orbite lente", dict(shape=2, speed=3, size=40)), ("Lead voix", corners(6, 7, 6, 2)), ("Lead métal", corners(11, 8, 2, 11)),
        ("Lead PM", dict(abmode=1, ablend=50, cbmode=1, cblend=50)), ("Lead carré", corners(3, 19, 3, 4)), ("Lead polygone", dict(shape=5, ngon=3, speed=6, size=50)),
        ("Lead impulsion", dict(shape=6, speed=7, pulsew=30, size=50)), ("Lead vibrato", dict(vibdepth=20)), ("Lead saturé", dict(drive=50)),
        ("Lead empilé", dict(stack=5, stackdet=15)), ("Lead doux", corners(15, 1, 15, 0)), ("Lead cordes", corners(9, 16, 9, 2)),
        ("Lead orgue", corners(5, 12, 5, 2)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe en voyage", {}), ("Nappe voix", corners(6, 7, 14, 9)), ("Nappe cloches", corners(8, 11, 0, 15)), ("Nappe cordes", corners(9, 9, 2, 16)),
        ("Nappe orgue", corners(5, 12, 5, 0)), ("Nappe grains", corners(14, 13, 14, 10)), ("Nappe polygone", dict(shape=5, ngon=6, speed=1)),
        ("Nappe PD", dict(abmode=3, ablend=40, bbmode=4, bblend=40, cbmode=3, cblend=30)), ("Nappe formant", dict(mixtype=5, mixcut=1000)),
        ("Nappe très lente", dict(speed=0, aa=2500, ar=4500)), ("Nappe bruit d'étoiles", dict(dg1=6, dg2=6, dftype=3, dcut=3000)), ("Nappe trémolo", dict(tremdepth=30, tremrate=3)),
        ("Nappe lissajous 3", dict(shape=4, speed=2)), ("Nappe scies douces", corners(16, 17, 18, 16)), ("Nappe creuse", corners(10, 15, 10, 0)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck en voyage", {}), ("Pluck cloche", corners(8, 11, 8, 0)), ("Pluck voix", corners(6, 7, 6, 7)), ("Pluck métal", corners(11, 11, 2, 8)),
        ("Pluck PM", dict(abmode=1, ablend=70)), ("Pluck carré", corners(3, 19, 4, 3)), ("Pluck formant clair", dict(mixtype=5, mixcut=1500)), ("Pluck doux", corners(15, 0, 1, 15)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Piano vectoriel", {}), ("Cloches vectorielles", mix(corners(8, 11, 8, 15), os=0, as_=0, ad=2200, ar=2000)),
        ("Orgue en coins", mix(corners(5, 12, 5, 0), as_=100, ad=150, ar=150, os=100)), ("Clavier voix", corners(6, 1, 7, 0)),
        ("Clavier PM", dict(abmode=1, ablend=40, bbmode=1, bblend=40)), ("Accords en orbite", dict(od=500, os=0, ad=600, as_=20)),
    ]:
        add(nm, KEYS, mix(CLAV, kw))
    for nm, kw in [("Cordes vectorielles", corners(9, 16, 9, 2)), ("Cordes lentes en orbite", mix(corners(9, 9, 16, 17), aa=900, ar=2200)),
                   ("Cordes pincées en orbite", mix(corners(9, 2, 9, 16), aa=1, ad=600, as_=0, ar=500))]:
        add(nm, STR, mix(NAPPE, dict(aa=350, ar=1300), kw))
    for nm, kw in [("Cuivres vectoriels", corners(2, 16, 17, 5)), ("Cuivres voix", corners(6, 2, 7, 16)), ("Cuivres en polygone", mix(corners(2, 17, 18, 16), shape=5, ngon=5))]:
        add(nm, BRASS, mix(BASE, dict(shape=1, orbenv=1, oa=60, od=600, os=50, aa=40, ad=800, as_=85, ar=300, mixtype=2, mixcut=1500), kw))
    for nm, kw in [("Arpège en orbite", dict(arp_mode=1, arp_oct=2)), ("Arpège cloches", mix(corners(8, 11, 8, 0), arp_mode=3, arp_oct=2)),
                   ("Arpège polygone", dict(shape=5, ngon=3, speed=6, arp_mode=6, arp_oct=2)), ("Arpège voix", mix(corners(6, 7, 6, 2), arp_mode=1, arp_oct=3))]:
        add(nm, ARP, mix(PLK, **kw))
    for nm, kw in [
        ("Tourbillon", dict(shape=2, speed=8, size=100)), ("Pulsar", dict(shape=6, speed=7, size=100, pulsew=10)), ("Vent cosmique", dict(ag1=6, bg1=6, cg1=6, dg1=6, mixtype=3, mixcut=1200, mixreso=50)),
        ("Signal lointain", mix(corners(8, 0, 11, 6), shape=4, speed=8, tremdepth=40, tremrate=9)),
    ]:
        add(nm, FX, mix(NAPPE, **kw))
    return b
