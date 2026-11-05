# Vocodeurs (v0.47 (maj. 5 nov.)) : 80 sons de plus par vocodeur, construits en croisant des porteuses (forme, octave,
# unisson, brillance) et des caractères (réponse, relâchement, souffle…). Catégories : Voix, Nappes, Cordes,
# Basses, Leads, Effets. cwave : 0 scie, 1 impulsion, 2 scie + sous-octave, 3 cordes, 4 chœur, 5 bruit · coct : 0 16', 1 8', 2 4'.
from banks_common import mix, BASS, LEAD, PAD, STR, FX, VOICE

CARRIERS = [
    ("Scie", dict(cwave=0)), ("Impulsion", dict(cwave=1, cpw=30)), ("Scie et sous-octave", dict(cwave=2, coct=0, cbright=7000)),
    ("Cordes", dict(cwave=3, cuni=2, cdetune=12, ca=80, cr=900)), ("Chœur", dict(cwave=4, cuni=3, cdetune=14, ca=60, cr=900)),
    ("Souffle", dict(cwave=5)), ("Scie large", dict(cwave=0, cuni=4, cdetune=18)), ("Impulsion fine", dict(cwave=1, cpw=12, coct=2)),
    ("Scie sombre", dict(cwave=0, cbright=3500)), ("Chœur aigu", dict(cwave=4, coct=2, cuni=2)),
]
# (nom, catégorie, réglages communs aux quatre vocodeurs)
CHARACTERS = [
    ("parlée", VOICE, dict(resp=1.5)), ("sèche", VOICE, dict(resp=0.6, rel=20)), ("lente", PAD, dict(resp=6, rel=300, ca=300, cr=1500)),
    ("de robot", VOICE, dict(resp=0.4, rel=10)), ("lointaine", PAD, dict(resp=4, rel=200, cbright=4000)), ("grave", BASS, dict(coct=0, tune=-12)),
    ("aiguë", LEAD, dict(coct=2)), ("vibrante", LEAD, dict(lf1_rate=5.5, mx1_src=6, mx1_amt=2, **{"mx1_dst@tune": 0})),
]


def fill(b, extra):
    """extra(caractère) : réglages propres au vocodeur pour un caractère donné. Les sons « Cordes » et les
    porteuses « Souffle » vont dans les catégories Cordes et Effets."""
    for cn, cv in CARRIERS:
        for hn, cat, hv in CHARACTERS:
            if cn == "Cordes" and cat == VOICE:
                cat = STR
            if cn == "Souffle":
                cat = FX
            b.add(f"{cn}, voix {hn}", cat, mix(cv, hv, extra(hn)))
