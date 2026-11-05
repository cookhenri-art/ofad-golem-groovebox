# Wave morph (ondes à positions, mutants, 2 filtres, enveloppes et LFO multiples) : 50 sons.
# Positions : 0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion, 5 orgue, 6 voix A, 7 voix O, 8 cloche,
#             9 cordes, 10 creux, 11 métal, 12 premiers, 13 bande, 14 grain, 15 doux.
# Mutants : 1 FM, 2 synchro, 3 compression, 4 repli, 5 pile d'ondes, 6 phase décalée, 7 harmonique.
# f1mode : 0 PB24, 1 PB12, 2 bande, 3 PH12, 4 PB18, 5 PB6, 6 PH24, 7 voyelle, 8 peigne +, 9 peigne −.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

BASE = dict(w1pos=2, w2pos=9, w2fine=7, mix1=70, mix2=50, f1cutoff=3000, f1reso=15, env1f1=35,
            e1a=3, e1d=700, e1s=30, e1r=600, e2a=3, e2d=900, e2s=80, e2r=700)
BASSE = mix(BASE, vmode=1, glide=25, w1pos=2, w2pos=3, w2semi=-12, w2fine=3, mix2=45, f1cutoff=700, f1reso=20, env1f1=50,
            e1d=280, e1s=10, e2d=600, e2s=85, e2r=150, f1drive=6)
LEAD_ = mix(BASE, vmode=1, glide=40, w1pos=2, w2pos=3, f1cutoff=3200, f1reso=25, env1f1=30, e1d=600, e1s=60, e2s=100, e2r=300)
NAPPE = mix(BASE, w1pos=9, w2pos=6, f1cutoff=2200, f1reso=10, env1f1=20, e1a=1500, e1d=3000, e1s=60, e1r=2500,
            e2a=1000, e2d=2000, e2s=90, e2r=2800, l1pos=2, l1rate=0.15, spread=70)
PLK = mix(BASE, f1cutoff=1100, f1reso=30, env1f1=60, e1a=1, e1d=230, e1s=0, e1r=250, e2a=1, e2d=500, e2s=0, e2r=380)
CLAV = mix(BASE, w1pos=5, w2pos=1, f1cutoff=2600, f1reso=10, env1f1=30, e1a=1, e1d=900, e1s=20, e1r=400, e2a=1, e2d=1500, e2s=35, e2r=450, velamp=50)


