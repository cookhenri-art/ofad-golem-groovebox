# Hot Sauce (hybride à deux moteurs) : 10 sons par catégorie. Réglages originaux.
# Types : 0 analogique (réglage 1 = forme, 2 = désaccord), 1 table d'ondes (1 = position, 2 = pli),
# 2 harmonique (1 = pente, 2 = parité), 3 modal (1 = matière corde → barre → plaque, 2 = durée).
# Table d'ondes : 0 sinus, 14 triangle, 29 scie, 43 carré, 57 voyelle a, 71 voyelle o, 86 métal, 100 impulsion.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

VA, WT, HA, MO = 0, 1, 2, 3
SIN, TRI, SAW, SQR, VA_, VO, MET, PUL = 0, 14, 29, 43, 57, 71, 86, 100


def eng(e, t=VA, p1=50, p2=20, lvl=80, oc=0, semi=0, fine=0):
    return {f"{e}type": t, f"{e}p1": p1, f"{e}p2": p2, f"{e}lvl": lvl, f"{e}oct": oc, f"{e}semi": semi, f"{e}fine": fine}


def A(*a, **k):
    return eng("a", *a, **k)


def B(*a, **k):
    return eng("b", *a, **k)


OFF_B = dict(blvl=0)


def flt(cut=8000, res=10, env=25, morph=0, cut2=18000, res2=0, drive=0, route=0, a=2, d=600, s=30, r=500, key=40):
    return dict(f1cut=cut, f1res=res, fenv=env, f1morph=morph, f2cut=cut2, f2res=res2, f2drive=drive, route=route, fa=a, fd=d, fs=s, fr=r, fkey=key)


def amp(a=3, d=900, s=80, r=500):
    return dict(aa=a, ad=d, as_=s, ar=r)


def lfo(dst, amt, rate, slot=1, src=6):
    return {f"mx{slot}_src": src, f"mx{slot}_dst@{dst}": 0, f"mx{slot}_amt": amt, f"lf{slot}_rate": rate}


MONO = dict(vmode=1, voices=1)
LEG = dict(vmode=2, voices=1, glide=45)


