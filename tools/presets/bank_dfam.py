# Kick Killer (v0.47 (maj. 5 nov.), synthé de percussion analogique à deux VCO, FM, filtre en échelle et séquenceur 8 pas) : 80 sons de plus.
# Chaque son a son motif de 8 pas : hauteurs p1..p8 (-100..100) et vélocités v1..v8.
# vco1wave / vco2wave : 0 triangle, 1 carré · vcfmode : 0 passe-bas, 1 passe-haut · pitchto : 0 VCO 1 et 2, 1 arrêt, 2 VCO 2
from banks_common import Bank, mix, BASS, FX, PERC, SEQ

KICK = dict(vco1freq=50, vco1eg=45, vcodecay=60, mix1=100, mix2=0, mixnoise=0, cutoff=600, reso=10, vcfeg=30, vcfdecay=120, vcadecay=480, prange=0, pitchto=1)
TOM = dict(vco1freq=130, vco1eg=25, vcodecay=100, mix1=100, cutoff=2200, reso=15, vcfeg=25, vcfdecay=150, vcadecay=380, prange=12)
SNARE = dict(vco1freq=190, vco1eg=20, vcodecay=40, mix1=55, mixnoise=100, cutoff=5000, reso=10, vcfeg=35, vcfdecay=120, vcadecay=190, pitchto=1)
HAT = dict(vco1freq=2800, vco1wave=1, vco2freq=4300, vco2wave=1, fm=65, mix1=50, mix2=50, mixnoise=45, vcfmode=1, cutoff=6000, reso=30,
           vcfeg=0, vcaattack=0, vcadecay=70, vco1eg=0, prange=6, pitchto=1)
SEQB = dict(vco1freq=110, vco1wave=1, vco1eg=0, mix1=90, cutoff=600, reso=45, vcfeg=55, vcfdecay=180, vcadecay=220, prange=12)
BASSB = dict(vco1freq=55, vco1wave=1, vco2freq=55.4, mix2=70, vco1eg=0, cutoff=260, reso=55, vcfeg=60, vcfdecay=200, vcadecay=280, prange=12)

FOUR = ([0] * 8, [100, 30, 60, 30, 100, 30, 70, 40])
OFF = ([0] * 8, [40, 100, 40, 100, 40, 100, 40, 100])
BACK = ([0] * 8, [50, 40, 100, 40, 50, 40, 100, 45])
EIGHTS = ([0, 10, 0, 20, 0, 10, 0, 30], [100, 40, 70, 40, 100, 40, 70, 55])
MEL = [
    [0, 30, -20, 50, 0, 70, -40, 20], [0, 0, 25, 0, -25, 0, 58, 33], [0, 50, 20, 80, 0, 50, 30, 100], [80, -30, 40, -60, 20, 90, -10, 50],
    [0, 12, 24, 36, 48, 36, 24, 12], [0, -50, 0, 50, 0, -50, 100, 50], [-100, -60, -20, 20, 60, 100, 60, 20], [0, 0, 70, 0, 0, 35, 0, 100],
]
ACC = [[100, 60, 90, 50, 100, 70, 85, 60], [100, 50, 50, 100, 50, 50, 100, 70], [100, 80, 60, 40, 100, 80, 60, 40], [90, 90, 50, 90, 90, 50, 90, 100]]


def steps(p, v):
    out = {f"p{k + 1}": x for k, x in enumerate(p)}
    out.update({f"v{k + 1}": x for k, x in enumerate(v)})
    return out


