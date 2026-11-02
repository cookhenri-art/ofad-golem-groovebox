# World Tour (poly analogique 8 voix : 2 oscillateurs à forme continue, oscillateur de modulation,
# 2 filtres en échelle série / parallèle / stéréo, retard diffusant) : 10 sons par catégorie.
# Formes : 0 sinus, 25 triangle, 50 scie, 75 carré, 100 impulsion étroite (fondu entre voisines).
# Pieds : 0 = 32', 1 = 16', 2 = 8', 3 = 4', 4 = 2'. Routage : 0 série, 1 parallèle, 2 stéréo.
# Réglages originaux (aucun son d'origine recopié).
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

SIN, TRI, SAW, SQR, PUL = 0, 25, 50, 75, 100


def osc(w1=SAW, w2=SAW, o1=2, o2=2, semi=0, fine=7, m1=80, m2=70, **kw):
    return mix(dict(o1wave=w1, o2wave=w2, o1oct=o1, o2oct=o2, o2semi=semi, o2fine=fine, mxo1=m1, mxo2=m2), kw)


def flt(cut=2500, res=10, env=40, route=1, cut2=None, res2=None, link=1, key=40, **kw):
    return mix(dict(f1cut=cut, f1res=res, f2cut=cut if cut2 is None else cut2, f2res=res if res2 is None else res2,
                    fenv=env, route=route, flink=link, fkey=key), kw)


def e1(a=4, d=700, s=25, r=600, loop=0):
    return dict(e1a=a, e1d=d, e1s=s, e1r=r, e1loop=loop)


def e2(a=3, d=900, s=80, r=500):
    return dict(e2a=a, e2d=d, e2s=s, e2r=r)


def dly(m=20, t=380, fb=35, diff=45, sync=None):
    d = dict(dlymix=m, dlytime=t, dlyfb=fb, dlydiff=diff)
    if sync is not None:
        d["dlysync"] = sync
    return d


MONO = dict(vmode=1, voices=1)
LEG = dict(vmode=2, voices=1)