def build():
    b = Bank("hots")
    add = b.add

    # --- Basses
    add("Basse pimentée", BASS, mix(A(VA, 50, 15), B(WT, SQR, 0, 50, oc=-1), flt(900, 25, 45, d=250, cut2=4000), amp(d=900, s=80, r=120), MONO))
    add("Basse table", BASS, mix(A(WT, SAW, 20, 90), OFF_B, flt(1200, 20, 40, d=300), amp(s=85, r=120), MONO, sub=40))
    add("Basse harmonique", BASS, mix(A(HA, 40, 50), OFF_B, flt(3000, 0, 10), amp(s=85, r=120), MONO, sub=30))
    add("Basse de corde", BASS, mix(A(MO, 0, 45), B(VA, 50, 0, 40), flt(2000, 10, 20, d=300), amp(d=1500, s=60, r=150), MONO))
    add("Basse saturée 2", BASS, mix(A(VA, 50, 30), B(VA, 75, 20, 60, oc=-1), flt(1500, 20, 35, cut2=2500, res2=30, drive=70, d=300), amp(s=85, r=120), LEG))
    add("Basse FM sauce", BASS, mix(A(WT, SIN, 0, 0), B(WT, SIN, 0, 80, oc=-1), flt(4000, 0, 20, d=250), amp(s=80, r=120), MONO, fmab=60))
    add("Basse voyelle", BASS, mix(A(WT, VA_, 20, 90), OFF_B, flt(1500, 30, 30, morph=10, d=300), amp(s=85, r=120), MONO))
    add("Basse acide 3", BASS, mix(A(VA, 50, 0), OFF_B, flt(18000, 0, 0, cut2=500, res2=75, drive=50, d=220), amp(d=300, s=70, r=80), LEG, fenv=70))
    add("Basse sub chaude", BASS, mix(A(VA, 30, 10, 60), OFF_B, flt(700, 10, 20, d=300), amp(s=90, r=150), MONO, sub=70))
    add("Basse métallique 2", BASS, mix(A(WT, MET, 40, 70), B(VA, 50, 0, 50, oc=-1), flt(1800, 25, 40, d=220), amp(d=600, s=60, r=100), MONO))

    # --- Leads
    add("Lead pimenté", LEAD, mix(A(VA, 50, 35), B(WT, SQR, 0, 40, oc=1), flt(4500, 20, 25), amp(s=90, r=200), LEG))
    add("Lead table qui bouge", LEAD, mix(A(WT, SAW, 30, 85), OFF_B, flt(5000, 15, 15), amp(s=90, r=200), LEG, **lfo("ap1", 25, 0.4)))
    add("Lead harmonique 2", LEAD, mix(A(HA, 20, 0), OFF_B, flt(6000, 10, 10), amp(s=90, r=200), LEG))
    add("Lead flûte modale", LEAD, mix(A(MO, 30, 90, 70), B(HA, 80, 50, 50), flt(5000, 0, 0), amp(a=40, s=90, r=250), LEG))
    add("Lead voyelle", LEAD, mix(A(WT, VA_, 20, 90), OFF_B, flt(4000, 20, 10), amp(s=90, r=200), LEG, **lfo("ap1", 20, 0.25)))
    add("Lead hurleur 2", LEAD, mix(A(VA, 50, 40), B(VA, 75, 40, 70), flt(3000, 40, 30, cut2=5000, drive=80, d=400), amp(s=90, r=200), LEG))
    add("Lead FM sauce", LEAD, mix(A(WT, SIN, 0, 0, oc=1), B(WT, SIN, 30, 90), flt(6000, 0, 10), amp(s=90, r=200), LEG, fmab=55))
    add("Lead carré creux", LEAD, mix(A(VA, 75, 20), OFF_B, flt(3500, 20, 15, morph=20), amp(s=90, r=200), LEG))
    add("Lead impulsion 2", LEAD, mix(A(WT, PUL, 40, 80), OFF_B, flt(5000, 15, 10), amp(s=90, r=200), LEG))
    add("Lead lointain 2", LEAD, mix(A(HA, 60, 50), B(VA, 25, 30, 40, oc=1), flt(3000, 10, 10), amp(a=60, s=90, r=600), LEG, glide=90))

    # --- Nappes
    NP = dict(spread=80)
    add("Nappe pimentée", PAD, mix(A(WT, SAW, 20, 70), B(VA, 50, 50, 50), flt(2500, 20, 20, a=1200), amp(a=900, d=3000, s=90, r=2500), NP, **lfo("ap1", 30, 0.15)))
    add("Nappe de verre 3", PAD, mix(A(HA, 10, 0, 70), B(WT, MET, 0, 40, oc=1), flt(6000, 0, 0), amp(a=1200, d=3000, s=85, r=3000), NP))
    add("Nappe voyelles", PAD, mix(A(WT, VA_, 0, 70), B(WT, VO, 10, 60, fine=9), flt(3000, 15, 10), amp(a=1000, d=3000, s=90, r=2800), NP, **lfo("ap1", 25, 0.1)))
    add("Nappe modale", PAD, mix(A(MO, 70, 100, 70), B(HA, 40, 40, 50), flt(4000, 10, 0), amp(a=800, d=3000, s=85, r=3000), NP))
    add("Nappe sombre 3", PAD, mix(A(VA, 50, 40, 70), B(WT, TRI, 0, 60, oc=-1), flt(1000, 20, 15, a=2000), amp(a=1500, d=3000, s=90, r=3000), NP))
    add("Nappe qui s'ouvre 2", PAD, mix(A(WT, SAW, 0, 70), B(VA, 50, 60, 50), flt(500, 25, 70, a=4000, d=5000, s=80), amp(a=2000, d=4000, s=95, r=3000), NP))
    add("Nappe harmonique", PAD, mix(A(HA, 50, 25, 70), B(HA, 70, 75, 50, oc=1, fine=6), flt(4000, 0, 0), amp(a=1000, d=3000, s=90, r=2500), NP, **lfo("ap2", 30, 0.2)))
    add("Nappe large 3", PAD, mix(A(VA, 50, 70, 70), B(VA, 50, 70, 70, fine=12), flt(3000, 10, 10), amp(a=900, d=3000, s=90, r=2500), NP, spread=100))
    add("Nappe en parallèle", PAD, mix(A(WT, SQR, 20, 70), B(WT, VO, 0, 60), flt(800, 40, 10, morph=50, cut2=3000, res2=30, route=100), amp(a=1200, d=3000, s=90, r=3000), NP))
    add("Nappe chaude 3", PAD, mix(A(VA, 40, 50, 70), B(WT, TRI, 0, 50), flt(1600, 15, 15, a=1500), amp(a=1200, d=3000, s=90, r=2800), NP, sub=20))

    # --- Plucks
    PK = amp(d=450, s=0, r=350)
    add("Pluck pimenté", PLUCK, mix(A(VA, 50, 20), OFF_B, flt(500, 30, 60, d=220), PK))
    add("Pluck de table", PLUCK, mix(A(WT, SQR, 0), B(WT, MET, 0, 30), flt(800, 25, 55, d=200), PK))
    add("Corde pincée", PLUCK, mix(A(MO, 0, 55), OFF_B, flt(8000, 0, 0), amp(d=2500, s=0, r=1200)))
    add("Pluck harmonique", PLUCK, mix(A(HA, 30, 0), OFF_B, flt(600, 20, 60, d=200), PK))
    add("Kalimba modale", PLUCK, mix(A(MO, 50, 45), OFF_B, flt(6000, 0, 0), amp(d=1500, s=0, r=800)))
    add("Pluck creux 3", PLUCK, mix(A(VA, 75, 10), OFF_B, flt(2500, 25, 40, morph=20, d=220), PK))
    add("Pluck voyelle", PLUCK, mix(A(WT, VO, 0), OFF_B, flt(1000, 25, 50, d=200), PK))
    add("Pluck FM sauce", PLUCK, mix(A(WT, SIN, 0, 0, oc=1), B(WT, SIN, 0, 80), flt(5000, 0, 20, d=200), PK, fmab=50))
    add("Harpe modale", PLUCK, mix(A(MO, 10, 65), OFF_B, flt(7000, 0, 0), amp(d=3000, s=0, r=1500)))
    add("Pluck sec 3", PLUCK, mix(A(VA, 50, 10), OFF_B, flt(400, 35, 70, d=140), amp(d=250, s=0, r=200)))

    # --- Claviers
    add("Piano modal", KEYS, mix(A(MO, 15, 70), B(HA, 40, 0, 30), flt(6000, 0, 10, d=800), amp(d=3000, s=0, r=700)))
    add("Piano électrique 3", KEYS, mix(A(WT, SIN, 0, 0), B(WT, SIN, 0, 60, oc=2), flt(5000, 0, 10), amp(d=2500, s=0, r=600), fmab=35))
    add("Orgue harmonique", KEYS, mix(A(HA, 70, 50), OFF_B, flt(8000, 0, 0), amp(a=4, d=500, s=100, r=120)))
    add("Orgue à tables", KEYS, mix(A(WT, SQR, 0), B(WT, SIN, 0, 50, oc=1), flt(5000, 0, 0), amp(a=4, d=500, s=100, r=150)))
    add("Clavinette 3", KEYS, mix(A(WT, PUL, 30), OFF_B, flt(2000, 35, 40, d=200), amp(d=900, s=40, r=120)))
    add("Célesta modal", KEYS, mix(A(MO, 85, 55), OFF_B, flt(8000, 0, 0), amp(d=2500, s=0, r=1200)))
    add("Vibraphone modal", KEYS, mix(A(MO, 65, 75), OFF_B, flt(7000, 0, 0), amp(d=3500, s=0, r=1500), **lfo("level", 8, 5.5)))
    add("Clavecin 3", KEYS, mix(A(WT, SAW, 0, 90), OFF_B, flt(7000, 10, 10), amp(d=1500, s=0, r=300)))
    add("Accords house 3", KEYS, mix(A(VA, 50, 40), B(VA, 75, 40, 50), flt(1200, 25, 35, d=300), amp(d=600, s=30, r=200)))
    add("Marimba modal", KEYS, mix(A(MO, 50, 25), OFF_B, flt(5000, 0, 0), amp(d=900, s=0, r=400)))

    # --- Cordes
    ST = dict(spread=85)
    add("Cordes pimentées", STR, mix(A(VA, 50, 60, 70), B(WT, SAW, 0, 60, fine=10), flt(4000, 10, 10, a=500), amp(a=500, d=2000, s=90, r=1500), ST))
    add("Cordes frottées", STR, mix(A(MO, 0, 90, 80), B(VA, 50, 50, 40), flt(4500, 5, 0), amp(a=600, d=2000, s=90, r=1500), ST))
    add("Violons 3", STR, mix(A(VA, 50, 50, 70, oc=1), B(WT, SAW, 0, 50, oc=1, fine=8), flt(5000, 10, 10), amp(a=350, d=2000, s=90, r=900), ST, **lfo("tune", 2, 5.5)))
    add("Violoncelles 3", STR, mix(A(VA, 50, 50, 70, oc=-1), B(WT, SAW, 0, 50, oc=-1, fine=8), flt(2500, 10, 10), amp(a=400, d=2000, s=90, r=900), ST))
    add("Pizzicato 3", STR, mix(A(MO, 5, 40), B(VA, 50, 40, 40), flt(3000, 10, 20, d=150), amp(d=300, s=0, r=250), ST))
    add("Cordes brillantes 3", STR, mix(A(VA, 50, 60, 70), B(WT, SAW, 20, 60, oc=1), flt(7000, 10, 10), amp(a=400, d=2000, s=90, r=1200), ST))
    add("Cordes sombres 3", STR, mix(A(VA, 50, 60, 70), B(WT, TRI, 0, 50), flt(1500, 10, 10), amp(a=700, d=3000, s=90, r=1500), ST))
    add("Trémolo 3", STR, mix(A(VA, 50, 60, 70), B(WT, SAW, 0, 60, fine=10), flt(4000, 10, 10), amp(a=300, d=2000, s=90, r=1000), ST, **lfo("level", 25, 7)))
    add("Ensemble 3", STR, mix(A(VA, 50, 80, 70), B(VA, 50, 80, 70, oc=1, fine=-8), flt(3500, 10, 10), amp(a=800, d=3000, s=90, r=2000), ST, spread=100))
    add("Cordes qui gonflent 3", STR, mix(A(VA, 50, 60, 70), B(WT, SAW, 0, 60, fine=10), flt(700, 15, 60, a=3000, d=4000, s=80), amp(a=2000, d=3000, s=95, r=2500), ST))

    # --- Cuivres
    BR = dict(spread=40)
    add("Cuivres pimentés", BRASS, mix(A(VA, 50, 30), B(WT, SAW, 0, 50, fine=8), flt(900, 15, 55, a=80, d=700, s=45), amp(a=40, d=1200, s=85, r=300), BR))
    add("Trompette 3", BRASS, mix(A(WT, SAW, 0), OFF_B, flt(1300, 20, 50, a=60, d=500, s=55), amp(a=30, d=1200, s=85, r=200), LEG, glide=25))
    add("Section 3", BRASS, mix(A(VA, 50, 50), B(VA, 50, 50, 70, fine=12), flt(1000, 15, 50, a=100, d=800, s=50), amp(a=60, d=1200, s=85, r=350), BR, spread=70))
    add("Trombone 3", BRASS, mix(A(VA, 50, 30, 80, oc=-1), OFF_B, flt(700, 15, 45, a=100, d=700, s=50), amp(a=60, d=1200, s=85, r=300), BR))
    add("Cuivres staccato 3", BRASS, mix(A(VA, 50, 30), B(WT, SAW, 0, 50, fine=8), flt(900, 15, 55, a=30, d=250, s=20), amp(a=15, d=300, s=20, r=150), BR))
    add("Cor 3", BRASS, mix(A(HA, 50, 30), OFF_B, flt(900, 5, 35, a=150, d=900, s=50), amp(a=100, d=1500, s=85, r=500), BR))
    add("Cuivres qui grognent 3", BRASS, mix(A(VA, 50, 40), B(VA, 75, 40, 50), flt(600, 30, 60, cut2=3000, drive=50, a=80, d=700, s=45), amp(a=40, d=1200, s=85, r=300), BR))
    add("Saxo harmonique", BRASS, mix(A(HA, 45, 55), B(WT, VO, 0, 30), flt(1500, 25, 35, d=800, s=60), amp(a=30, d=1200, s=90, r=200), LEG, glide=35))
    add("Cuivres doux 3", BRASS, mix(A(VA, 50, 30), OFF_B, flt(600, 5, 40, a=200, d=1000, s=50), amp(a=150, d=1500, s=85, r=500), BR))
    add("Fanfare 3", BRASS, mix(A(VA, 50, 40), B(WT, SAW, 0, 60, oc=1), flt(1000, 15, 50, a=70, d=700, s=50), amp(a=40, d=1200, s=85, r=300), BR))

    # --- Arpèges
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège pimenté", ARP, mix(A(VA, 50, 20), OFF_B, flt(700, 30, 55, d=180), amp(d=300, s=0, r=200), AR))
    add("Arpège de tables", ARP, mix(A(WT, SQR, 0), B(WT, MET, 0, 30), flt(1500, 20, 40, d=150), amp(d=300, s=0, r=200), AR, arp_mode=3))
    add("Arpège modal", ARP, mix(A(MO, 50, 40), OFF_B, flt(7000, 0, 0), amp(d=800, s=0, r=500), AR, arp_mode=2))
    add("Arpège harmonique", ARP, mix(A(HA, 30, 0), OFF_B, flt(1500, 20, 40, d=150), amp(d=300, s=0, r=200), AR, arp_rate=4))
    add("Arpège de verre 3", ARP, mix(A(WT, MET, 0, 100, oc=1), B(WT, SIN, 0, 70, oc=1), flt(9000, 0, 0), amp(d=900, s=10, r=500), AR, arp_mode=3))
    add("Arpège basse 3", ARP, mix(A(VA, 50, 15, 100, oc=-1), OFF_B, flt(2500, 20, 40, d=250), amp(d=400, s=50, r=120), AR, arp_oct=1, sub=40))
    add("Arpège aléatoire 3", ARP, mix(A(WT, VA_, 0), B(MO, 70, 30, 40), flt(3000, 20, 20, d=150), amp(d=300, s=0, r=200), AR, arp_mode=6, arp_oct=3))
    add("Arpège court 3", ARP, mix(A(VA, 75, 0), OFF_B, flt(1500, 20, 30, d=100), amp(d=150, s=0, r=100), AR, arp_gate=25, arp_rate=5))
    add("Arpège octaves 3", ARP, mix(A(VA, 50, 20), OFF_B, flt(2000, 15, 30, d=200), amp(d=350, s=0, r=250), AR, arp_oct=3))
    add("Arpège qui bouge", ARP, mix(A(WT, SAW, 20), OFF_B, flt(1800, 25, 30), amp(d=400, s=20, r=300), AR, arp_rate=1, **lfo("ap1", 40, 0.3)))

    # --- Effets
    add("Sirène pimentée", FX, mix(A(VA, 50, 20), OFF_B, flt(5000, 20, 0), amp(d=3000, s=100, r=600), **lfo("tune", 40, 0.5)))
    add("Vent de sauce", FX, mix(A(VA, 50, 0, 0), OFF_B, flt(1200, 70, 0, morph=50), amp(a=1500, d=3000, s=90, r=3000), noise=100, ncolor=80, spread=100, **lfo("f1cut", 30, 0.12)))
    add("Laser 3", FX, mix(A(WT, SAW, 0), OFF_B, flt(9000, 40, -60, d=400), amp(d=600, s=0, r=300)))
    add("Cloche lointaine", FX, mix(A(MO, 100, 90), B(MO, 85, 80, 50, semi=7), flt(6000, 0, 0), amp(d=5000, s=0, r=4000), spread=70))
    add("Gong modal", FX, mix(A(MO, 95, 100), OFF_B, flt(5000, 0, 0), amp(d=6000, s=0, r=5000), tune=-12))
    add("Robot pimenté", FX, mix(A(WT, PUL, 40), OFF_B, flt(1500, 70, 0, morph=50), amp(d=900, s=90, r=200), **lfo("ap1", 60, 7)))
    add("Ondes de sauce", FX, mix(A(HA, 30, 50, 70), B(WT, VO, 0, 60, fine=40), flt(3000, 30, 0), amp(a=1500, d=3000, s=90, r=3000), spread=100, **lfo("bp1", 70, 0.3)))
    add("Machine frottée", FX, mix(A(MO, 40, 100, 80), OFF_B, flt(2000, 40, 0, morph=50), amp(a=500, d=3000, s=90, r=2000), noise=20, ncolor=40))
    add("Impact pimenté", FX, mix(A(VA, 50, 30, oc=-2), B(MO, 90, 60, 70), flt(3000, 20, 50, d=300), amp(d=800, s=0, r=400), sub=60))
    add("Rêve modal", FX, mix(A(MO, 60, 100, 60), B(HA, 20, 60, 60, oc=1), flt(2500, 30, 0), amp(a=2500, d=4000, s=90, r=4000), spread=90, **lfo("f1cut", 25, 0.08)))
    return b