def build():
    b = Bank("dfam")
    add = b.add

    # --- Percussions (40)
    for nm, st, kw in [
        ("Grosse caisse ronde", FOUR, {}), ("Grosse caisse sèche", FOUR, dict(vcadecay=250, vco1eg=55, vcodecay=40)),
        ("Grosse caisse longue", FOUR, dict(vcadecay=1200, vco1freq=42)), ("Grosse caisse qui claque", FOUR, dict(vco1eg=80, vcodecay=25, mixnoise=10)),
        ("Grosse caisse carrée", FOUR, dict(vco1wave=1, cutoff=300, reso=20)), ("Grosse caisse saturée", FOUR, dict(vco1eg=60, reso=40, cutoff=900, vcfeg=50)),
        ("Grosse caisse sub", FOUR, dict(vco1freq=38, vco1eg=30, vcadecay=900, cutoff=250)), ("Grosse caisse techno", FOUR, dict(vco1freq=48, vco1eg=70, vcodecay=50, mixnoise=8, vcadecay=380)),
        ("Grosse caisse molle", FOUR, dict(vco1eg=20, vcodecay=90, cutoff=350)), ("Grosse caisse FM", FOUR, dict(vco2freq=90, fm=35, vco2eg=30)),
    ]:
        add(nm, PERC, mix(KICK, steps(*st), **kw))
    for nm, st, kw in [
        ("Tom grave", EIGHTS, dict(vco1freq=85, prange=6)), ("Tom aigu", EIGHTS, dict(vco1freq=210, prange=6)), ("Toms qui chantent", (MEL[0], ACC[0]), {}),
        ("Tom électronique", (MEL[4], ACC[2]), dict(vco1eg=45, vcodecay=70)), ("Tom carré", (MEL[0], ACC[1]), dict(vco1wave=1, cutoff=1200)),
        ("Tom FM", (MEL[5], ACC[0]), dict(vco2freq=300, fm=40, vco2eg=20)), ("Conga", (MEL[7], ACC[1]), dict(vco1freq=260, vco1eg=15, vcodecay=50, vcadecay=200, reso=35)),
        ("Bongos", (MEL[2], ACC[3]), dict(vco1freq=380, vco1eg=18, vcadecay=150, reso=30)), ("Tabla", (MEL[3], ACC[0]), dict(vco1freq=300, vco1eg=-25, vcodecay=200, reso=50, vcadecay=350)),
        ("Timbales", (MEL[1], ACC[2]), dict(vco1freq=330, vco1wave=1, cutoff=3000, reso=25, vcadecay=260)),
    ]:
        add(nm, PERC, mix(TOM, steps(*st), **kw))
    for nm, st, kw in [
        ("Caisse claire sèche", BACK, dict(vcadecay=130)), ("Caisse claire grasse", BACK, dict(vco1freq=160, mix1=80, vcadecay=260)),
        ("Caisse claire filtrée", BACK, dict(vcfmode=1, cutoff=1200, reso=20)), ("Claquement", BACK, dict(mix1=0, vcadecay=260, cutoff=3500, reso=30, velsens=0)),
        ("Coup de bord", BACK, dict(mixnoise=30, mix1=100, vco1freq=420, vco1wave=1, vcadecay=140, cutoff=3000, velsens=0)), ("Caisse claire résonante", BACK, dict(reso=45, cutoff=3500, vcadecay=300, velsens=0)),
        ("Caisse claire électro", BACK, dict(vco1eg=50, vcodecay=30, mixnoise=70)), ("Claquement long", BACK, dict(mix1=0, vcadecay=700, cutoff=3500, reso=25, velsens=0)),
    ]:
        add(nm, PERC, mix(SNARE, steps(*st), **kw))
    for nm, st, kw in [
        ("Charley fermé", EIGHTS, dict(vcadecay=45)), ("Charley ouvert", OFF, dict(vcadecay=450, cutoff=4000, velsens=0)), ("Charley bruit", EIGHTS, dict(mix1=0, mix2=0, mixnoise=100, vcadecay=60)),
        ("Cymbale", ([0] * 8, [100, 0, 0, 0, 60, 0, 0, 0]), dict(vcadecay=1500, cutoff=5000)), ("Cloche de vache", OFF, dict(vco1freq=560, vco2freq=845, fm=0, mixnoise=0, vcfmode=0, cutoff=3000, reso=40, vcadecay=180)),
        ("Shaker", EIGHTS, dict(mix1=0, mix2=0, mixnoise=100, vcaattack=1, vcadecay=80, cutoff=7000)), ("Métal frappé", (MEL[3], ACC[0]), dict(fm=90, vcadecay=300)),
        ("Clochette", (MEL[4], ACC[2]), dict(vco1freq=1200, vco2freq=3100, fm=30, mixnoise=0, vcfmode=0, cutoff=8000, vcadecay=400)),
        ("Triangle", OFF, dict(vco1freq=2200, vco1wave=0, vco2freq=3300, fm=10, mixnoise=0, vcfmode=0, cutoff=9000, vcadecay=900, velsens=0)),
        ("Charleys en marche", EIGHTS, dict(vcadecay=90, prange=24, pitchto=0)),
        ("Zap", FOUR, dict(vco1freq=900, vco1eg=100, vcodecay=40, mix2=0, mixnoise=0, vcfmode=0, cutoff=8000, vcadecay=120, pitchto=1)),
        ("Percussion de verre", (MEL[2], ACC[1]), dict(vco1freq=1800, vco2freq=2700, fm=50, vcadecay=500, reso=60, vcfmode=0, cutoff=4000)),
    ]:
        add(nm, PERC, mix(HAT, steps(*st), **kw))

    # --- Séquences (20)
    for k in range(20):
        p = MEL[k % len(MEL)]
        v = ACC[k % len(ACC)]
        kw = [
            dict(), dict(vco1wave=0), dict(reso=70, cutoff=400), dict(fm=30, vco2freq=330), dict(sync=1, vco2freq=330, mix2=60, mix1=0),
            dict(vcfmode=1, cutoff=300), dict(mixnoise=25), dict(prange=24), dict(prange=48, vcadecay=150), dict(vcadecay=500, vcfdecay=400),
            dict(vco1freq=220), dict(vco1freq=55, cutoff=250), dict(seqlen=5), dict(seqlen=7, reso=60), dict(seqlen=6, fm=50, vco2freq=170),
            dict(vco1eg=40, vcodecay=60), dict(pitchto=2, mix2=80, vco2freq=165), dict(reso=85, vcfeg=80, cutoff=200), dict(vcaattack=1, vcadecay=300), dict(drift=80),
        ][k]
        names = ["Séquence carrée", "Séquence triangle", "Séquence résonante", "Séquence FM", "Séquence synchro", "Séquence passe-haut", "Séquence bruitée",
                 "Séquence deux octaves", "Séquence quatre octaves", "Séquence longue", "Séquence aiguë", "Séquence grave", "Séquence en 5", "Séquence en 7",
                 "Séquence en 6 FM", "Séquence qui tombe", "Séquence sur le VCO 2", "Séquence qui crie", "Séquence douce", "Séquence qui dérive"]
        add(names[k], SEQ, mix(SEQB, steps(p, v), **kw))

    # --- Basses (10)
    for nm, k, kw in [
        ("Basse en boucle", 1, {}), ("Basse carrée en boucle", 1, dict(vco2wave=1)), ("Basse acide 8 pas", 2, dict(reso=80, vcfeg=75)),
        ("Basse grave en boucle", 6, dict(vco1freq=41, vco2freq=41.3, prange=12)), ("Basse FM en boucle", 3, dict(fm=40)),
        ("Basse note jouée", 0, dict(seqmode=1)), ("Basse qui gronde", 1, dict(drift=60, reso=40, cutoff=350)), ("Basse pincée en boucle", 7, dict(vcadecay=150, vcfdecay=120)),
        ("Basse synchro en boucle", 5, dict(sync=1, vco2freq=110, mix1=0, mix2=90)), ("Basse sourde en boucle", 1, dict(cutoff=150, reso=20)),
    ]:
        add(nm, BASS, mix(BASSB, steps(MEL[k], ACC[k % 4]), **kw))

    # --- Effets (10)
    for nm, kw in [
        ("Goutte", dict(vco1freq=600, vco1eg=-60, vcodecay=120, reso=70, cutoff=1500, vcadecay=200)),
        ("Laser", dict(vco1freq=2000, vco1eg=-100, vcodecay=150, cutoff=8000, vcadecay=250)),
        ("Pas sur le gravier", dict(mix1=0, mixnoise=100, cutoff=900, reso=40, vcfeg=50, vcadecay=120)),
        ("Bip de sonar", dict(vco1freq=1200, vco1eg=0, reso=80, cutoff=1200, vcadecay=900)),
        ("Respiration", dict(mix1=0, mixnoise=80, vcaattack=1, vcadecay=1400, cutoff=1500, vcfeg=20)),
        ("Ressort", dict(vco1freq=200, fm=70, vco2freq=37, vco2eg=60, vcadecay=700, reso=40)),
        ("Électricité", dict(vco1freq=1500, vco1wave=1, fm=100, vco2freq=61, mixnoise=60, vcadecay=500, cutoff=6000)),
        ("Chute", dict(vco1freq=1500, vco1eg=-100, vcodecay=900, vcadecay=1200, cutoff=3000)),
        ("Insecte", dict(vco1freq=4000, fm=60, vco2freq=27, vcadecay=200, vcfmode=1, cutoff=2000)),
        ("Porte qui grince", dict(vco1freq=90, fm=90, vco2freq=15, reso=60, vcaattack=1, vcadecay=1500, cutoff=700)),
    ]:
        add(nm, FX, mix(TOM, steps(MEL[3], ACC[0]), **kw))
    return b
