# Komodo Dragon (synthèse par masques de bits) : 10 sons par catégorie. Réglages originaux.
# Masques de 8 bits répétés le long de la période (segments : 0 = 8 … 5 = 256) ; bit à 0 :
# 0 coupe, 1 inverse, 2 redresse. Delta : passage du masque A au masque B.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

FULL, HALF, ALT, QUART, EDGE, SPARSE, DENSE, STEP = 255, 15, 85, 51, 129, 17, 239, 119


def osc(a1=FULL, b1=HALF, res=0, mode=0, a2=FULL, b2=ALT, semi=0, fine=6, m1=80, m2=0):
    return dict(o1a=a1, o1b=b1, res=res, mmode=mode, o2a=a2, o2b=b2, o2semi=semi, o2fine=fine, mix1=m1, mix2=m2)


def delta(amt=0, env=0, a=300, d=900, loop=0):
    return dict(damt=amt, denv=env, da=a, dd=d, dloop=loop)


def flt(cut=6000, res=15, env=20, kind=0, a=2, d=500, s=30, r=400, key=50):
    return dict(ftype=kind, fcut=cut, fres=res, fenv=env, fa=a, fd=d, fs=s, fr=r, fkey=key)


def amp(a=2, d=800, s=80, r=300):
    return dict(aa=a, ad=d, as_=s, ar=r)


MONO = dict(vmode=1, voices=1)
LEG = dict(vmode=2, voices=1, glide=45)


