# Robot Choral (v0.47 (maj. 5 nov.)) : 80 sons de plus — vocodeur à cordes et chœur, ensemble, formants, bandes.
from banks_common import Bank
from bank_voc_common import fill

EXTRA = {"parlée": dict(bands=2), "sèche": dict(bands=1, ensemble=30), "lente": dict(ensemble=80, bands=3), "de robot": dict(bands=0, sib=70),
         "lointaine": dict(formant=-3, ensemble=70), "grave": dict(formant=-5), "aiguë": dict(formant=4), "vibrante": dict(ensemble=60, creg=60)}


def build():
    b = Bank("vocstr")
    fill(b, lambda h: EXTRA[h])
    return b
