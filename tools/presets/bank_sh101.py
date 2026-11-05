# Mono First (v0.47 (maj. 5 nov.), mono à mélangeur : scie, impulsion, sous-oscillateur, bruit, filtre 24 dB) : 80 sons de plus.
# range : 0 16', 1 8', 2 4', 3 2' · pwmsrc : 0 manuel, 1 LFO, 2 enveloppe · subtype : 0 carré -1 oct., 1 carré -2 oct., 2 impulsion -2 oct.
# envtrig : 0 porte + note, 1 porte, 2 LFO · vcamode : 0 enveloppe, 1 porte · lfowave : 0 triangle, 1 carré, 2 aléatoire, 3 bruit
# lfoamt : LFO vers le filtre · vcolfo : LFO vers la hauteur
from banks_common import Bank, mix, ACID, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

BASE = dict(range=1, saw=80, pulse=0, sub=40, noise=0, cutoff=900, reso=25, envamt=45, kbd=50, attack=2, decay=350, sustain=40, release=120)
BASSE = mix(BASE, range=0, saw=80, sub=60, cutoff=420, reso=25, envamt=50, decay=260, sustain=25, release=90)
LEAD_ = mix(BASE, range=1, saw=80, pulse=40, pw=30, sub=0, cutoff=2000, reso=30, envamt=30, decay=600, sustain=70, release=200,
            porta=60, portamode=2, vcolfo=5, lforate=5.5, lfodelay=400)
PLK = mix(BASE, cutoff=380, reso=45, envamt=70, attack=0.5, decay=200, sustain=0, release=150)
NAPPE = mix(BASE, pulse=70, pw=40, pwmsrc=1, lforate=0.4, saw=50, sub=30, cutoff=1100, reso=15, envamt=25, attack=700, decay=2000,
            sustain=80, release=1800)