def build():
    b = Bank("muse")
    add = b.add

    # --- Basses
    add("Basse de route", BASS, mix(osc(SAW, SQR, 1, 2, fine=5), flt(800, 30, 50, route=0, cut2=2500, link=0), e1(d=320, s=15), e2(d=600, s=85, r=120), LEG, overload=35, glide=30))
    add("Basse sub ronde", BASS, mix(osc(TRI, SIN, 1, 0, fine=0), flt(700, 5, 25), e1(d=400, s=20), e2(d=800, s=90, r=150), MONO, overload=20))
    add("Basse carrée creuse", BASS, mix(osc(SQR, SQR, 1, 1, fine=-6), flt(900, 25, 40), e1(d=280, s=10), e2(d=500, s=80, r=100), MONO))
    add("Basse qui claque", BASS, mix(osc(SAW, SAW, 1, 2, fine=9), flt(750, 35, 60, route=0, cut2=3000, link=0), e1(d=180, s=5), e2(d=400, s=70, r=90), MONO, overload=45))
    add("Basse épaisse", BASS, mix(osc(SAW, SAW, 1, 0, fine=0), flt(600, 20, 45), e1(d=500, s=30), e2(d=900, s=90, r=150), LEG, overload=55, glide=60))
    add("Basse en anneau 2", BASS, mix(osc(SAW, SQR, 1, 2, semi=7), flt(800, 20, 40), e1(d=250, s=0), e2(d=500, s=70, r=120), MONO, mxring=45, mxo2=20))
    add("Basse FM analogique", BASS, mix(osc(SIN, SIN, 1, 2, fine=0, m2=0, o1fm=55), flt(1500, 10, 30), e1(d=300, s=20), e2(d=700, s=80, r=120), MONO))
    add("Basse acide 2", BASS, mix(osc(SAW, SQR, 2, 1, fine=0, m2=45), flt(1100, 45, 55, route=0, cut2=4000, link=0), e1(d=220, s=0), e2(d=300, s=80, r=80), LEG, glide=80, overload=50))
    add("Basse pulsée", BASS, mix(osc(PUL, SQR, 1, 1, fine=4), flt(1000, 20, 35), e1(d=300, s=20), e2(d=600, s=80, r=110), MONO,
                                  mowave=0, morate=0.4, mowaveamt=35))
    add("Basse voyageuse", BASS, mix(osc(SAW, TRI, 1, 1, fine=8), flt(650, 35, 50, route=2, cut2=900, link=0), e1(d=350, s=20), e2(d=700, s=85, r=140), LEG, overload=30, glide=40))

    # --- Leads
    LD = mix(LEG, glide=45, vibamt=8)
    add("Lead chaud", LEAD, mix(osc(SAW, SAW, fine=10), flt(1800, 25, 45), e1(d=600, s=40), e2(d=900, s=90, r=200), LD, overload=35, **dly(15)))
    add("Lead carré 2", LEAD, mix(osc(SQR, SQR, 2, 3, fine=6), flt(2500, 15, 30), e1(d=500, s=50), e2(d=900, s=90, r=200), LD))
    add("Lead synchro 2", LEAD, mix(osc(SAW, SAW, 2, 2, semi=7, m1=0, m2=90, o2sync=1), flt(4000, 15, 20), e1(d=600, s=40), e2(d=900, s=90, r=200), LD,
                                    mopitch=12, modest=2, mowave=1, morate=0.25, **dly(18)))
    add("Lead flûte 2", LEAD, mix(osc(TRI, SIN, 2, 3, m2=30, fine=0), flt(3000, 5, 10), e1(a=60, d=800, s=70), e2(a=50, d=900, s=90, r=250), LD, mxnoise=8, vibamt=12))
    add("Lead hurlant 2", LEAD, mix(osc(SAW, SQR, 2, 2, fine=12), flt(1500, 60, 50, route=0, cut2=3000, link=0), e1(d=400, s=50), e2(d=900, s=90, r=200), LD, overload=70))
    add("Lead en quintes", LEAD, mix(osc(SAW, SAW, 2, 2, semi=7, fine=0), flt(2200, 20, 35), e1(d=500, s=40), e2(d=900, s=90, r=200), LD, **dly(20)))
    add("Lead FM 2", LEAD, mix(osc(SIN, SIN, 2, 3, m2=0, fine=0, o1fm=60), flt(5000, 0, 10), e1(d=600, s=60), e2(d=900, s=90, r=200), LD))
    add("Lead souple", LEAD, mix(osc(TRI, SAW, 2, 2, fine=5, m2=40), flt(1500, 20, 40), e1(a=30, d=600, s=50), e2(a=20, d=900, s=90, r=300), LD, glide=90, **dly(25, diff=60)))
    add("Lead impulsion", LEAD, mix(osc(PUL, PUL, 2, 2, fine=8), flt(3000, 20, 25), e1(d=500, s=50), e2(d=900, s=90, r=200), LD, mowave=0, morate=4, mowaveamt=40))
    add("Lead rêche", LEAD, mix(osc(SAW, SAW, 2, 1, fine=0), flt(2800, 35, 30, route=2, cut2=1800, link=0), e1(d=400, s=50), e2(d=900, s=90, r=200), LD, overload=55, mxnoise=10))

    # --- Nappes
    NP = mix(spread=40, drift=30)
    add("Nappe de voyage", PAD, mix(osc(SAW, SAW, fine=10), flt(1200, 15, 30, route=2, cut2=1600, link=0), e1(a=1500, d=3000, s=60, r=2500), e2(a=1200, d=3000, s=90, r=2800), NP,
                                    **dly(30, fb=50, diff=75)))
    add("Nappe douce 2", PAD, mix(osc(TRI, TRI, fine=9), flt(1600, 5, 20), e1(a=1000, d=3000, s=70, r=2500), e2(a=900, d=3000, s=90, r=2500), NP, **dly(20, diff=70)))
    add("Nappe carrée", PAD, mix(osc(SQR, SAW, 2, 3, fine=6), flt(1400, 20, 25), e1(a=800, d=3000, s=60, r=2500), e2(a=700, d=3000, s=90, r=2500), NP,
                                 mowave=0, morate=0.3, mowaveamt=25, **dly(25)))
    add("Nappe qui tourne", PAD, mix(osc(SAW, SAW, fine=12), flt(900, 35, 20, route=2, cut2=1300, link=0), e1(a=1000, d=3000, s=70, r=2500), e2(a=1000, d=3000, s=90, r=2800), NP,
                                     mowave=0, morate=0.2, mofilter=35, mopan=1, **dly(25, diff=70)))
    add("Nappe sombre 2", PAD, mix(osc(SAW, TRI, 2, 1, fine=7), flt(700, 15, 20), e1(a=1500, d=3000, s=60, r=3000), e2(a=1500, d=3000, s=90, r=3000), NP, **dly(20)))
    add("Nappe de lumière", PAD, mix(osc(SAW, SAW, 2, 3, fine=8), flt(1800, 10, 40, route=1, cut2=900, link=0, f1mode=1), e1(a=2000, d=4000, s=70, r=3000),
                                     e2(a=1500, d=3000, s=90, r=3000), NP, **dly(35, fb=55, diff=80)))
    add("Nappe en boucle", PAD, mix(osc(SAW, SQR, fine=9), flt(800, 40, 50), e1(a=300, d=600, s=0, r=1500, loop=1), e2(a=800, d=3000, s=90, r=2500), NP, **dly(25)))
    add("Nappe large 2", PAD, mix(osc(SAW, SAW, fine=15), flt(2000, 10, 20, route=2, cut2=2000, link=0), e1(a=900, d=3000, s=70, r=2500), e2(a=800, d=3000, s=90, r=2500),
                                  NP, spread=70, **dly(30, diff=70)))
    add("Nappe à la quinte", PAD, mix(osc(SAW, TRI, 2, 2, semi=7, fine=0), flt(1300, 10, 25), e1(a=1000, d=3000, s=60, r=2500), e2(a=900, d=3000, s=90, r=2500), NP, **dly(25)))
    add("Nappe de brume", PAD, mix(osc(TRI, SIN, fine=12, m1=70, m2=60), flt(1100, 25, 15), e1(a=2500, d=4000, s=70, r=3500), e2(a=2500, d=4000, s=90, r=4000), NP,
                                   mxnoise=12, **dly(40, fb=60, diff=85)))

    # --- Plucks
    PK = e2(d=450, s=0, r=350)
    add("Pluck chaud", PLUCK, mix(osc(SAW, SAW, fine=8), flt(500, 25, 60), e1(d=250, s=0), PK, **dly(20)))
    add("Pluck stéréo 2", PLUCK, mix(osc(SAW, SQR, fine=6), flt(600, 30, 55, route=2, cut2=1000, link=0), e1(d=220, s=0), PK, **dly(25)))
    add("Pluck rond", PLUCK, mix(osc(TRI, SQR, 2, 3, fine=0, m2=40), flt(900, 15, 45), e1(d=200, s=0), PK))
    add("Pluck creux 2", PLUCK, mix(osc(SQR, SQR, fine=5), flt(700, 35, 50), e1(d=180, s=0), e2(d=300, s=0, r=250)))
    add("Pluck de cristal", PLUCK, mix(osc(SIN, SIN, 2, 4, m2=0, fine=0, o1fm=40), flt(4000, 5, 20), e1(d=200, s=0), e2(d=600, s=0, r=500), **dly(30, diff=70)))
    add("Pluck sec 2", PLUCK, mix(osc(SAW, SAW, fine=4), flt(700, 35, 60), e1(d=120, s=0), e2(d=250, s=0, r=150)))
    add("Pluck en écho", PLUCK, mix(osc(SAW, SQR, fine=7), flt(800, 20, 45), e1(d=200, s=0), PK, **dly(40, t=375, fb=50, sync=None)))
    add("Pluck en anneau", PLUCK, mix(osc(SAW, SIN, 2, 3, semi=5), flt(1500, 20, 40), e1(d=180, s=0), PK, mxring=50, mxo2=10))
    add("Pluck harpe 2", PLUCK, mix(osc(TRI, SAW, 2, 3, fine=0, m2=25), flt(2000, 5, 30), e1(d=400, s=0), e2(d=1500, s=0, r=1000)))
    add("Pluck nasillard", PLUCK, mix(osc(PUL, SAW, fine=6), flt(900, 50, 40, f1mode=0), e1(d=200, s=0), PK))

    # --- Claviers
    add("Clavier de voyage", KEYS, mix(osc(SAW, PUL, 2, 3, m2=35), flt(1800, 10, 30), e1(d=900, s=20), e2(d=1500, s=40, r=500)))
    add("Piano analogique", KEYS, mix(osc(TRI, SAW, 2, 3, fine=0, m2=25), flt(2200, 5, 35), e1(d=1200, s=10), e2(d=2500, s=0, r=600)))
    add("Orgue analogique", KEYS, mix(osc(SQR, SQR, 2, 3, fine=0, m2=60), flt(4000, 0, 0), e1(), e2(a=5, d=500, s=100, r=120), mowave=0, morate=6, movca=10))
    add("Clavinette analogique", KEYS, mix(osc(PUL, PUL, 2, 2, fine=3), flt(1500, 35, 45), e1(d=250, s=10), e2(d=500, s=30, r=100)))
    add("Piano électrique 2", KEYS, mix(osc(SIN, TRI, 2, 3, fine=0, m2=30, o1fm=25), flt(3000, 0, 20), e1(d=800, s=10), e2(d=2500, s=0, r=500), mowave=0, morate=5, movca=15, mopan=1))
    add("Accords house 2", KEYS, mix(osc(SAW, SQR, 2, 2, fine=6), flt(1000, 25, 40), e1(d=300, s=10), e2(d=500, s=30, r=200), **dly(25)))
    add("Célesta analogique", KEYS, mix(osc(SIN, SIN, 3, 4, m2=40, fine=0), flt(6000), e1(), e2(d=1500, s=0, r=800), **dly(20, diff=70)))
    add("Vibraphone 2", KEYS, mix(osc(SIN, TRI, 2, 4, m2=30, fine=0), flt(5000), e1(), e2(d=2500, s=0, r=1000), mowave=0, morate=5, movca=25))
    add("Clavier brillant", KEYS, mix(osc(SAW, SAW, 2, 3, fine=5, m2=40), flt(2500, 15, 35), e1(d=700, s=20), e2(d=1200, s=40, r=400)))
    add("Orgue d'église 2", KEYS, mix(osc(SQR, SAW, 1, 3, fine=0, m2=50), flt(3000, 0, 0), e2(a=80, d=500, s=100, r=900), **dly(35, fb=55, diff=90)))

    # --- Cordes
    ST = mix(spread=50, drift=25)
    add("Cordes de route", STR, mix(osc(SAW, SAW, fine=12), flt(180, 5, 10, route=0, cut2=3500, link=0, f1mode=1), e1(), e2(a=600, d=3000, s=90, r=1300), ST, **dly(20, diff=80)))
    add("Cordes lentes 2", STR, mix(osc(SAW, SAW, fine=10), flt(2200, 5, 15), e1(a=1500, d=3000, s=70, r=2500), e2(a=1500, d=3000, s=90, r=2500), ST, **dly(25, diff=80)))
    add("Violons analogiques", STR, mix(osc(SAW, SAW, 2, 3, fine=8), flt(3000, 10, 15), e1(a=300, d=2000, s=80), e2(a=350, d=2000, s=90, r=900), ST, vibamt=8))
    add("Violoncelles 2", STR, mix(osc(SAW, SAW, 1, 1, fine=8), flt(1800, 10, 15), e1(a=300, d=2000, s=80), e2(a=400, d=2000, s=90, r=900), ST, vibamt=6))
    add("Cordes en ensemble", STR, mix(osc(SAW, SAW, fine=18), flt(2500, 5, 10, route=2, cut2=2200, link=0), e1(), e2(a=500, d=3000, s=90, r=1500), ST, spread=80,
                                       mowave=0, morate=0.5, mopitch=0.1, **dly(25, diff=85)))
    add("Pizzicato analogique", STR, mix(osc(SAW, TRI, fine=6), flt(1200, 10, 40), e1(d=200, s=0), e2(d=300, s=0, r=250), ST))
    add("Cordes brillantes 2", STR, mix(osc(SAW, SAW, 2, 3, fine=10), flt(5000, 10, 10), e1(), e2(a=400, d=2000, s=90, r=1200), ST))
    add("Cordes sombres 2", STR, mix(osc(SAW, TRI, 2, 1, fine=10), flt(1000, 10, 10), e1(), e2(a=700, d=3000, s=90, r=1500), ST))
    add("Trémolo de cordes 2", STR, mix(osc(SAW, SAW, fine=10), flt(2500, 5, 10), e1(), e2(a=300, d=2000, s=90, r=1000), ST, mowave=0, morate=7, movca=40))
    add("Cordes qui gonflent 2", STR, mix(osc(SAW, SAW, fine=12), flt(600, 10, 60), e1(a=3000, d=4000, s=90, r=2500), e2(a=2000, d=3000, s=95, r=2500), ST, **dly(20)))

    # --- Cuivres
    BR = dict(spread=25, overload=30)
    add("Cuivres de voyage", BRASS, mix(osc(SAW, SAW, fine=8), flt(700, 10, 55), e1(a=80, d=700, s=45), e2(a=40, d=1200, s=85, r=300), BR))
    add("Section analogique", BRASS, mix(osc(SAW, SAW, fine=14), flt(900, 15, 50), e1(a=100, d=800, s=50), e2(a=60, d=1200, s=85, r=350), BR, spread=55))
    add("Trompette analogique", BRASS, mix(osc(SAW, SQR, fine=4, m2=40), flt(1100, 20, 50), e1(a=60, d=500, s=55), e2(a=30, d=1200, s=85, r=200), BR, LEG, glide=25, vibamt=6))
    add("Trombone analogique", BRASS, mix(osc(SAW, SAW, 1, 1, fine=6), flt(600, 15, 45), e1(a=100, d=700, s=50), e2(a=60, d=1200, s=85, r=300), BR))
    add("Cuivres staccato 2", BRASS, mix(osc(SAW, SAW, fine=10), flt(800, 15, 55), e1(a=30, d=250, s=20), e2(a=15, d=300, s=20, r=150), BR))
    add("Cor analogique", BRASS, mix(osc(TRI, SAW, fine=5, m2=40), flt(700, 5, 35), e1(a=150, d=900, s=50), e2(a=100, d=1500, s=85, r=500), BR))
    add("Cuivres qui grognent", BRASS, mix(osc(SAW, SAW, fine=9), flt(500, 35, 60, route=0, cut2=2500, link=0), e1(a=80, d=700, s=45), e2(a=40, d=1200, s=85, r=300), BR, overload=65))
    add("Saxo analogique", BRASS, mix(osc(PUL, SAW, fine=3, m2=40), flt(1300, 30, 35), e1(a=40, d=800, s=60), e2(a=30, d=1200, s=90, r=200), BR, LEG, glide=35, vibamt=10))
    add("Cuivres doux 2", BRASS, mix(osc(SAW, TRI, fine=8), flt(600, 5, 40), e1(a=200, d=1000, s=50), e2(a=150, d=1500, s=85, r=500), BR))
    add("Fanfare analogique", BRASS, mix(osc(SAW, SAW, 2, 3, fine=10), flt(1000, 15, 50), e1(a=70, d=700, s=50), e2(a=40, d=1200, s=85, r=300), BR, spread=45))

    # --- Arpèges
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège de voyage", ARP, mix(osc(SAW, SQR, fine=6), flt(800, 30, 55), e1(d=180, s=0), e2(d=300, s=0, r=250), AR, **dly(25, sync=7)))
    add("Arpège rond 2", ARP, mix(osc(TRI, SQR, 2, 3, fine=0, m2=40), flt(1200, 15, 45), e1(d=150, s=0), e2(d=250, s=0, r=200), AR, arp_mode=2))
    add("Arpège sec 2", ARP, mix(osc(SAW, SAW, fine=4), flt(500, 45, 65), e1(d=120, s=0), e2(d=200, s=0, r=120), AR, arp_rate=4))
    add("Arpège cristal 2", ARP, mix(osc(SIN, SIN, 3, 4, m2=0, fine=0, o1fm=35), flt(5000), e1(), e2(d=300, s=0, r=300), AR, arp_mode=3, **dly(30, sync=7)))
    add("Arpège en anneau", ARP, mix(osc(SAW, SIN, 2, 3, semi=7, m1=90), flt(2500, 20, 40), e1(d=150, s=0), e2(d=300, s=0, r=200), AR, mxring=70, mxo2=20, arp_mode=6))
    add("Arpège basse 2", ARP, mix(osc(SAW, SQR, 1, 1, fine=5), flt(400, 40, 60), e1(d=160, s=0), e2(d=250, s=30, r=100), AR, arp_oct=1))
    add("Arpège stéréo", ARP, mix(osc(SAW, PUL, fine=7), flt(600, 30, 55, route=2, cut2=1100, link=0), e1(d=170, s=0), e2(d=300, s=0, r=250), AR, arp_mode=3, **dly(30)))
    add("Arpège court 2", ARP, mix(osc(SQR, SQR, fine=4), flt(1000, 20, 40), e1(d=100, s=0), e2(d=150, s=0, r=100), AR, arp_gate=25, arp_rate=5))
    add("Arpège octaves 2", ARP, mix(osc(SAW, SAW, 2, 3, fine=0), flt(2000, 20, 45), e1(d=150, s=0), e2(d=300, s=0, r=200), AR, arp_oct=3))
    add("Arpège flottant", ARP, mix(osc(TRI, SAW, fine=8, m2=35), flt(1500, 15, 30), e1(d=300, s=0), e2(a=10, d=500, s=0, r=500), AR, arp_rate=1, **dly(40, fb=55, diff=70)))

    # --- Effets
    add("Sirène de route", FX, mix(osc(SAW, SAW, fine=0), flt(2000, 20, 10), e1(), e2(d=3000, s=100, r=800), mowave=0, morate=0.5, mopitch=12, **dly(30, fb=50)))
    add("Vent du désert", FX, mix(osc(m1=0, m2=0), flt(1800, 55, 0, route=2, cut2=2600, link=0), e1(), e2(a=1500, d=3000, s=90, r=3000), mxnoise=100,
                                  mowave=0, morate=0.12, mofilter=45, mopan=1, **dly(30, diff=80)))
    add("Laser analogique", FX, mix(osc(SAW, SQR, 2, 3, fine=0), flt(3000, 50, 70), e1(d=300, s=0), e2(d=500, s=0, r=300), mowave=1, morate=8, mopitch=-24, moreset=1))
    add("Battement lent", FX, mix(osc(SAW, SAW, semi=0, fine=40), flt(1200, 20, 20), e1(), e2(a=500, d=3000, s=90, r=2500), **dly(25)))
    add("Gouttes", FX, mix(osc(SIN, SIN, 3, 4, m2=0, fine=0), flt(2000, 70, 50), e1(d=80, s=0, loop=1), e2(a=200, d=3000, s=80, r=2000), **dly(45, fb=60, diff=60)))
    add("Moteur", FX, mix(osc(SAW, SQR, 1, 1, fine=0), flt(600, 50, 0), e1(), e2(d=3000, s=90, r=500), mowave=3, morate=30, movca=60, overload=60))
    add("Cloche en anneau", FX, mix(osc(SIN, SIN, 2, 3, semi=6, fine=20, m1=20, m2=10), flt(5000, 0, 0), e1(), e2(d=3000, s=0, r=2500), mxring=90, **dly(30, diff=75)))
    add("Signal lointain", FX, mix(osc(SQR, SIN, 2, 3, fine=0), flt(500, 40, 0, f1mode=1, route=0, cut2=5000, link=0), e1(), e2(a=20, d=3000, s=80, r=2000),
                                   mowave=3, morate=3, movca=50, **dly(40, fb=65, diff=40)))
    add("Tempête", FX, mix(osc(SAW, SAW, 1, 2, fine=30, m1=70, m2=70), flt(1200, 45, 40, route=2, cut2=1600, link=0), e1(a=2000, d=3000, s=50, r=3000), e2(a=2000, d=3000, s=90, r=3000),
                           mxnoise=70, overload=50, mowave=4, morate=0.3, mofilter=40, **dly(35, fb=60, diff=90)))
    add("Machine à rêves", FX, mix(osc(TRI, SAW, 2, 3, semi=7, fine=0, m2=40), flt(1500, 30, 30), e1(a=50, d=400, s=0, r=500, loop=1), e2(a=800, d=3000, s=90, r=3000),
                                   mowave=0, morate=0.25, mopan=1, mofilter=30, **dly(45, fb=65, diff=85)))
    return b
