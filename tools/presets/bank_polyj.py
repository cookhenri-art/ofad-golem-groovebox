# Poly à chorus (un oscillateur par voix, sous-oscillateur, passe-haut, chorus I / II) : 50 sons.
# range : 0 = 16', 1 = 8', 2 = 4'.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

BASE = dict(saw=1, pulse=0, pw=0, sub=0, noise=0, cutoff=2200, reso=10, envamt=35, ea=4, ed=900, es=60, er=600, chorus=1)
BASSE = mix(BASE, voices=1, range=0, saw=1, pulse=0, sub=60, cutoff=600, reso=15, envamt=40, ed=300, es=20, er=150, chorus=0, keytrack=30)
LEAD_ = mix(BASE, voices=1, saw=1, pulse=1, pw=20, cutoff=2600, reso=25, envamt=30, ed=600, es=70, er=250, chorus=1, lfodelay=400, lforate=5.5, lfopitch=12)
NAPPE = mix(BASE, saw=0, pulse=1, pwmsrc=1, pwmdepth=65, lforate=0.5, cutoff=1500, envamt=20, ea=1500, ed=2000, es=90, er=3000, chorus=2)
PLK = mix(BASE, saw=1, cutoff=900, reso=35, envamt=50, ea=1, ed=250, es=0, er=250, chorus=1)
CLAV = mix(BASE, saw=0, pulse=1, pw=40, cutoff=1800, reso=10, envamt=30, ea=1, ed=1200, es=30, er=400, velfilter=40, velamp=50, chorus=1)
CORD = mix(BASE, saw=1, pulse=1, pwmsrc=1, pwmdepth=40, lforate=0.8, cutoff=3500, reso=0, envamt=10, ea=500, ed=1500, es=85, er=1400, chorus=3, hpf=1)
CUIV = mix(BASE, saw=1, pulse=0, cutoff=1000, reso=10, envamt=55, ea=40, ed=500, es=70, er=300, chorus=1)