def build():
    b = Bank("sh101")
    add = b.add

    for nm, kw in [
        ("Basse scie et sous-osc.", {}), ("Basse carrée profonde", dict(saw=0, pulse=80, pw=0, sub=70, subtype=1)),
        ("Basse impulsion fine", dict(saw=0, pulse=85, pw=70, cutoff=600)), ("Basse PWM", dict(saw=0, pulse=80, pwmsrc=1, pw=40, lforate=0.8)),
        ("Basse deux octaves", dict(subtype=1, sub=80)), ("Basse impulsion -2 oct.", dict(subtype=2, sub=70, cutoff=500)),
        ("Basse résonante", dict(reso=60, envamt=65, decay=200)), ("Basse molle", dict(cutoff=250, reso=10, envamt=25, decay=500, sustain=60)),
        ("Basse claquante", dict(decay=140, sustain=0, envamt=70, reso=35)), ("Basse glissée", dict(porta=90, portamode=2)),
        ("Basse bruitée", dict(noise=20, cutoff=500)), ("Basse 8'", dict(range=1, cutoff=600)),
        ("Basse funky", dict(saw=0, pulse=80, pw=50, reso=50, envamt=65, decay=180, sustain=10)),
        ("Basse électro", dict(saw=80, pulse=50, pw=30, cutoff=700, reso=40)), ("Basse en porte", dict(vcamode=1, decay=200, sustain=10)),
        ("Basse wobble", dict(lfoamt=50, lfosync=8, reso=50, sustain=80, cutoff=300)), ("Basse aléatoire", dict(lfoamt=35, lfowave=2, lfosync=9, reso=45)),
        ("Basse de minuit", dict(cutoff=320, reso=40, envamt=40, decay=400)), ("Basse franche", dict(cutoff=800, reso=15, envamt=35)),
        ("Basse percutante", dict(pwmsrc=2, saw=0, pulse=80, pw=60, decay=180, sustain=10)),
        ("Basse sub pure", dict(saw=0, pulse=0, sub=100, subtype=0, cutoff=300, reso=0, envamt=10)),
        ("Basse vibrée", dict(vcolfo=6, lforate=5)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Acide mono", dict(reso=80, envamt=75, decay=180, sustain=0, porta=70, portamode=2, sub=0)),
        ("Acide carrée mono", dict(saw=0, pulse=80, reso=82, envamt=75, decay=200, sustain=0, porta=70, portamode=2, sub=0)),
        ("Acide grave", dict(reso=78, envamt=70, decay=250, sustain=0, sub=50)),
        ("Acide au LFO", dict(reso=80, lfoamt=40, lfosync=9, lfowave=2, sustain=50, sub=0)),
    ]:
        add(nm, ACID, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead mono classique", {}), ("Lead sciée", dict(pulse=0)), ("Lead carré", dict(saw=0, pulse=80, pw=0)),
        ("Lead impulsion fine", dict(saw=0, pulse=80, pw=80)), ("Lead PWM", dict(saw=0, pulse=80, pwmsrc=1, pw=40, lforate=4)),
        ("Lead 4'", dict(range=2)), ("Lead 2'", dict(range=3, cutoff=3000)), ("Lead résonant", dict(reso=65, cutoff=1300)),
        ("Lead sifflé", dict(saw=0, pulse=0, sub=0, noise=0, reso=95, cutoff=1500, kbd=100)), ("Lead bruité", dict(noise=20)),
        ("Lead glissé long", dict(porta=250, portamode=1)), ("Lead trille", dict(vcolfo=30, lfowave=1, lforate=7, lfodelay=0)),
        ("Lead au filtre lent", dict(lfoamt=30, lforate=0.5)), ("Lead sous-octave", dict(sub=50)), ("Lead doux", dict(cutoff=1100, reso=10, attack=30)),
        ("Lead vibrato marqué", dict(vcolfo=10, lfodelay=200)), ("Lead criard", dict(reso=75, cutoff=1500)), ("Lead impulsion lente", dict(saw=0, pulse=80, pwmsrc=1, lforate=0.3, pw=50)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [("Nappe PWM", {}), ("Nappe sombre mono", dict(cutoff=600)), ("Nappe qui respire", dict(lfoamt=25, lforate=0.2)),
                   ("Nappe bruit de vent", dict(noise=40, saw=0, pulse=40)), ("Nappe aiguë", dict(range=2, cutoff=2000))]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck mono", {}), ("Pluck carré", dict(saw=0, pulse=80)), ("Pluck résonant", dict(reso=75)), ("Pluck sous-octave", dict(sub=70)),
        ("Pluck percutant", dict(decay=120, envamt=85)), ("Pluck PWM", dict(saw=0, pulse=80, pwmsrc=2, pw=50)), ("Pluck 4'", dict(range=2, cutoff=600)),
        ("Pluck bruité", dict(noise=30)), ("Séquence mono", dict(decay=260, reso=50)), ("Séquence grave", dict(range=0, cutoff=300)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [("Orgue mono", dict(saw=0, pulse=70, pw=0, sub=50, envamt=0, cutoff=3000, sustain=100, decay=100, release=80)),
                   ("Clavier mono", dict(cutoff=1500, decay=800, sustain=30, envamt=30)),
                   ("Clavinette", dict(saw=0, pulse=80, pw=80, cutoff=1800, reso=30, decay=400, sustain=10, envamt=50))]:
        add(nm, KEYS, mix(BASE, **kw))
    for nm, kw in [("Cordes mono", dict(attack=300, release=900, cutoff=2000)), ("Violon mono", dict(attack=150, vcolfo=6, lfodelay=500, cutoff=2500, range=2))]:
        add(nm, STR, mix(NAPPE, **kw))
    for nm, kw in [("Cuivre mono", dict(cutoff=600, envamt=60, attack=40, decay=500, sustain=60)), ("Trompette mono", dict(range=2, cutoff=800, envamt=55, attack=30)),
                   ("Cuivre sous-octave", dict(sub=60, cutoff=500, envamt=60, attack=60))]:
        add(nm, BRASS, mix(BASE, **kw))
    for nm, kw in [("Arpège mono", dict(arp_mode=1, arp_oct=2)), ("Arpège carré mono", dict(saw=0, pulse=80, arp_mode=3, arp_oct=2)),
                   ("Arpège aléatoire", dict(arp_mode=6, arp_oct=2, arp_rate=4)), ("Arpège grave", dict(range=0, arp_mode=13, arp_oct=2))]:
        add(nm, ARP, mix(PLK, **kw))
    for nm, kw in [
        ("Sirène mono", dict(vcolfo=80, lforate=0.5, lfowave=0)), ("Échantillonneur", dict(vcolfo=60, lfowave=2, lforate=8)),
        ("Vent", dict(saw=0, sub=0, noise=100, cutoff=700, reso=70, lfoamt=40, lforate=0.2, sustain=100)),
        ("Bulles", dict(reso=95, cutoff=300, lfoamt=60, lfowave=2, lforate=6)), ("Pas de robot", dict(envtrig=2, lforate=6, vcamode=0, decay=100, sustain=0)),
        ("Laser mono", dict(vcolfo=100, lfowave=1, lforate=20, decay=150, sustain=0)), ("Pluie", dict(saw=0, sub=0, noise=80, reso=60, envtrig=2, lforate=9, decay=60, sustain=0)),
        ("Moteur", dict(range=0, lfoamt=70, lfowave=1, lforate=14, cutoff=250, reso=60)),
    ]:
        add(nm, FX, mix(BASE, **kw))
    return b
