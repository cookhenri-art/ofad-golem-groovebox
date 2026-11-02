# New Metal (FM à 4 opérateurs C, A, B1, B2, 8 algorithmes à deux sorties X / Y) : 10 sons par catégorie.
# Réglages originaux (aucun son d'origine recopié). Rappel des algorithmes :
#   1 : A→C (X) · B2→B1 (Y), rétroaction sur A      2 : pile B2→B1→A→C, rétroaction sur B2
#   3 : A et B1 → C, B2→B1                          4 : B2 → A et B1, A→C
#   5 : comme 3, sortie Y = B1                      6 : B2 → C, A, B1 (Y = A + B1)
#   7 : B1→C, B2→A                                  8 : C et A seuls, B2→B1 (Y = A + B1)
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

RC = [0.25, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]
RAB = [0.25, 0.5, 0.75] + [1.0 + 0.5 * i for i in range(31)]


def rc(r):
    return RC.index(r)


def rab(r):
    return RAB.index(r)


def fm(algo, c=1, a=1, b1=1, b2=1, la=60, lb=0, **kw):
    """Cœur FM : algorithme, rapports (valeurs réelles), niveaux de modulation A / B (0..127)."""
    return mix(dict(algo=algo, ratc=rc(c), rata=rab(a), ratb1=rab(b1), ratb2=rab(b2), leva=la, levb=lb), kw)


def envA(atk=0, dec=70, end=0, gated=0):
    return dict(aatk=atk, adec=dec, aend=end, amode=gated)


def envB(atk=0, dec=70, end=0, gated=0):
    return dict(batk=atk, bdec=dec, bend=end, bmode=gated)


def amp(a=1, d=600, s=80, r=250):
    return dict(aa=a, ad=d, as_=s, ar=r)


def filt(freq=18000, res=0, env=0, a=2, d=400, s=0, r=300, kind=0, **kw):
    return mix(dict(ftype=kind, ffreq=freq, fres=res, fenv=env, fa=a, fd=d, fs=s, fr=r), kw)


MONO = dict(vmode=1, voices=1)
LEG = dict(vmode=2, voices=1)