def build():
    b = Bank("bitm")
    add = b.add

    # --- Basses
    add("Basse de crocs", BASS, mix(osc(FULL, HALF, 0, 1), delta(env=60, a=5, d=250), flt(1400, 25, 40, d=250), amp(d=900, s=80, r=120), MONO))
    add("Basse en escalier", BASS, mix(osc(STEP, FULL, 1, 0), delta(env=-50, a=5, d=400), flt(1200, 20, 35, d=300), amp(s=85, r=120), MONO))
    add("Basse redressée 2", BASS, mix(osc(ALT, FULL, 0, 2), flt(900, 30, 45, d=220), amp(s=80, r=120), LEG))
    add("Basse sub masquée", BASS, mix(osc(FULL, FULL, 0, 0, m2=50, semi=-12, fine=0), flt(700, 10, 20), amp(s=90, r=150), MONO))
    add("Basse qui mord", BASS, mix(osc(DENSE, SPARSE, 2, 1), delta(env=80, a=5, d=180), flt(2000, 35, 50, d=200), amp(d=600, s=60, r=100), MONO))
    add("Basse octaviée", BASS, mix(osc(FULL, QUART, 0, 1, m2=60, semi=12, fine=0), delta(amt=30), flt(1600, 20, 30, d=300), amp(s=80, r=120), MONO))
    add("Basse pulsante", BASS, mix(osc(FULL, HALF, 1, 1), delta(env=70, a=60, d=120, loop=1), flt(1500, 25, 20), amp(s=85, r=150), MONO))
    add("Basse acide masquée", BASS, mix(osc(FULL, EDGE, 0, 1), flt(600, 70, 65, d=200), amp(d=300, s=70, r=80), LEG))
    add("Basse de bois masquée", BASS, mix(osc(QUART, FULL, 0, 2), flt(2500, 10, 25, d=180), amp(d=500, s=30, r=150), MONO))
    add("Basse numérique 8", BASS, mix(osc(SPARSE, DENSE, 0, 0, m2=40, a2=ALT), delta(env=40, a=5, d=300), flt(1800, 20, 30, d=250), amp(s=80, r=120), MONO, noise=10, noisehz=3000))

    # --- Leads
    add("Lead dragon", LEAD, mix(osc(FULL, ALT, 1, 1), delta(env=60, a=200, d=900), flt(4000, 25, 20), amp(s=90, r=200), LEG))
    add("Lead à bits", LEAD, mix(osc(DENSE, SPARSE, 3, 0), delta(amt=40), flt(5000, 20, 10), amp(s=90, r=200), LEG))
    add("Lead carré masqué", LEAD, mix(osc(ALT, FULL, 0, 1), flt(4500, 15, 15), amp(s=90, r=200), LEG))
    add("Lead qui chante", LEAD, mix(osc(FULL, QUART, 1, 2, m2=40, a2=FULL, b2=EDGE, fine=10), delta(env=50, a=500, d=1500, loop=1), flt(3500, 30, 10), amp(a=20, s=90, r=250), LEG))
    add("Lead sifflant masqué", LEAD, mix(osc(FULL, FULL, 0, 0), flt(6000, 0, 0), amp(a=30, s=90, r=250), LEG, pamt=2, pdec=300))
    add("Lead rugueux", LEAD, mix(osc(EDGE, DENSE, 4, 1), delta(env=40, a=100, d=400), flt(3000, 40, 25, d=400), amp(s=90, r=200), LEG))
    add("Lead quinte masquée", LEAD, mix(osc(FULL, HALF, 1, 1, m2=60, semi=7, fine=0, a2=HALF, b2=FULL), flt(4000, 20, 15), amp(s=90, r=200), LEG))
    add("Lead impulsions", LEAD, mix(osc(SPARSE, EDGE, 2, 0), delta(env=70, a=300, d=600, loop=1), flt(5000, 20, 10), amp(s=90, r=200), LEG))
    add("Lead synchro masquée", LEAD, mix(osc(FULL, ALT, 2, 1), delta(env=100, a=5, d=600), flt(6000, 20, 0), amp(s=90, r=200), LEG))
    add("Lead lointain", LEAD, mix(osc(FULL, QUART, 0, 2), flt(2500, 15, 10), amp(a=60, s=90, r=500), LEG, glide=90))

    # --- Nappes
    NP = dict(spread=70)
    add("Nappe de masques", PAD, mix(osc(FULL, ALT, 1, 1, m2=50, a2=FULL, b2=QUART), delta(env=70, a=1500, d=2500, loop=1), flt(3000, 15, 10, a=1000), amp(a=900, d=3000, s=90, r=2500), NP))
    add("Nappe en escalier", PAD, mix(osc(STEP, FULL, 2, 0, m2=40, a2=FULL, b2=STEP), delta(env=60, a=2500, d=3000, loop=1), flt(2500, 20, 10), amp(a=1200, d=3000, s=90, r=3000), NP))
    add("Nappe redressée 2", PAD, mix(osc(ALT, HALF, 0, 2, m2=50, a2=HALF, b2=ALT), delta(amt=20, env=50, a=2000, d=2000, loop=1), flt(2200, 15, 10), amp(a=1000, d=3000, s=90, r=2800), NP))
    add("Nappe numérique 8", PAD, mix(osc(DENSE, EDGE, 3, 1, m2=40, semi=12, fine=0), delta(env=40, a=3000, d=3000, loop=1), flt(3500, 15, 10), amp(a=800, d=3000, s=85, r=2500), NP))
    add("Nappe de verre masquée", PAD, mix(osc(FULL, SPARSE, 4, 1, m2=40, semi=19, fine=0), delta(env=50, a=2000, d=2500, loop=1), flt(6000, 10, 0), amp(a=1500, d=3000, s=85, r=3000), NP))
    add("Nappe sombre masquée", PAD, mix(osc(FULL, HALF, 0, 0, m2=60, semi=-12, fine=0), delta(env=40, a=3000, d=3000, loop=1), flt(1200, 15, 10), amp(a=1500, d=3000, s=90, r=3000), NP))
    add("Nappe chorale masquée", PAD, mix(osc(FULL, QUART, 1, 2, m2=70, fine=12), delta(env=40, a=1800, d=2200, loop=1), flt(2800, 25, 0, kind=1), amp(a=1000, d=3000, s=90, r=2500), NP))
    add("Nappe qui grésille", PAD, mix(osc(EDGE, DENSE, 5, 1, m2=40, fine=9), delta(env=50, a=2500, d=2500, loop=1), flt(3000, 15, 10), amp(a=1200, d=3000, s=90, r=2800), NP, noise=8, noisehz=2000))
    add("Nappe lente masquée", PAD, mix(osc(FULL, ALT, 0, 1, m2=50, fine=8), delta(env=100, a=5000, d=6000, loop=1), flt(2000, 15, 20, a=3000, d=4000, s=70), amp(a=2500, d=4000, s=90, r=4000), NP))
    add("Nappe de quintes", PAD, mix(osc(FULL, HALF, 1, 2, m2=60, semi=7, fine=0), delta(env=50, a=2000, d=2500, loop=1), flt(2600, 15, 10), amp(a=1000, d=3000, s=90, r=2500), NP))

    # --- Plucks
    PK = amp(d=450, s=0, r=300)
    add("Pluck à bits", PLUCK, mix(osc(DENSE, FULL, 2, 1), delta(env=100, a=1, d=200), flt(5000, 15, 20, d=200), PK))
    add("Pluck masqué", PLUCK, mix(osc(FULL, ALT, 0, 1), delta(env=-100, a=1, d=250), flt(4000, 20, 30, d=200), PK))
    add("Pluck redressé", PLUCK, mix(osc(ALT, FULL, 1, 2), flt(3000, 25, 40, d=180), PK))
    add("Pluck sec masqué", PLUCK, mix(osc(QUART, FULL, 0, 0), flt(2000, 30, 50, d=150), amp(d=250, s=0, r=200)))
    add("Pluck de verre 8", PLUCK, mix(osc(FULL, SPARSE, 4, 1, m2=40, semi=12, fine=0), delta(env=80, a=1, d=300), flt(7000, 10, 10), amp(d=900, s=0, r=600)))
    add("Pluck creux masqué", PLUCK, mix(osc(HALF, FULL, 0, 2), flt(2500, 25, 35, d=200), PK))
    add("Koto masqué", PLUCK, mix(osc(FULL, EDGE, 1, 1), delta(env=70, a=1, d=400), flt(4500, 15, 20, d=300), amp(d=1200, s=0, r=600)))
    add("Pluck en écho masqué", PLUCK, mix(osc(STEP, FULL, 2, 0), delta(env=60, a=1, d=200), flt(3500, 20, 30, d=200), amp(d=600, s=0, r=500), spread=80))
    add("Harpe masquée", PLUCK, mix(osc(FULL, QUART, 0, 2), delta(env=40, a=1, d=600), flt(5000, 5, 10), amp(d=2500, s=0, r=1200)))
    add("Pluck bruité", PLUCK, mix(osc(ALT, DENSE, 3, 1), flt(4000, 20, 30, d=150), PK, noise=25, noisehz=6000))

    # --- Claviers
    add("Piano masqué", KEYS, mix(osc(FULL, ALT, 1, 1, m2=30, semi=12, fine=0), delta(env=60, a=1, d=800), flt(5000, 5, 20, d=800), amp(d=2500, s=0, r=600)))
    add("Orgue à bits", KEYS, mix(osc(FULL, FULL, 0, 0, m2=60, semi=12, fine=0, a2=ALT), flt(6000, 0, 0), amp(a=4, d=500, s=100, r=120)))
    add("Orgue redressé", KEYS, mix(osc(ALT, ALT, 0, 2, m2=50, semi=19, fine=0, a2=ALT), flt(5000, 0, 0), amp(a=4, d=500, s=100, r=150)))
    add("Clavinette masquée", KEYS, mix(osc(SPARSE, DENSE, 2, 1), delta(env=60, a=1, d=200), flt(3000, 30, 40, d=200), amp(d=900, s=40, r=120)))
    add("Célesta masqué", KEYS, mix(osc(FULL, EDGE, 3, 1, m2=40, semi=24, fine=0), delta(env=50, a=1, d=600), flt(8000, 0, 0), amp(d=2500, s=0, r=1200)))
    add("Vibraphone masqué", KEYS, mix(osc(FULL, HALF, 0, 0, m2=30, semi=24, fine=0), flt(6000, 0, 0), amp(d=3000, s=0, r=1500)))
    add("Piano électrique à bits", KEYS, mix(osc(FULL, QUART, 1, 2, m2=40, fine=5), delta(env=60, a=1, d=500), flt(4000, 10, 20, d=600), amp(d=2500, s=0, r=500)))
    add("Accords numériques 8", KEYS, mix(osc(ALT, FULL, 1, 1, m2=50, fine=9), flt(2200, 25, 35, d=300), amp(d=600, s=30, r=200)))
    add("Clavecin masqué", KEYS, mix(osc(DENSE, EDGE, 4, 1), flt(7000, 10, 10), amp(d=1500, s=0, r=300)))
    add("Marimba masqué", KEYS, mix(osc(FULL, HALF, 0, 1), delta(env=100, a=1, d=100), flt(4000, 5, 20, d=150), amp(d=600, s=0, r=300)))

    # --- Cordes
    ST = dict(spread=80)
    add("Cordes masquées", STR, mix(osc(DENSE, FULL, 3, 1, m2=70, a2=DENSE, fine=12), delta(env=30, a=1500, d=2000, loop=1), flt(4000, 10, 10, a=500), amp(a=500, d=2000, s=90, r=1500), ST))
    add("Cordes lentes 8", STR, mix(osc(DENSE, FULL, 3, 1, m2=70, a2=DENSE, fine=10), flt(3000, 10, 10), amp(a=1500, d=3000, s=90, r=2500), ST))
    add("Violons à bits", STR, mix(osc(DENSE, EDGE, 4, 1, m2=60, a2=DENSE, fine=8, semi=12), flt(5000, 10, 10), amp(a=350, d=2000, s=90, r=900), ST))
    add("Violoncelles masqués", STR, mix(osc(DENSE, FULL, 3, 1, m2=60, a2=DENSE, fine=8, semi=-12), flt(2500, 10, 10), amp(a=400, d=2000, s=90, r=900), ST))
    add("Pizzicato masqué", STR, mix(osc(DENSE, FULL, 3, 1, m2=50, a2=DENSE, fine=8), flt(2500, 10, 30, d=150), amp(d=300, s=0, r=250), ST))
    add("Cordes brillantes 8", STR, mix(osc(DENSE, SPARSE, 5, 1, m2=60, a2=DENSE, fine=12), flt(7000, 10, 10), amp(a=400, d=2000, s=90, r=1200), ST))
    add("Cordes sombres masquées", STR, mix(osc(DENSE, FULL, 2, 1, m2=60, a2=DENSE, fine=10), flt(1500, 10, 10), amp(a=700, d=3000, s=90, r=1500), ST))
    add("Trémolo masqué", STR, mix(osc(DENSE, HALF, 3, 1, m2=60, a2=DENSE, fine=10), delta(env=60, a=60, d=60, loop=1), flt(4000, 10, 10), amp(a=300, d=2000, s=90, r=1000), ST))
    add("Ensemble masqué", STR, mix(osc(DENSE, FULL, 3, 1, m2=80, a2=EDGE, fine=15), delta(env=30, a=2500, d=2500, loop=1), flt(3500, 10, 10), amp(a=800, d=3000, s=90, r=2000), ST, spread=100))
    add("Cordes qui gonflent 8", STR, mix(osc(DENSE, FULL, 3, 1, m2=70, a2=DENSE, fine=12), flt(800, 15, 60, a=3000, d=4000, s=80), amp(a=2000, d=3000, s=95, r=2500), ST))

    # --- Cuivres
    BR = dict(spread=40)
    add("Cuivres masqués", BRASS, mix(osc(DENSE, FULL, 2, 1, m2=50, a2=DENSE, fine=8), flt(900, 15, 55, a=80, d=700, s=45), amp(a=40, d=1200, s=85, r=300), BR))
    add("Trompette à bits", BRASS, mix(osc(DENSE, EDGE, 3, 1), flt(1300, 20, 50, a=60, d=500, s=55), amp(a=30, d=1200, s=85, r=200), LEG, glide=25))
    add("Section masquée", BRASS, mix(osc(DENSE, FULL, 2, 1, m2=70, a2=DENSE, fine=14), flt(1000, 15, 50, a=100, d=800, s=50), amp(a=60, d=1200, s=85, r=350), BR, spread=70))
    add("Trombone masqué", BRASS, mix(osc(DENSE, FULL, 2, 1, m2=50, a2=DENSE, fine=6, semi=-12), flt(700, 15, 45, a=100, d=700, s=50), amp(a=60, d=1200, s=85, r=300), BR))
    add("Cuivres staccato 8", BRASS, mix(osc(DENSE, FULL, 2, 1, m2=50, a2=DENSE, fine=10), flt(900, 15, 55, a=30, d=250, s=20), amp(a=15, d=300, s=20, r=150), BR))
    add("Cor masqué", BRASS, mix(osc(FULL, DENSE, 1, 0, m2=40, fine=5), flt(800, 5, 35, a=150, d=900, s=50), amp(a=100, d=1500, s=85, r=500), BR))
    add("Cuivres qui grognent 8", BRASS, mix(osc(DENSE, EDGE, 2, 1, m2=60, a2=DENSE, fine=9), flt(600, 35, 60, a=80, d=700, s=45), amp(a=40, d=1200, s=85, r=300), BR))
    add("Saxo masqué", BRASS, mix(osc(EDGE, DENSE, 2, 2), delta(env=40, a=200, d=800), flt(1300, 30, 35, d=800, s=60), amp(a=30, d=1200, s=90, r=200), LEG, glide=35))
    add("Cuivres doux 8", BRASS, mix(osc(DENSE, FULL, 1, 1, m2=50, a2=DENSE, fine=8), flt(600, 5, 40, a=200, d=1000, s=50), amp(a=150, d=1500, s=85, r=500), BR))
    add("Fanfare masquée", BRASS, mix(osc(DENSE, FULL, 3, 1, m2=60, a2=DENSE, fine=10, semi=12), flt(1000, 15, 50, a=70, d=700, s=50), amp(a=40, d=1200, s=85, r=300), BR))

    # --- Arpèges
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège masqué", ARP, mix(osc(DENSE, FULL, 2, 1), delta(env=100, a=1, d=150), flt(4000, 20, 30, d=150), amp(d=300, s=0, r=200), AR))
    add("Arpège à bits", ARP, mix(osc(ALT, SPARSE, 3, 0), flt(5000, 15, 20, d=150), amp(d=300, s=0, r=200), AR, arp_mode=3))
    add("Arpège redressé", ARP, mix(osc(ALT, FULL, 1, 2), flt(3000, 25, 40, d=150), amp(d=250, s=0, r=150), AR, arp_mode=2))
    add("Arpège en escalier", ARP, mix(osc(STEP, FULL, 2, 0), delta(env=60, a=1, d=200), flt(3500, 20, 30, d=150), amp(d=300, s=0, r=200), AR, arp_rate=4))
    add("Arpège de verre 8", ARP, mix(osc(FULL, SPARSE, 4, 1, m2=30, semi=12, fine=0), flt(7000, 10, 10), amp(d=500, s=0, r=400), AR, arp_mode=3))
    add("Arpège basse masqué", ARP, mix(osc(FULL, HALF, 0, 1), flt(1200, 35, 50, d=180), amp(d=300, s=30, r=100), AR, arp_oct=1))
    add("Arpège aléatoire 8", ARP, mix(osc(DENSE, EDGE, 3, 1), delta(env=80, a=1, d=150), flt(4500, 20, 20, d=150), amp(d=300, s=0, r=200), AR, arp_mode=6, arp_oct=3))
    add("Arpège court 8", ARP, mix(osc(QUART, FULL, 1, 0), flt(3500, 20, 30, d=100), amp(d=150, s=0, r=100), AR, arp_gate=25, arp_rate=5))
    add("Arpège octaves 8", ARP, mix(osc(FULL, ALT, 1, 1), flt(4000, 15, 25, d=200), amp(d=350, s=0, r=250), AR, arp_oct=3))
    add("Arpège qui boucle", ARP, mix(osc(FULL, DENSE, 2, 1), delta(env=70, a=150, d=150, loop=1), flt(3000, 25, 20), amp(d=400, s=20, r=300), AR, arp_rate=1))

    # --- Effets
    add("Sirène à bits", FX, mix(osc(FULL, ALT, 2, 1), delta(env=100, a=800, d=800, loop=1), flt(5000, 20, 0), amp(d=3000, s=100, r=600), pamt=12, pdec=2000))
    add("Radio perdue", FX, mix(osc(SPARSE, DENSE, 5, 1), delta(env=100, a=300, d=500, loop=1), flt(1500, 60, 0, kind=1), amp(a=200, d=3000, s=90, r=2000), noise=40, noisehz=800))
    add("Laser masqué", FX, mix(osc(FULL, EDGE, 3, 1), flt(8000, 30, -40, d=400), amp(d=600, s=0, r=300), pamt=24, pdec=250))
    add("Goutte masquée", FX, mix(osc(FULL, FULL, 0, 0), flt(3000, 50, 40, d=150), amp(d=500, s=0, r=400), pamt=12, pdec=60))
    add("Machine qui pense", FX, mix(osc(STEP, SPARSE, 4, 0, m2=40, semi=7, fine=0), delta(env=100, a=40, d=70, loop=1), flt(3500, 40, 0, kind=1), amp(d=3000, s=90, r=800)))
    add("Vent numérique", FX, mix(osc(m1=0), flt(1500, 70, 0, kind=1), amp(a=1500, d=3000, s=90, r=3000), noise=100, noisehz=5000, spread=100))
    add("Cloche à bits", FX, mix(osc(FULL, EDGE, 4, 1, m2=50, semi=6, fine=20), delta(env=60, a=1, d=1500), flt(7000, 0, 0), amp(d=4000, s=0, r=3000)))
    add("Moteur masqué", FX, mix(osc(DENSE, SPARSE, 0, 1, m2=60, semi=-12, fine=40), delta(env=100, a=30, d=30, loop=1), flt(900, 40, 0), amp(d=3000, s=90, r=500)))
    add("Glitch", FX, mix(osc(SPARSE, DENSE, 5, 1), delta(env=100, a=20, d=50, loop=1), flt(6000, 30, 0, kind=2), amp(d=800, s=40, r=300), noise=20, noisehz=1200))
    add("Dragon qui dort", FX, mix(osc(FULL, ALT, 0, 2, m2=50, semi=-12, fine=0), delta(env=100, a=4000, d=6000, loop=1), flt(800, 30, 20, a=4000, d=5000, s=60), amp(a=3000, d=4000, s=90, r=4000), spread=90))
    return b
