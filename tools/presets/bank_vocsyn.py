# Speak Synth (v0.47 (maj. 5 nov.)) : 80 sons de plus — vocodeur, compresseur de voix, boîte parlante, voix modélisée, chœur.
from banks_common import Bank
from bank_voc_common import fill

EXTRA = {"parlée": dict(mvoc=80), "sèche": dict(mvoc=70, mcomp=40), "lente": dict(mvoc=60, mpoly=60), "de robot": dict(mvoc=60, mcomp=60, crush=4),
         "lointaine": dict(mvoc=50, mbio=40), "grave": dict(mvoc=80, vformant=-5), "aiguë": dict(mvoc=70, vformant=4), "vibrante": dict(mvoc=50, mtalk=60, vowel=50)}


def build():
    b = Bank("vocsyn")
    fill(b, lambda h: EXTRA[h])
    return b