def build():
    b = Bank("polyj")
    b.add("Init", INIT, dict(saw=1, pulse=0, sub=0, noise=0, hpf=0, cutoff=9000, reso=0, envamt=0, velfilter=0, keytrack=0, ea=2, ed=900, es=100, er=300, chorus=0, drift=0))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (6)
    add("Basse sous-oscillateur", BASS, mix(BASSE))
    add("Basse carrée house", BASS, mix(BASSE, saw=0, pulse=1, pw=0, sub=80, cutoff=450, envamt=50, ed=200, es=0))
    add("Basse résonante", BASS, mix(BASSE, reso=55, cutoff=400, envamt=60, ed=220, es=10))
    add("Basse chorus", BASS, mix(BASSE, chorus=1, sub=40, cutoff=800))
    add("Basse largeur", BASS, mix(BASSE, saw=0, pulse=1, pwmsrc=2, pwmdepth=60, sub=50, cutoff=700))
    add("Basse porte", BASS, mix(BASSE, vca=1, cutoff=500, envamt=45, ed=180, es=0))

    # --- Leads (4)
    add("Lead chorus", LEAD, mix(LEAD_))
    add("Lead aigu 4'", LEAD, mix(LEAD_, range=2, cutoff=3000, chorus=0))
    add("Lead impulsion fine", LEAD, mix(LEAD_, saw=0, pulse=1, pw=75, cutoff=3500, reso=15))
    add("Lead au levier", LEAD, mix(LEAD_, bendvcf=60, pb_range=7, cutoff=1500))

    # --- Nappes (8)
    add("Nappe largeur", PAD, mix(NAPPE))
    add("Nappe chorus II", PAD, mix(NAPPE, saw=1, chorus=2, pwmdepth=40))
    add("Nappe sombre", PAD, mix(NAPPE, cutoff=700, reso=20, chorus=3))
    add("Nappe passe-haut", PAD, mix(NAPPE, hpf=2, cutoff=4000))
    add("Nappe respirante", PAD, mix(NAPPE, lfofilter=30, lforate=0.25, cutoff=1000, reso=25))
    add("Nappe sous-octave", PAD, mix(NAPPE, sub=50, cutoff=1200))
    add("Nappe enveloppe inverse", PAD, mix(NAPPE, envpol=1, envamt=40, cutoff=3500, ea=1200, ed=3000, es=20))
    add("Nappe souffle", PAD, mix(NAPPE, noise=25, cutoff=2500, hpf=1))

    # --- Plucks (6)
    add("Pluck chorus", PLUCK, mix(PLK))
    add("Pluck carré", PLUCK, mix(PLK, saw=0, pulse=1, pw=0, cutoff=1200))
    add("Pluck résonant", PLUCK, mix(PLK, reso=60, cutoff=600, envamt=60))
    add("Pluck 4'", PLUCK, mix(PLK, range=2, cutoff=1500, ed=180))
    add("Pluck largeur", PLUCK, mix(PLK, saw=0, pulse=1, pwmsrc=2, pwmdepth=70))
    add("Goutte", PLUCK, mix(PLK, saw=0, pulse=1, pw=60, cutoff=400, reso=70, envamt=70, ed=150, er=150, chorus=0))

    # --- Claviers (7)
    add("Piano analogique", KEYS, mix(CLAV))
    add("Accords club", KEYS, mix(CLAV, saw=1, pulse=0, cutoff=1400, reso=30, envamt=45, ed=250, es=0, er=250))
    add("Orgue chorus", KEYS, mix(CLAV, saw=0, pulse=1, pw=0, sub=50, range=1, envamt=0, cutoff=5000, ea=2, ed=100, es=100, er=60, vca=1, chorus=3))
    add("Clavier électrique", KEYS, mix(CLAV, pw=10, cutoff=1300, ed=1800, es=20, chorus=2))
    add("Cloche ronde", KEYS, mix(CLAV, range=2, pw=70, cutoff=3500, es=0, ed=1500, er=1500))
    add("Accords techno", KEYS, mix(CLAV, saw=1, pulse=1, pw=30, cutoff=900, reso=40, envamt=55, ed=200, es=0, er=200, chorus=1))
    add("Clavier doux", KEYS, mix(CLAV, cutoff=900, envamt=20, chorus=2))

    # --- Cordes (6)
    add("Cordes en chœur", STR, mix(CORD))
    add("Cordes lentes", STR, mix(CORD, ea=1200, er=2500))
    add("Cordes sombres", STR, mix(CORD, cutoff=1800, hpf=0))
    add("Cordes brillantes", STR, mix(CORD, cutoff=6000, hpf=2))
    add("Ensemble", STR, mix(CORD, sub=20, chorus=2))
    add("Cordes pincées", STR, mix(CORD, ea=1, ed=350, es=0, er=350, envamt=35, cutoff=2500))

    # --- Cuivres (4)
    add("Cuivres chorus", BRASS, mix(CUIV))
    add("Cuivres brillants", BRASS, mix(CUIV, cutoff=1500, envamt=60))
    add("Cuivres doux", BRASS, mix(CUIV, cutoff=700, ea=80, envamt=45))
    add("Stab cuivré", BRASS, mix(CUIV, ed=250, es=0, er=200, envamt=65))

    # --- Arpèges (6)
    ARPP = mix(PLK, arp_gate=50, arp_rate=3)
    add("Arpège scintillant", ARP, mix(ARPP, arp_mode=3, arp_oct=2, cutoff=2600, reso=40, envamt=30, ed=180, er=160))
    add("Arpège montant", ARP, mix(ARPP, arp_mode=1, arp_oct=3))
    add("Arpège descendant", ARP, mix(ARPP, arp_mode=2, arp_oct=2))
    add("Arpège aléatoire", ARP, mix(ARPP, arp_mode=6, arp_oct=2, chorus=2))
    add("Arpège divergent", ARP, mix(ARPP, arp_mode=11, arp_oct=2, saw=0, pulse=1, pw=30))
    add("Arpège croches", ARP, mix(ARPP, arp_mode=1, arp_oct=1, arp_rate=1, arp_gate=70, sub=40))

    # --- Effets (3)
    add("Vent chorus", FX, mix(NAPPE, saw=0, pulse=0, noise=100, cutoff=1200, reso=60, lfofilter=40, lforate=0.2, chorus=3))
    add("Sirène", FX, mix(LEAD_, lfopitch=100, lforate=0.8, lfodelay=0))
    add("Bulles", FX, mix(PLK, lfofilter=80, lforate=8, reso=80, cutoff=500, es=80, ed=1000, er=500))

    # --- v0.47 (maj. 5 nov.) : 34 sons de plus (au moins 90 par synthé)
    for nm, kw in [
        ("Basse sciée 16'", dict(cutoff=500, reso=25, envamt=50)), ("Basse impulsion chorus", dict(saw=0, pulse=1, pw=30, chorus=1, cutoff=700)),
        ("Basse sombre", dict(cutoff=320, sub=80, envamt=25)), ("Basse bruitée", dict(noise=20, cutoff=600)),
        ("Basse pincée", dict(ed=180, es=0, er=120, envamt=65, reso=30)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead sciée", dict(pulse=0, cutoff=2200)), ("Lead 2'", dict(range=2, cutoff=3000, reso=15)),
        ("Lead sous-octave", dict(sub=50, cutoff=1900)), ("Lead vibrato tardif", dict(lfodelay=900, lfopitch=18)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe chorus I", dict(chorus=1)), ("Nappe sciée", dict(saw=1, pulse=0, cutoff=1800)),
        ("Nappe lente qui s'ouvre", dict(ea=2500, cutoff=700, envamt=45)), ("Nappe 4'", dict(range=2, cutoff=2400)),
        ("Nappe bruit doux", dict(noise=15, cutoff=1300)), ("Nappe PWM rapide", dict(lforate=2.5, pwmdepth=50)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck impulsion", dict(saw=0, pulse=1, pw=35)), ("Pluck sous-octave", dict(sub=60, cutoff=700)),
        ("Pluck sec", dict(ed=150, er=150, chorus=0)), ("Pluck chorus II", dict(chorus=2, cutoff=1200)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Piano chorus", dict(cutoff=2400, ed=1600)), ("Clavier sciée", dict(saw=1, pulse=0, cutoff=1500)),
        ("Orgue sous-octave", dict(sub=60, es=100, ed=100, er=150, envamt=0)), ("Clavier passe-haut", dict(hpf=2, cutoff=3000)),
    ]:
        add(nm, KEYS, mix(CLAV, **kw))
    for nm, kw in [("Cordes chorus I", dict(chorus=1)), ("Cordes passe-haut", dict(hpf=2)), ("Cordes de nuit", dict(cutoff=1600, ea=900))]:
        add(nm, STR, mix(CORD, **kw))
    for nm, kw in [("Cuivres impulsion", dict(saw=0, pulse=1, pw=25)), ("Cuivres larges", dict(chorus=2, cutoff=1300)), ("Cors", dict(range=0, cutoff=700))]:
        add(nm, BRASS, mix(CUIV, **kw))
    add("Arpège carré chorus", ARP, mix(PLK, saw=0, pulse=1, pw=40, arp_mode=1, arp_oct=2, arp_rate=3))
    add("Arpège en quinconce", ARP, mix(PLK, arp_mode=10, arp_oct=2, arp_rate=4, chorus=2))
    add("Tempête", FX, mix(NAPPE, saw=0, pulse=0, noise=100, cutoff=2500, reso=40, lforate=0.3, envamt=0))
    add("Goutte d'eau", FX, mix(PLK, cutoff=500, reso=85, envamt=90, ed=260, range=2))
    return b
