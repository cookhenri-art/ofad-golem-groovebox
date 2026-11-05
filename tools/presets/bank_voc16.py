# Robot 78 (v0.47 (maj. 5 nov.)) : 80 sons de plus — vocodeur à 16 bandes, souffle et bourdon pour les consonnes.
from banks_common import Bank
from bank_voc_common import fill

EXTRA = {"parlée": dict(hiss=45), "sèche": dict(hiss=30), "lente": dict(hold=0, hiss=20), "de robot": dict(buzz=40, hiss=20),
         "lointaine": dict(shift=-2, hiss=60), "grave": dict(shift=-3), "aiguë": dict(shift=3), "vibrante": dict(hiss=40)}


def build():
    b = Bank("voc16")
    fill(b, lambda h: EXTRA[h])
    return b
