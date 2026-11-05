# Hurleur 200 (v0.47 (maj. 5 nov.), mono semi-modulaire à deux filtres Sallen-Key passe-haut et passe-bas) : 76 sons de plus.
# v1wave : 0 triangle, 1 scie, 2 impulsion, 3 bruit · v2wave : 0 scie, 1 carré, 2 impulsion étroite, 3 anneau
# scale : 0 32', 1 16', 2 8', 3 4' · modsrc : 0 rampe/triangle, 1 carré, 2 échantillonné-bloqué
from banks_common import Bank, mix, ACID, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

BASE = dict(v1wave=1, v1scale=2, v1level=80, v2level=0, hpcut=40, hppeak=0, lpcut=1200, lppeak=35, lpeg2=45, drive=30,
            e2attack=3, e2decay=400, e2sustain=60, e2release=250, e1attack=5, e1release=300)
BASSE = mix(BASE, v1scale=1, v2scale=1, lpcut=380, lppeak=30, lpeg2=55, e2decay=280, e2sustain=30, e2release=120, drive=40)
LEAD_ = mix(BASE, v1scale=2, v2scale=2, v2level=60, v2pitch=0, lpcut=1500, lppeak=45, lpeg2=35, e2decay=600, e2sustain=70, porta=40,
            mgrate=5.5, mgshape=50, pitchmg=6, mgdelay=500)
NAPPE = mix(BASE, v2level=60, v2wave=0, v2pitch=0, lpcut=900, lppeak=20, lpeg2=40, e2attack=900, e2decay=2000, e2sustain=70,
            e2release=1800, e1attack=600, e1release=1800, mgrate=0.3, lpmg=20)
PLK = mix(BASE, lpcut=350, lppeak=55, lpeg2=75, e2attack=0.5, e2decay=200, e2sustain=0, e2release=200, e1release=200)
FXB = mix(BASE, lppeak=70, drive=50)


