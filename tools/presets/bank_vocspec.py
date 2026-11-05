# Spectral Vocal (v0.47 (maj. 5 nov.)) : 80 sons de plus — vocodeur à algorithmes (analogique, numérique, voyelles, cristal, velours…).
from banks_common import Bank
from bank_voc_common import fill

EXTRA = {"parlée": dict(algo=1), "sèche": dict(algo=2), "lente": dict(algo=7, traces=40), "de robot": dict(algo=0, expand=40),
         "lointaine": dict(algo=6, stereo=80), "grave": dict(algo=1, formant=-5), "aiguë": dict(algo=3, formant=4), "vibrante": dict(algo=4)}


def build():
    b = Bank("vocspec")
    fill(b, lambda h: EXTRA[h])
    return b