def build():
    b = Bank("fm4")
    add = b.add

    # --- Basses
    add("Basse FM sourde", BASS, mix(fm(1, c=0.5, a=0.5, la=62), envA(dec=55, end=20), amp(d=900, s=70, r=120), filt(900, 15, 40, d=250), MONO, tune=0))
    add("Basse à clic", BASS, mix(fm(1, c=0.5, a=1, la=88, feedback=25), envA(dec=30, end=8), amp(d=700, s=60, r=100), filt(2200, 10, 30, d=180), MONO))
    add("Basse de tôle", BASS, mix(fm(2, c=0.5, a=1, b1=2, b2=1, la=70, lb=45, feedback=35), envA(dec=50, end=15), envB(dec=40, end=10), amp(d=800, s=75, r=120),
                                   filt(1600, 25, 45, d=300), LEG, glide=40, drive=20))
    add("Basse ronde FM", BASS, mix(fm(8, c=0.5, a=0.5, b1=1, b2=1, la=0, lb=20, mix=-40), envB(dec=60, end=10), amp(d=1200, s=85, r=150), filt(700, 10, 25, d=300), MONO))
    add("Basse pincée FM", BASS, mix(fm(1, c=0.5, a=3, la=70), envA(dec=35, end=0), amp(d=500, s=30, r=150), filt(3000, 0, 20, d=200), MONO))
    add("Basse grondante", BASS, mix(fm(4, c=0.5, a=0.5, b1=1, b2=0.5, la=60, lb=55, feedback=50), envA(dec=80, end=40), envB(dec=70, end=30), amp(d=1500, s=85, r=180),
                                     filt(1200, 30, 35, d=500, kind=1), MONO, drive=35))
    add("Basse sub et grain", BASS, mix(fm(5, c=0.5, a=1, b1=1, b2=2, la=30, lb=60, mix=-20), envA(dec=50, end=10), envB(dec=35, end=0), amp(d=900, s=75, r=120),
                                        filt(2500, 10, 30, d=250), MONO))
    add("Basse acide FM", BASS, mix(fm(1, c=0.5, a=1, la=75, feedback=40), envA(dec=40, end=20), amp(d=400, s=60, r=80),
                                    filt(900, 55, 60, d=220, kind=1), LEG, glide=70, drive=40))
    add("Basse bois FM", BASS, mix(fm(7, c=0.5, a=1, b1=2, b2=3, la=50, lb=55, mix=-30), envA(dec=30), envB(dec=25), amp(d=600, s=40, r=120), filt(4000), MONO))
    add("Basse de fer 2", BASS, mix(fm(6, c=0.5, a=1, b1=2, b2=1, la=0, lb=75, feedback=45, mix=-50), envB(dec=45, end=12), amp(d=900, s=70, r=120),
                                    filt(1800, 20, 50, d=260), LEG, glide=30, drive=25))

    # --- Leads
    LD = mix(LEG, glide=45)
    add("Lead cuivre vif", LEAD, mix(fm(2, c=1, a=1, b1=1, b2=1, la=72, lb=40, feedback=40), envA(atk=40, dec=90, end=80), amp(d=1500, s=85, r=200), filt(6000, 10, 20, d=500), LD))
    add("Lead carré FM", LEAD, mix(fm(1, c=1, a=2, la=70), envA(dec=127, end=110, gated=1), amp(d=900, s=90, r=180), filt(7000), LD, detune=15))
    add("Lead verre FM", LEAD, mix(fm(1, c=2, a=3.5, b1=4, b2=7, la=40, lb=30, mix=-10), envA(dec=80, end=50), envB(dec=60, end=20), amp(d=900, s=80, r=250), LD))
    add("Lead hurlant FM", LEAD, mix(fm(2, c=1, a=1, b1=1, b2=1, la=95, lb=70, feedback=70), envA(dec=127, end=115, gated=1), envB(dec=127, end=100, gated=1),
                                     amp(d=900, s=90, r=200), filt(5000, 20, 15, d=600), LD, drive=45))
    add("Lead flûte FM", LEAD, mix(fm(1, c=1, a=1, la=25, lb=0), envA(atk=60, dec=127, end=90, gated=1), amp(a=40, d=900, s=90, r=200), LD, lf1_rate=5.2,
                                   mx1_src=6, **{"mx1_dst@tune": 0}, mx1_amt=3))
    add("Lead synchro FM", LEAD, mix(fm(1, c=1, a=3, la=80, feedback=20), envA(atk=20, dec=60, end=40), amp(d=800, s=85, r=200), filt(8000, 15, 10), LD))
    add("Lead octaves FM", LEAD, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=40, mix=0), envB(dec=100, end=70), amp(d=900, s=85, r=200), LD, detune=20))
    add("Lead nasillard", LEAD, mix(fm(3, c=1, a=3, b1=5, b2=1, la=50, lb=45), envA(dec=127, end=100, gated=1), envB(dec=127, end=90, gated=1), amp(d=900, s=85, r=200),
                                    filt(3000, 40, 0, fmorph=40), LD))
    add("Lead sifflant", LEAD, mix(fm(8, c=2, a=4, b1=1, b2=1, la=0, lb=10, mix=-30), envB(dec=60), amp(a=20, d=900, s=90, r=250), LD, glide=90))
    add("Lead mordant", LEAD, mix(fm(1, c=1, a=1, la=85, feedback=55), envA(dec=45, end=60), amp(d=900, s=80, r=180), filt(4500, 35, 30, d=250, kind=1), LD, drive=50))

    # --- Nappes
    NP = dict(spread=75, detune=35, harm=0)
    add("Nappe de cristal", PAD, mix(fm(1, c=1, a=3, b1=2, b2=5, la=35, lb=30, mix=0), envA(atk=80, dec=127, end=60), envB(atk=100, dec=127, end=50),
                                     amp(a=900, d=3000, s=85, r=2500), filt(7000), NP))
    add("Nappe tiède FM", PAD, mix(fm(6, c=1, a=1, b1=2, b2=1, la=0, lb=45, mix=-10), envB(atk=90, dec=127, end=70), amp(a=1200, d=3000, s=90, r=2800),
                                   filt(3500, 10, 15, a=1500, d=3000, s=60), NP))
    add("Nappe qui s'ouvre", PAD, mix(fm(2, c=1, a=1, b1=1, b2=2, la=55, lb=40), envA(atk=115, dec=127, end=90, gated=1), envB(atk=110, dec=127, end=80, gated=1),
                                      amp(a=1500, d=3000, s=90, r=3000), filt(1500, 20, 45, a=3000, d=4000, s=70), NP))
    add("Nappe numérique 4", PAD, mix(fm(5, c=1, a=2, b1=1, b2=3, la=40, lb=35, mix=10), envA(atk=70, dec=127, end=70), envB(atk=90, dec=127, end=60),
                                      amp(a=700, d=3000, s=85, r=2500), NP, harm=3))
    add("Nappe sombre FM", PAD, mix(fm(1, c=0.5, a=1, b1=1, b2=1, la=40, lb=35, mix=0), envA(atk=90, dec=127, end=60), envB(atk=90, dec=127, end=60),
                                    amp(a=1500, d=3000, s=90, r=3000), filt(1800, 15, 0), NP))
    add("Nappe de verre filé", PAD, mix(fm(8, c=2, a=3, b1=1, b2=4, la=0, lb=25, mix=20), envB(atk=100, dec=127, end=70), amp(a=1200, d=3000, s=85, r=3000), NP,
                                        mx1_src=6, **{"mx1_dst@mix": 0}, mx1_amt=30, lf1_rate=0.25))
    add("Nappe chorale FM", PAD, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=35, mix=-5, feedback=20), envB(atk=100, dec=127, end=80), amp(a=1000, d=3000, s=90, r=2500),
                                     filt(3000, 30, 0, fmorph=35), NP, spread=90, detune=50))
    add("Nappe métallique 4", PAD, mix(fm(4, c=1, a=3.5, b1=5, b2=1.5, la=35, lb=30, offa=0.05), envA(atk=80, dec=127, end=50), envB(atk=80, dec=127, end=45),
                                       amp(a=900, d=3000, s=80, r=3000), filt(6000), NP))
    add("Nappe respirante", PAD, mix(fm(1, c=1, a=1, b1=1, b2=2, la=45, lb=40, mix=0), envA(atk=80, dec=127, end=70), amp(a=1200, d=3000, s=90, r=2800),
                                     filt(2500, 20, 0), NP, mx1_src=6, **{"mx1_dst@ffreq": 0}, mx1_amt=20, lf1_rate=0.2))
    add("Nappe lointaine", PAD, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=15, mix=-20), envB(atk=110, dec=127, end=60), amp(a=2500, d=4000, s=90, r=4000),
                                    filt(2200), NP, base=150, width=4))

    # --- Plucks
    PK = amp(d=500, s=0, r=300)
    add("Pluck FM sec", PLUCK, mix(fm(1, c=1, a=1, la=70), envA(dec=30), PK))
    add("Pluck en verre", PLUCK, mix(fm(1, c=1, a=3.5, b1=5, b2=2, la=55, lb=40, mix=-10), envA(dec=35), envB(dec=30), PK))
    add("Pluck creux FM", PLUCK, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=50, mix=-20), envB(dec=30), amp(d=900, s=0, r=400), filt(6000, 15, 0, fmorph=15)))
    add("Pizz numérique 4", PLUCK, mix(fm(2, c=1, a=1, b1=2, b2=1, la=60, lb=40), envA(dec=25), envB(dec=20), amp(d=300, s=0, r=200), filt(5000, 10, 30, d=200)))
    add("Koto de fer", PLUCK, mix(fm(1, c=1, a=3, b1=1, b2=5, la=70, lb=40, feedback=30), envA(dec=40), envB(dec=35), amp(d=1200, s=0, r=600)))
    add("Pluck à la quinte", PLUCK, mix(fm(5, c=1, a=1.5, b1=3, b2=1, la=50, lb=45, mix=10), envA(dec=35), envB(dec=30), PK))
    add("Pluck sourd FM", PLUCK, mix(fm(1, c=1, a=1, la=45), envA(dec=25), PK, filt(1800, 20, 50, d=200)))
    add("Steel FM", PLUCK, mix(fm(1, c=1, a=2.5, b1=4, b2=1, la=55, lb=30, mix=-20), envA(dec=45), envB(dec=50), amp(d=1500, s=0, r=800)))
    add("Pluck bouclé", PLUCK, mix(fm(2, c=1, a=1, b1=3, b2=1, la=60, lb=50, feedback=25), envA(dec=35), envB(dec=30), amp(d=700, s=0, r=400), filt(4000, 35, 25, d=300, kind=1)))
    add("Harpe FM", PLUCK, mix(fm(1, c=1, a=2, la=40), envA(dec=50), amp(d=2500, s=0, r=1200), spread=40))

    # --- Claviers
    add("Piano FM clair", KEYS, mix(fm(1, c=1, a=14, b1=1, b2=1, la=45, lb=60, mix=-10), envA(dec=50), envB(dec=75, end=10), amp(d=2400, s=0, r=700), velmod=55))
    add("Piano FM doux", KEYS, mix(fm(1, c=1, a=1, b1=1, b2=14, la=40, lb=35, mix=10), envA(dec=60, end=5), envB(dec=45), amp(d=2800, s=0, r=800), velmod=70, detune=12))
    add("Clavinette 4", KEYS, mix(fm(3, c=1, a=3, b1=1, b2=1, la=60, lb=55, feedback=20), envA(dec=35, end=10), envB(dec=30), amp(d=900, s=40, r=120), filt(5000, 30, 30, d=200)))
    add("Orgue FM rond", KEYS, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=0, mix=-15), amp(a=4, d=500, s=100, r=120), harm=-4))
    add("Orgue percussif 4", KEYS, mix(fm(8, c=1, a=3, b1=2, b2=1, la=0, lb=0, mix=0), amp(a=4, d=500, s=100, r=120), harm=-6, offa=0))
    add("Vibraphone de fer", KEYS, mix(fm(1, c=1, a=4, b1=1, b2=1, la=35, lb=0, mix=-40), envA(dec=40), amp(d=3500, s=0, r=1500), velmod=60,
                                       mx1_src=6, **{"mx1_dst@level": 0}, mx1_amt=8, lf1_rate=5.5))
    add("Célesta FM 4", KEYS, mix(fm(1, c=2, a=7, b1=4, b2=3, la=35, lb=30, mix=0), envA(dec=45), envB(dec=50), amp(d=2500, s=0, r=1200)))
    add("Clavecin de fer", KEYS, mix(fm(2, c=1, a=3, b1=5, b2=1, la=55, lb=40, feedback=20), envA(dec=40, end=10), envB(dec=35), amp(d=1500, s=0, r=300), filt(7000)))
    add("Piano électrique dur 4", KEYS, mix(fm(1, c=1, a=1, b1=1, b2=14, la=60, lb=55, mix=0, feedback=10), envA(dec=60, end=5), envB(dec=40), amp(d=2500, s=0, r=600), velmod=75))
    add("Marimba FM", KEYS, mix(fm(1, c=1, a=4, la=55), envA(dec=20), amp(d=600, s=0, r=300)))

    # --- Cordes
    ST = dict(spread=80, detune=45)
    add("Cordes de fer", STR, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=45, feedback=55, mix=10), envB(atk=80, dec=127, end=60), amp(a=500, d=2000, s=90, r=1500), filt(4000), ST))
    add("Violons FM 4", STR, mix(fm(2, c=1, a=1, b1=1, b2=1, la=50, lb=30, feedback=45), envA(atk=60, dec=127, end=80), amp(a=350, d=2000, s=90, r=900), filt(5000, 10, 10, a=500), ST,
                                 mx1_src=6, **{"mx1_dst@tune": 0}, mx1_amt=2, lf1_rate=5.5))
    add("Violoncelles FM 4", STR, mix(fm(2, c=1, a=1, b1=1, b2=1, la=55, lb=30, feedback=50), envA(atk=60, dec=127, end=80), amp(a=400, d=2000, s=90, r=900), filt(2500), ST, tune=-12))
    add("Ensemble lent", STR, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=40, feedback=50, mix=0), envB(atk=100, dec=127, end=70), amp(a=1500, d=3000, s=90, r=2500), filt(3000), ST))
    add("Cordes brillantes 4", STR, mix(fm(6, c=1, a=1, b1=2, b2=1, la=0, lb=55, feedback=60, mix=15), envB(atk=70, dec=127, end=70), amp(a=400, d=2000, s=90, r=1200), filt(8000), ST))
    add("Cordes sombres 4", STR, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=35, feedback=40, mix=0), envB(atk=80, dec=127, end=60), amp(a=600, d=2000, s=90, r=1500), filt(1800, 10), ST))
    add("Pizzicato FM", STR, mix(fm(2, c=1, a=1, b1=1, b2=1, la=50, lb=25, feedback=35), envA(dec=25), amp(d=350, s=0, r=250), filt(3500), ST))
    add("Trémolo FM", STR, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=45, feedback=50, mix=10), envB(atk=60, dec=127, end=70), amp(a=300, d=2000, s=90, r=1200), filt(4500), ST,
                               mx1_src=6, **{"mx1_dst@level": 0}, mx1_amt=20, lf1_rate=7))
    add("Cordes octaviées", STR, mix(fm(6, c=1, a=2, b1=1, b2=1, la=0, lb=45, feedback=50, mix=0), envB(atk=80, dec=127, end=70), amp(a=600, d=2000, s=90, r=1500), filt(4000), ST))
    add("Cordes qui gonflent", STR, mix(fm(6, c=1, a=1, b1=1, b2=1, la=0, lb=45, feedback=55, mix=10), envB(atk=120, dec=127, end=90, gated=1), amp(a=2000, d=3000, s=95, r=2000),
                                        filt(1200, 15, 50, a=3000, d=4000, s=80), ST))

    # --- Cuivres
    BR = dict(detune=20, spread=40)
    add("Cuivres FM 4", BRASS, mix(fm(2, c=1, a=1, b1=1, b2=1, la=72, lb=35, feedback=30), envA(atk=50, dec=85, end=70), amp(a=40, d=1200, s=85, r=300),
                                   filt(3500, 0, 35, a=60, d=500, s=50), BR))
    add("Trompette de fer", BRASS, mix(fm(2, c=1, a=1, b1=1, b2=1, la=78, lb=40, feedback=40), envA(atk=45, dec=80, end=75), amp(a=30, d=1200, s=85, r=200),
                                       filt(4500, 10, 35, a=50, d=400, s=60), LEG, glide=25))
    add("Section FM", BRASS, mix(fm(1, c=1, a=1, b1=1, b2=1, la=70, lb=65, mix=0, feedback=30), envA(atk=50, dec=90, end=70), envB(atk=55, dec=90, end=65),
                                 amp(a=45, d=1200, s=85, r=300), filt(4000, 0, 30, a=60, d=500, s=60), BR, detune=35, spread=70))
    add("Trombone FM 4", BRASS, mix(fm(2, c=1, a=1, b1=1, b2=1, la=70, lb=30, feedback=30), envA(atk=55, dec=85, end=70), amp(a=50, d=1200, s=85, r=300), filt(2500, 0, 30, a=80, d=500, s=60), BR, tune=-12))
    add("Cuivres staccato 4", BRASS, mix(fm(2, c=1, a=1, b1=1, b2=1, la=75, lb=35, feedback=35), envA(atk=30, dec=50, end=40), amp(a=20, d=350, s=20, r=150),
                                         filt(4000, 0, 35, a=20, d=250), BR))
    add("Cor FM 4", BRASS, mix(fm(1, c=1, a=1, la=45, lb=0), envA(atk=60, dec=90, end=60), amp(a=80, d=1500, s=85, r=400), filt(1800, 0, 20, a=100, d=600, s=50), BR))
    add("Saxo FM", BRASS, mix(fm(3, c=1, a=1, b1=3, b2=1, la=60, lb=40, feedback=30), envA(atk=40, dec=127, end=90, gated=1), envB(atk=40, dec=127, end=80, gated=1),
                              amp(a=30, d=1500, s=90, r=200), filt(3000, 20, 15, d=500), LEG, glide=30, drive=20))
    add("Cuivres synthé 4", BRASS, mix(fm(1, c=1, a=1, b1=1, b2=1, la=80, lb=75, mix=0, feedback=50), envA(atk=40, dec=90, end=80), envB(atk=45, dec=90, end=75),
                                       amp(a=30, d=1200, s=90, r=300), filt(2500, 25, 45, a=80, d=600, s=50, kind=1), BR, detune=40, spread=70))
    add("Fanfare de fer", BRASS, mix(fm(5, c=1, a=1, b1=2, b2=1, la=70, lb=60, mix=0, feedback=40), envA(atk=45, dec=85, end=75), envB(atk=45, dec=85, end=70),
                                     amp(a=35, d=1200, s=85, r=300), filt(5000, 0, 25, a=50, d=500, s=60), BR))
    add("Cuivres doux 4", BRASS, mix(fm(2, c=1, a=1, b1=1, b2=1, la=55, lb=25, feedback=20), envA(atk=65, dec=90, end=60), amp(a=90, d=1500, s=85, r=500),
                                     filt(2200, 0, 25, a=150, d=700, s=60), BR))

    # --- Arpèges
    AR = dict(arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45)
    add("Arpège de fer", ARP, mix(fm(1, c=1, a=2, la=60), envA(dec=30), amp(d=300, s=0, r=200), AR))
    add("Arpège verre 4", ARP, mix(fm(1, c=2, a=3.5, b1=5, b2=2, la=45, lb=35, mix=0), envA(dec=35), envB(dec=35), amp(d=400, s=0, r=300), AR, arp_mode=3))
    add("Arpège rond 4", ARP, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=40, mix=-20), envB(dec=30), amp(d=350, s=0, r=250), AR, arp_mode=2))
    add("Arpège claquant 4", ARP, mix(fm(2, c=1, a=1, b1=2, b2=1, la=70, lb=45, feedback=25), envA(dec=25), envB(dec=20), amp(d=250, s=0, r=150), AR, arp_rate=4))
    add("Arpège en cloches", ARP, mix(fm(1, c=1, a=3.5, b1=7, b2=2, la=45, lb=30, mix=0), envA(dec=50), envB(dec=50), amp(d=1500, s=0, r=800), AR, arp_rate=1, arp_oct=3))
    add("Arpège filtré 4", ARP, mix(fm(1, c=1, a=1, la=70, feedback=30), envA(dec=40, end=20), amp(d=300, s=20, r=150), filt(1200, 50, 50, d=180, kind=1), AR, arp_mode=5))
    add("Arpège aléatoire 4", ARP, mix(fm(5, c=1, a=1.5, b1=3, b2=1, la=45, lb=40, mix=0), envA(dec=35), envB(dec=30), amp(d=300, s=0, r=200), AR, arp_mode=6, arp_oct=3))
    add("Arpège basse 4", ARP, mix(fm(1, c=0.5, a=1, la=70), envA(dec=35, end=10), amp(d=300, s=30, r=100), filt(1500, 30, 40, d=200), AR, arp_oct=1, tune=-12))
    add("Arpège court 4", ARP, mix(fm(1, c=1, a=1, b1=2, b2=1, la=55, lb=35, mix=-10), envA(dec=25), envB(dec=25), amp(d=200, s=0, r=120), AR, arp_gate=25, arp_rate=5))
    add("Arpège octaves 4", ARP, mix(fm(8, c=1, a=2, b1=1, b2=1, la=0, lb=25, mix=0), envB(dec=40), amp(d=400, s=0, r=250), AR, arp_oct=4))

    # --- Effets
    add("Gong de fer", FX, mix(fm(1, c=1, a=3.5, b1=5.5, b2=2.5, la=75, lb=60, offa=0.07, offb=0.11, mix=10), envA(dec=110, end=10), envB(dec=100),
                               amp(d=6000, s=0, r=4000), spread=60))
    add("Tôle qui vibre", FX, mix(fm(2, c=1, a=3.5, b1=7.5, b2=1.5, la=90, lb=80, feedback=90), envA(dec=90), envB(dec=80), amp(d=3000, s=0, r=2000), filt(6000, 30, 0)))
    add("Sirène de fer", FX, mix(fm(1, c=1, a=2, la=60), amp(d=3000, s=100, r=500), mx1_src=6, **{"mx1_dst@tune": 0}, mx1_amt=40, lf1_rate=0.6))
    add("Laser FM 4", FX, mix(fm(1, c=2, a=3, la=90, feedback=50), envA(dec=30), amp(d=600, s=0, r=300), filt(9000, 40, -60, d=400)))
    add("Goutte de métal", FX, mix(fm(1, c=4, a=1, la=60), envA(dec=15), amp(d=250, s=0, r=200), filt(3000, 60, 60, d=120)))
    add("Vent de tôle", FX, mix(fm(2, c=1, a=7.5, b1=11, b2=13, la=110, lb=100, feedback=127), envA(atk=100, dec=127, end=100), envB(atk=100, dec=127, end=100),
                                amp(a=2000, d=3000, s=90, r=3000), filt(2000, 60, 0, fmorph=50), spread=90,
                                mx1_src=6, **{"mx1_dst@ffreq": 0}, mx1_amt=35, lf1_rate=0.15))
    add("Robot de fer", FX, mix(fm(1, c=1, a=1, la=80, feedback=40), envA(dec=127, end=127), amp(d=900, s=90, r=200), filt(2000, 70, 0, fmorph=50),
                                mx1_src=6, **{"mx1_dst@leva": 0}, mx1_amt=40, lf1_rate=7))
    add("Cloche d'alarme", FX, mix(fm(1, c=2, a=5.5, b1=3, b2=7, la=70, lb=60, mix=0), envA(dec=90), envB(dec=85), amp(d=4000, s=0, r=3000)))
    add("Impact FM", FX, mix(fm(2, c=0.5, a=1, b1=3, b2=7, la=110, lb=100, feedback=80), envA(dec=35), envB(dec=25), amp(d=800, s=0, r=400), drive=60, tune=-12))
    add("Ondes de fer", FX, mix(fm(8, c=1, a=1.5, b1=1, b2=1, la=0, lb=40, mix=0), envB(atk=80, dec=127, end=80), amp(a=1500, d=3000, s=90, r=3000), spread=100, detune=90,
                                mx1_src=6, **{"mx1_dst@mix": 0}, mx1_amt=60, lf1_rate=0.3))
    return b