def build():
    b = Bank("ms20")
    add = b.add

    for nm, kw in [
        ("Basse sciée de 16'", {}), ("Basse impulsion", dict(v1wave=2, v1pw=25)), ("Basse triangle ronde", dict(v1wave=0, lpcut=600, lppeak=15, drive=20)),
        ("Basse deux scies", dict(v2level=70, v2wave=0, v2pitch=0)), ("Basse en quinte", dict(v2level=55, v2wave=0, v2pitch=7)),
        ("Basse anneau", dict(v2level=60, v2wave=3, v2pitch=5, lpcut=600)), ("Basse 32'", dict(v1scale=0, lpcut=500)),
        ("Basse qui crache", dict(lppeak=70, drive=70, lpeg2=70)), ("Basse passe-haut", dict(hpcut=120, hppeak=50, lpcut=900)),
        ("Basse carrée creuse", dict(v2level=80, v1level=0, v2wave=1)), ("Basse impulsion étroite", dict(v2level=80, v1level=0, v2wave=2, lpcut=700)),
        ("Basse glissée", dict(porta=120)), ("Basse molle", dict(lpcut=260, lppeak=10, lpeg2=25, drive=15)),
        ("Basse claquante", dict(e2decay=150, e2sustain=0, lpeg2=75, lppeak=45)), ("Basse grondante", dict(v1scale=0, drive=60, lppeak=45, lpcut=500)),
        ("Basse vibrée", dict(mgrate=6, pitchmg=8)), ("Basse wobble", dict(mgsync=8, lpmg=60, lpcut=300, lppeak=60, e2sustain=80)),
        ("Basse échantillonnée", dict(modsrc=2, mgsync=9, lpmg=45, lppeak=55)), ("Basse descendante", dict(pitcheg1=-30, e1attack=1, e1release=120)),
        ("Basse de garage", dict(v2level=50, v2wave=1, drive=60, lppeak=40)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Acide Sallen-Key", dict(lppeak=80, lpeg2=70, e2decay=180, e2sustain=0, porta=60)),
        ("Acide qui siffle", dict(lppeak=92, lpcut=300, lpeg2=80, e2decay=220, e2sustain=0)),
        ("Acide carrée", dict(v1level=0, v2level=80, v2wave=1, lppeak=85, lpeg2=75, e2decay=200, e2sustain=0)),
        ("Acide saturée", dict(lppeak=85, drive=80, lpeg2=70, e2decay=200, e2sustain=0, porta=60)),
    ]:
        add(nm, ACID, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead hurleur", dict(lppeak=75, drive=60)), ("Lead deux scies", {}), ("Lead quinte", dict(v2pitch=7)), ("Lead octave", dict(v2scale=3)),
        ("Lead impulsion", dict(v1wave=2, v1pw=20, v2wave=2)), ("Lead anneau", dict(v2wave=3, v2pitch=7)), ("Lead passe-haut", dict(hpcut=600, hppeak=60, lpcut=4000)),
        ("Lead chanté", dict(lppeak=60, lpcut=1100, pitchmg=10)), ("Lead fin", dict(v2level=0, lpcut=2500, lppeak=30, drive=10)),
        ("Lead glissé", dict(porta=180)), ("Lead triangle doux", dict(v1wave=0, v2level=0, lpcut=2000, lppeak=10, drive=5)),
        ("Lead crie quand on pousse", dict(lppeak=88, lpeg2=55, drive=70)), ("Lead 4'", dict(v1scale=3, v2scale=3)),
        ("Lead modulé", dict(lpmg=30, mgrate=4)), ("Lead échantillonné", dict(modsrc=2, mgrate=8, pitchmg=12, pitchmg_=0)),
        ("Lead à deux filtres", dict(hpcut=300, hppeak=70, lppeak=70, lpcut=1800)),
    ]:
        kw = {k: v for k, v in kw.items() if not k.endswith("_")}
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe Sallen-Key", {}), ("Nappe sombre", dict(lpcut=500)), ("Nappe passe-haut", dict(hpcut=300, hppeak=30, lpcut=3000)),
        ("Nappe anneau lente", dict(v2wave=3, v2pitch=7)), ("Nappe impulsion", dict(v1wave=2, v1pw=20)), ("Nappe qui ondule", dict(lpmg=45, mgrate=0.15)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck Sallen-Key", {}), ("Pluck résonant", dict(lppeak=80)), ("Pluck deux scies", dict(v2level=60, v2wave=0)),
        ("Pluck impulsion", dict(v1wave=2, v1pw=30)), ("Pluck passe-haut", dict(hpcut=400, hppeak=40, lpcut=900)),
        ("Pluck grave", dict(v1scale=1, lpcut=250)), ("Pluck anneau", dict(v2level=70, v2wave=3, v2pitch=7)), ("Goutte", dict(lppeak=90, lpeg2=90, e2decay=120)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [("Orgue sale", dict(v2level=60, v2wave=1, lpcut=3000, lpeg2=0, e2sustain=100, e2decay=100)),
                   ("Clavier dur", dict(lpcut=1800, lpeg2=40, e2decay=700, e2sustain=30)),
                   ("Clavier impulsion", dict(v1wave=2, v1pw=35, lpcut=2000, e2decay=900, e2sustain=20)),
                   ("Clavecin passe-haut", dict(hpcut=500, hppeak=20, lpcut=5000, lpeg2=0, e2decay=600, e2sustain=0))]:
        add(nm, KEYS, mix(BASE, **kw))
    for nm, kw in [("Cordes Sallen-Key", dict(lpcut=1800, e2attack=400, e1attack=300)), ("Cordes graves", dict(v1scale=1, v2scale=1, lpcut=1200, e2attack=500)),
                   ("Cordes qui s'ouvrent", dict(lpcut=500, lpeg2=60, e2attack=1500))]:
        add(nm, STR, mix(NAPPE, **kw))
    for nm, kw in [("Cuivre hurleur", dict(v2level=60, lpcut=600, lpeg2=60, e2attack=60, e2decay=500, e2sustain=60, lppeak=40)),
                   ("Cuivre sourd", dict(lpcut=400, lpeg2=50, e2attack=80)), ("Cor", dict(v1scale=1, lpcut=500, lpeg2=45, e2attack=100)),
                   ("Cuivre synchro", dict(v2level=70, v2wave=3, v2pitch=12, lpcut=800, lpeg2=55, e2attack=50))]:
        add(nm, BRASS, mix(BASE, **kw))
    for nm, kw in [("Séquence Sallen-Key", dict(e2decay=260)), ("Séquence résonante", dict(lppeak=75, e2decay=180)),
                   ("Séquence carrée", dict(v1level=0, v2level=80, v2wave=1)), ("Séquence grave", dict(v1scale=1, lpcut=300, lppeak=65))]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Vent hurlant", dict(v1wave=3, lpcut=900, lppeak=80, lpmg=40, mgrate=0.3, e2sustain=100, e1attack=500)),
        ("Sirène hurlante", dict(v2level=0, pitchmg=60, mgrate=0.6, mgshape=50)), ("Alarme", dict(pitchmg=40, modsrc=1, mgrate=3)),
        ("Ordinateur", dict(modsrc=2, mgrate=10, pitchmg=50, lppeak=60)), ("Larsen", dict(lppeak=100, hppeak=100, hpcut=800, lpcut=900, drive=90)),
        ("Explosion", dict(v1wave=3, v1scale=0, lpcut=600, lpeg2=80, e2decay=1500, e2sustain=0, e1release=1500, drive=80)),
        ("Laser", dict(pitcheg1=80, e1release=200, lpcut=3000)), ("Bulle", dict(lppeak=95, lpcut=200, lpeg2=90, e2decay=150, e2sustain=0)),
        ("Hélicoptère", dict(v1wave=3, lpmg=80, modsrc=1, mgrate=12, lpcut=500)), ("Robot hurleur", dict(v2level=80, v2wave=3, v2pitch=11, modsrc=2, mgrate=7, lpmg=40)),
    ]:
        add(nm, FX, mix(FXB, **kw))
    return b