def build():
    b = Bank("morph")
    b.add("Init", INIT, dict(w1pos=2, mix1=70, mix2=0, mix3=0, spread=0, f1cutoff=9000, f1reso=0, env1f1=0, keytrack=0, e2a=2, e2d=900, e2s=100, e2r=300))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (6)
    add("Basse morph", BASS, mix(BASSE))
    add("Basse FM mutante", BASS, mix(BASSE, w1pos=0, w1mut=1, w1mutamt=55, w1ratio=1, mix2=0, f1cutoff=1500))
    add("Basse repliée", BASS, mix(BASSE, w1mut=4, w1mutamt=45, f1cutoff=900))
    add("Basse voix O", BASS, mix(BASSE, w1pos=7, w2pos=7, f1mode=7, f1reso=40, f1cutoff=600))
    add("Basse compressée", BASS, mix(BASSE, w1mut=3, w1mutamt=60, w2mut=3, w2mutamt=40, f1cutoff=650))
    add("Basse unisson", BASS, mix(BASSE, vmode=2, univoices=4, unidetune=10, f1cutoff=800))

    # --- Leads (6)
    add("Lead morph", LEAD, mix(LEAD_))
    add("Lead harmonique", LEAD, mix(LEAD_, w1mut=7, w1mutamt=45, w1ratio=3, f1cutoff=4000))
    add("Lead synchro mutant", LEAD, mix(LEAD_, w1mut=2, w1mutamt=55, l1pos=1, l1rate=0.4, mix2=30))
    add("Lead voix", LEAD, mix(LEAD_, w1pos=6, w2pos=7, f1mode=7, f1reso=45, l2pitch=10, l2rate=5.5))
    add("Lead unisson épais", LEAD, mix(LEAD_, vmode=2, univoices=6, unidetune=16, f1cutoff=3500))
    add("Lead métal", LEAD, mix(LEAD_, w1pos=11, w2pos=8, mixring=30, f1cutoff=5000))

    # --- Nappes (9)
    add("Nappe morph", PAD, mix(NAPPE))
    add("Nappe chorale", PAD, mix(NAPPE, w1pos=6, w2pos=7, l1pos=3, l1rate=0.1))
    add("Nappe pile d'ondes", PAD, mix(NAPPE, w1mut=5, w1mutamt=70, w2mut=5, w2mutamt=50))
    add("Nappe balayée", PAD, mix(NAPPE, env3pos=6, e3a=2500, e3d=4000, e3s=60))
    add("Nappe phase", PAD, mix(NAPPE, w1mut=6, w1mutamt=60, w1pos=2, w2pos=2, w2fine=12))
    add("Nappe peigne", PAD, mix(NAPPE, f1mode=8, f1cutoff=300, f1reso=60, mixnoise=10))
    add("Nappe parallèle", PAD, mix(NAPPE, routing=1, f2morph=50, f2cutoff=2000, f2reso=30, env1f2=20))
    add("Nappe sombre grain", PAD, mix(NAPPE, w1pos=14, w2pos=15, f1cutoff=1000, f1reso=20))
    add("Nappe cloches lentes", PAD, mix(NAPPE, w1pos=8, w2pos=12, mixring=15, f1cutoff=4000))

    # --- Plucks (6)
    add("Pluck morph", PLUCK, mix(PLK))
    add("Pluck FM", PLUCK, mix(PLK, w1pos=0, w1mut=1, w1mutamt=60, w1ratio=3, mix2=0, f1cutoff=3500))
    add("Pluck cloche", PLUCK, mix(PLK, w1pos=8, w2pos=11, mixring=25, f1cutoff=4000, e2d=900, e2r=700))
    add("Pluck creux", PLUCK, mix(PLK, w1pos=10, w2pos=13, f1reso=45))
    add("Pluck peigne", PLUCK, mix(PLK, f1mode=8, f1cutoff=500, f1reso=70, mixnoise=30))
    add("Pluck balayé", PLUCK, mix(PLK, env3pos=-6, e3a=1, e3d=250, e3s=0))

    # --- Claviers (6)
    add("Orgue morph", KEYS, mix(CLAV, w1pos=5, w2pos=5, w2semi=12, env1f1=0, f1cutoff=7000, e2a=3, e2d=100, e2s=100, e2r=80))
    add("Piano de verre", KEYS, mix(CLAV, w1pos=8, w2pos=0, w2semi=12, mix2=40))
    add("Clavier électrique", KEYS, mix(CLAV, w1pos=1, w1mut=1, w1mutamt=25, w1ratio=1, w2pos=0, e2s=15))
    add("Clavier harmonique", KEYS, mix(CLAV, w1mut=7, w1mutamt=30, w1ratio=2))
    add("Clavecin numérique", KEYS, mix(CLAV, w1pos=4, w2pos=3, mix1=85, mix2=60, f1mode=3, f1cutoff=250, env1f1=0, e2d=900, e2s=0, e2r=200))
    add("Accords stab", KEYS, mix(CLAV, w1pos=2, w2pos=3, f1cutoff=1300, f1reso=30, env1f1=55, e1d=250, e1s=0, e2d=350, e2s=0, e2r=300))

    # --- Cordes (3)
    CORD = mix(NAPPE, w1pos=9, w2pos=2, e2a=400, e2r=1300, e1a=300, f1cutoff=4000, f1reso=5, l2pitch=6, l2rate=5)
    add("Cordes morph", STR, mix(CORD))
    add("Cordes pile", STR, mix(CORD, w1mut=5, w1mutamt=50))
    add("Cordes pincées", STR, mix(CORD, e2a=1, e2d=400, e2s=0, e2r=400, e1a=1, e1d=300, e1s=0, env1f1=40))

    # --- Cuivres (3)
    CUIV = mix(BASE, w1pos=2, w2pos=2, f1cutoff=1100, env1f1=55, e1a=50, e1d=600, e1s=50, e2a=25, e2d=700, e2s=85, e2r=300)
    add("Cuivres morph", BRASS, mix(CUIV))
    add("Cuivres voyelle", BRASS, mix(CUIV, f1mode=7, f1reso=35))
    add("Stab cuivré", BRASS, mix(CUIV, e1d=250, e1s=0, e2d=300, e2s=0, e2r=200))

    # --- Arpèges (5)
    ARPP = mix(PLK, arp_gate=50, arp_rate=3)
    add("Arpège morph", ARP, mix(ARPP, arp_mode=1, arp_oct=2))
    add("Arpège à pas", ARP, mix(ARPP, arp_mode=1, arp_oct=1, arp_steps=8, arp_t3=12, arp_t5=7, arp_t7=-5, arp_v2=60, arp_v6=70))
    add("Arpège cloches", ARP, mix(ARPP, w1pos=8, w2pos=11, arp_mode=11, arp_oct=2))
    add("Arpège sauts", ARP, mix(ARPP, arp_mode=15, arp_oct=2, w1mut=7, w1mutamt=30))
    add("Arpège mélangé", ARP, mix(ARPP, arp_mode=18, arp_oct=2, arp_rate=4))

    # --- Effets (6)
    add("Voyelles parlantes", FX, mix(NAPPE, f1mode=7, f1reso=55, l1pos=0, mx1_src=7, **{"mx1_dst@f1cutoff": 0}, mx1_amt=40, l2rate=0.8))
    add("Métal grinçant", FX, mix(LEAD_, w1pos=11, w1mut=4, w1mutamt=70, mixring=50, f1cutoff=4000))
    add("Grains", FX, mix(NAPPE, w1pos=14, w2pos=14, mixnoise=40, l1pos=6, l1rate=4, l1shape=4))
    add("Peigne qui tourne", FX, mix(NAPPE, f1mode=9, f1cutoff=200, f1reso=80, mixnoise=60, l2cutoff=40, l2rate=0.3))
    add("Alarme", FX, mix(LEAD_, l2pitch=100, l2rate=3, l2shape=3))
    add("Descente spectrale", FX, mix(PLK, env3pos=-8, e3a=1, e3d=1500, e3s=0, e2d=2000, e2r=1500))

    # --- v0.47 (maj. 5 nov.) : 31 sons de plus (au moins 90 par synthé)
    for nm, kw in [
        ("Basse carrée morph", dict(w1pos=3, w2pos=3, f1cutoff=600)), ("Basse orgue", dict(w1pos=5, w2pos=0, f1cutoff=1200, env1f1=20)),
        ("Basse grain", dict(w1pos=14, f1cutoff=900)), ("Basse métal", dict(w1pos=11, f1cutoff=800, f1reso=30)),
        ("Basse synchro mutante", dict(w1mut=2, w1mutamt=45, f1cutoff=1000)), ("Basse douce morph", dict(w1pos=15, w2pos=1, f1cutoff=500)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead voix A", dict(w1pos=6, f1cutoff=2500)), ("Lead cloche", dict(w1pos=8, w2pos=8, w2semi=12)),
        ("Lead repli", dict(w1mut=4, w1mutamt=50)), ("Lead bande", dict(f1mode=2, f1cutoff=1800, f1reso=40)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe orgue", dict(w1pos=5, w2pos=15)), ("Nappe métal lente", dict(w1pos=11, w2pos=8, f1cutoff=1500)),
        ("Nappe grains", dict(w1pos=14, w2pos=13)), ("Nappe voix O", dict(w1pos=7, w2pos=6, f1mode=7)),
        ("Nappe douce", dict(w1pos=15, w2pos=1, f1cutoff=1600)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck orgue", dict(w1pos=5)), ("Pluck métal", dict(w1pos=11, mix1=100, mix2=70, f1cutoff=4000)), ("Pluck voix", dict(w1pos=6, f1mode=7, f1cutoff=1400)),
        ("Pluck doux morph", dict(w1pos=15, f1cutoff=800)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [("Orgue creux", dict(w1pos=10, w2pos=5)), ("Clavier cloche", dict(w1pos=8, w2pos=1)), ("Piano métal", dict(w1pos=11, w2pos=2, f1cutoff=3000))]:
        add(nm, KEYS, mix(CLAV, **kw))
    CORD2 = mix(NAPPE, w1pos=9, w2pos=2, e2a=400, e2r=1300, e1a=300, f1cutoff=4000, f1reso=5, l2pitch=6, l2rate=5)
    add("Cordes voyelles", STR, mix(CORD2, w2pos=6))
    add("Cordes graves", STR, mix(CORD2, w1semi=-12, f1cutoff=2500))
    CUIV2 = mix(BASE, w1pos=2, w2pos=2, f1cutoff=1100, env1f1=55, e1a=50, e1d=600, e1s=50, e2a=25, e2d=700, e2s=85, e2r=300)
    add("Cuivres creux", BRASS, mix(CUIV2, w2pos=10))
    add("Cuivres orgue", BRASS, mix(CUIV2, w1pos=5, f1cutoff=900))
    add("Arpège voix", ARP, mix(PLK, w1pos=6, f1mode=7, arp_gate=50, arp_rate=3, arp_mode=1, arp_oct=2))
    add("Arpège métal", ARP, mix(PLK, w1pos=11, mix1=100, mix2=70, f1cutoff=4000, arp_gate=40, arp_rate=4, arp_mode=3, arp_oct=2))
    add("Vent de grain", FX, mix(NAPPE, w1pos=14, w2pos=14, f1mode=2, f1cutoff=900, f1reso=50))
    return b
