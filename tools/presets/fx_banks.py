# Groovebox — OFAD — AGPL-3.0-or-later
# v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires des effets (au moins 20 par effet). Réglages originaux, construits
# sur les usages courants de chaque famille d'effet (aucun préréglage d'un autre fabricant n'est recopié).
# Écrit src/effects/FxBanks.h par fxgen.py.


def eqb(n, typ, freq, gain=0.0, q=0.9, on=1):
    return {f"b{n}_on": on, f"b{n}_type": typ, f"b{n}_freq": freq, f"b{n}_gain": gain, f"b{n}_q": q}


def merge(*ds, **kw):
    out = {}
    for d in ds:
        out.update(d)
    out.update(kw)
    return out


# Types de bande de l'égaliseur : 0 cloche, 1 plateau bas, 2 plateau haut, 3 coupe-bas 12, 4 coupe-bas 24, 5 coupe-haut 12,
# 6 coupe-haut 24, 7 coupe-bande, 8..11 coupe-bas/haut 36 et 48.
EQ = [
    ("Basse : rondeur et attaque", merge(eqb(1, 3, 35), eqb(3, 0, 90, 3, 1.2), eqb(4, 0, 300, -3, 1.4), eqb(6, 0, 1800, 3, 1.5))),
    ("Synthé : enlever la boue", merge(eqb(1, 4, 120), eqb(4, 0, 350, -4, 1.2), eqb(7, 2, 9000, 2))),
    ("Charley : air", merge(eqb(1, 4, 400), eqb(7, 2, 11000, 4), eqb(5, 0, 2500, -2, 2))),
    ("Toms : corps", merge(eqb(1, 3, 50), eqb(3, 0, 120, 3, 1.5), eqb(4, 0, 450, -4, 2), eqb(6, 0, 4000, 3, 1.4))),
    ("Radio AM", merge(eqb(1, 4, 500), eqb(8, 6, 3500), eqb(4, 0, 1500, 6, 1.2))),
    ("Mégaphone", merge(eqb(1, 9, 700), eqb(8, 11, 4000), eqb(4, 0, 1800, 9, 2))),
    ("Creux du médium", merge(eqb(4, 0, 800, -5, 0.8))),
    ("Sourire (graves et aigus)", merge(eqb(2, 1, 100, 4), eqb(7, 2, 9000, 4))),
    ("Coupe-bas doux", merge(eqb(1, 3, 80))),
    ("Coupe-bas raide", merge(eqb(1, 9, 100))),
    ("Nappe au fond", merge(eqb(1, 4, 200), eqb(7, 5, 6000), eqb(5, 0, 2500, -3, 1))),
    ("Lead en avant", merge(eqb(1, 4, 150), eqb(5, 0, 2800, 4, 1.2), eqb(7, 2, 10000, 2))),
    ("Grave chaud", merge(eqb(2, 1, 180, 3), eqb(7, 2, 8000, -2))),
    ("Clavier clair", merge(eqb(1, 3, 90), eqb(4, 0, 400, -2), eqb(6, 0, 3500, 3, 1))),
]

OPTO = [
    ("Voix douce", dict(peak=30, gain=4)), ("Voix tenue", dict(peak=60, gain=9)), ("Basse ronde", dict(peak=55, gain=8, emphasis=0)),
    ("Batterie parallèle", dict(peak=75, gain=14, mix=40)), ("Synthé lissé", dict(peak=40, gain=5)), ("Piano égal", dict(peak=35, gain=5, emphasis=30)),
    ("Bus de nappes", dict(peak=25, gain=3)), ("Limitation dure", dict(mode=1, peak=70, gain=10)), ("Aigus calmés", dict(peak=45, gain=6, emphasis=80)),
    ("Guitare acoustique", dict(peak=40, gain=6, emphasis=40)), ("Écrasement doux", dict(peak=90, gain=18, mix=50)), ("Couleur légère", dict(peak=15, gain=2)),
    ("Pompe par la grosse caisse", dict(peak=70, gain=6, scsrc=1)), ("Lead tenu", dict(peak=55, gain=8)),
]

FET = [
    ("Grosse caisse", dict(input=20, attack=400, release=150, ratio=0)), ("Voix pop", dict(input=22, attack=300, release=300, ratio=0)),
    ("Voix rap serrée", dict(input=28, attack=600, release=500, ratio=1)), ("Guitare électrique", dict(input=24, attack=200, release=350, ratio=0)),
    ("Bus batterie", dict(input=26, attack=100, release=600, ratio=0, mix=60)), ("Basse funk", dict(input=24, attack=500, release=200, ratio=1)),
    ("Toms qui ressortent", dict(input=30, attack=80, release=700, ratio=2, mix=50)), ("Pièce écrasée", dict(input=40, attack=800, release=1100, ratio=4, mix=35)),
    ("Synthé mordant", dict(input=22, attack=300, release=250, ratio=0)), ("Limitation", dict(input=30, attack=700, release=200, ratio=3)),
    ("Charley dompté", dict(input=20, attack=500, release=150, ratio=1)), ("Piano rock", dict(input=20, attack=250, release=400, ratio=0)),
    ("Tous les boutons", dict(input=36, attack=800, release=1000, ratio=4, mix=70)), ("Ambiance de salle", dict(input=34, attack=700, release=900, ratio=4, mix=45)),
    ("Voix discrète", dict(input=14, attack=400, release=400, ratio=0)),
]

BUS = [
    ("Colle douce", dict(threshold=-8, ratio=0, attack=4, release=4, makeup=1)), ("Colle serrée", dict(threshold=-16, ratio=1, attack=3, release=1, makeup=4)),
    ("Batterie qui claque", dict(threshold=-18, ratio=1, attack=4, release=0, makeup=5)), ("Pompe au tempo", dict(threshold=-24, ratio=2, attack=0, release=1, makeup=6)),
    ("Bus de synthés", dict(threshold=-12, ratio=0, attack=3, release=2, makeup=2)), ("Bus de basses", dict(threshold=-14, ratio=1, attack=4, release=2, makeup=3, schpf=20)),
    ("Master qui respire", dict(threshold=-6, ratio=0, attack=5, release=4, makeup=1)), ("Parallèle lourd", dict(threshold=-30, ratio=2, attack=1, release=1, makeup=10, mix=35)),
    ("Grave libre", dict(threshold=-14, ratio=1, attack=3, release=4, makeup=3, schpf=150)), ("Voix en groupe", dict(threshold=-12, ratio=1, attack=2, release=2, makeup=3)),
    ("Pompe sur la grosse caisse", dict(threshold=-28, ratio=2, attack=0, release=1, makeup=4, scsrc=1)), ("Nappes lissées", dict(threshold=-10, ratio=0, attack=5, release=3, makeup=2)),
    ("Bus percussions", dict(threshold=-16, ratio=1, attack=2, release=1, makeup=4)), ("Sécurité légère", dict(threshold=-4, ratio=2, attack=0, release=0, makeup=0)),
    ("Retour d'effets", dict(threshold=-20, ratio=1, attack=3, release=3, makeup=4)),
]

COMP = [
    ("Synthé lead", dict(threshold=-16, ratio=4, attack=8, release=100, style=0)), ("Nappe égale", dict(threshold=-20, ratio=2.5, attack=40, release=400, style=2)),
    ("Grosse caisse sèche", dict(threshold=-14, ratio=5, attack=20, release=60, style=6)), ("Caisse claire claquante", dict(threshold=-18, ratio=6, attack=15, release=80, style=6)),
    ("Charley calmé", dict(threshold=-22, ratio=3, attack=2, release=50, style=0)),
]

LIMITER = [
    ("Club très fort", dict(gain=10, ceiling=-0.3, release=40, style=1, clip=1)), ("Master doux", dict(gain=3, ceiling=-1, release=200, style=0)),
    ("Bus de basses", dict(gain=4, ceiling=-3, release=60, style=0)), ("Voix tenue", dict(gain=4, ceiling=-6, release=100, style=0)),
    ("Écrêtage franc", dict(gain=8, ceiling=-1, release=10, style=1, clip=1)), ("Diffusion radio", dict(gain=7, ceiling=-1, release=50, style=1, truePeak=1)),
    ("Grosse caisse en avant", dict(gain=5, ceiling=-2, release=20, style=1)), ("Batterie écrasée", dict(gain=12, ceiling=-3, release=30, style=1, clip=1)),
    ("Dynamique gardée", dict(gain=1, ceiling=-0.5, release=300, style=0)), ("Podcast (-16 LUFS)", dict(gain=4, ceiling=-1.5, release=150, style=0, truePeak=1)),
    ("Nappes au plafond", dict(gain=6, ceiling=-6, release=400, style=0)), ("Synthés poussés", dict(gain=6, ceiling=-2, release=80, style=1)),
    ("Pré-master", dict(gain=2, ceiling=-3, release=120, style=0, truePeak=1)), ("Effets tenus", dict(gain=8, ceiling=-8, release=250, style=0)),
    ("Plafond -6 dB", dict(gain=0, ceiling=-6, release=80, style=0)),
]

MASTER = [
    ("Techno (-8 LUFS)", dict(drive=8, ceiling=-0.5, character=60, clipAmt=40, shine=20, warmth=20)),
    ("House chaude", dict(drive=6, warmth=35, shine=10, lowGain=1, ceiling=-0.8)), ("Hip-hop grave", dict(drive=6, lowGain=2, warmth=30, gainLow=1, ceiling=-0.8)),
    ("Ambient ouvert", dict(drive=1, shine=25, warmth=10, widthHigh=130, ceiling=-1)), ("Pop brillante", dict(drive=5, shine=35, airGain=2, ceiling=-1)),
    ("Démo rapide", dict(drive=4, ceiling=-1)), ("Lo-fi douce", dict(drive=3, airGain=-3, warmth=40, shine=0, ceiling=-1.5)),
    ("Mono dans le grave", dict(widthLow=0, splitLow=1, drive=4, ceiling=-1)), ("Large et aéré", dict(widthMid=120, widthHigh=150, shine=25, ceiling=-1)),
    ("Médium en avant", dict(midFreq=2000, midGain=2, drive=4, ceiling=-1)), ("Dynamique gardée", dict(dynOn=0, drive=2, ceiling=-1)),
    ("Drum and bass", dict(drive=8, gainLow=1, thrLow=-16, shine=20, ceiling=-0.5)), ("Dub profond", dict(lowGain=2, airGain=-2, warmth=30, drive=5, ceiling=-1)),
    ("Électro dure", dict(drive=10, character=80, clipAmt=60, ceiling=-0.3)),
]

GATE = [
    ("Grosse caisse sèche", dict(gThresh=-30, gRange=60, gAttack=0.1, gHold=40, gRelease=80)), ("Toms courts", dict(gThresh=-35, gRange=50, gHold=80, gRelease=200)),
    ("Bruit de fond léger", dict(gThresh=-60, gRange=15, gHold=50, gRelease=300)), ("Nappe hachée", dict(gThresh=-20, gRange=80, gAttack=0.5, gHold=0, gRelease=20)),
    ("Porte lente", dict(gThresh=-45, gRange=30, gAttack=10, gHold=100, gRelease=800)), ("Dé-esseur doux", dict(gRange=0, dsThresh=-28, dsAmount=6, dsFreq=6500)),
    ("Dé-esseur fort", dict(gRange=0, dsThresh=-35, dsAmount=14, dsFreq=7000)), ("Cymbales adoucies", dict(gRange=0, dsThresh=-30, dsAmount=8, dsFreq=9000, dsMode=1)),
    ("Grave seul (déclenchement)", dict(gThresh=-30, gRange=60, gHp=20, gHold=30, gRelease=100)), ("Coupure nette", dict(gThresh=-25, gRange=80, gAttack=0.05, gHold=10, gRelease=10)),
    ("Porte et respiration", dict(gThresh=-40, gRange=25, gHold=150, gRelease=600)), ("Caisse claire 80", dict(gThresh=-20, gRange=80, gHold=200, gRelease=60)),
]

MBAND = [
    ("Basses : grave serré", dict(b1thr=-24, b1rat=4, b1att=20, b1rel=120)), ("Synthé : médium calmé", dict(b2thr=-22, b2rat=3, b3thr=-22, b3rat=3)),
    ("Nappe : aigus doux", dict(b4thr=-26, b4rat=3, b4att=5, b4rel=200)), ("Batterie : grave en avant", dict(b1gain=2, b1thr=-20, b1rat=3, b4thr=-24, b4rat=2)),
    ("Expansion des transitoires", dict(b3mode=1, b3thr=-30, b3rat=2, b4mode=1, b4thr=-30, b4rat=2)), ("Master quatre bandes", dict(b1thr=-16, b1rat=2, b2thr=-18, b2rat=1.5, b3thr=-18, b3rat=1.5, b4thr=-20, b4rat=2)),
    ("Voix : présence", dict(b3gain=2, b3thr=-24, b3rat=2)), ("Aigus seuls (écoute)", dict(b4solo=1)), ("Médium seul (écoute)", dict(b2solo=1, b3solo=1)),
    ("Grave dynamique", dict(f1on=1, f1freq=80, f1thr=-24, f1range=-6)), ("Chuintantes dynamiques", dict(f2on=1, f2freq=7000, f2thr=-30, f2range=-8)),
    ("Boue dynamique", dict(f1on=1, f1freq=300, f1thr=-26, f1range=-6, f1q=1.2)), ("Colle légère", dict(b1thr=-10, b1rat=1.5, b2thr=-10, b2rat=1.5, b3thr=-10, b3rat=1.5, b4thr=-10, b4rat=1.5)),
]

VARIMU = [
    ("Voix en douceur", dict(input=4, threshold=-12, tc=1, tube=20)), ("Nappes collées", dict(input=6, threshold=-16, tc=3, tube=30)),
    ("Basse chaude", dict(input=10, threshold=-18, tc=0, tube=50, output=-5)), ("Bus de synthés", dict(input=6, threshold=-14, tc=4, tube=35)),
    ("Lampe poussée", dict(input=18, threshold=-6, tc=1, tube=80, output=-10)), ("Master gras", dict(input=8, threshold=-12, tc=4, tube=40, output=-4)),
    ("Parallèle épais", dict(input=14, threshold=-24, tc=0, tube=60, mix=40, output=-6)), ("Milieu et côtés", dict(link=1, input=6, threshold=-14, tc=2)),
    ("Lent et musical", dict(input=6, threshold=-16, tc=5, tube=30)), ("Piano rond", dict(input=4, threshold=-14, tc=1, tube=25)),
    ("Batterie dense", dict(input=12, threshold=-20, tc=0, tube=45, output=-6)), ("Pompe par la grosse caisse", dict(input=6, threshold=-22, tc=0, scsrc=1)),
    ("Guitare chaude", dict(input=8, threshold=-14, tc=1, tube=55)), ("Effets lissés", dict(input=6, threshold=-18, tc=3, tube=20)),
]

TUBEEQ = [
    ("Basse ronde", dict(lowfreq=2, lowboost=5, lowatten=2)), ("Voix brillante", dict(hifreq=5, hiboost=4, hibw=4)), ("Synthé aéré", dict(hifreq=6, hiboost=5)),
    ("Grave puissant", dict(lowfreq=1, lowboost=6, lowatten=3)), ("Caisse claire claquante", dict(hifreq=1, hiboost=5, hibw=3)), ("Nappe adoucie", dict(attfreq=0, hiatten=5)),
    ("Master chaud", dict(lowfreq=2, lowboost=2, hifreq=5, hiboost=2, tube=35)), ("Lampe seule poussée", dict(tube=80)), ("Piano présent", dict(hifreq=2, hiboost=3, hibw=6)),
    ("Guitare brillante", dict(hifreq=3, hiboost=4)), ("Grosse caisse sourde", dict(lowfreq=2, lowboost=6, attfreq=1, hiatten=6)), ("Bus chaleureux", dict(lowfreq=3, lowboost=2, tube=40)),
    ("Charley soyeux", dict(hifreq=6, hiboost=3, attfreq=2, hiatten=3)), ("Basse grasse et sombre", dict(lowfreq=2, lowboost=7, lowatten=4, attfreq=0, hiatten=4, tube=50)),
]

CONSOLEEQ = [
    ("Basse qui perce", dict(hpf=30, lfgain=4, lffreq=80, lmfgain=-3, lmffreq=300, hmfgain=4, hmffreq=1200)),
    ("Grosse caisse sèche", dict(hpf=30, lfgain=5, lffreq=60, lmfgain=-6, lmffreq=400, hmfgain=4, hmffreq=3500)),
    ("Toms", dict(hpf=60, lfgain=3, lffreq=100, lmfgain=-4, lmffreq=500, hmfgain=3, hmffreq=4000)), ("Charley", dict(hpf=300, hfgain=4, hffreq=10000)),
    ("Voix rock", dict(hpf=100, lmfgain=-3, lmffreq=350, hmfgain=5, hmffreq=3000, character=2, drive=30)), ("Piano", dict(hpf=40, lmfgain=-2, lmffreq=250, hfgain=3, hffreq=10000)),
    ("Guitare rythmique", dict(hpf=90, lmfgain=-3, lmffreq=300, hmfgain=3, hmffreq=2500)), ("Nappe dans le fond", dict(hpf=150, lpf=9000, hmfgain=-3, hmffreq=2500)),
    ("Synthé gras", dict(hpf=40, lfgain=3, lffreq=120, character=2, drive=50)), ("Console poussée", dict(character=2, drive=70)),
    ("Téléphone", dict(hpf=350, lpf=3500, hmfgain=8, hmffreq=1500, hmfq=1.5)), ("Master de console", dict(hpf=20, lfgain=1.5, lffreq=60, hfgain=1.5, hffreq=12000, character=0, drive=15)),
    ("Lead sans boue", dict(hpf=120, lmfgain=-4, lmffreq=400, hmfgain=2, hmffreq=3000)), ("Effets assombris", dict(hpf=200, lpf=6000)),
]

DIST = [
    ("Basse grasse", dict(typeA=1, driveA=55, toneA=40, routing=4, xover=120)), ("Synthé déchiré", dict(typeA=6, driveA=70, toneA=60)),
    ("Bits qui bougent", dict(typeA=7, driveA=50, m1type=1, m1div=8, m1shape=0, m1amt=60, m1dest=0)), ("Batterie en bande", dict(typeA=0, driveA=40, mix=70)),
    ("Lampe et repli", dict(typeA=1, driveA=40, typeB=9, driveB=40, mixB=50, routing=0)), ("Fuzz sur le grave", dict(typeA=6, driveA=60, routing=4, xover=200, balance=30)),
]

GRINDER = [
    ("Basse quatre bandes", dict(b1type=2, b1drive=40, b2type=7, b2drive=50, b3type=3, b3drive=35, b4type=0)),
    ("Médium seul broyé", dict(b1type=0, b2type=7, b2drive=70, b3type=7, b3drive=60, b4type=0)), ("Aigus en bits", dict(b1type=0, b2type=0, b3type=0, b4type=8, b4drive=60)),
    ("Larsen grave", dict(b1fb=60, b1fbfreq=55, b1type=2, b1drive=50)), ("Batterie par bandes", dict(b1type=1, b1drive=40, b2type=3, b2drive=50, b3type=5, b3drive=30, b4type=12, b4drive=30)),
    ("Bandes en stéréo", dict(b1pan=0, b2pan=-60, b3pan=60, b4pan=-30)), ("Repli au suiveur", dict(m1type=2, m1amt=60, m1dest=6, b2type=10)),
    ("Douce chaleur", dict(b1type=1, b1drive=15, b2type=2, b2drive=20, b3type=2, b3drive=15, b4type=1, b4drive=10)),
    ("Grave propre, haut sale", dict(b1type=0, b2type=3, b2drive=45, b3type=7, b3drive=55, b4type=8, b4drive=40)),
    ("Deux bandes", dict(bands=0, b1type=2, b1drive=40, b2type=7, b2drive=55)), ("Trois bandes", dict(bands=1, b1type=1, b1drive=30, b2type=3, b2drive=45, b3type=9, b3drive=35)),
]

TAPESAT = [
    ("Grave de bande", dict(drive=8, bump=5, speed=0)), ("Aigus doux", dict(drive=6, treble=-5)), ("Bande poussée fort", dict(drive=18, bias=20, output=-8)),
    ("Souffle et pleurage", dict(drive=4, wow=50, hiss=40)), ("Master légère", dict(drive=3, speed=2, output=-2)), ("Basse saturée", dict(drive=14, bump=4, bias=-30)),
    ("Voix chaude", dict(drive=6, treble=-2, wow=5)), ("Cassette fatiguée", dict(drive=10, speed=0, treble=-8, wow=60, hiss=50)), ("Bande neuve", dict(drive=4, speed=2, wow=0, hiss=0)),
    ("Batterie dense", dict(drive=14, speed=1, bump=3, mix=70)), ("Synthés collés", dict(drive=7, speed=1, wow=8)), ("Nappe usée", dict(drive=5, wow=40, treble=-6, hiss=20)),
    ("Bande en parallèle", dict(drive=20, mix=40)), ("Polarisation basse", dict(drive=10, bias=-70)), ("Polarisation haute", dict(drive=10, bias=70)), ("Bande discrète", dict(drive=2, wow=5, output=0)),
]

TUBESAT = [
    ("Lampe douce", dict(drive=20, bias=10, sag=20)), ("Lampe poussée", dict(drive=70, sag=50, output=-6)), ("Basse lampe", dict(drive=50, tone=35, bias=30)),
    ("Synthé chaud", dict(drive=35, tone=55)), ("Préampli brillant", dict(drive=40, tone=75)), ("Préampli sombre", dict(drive=40, tone=25)),
    ("Ampli qui s'affaisse", dict(drive=60, sag=90)), ("Asymétrique", dict(drive=50, bias=80)), ("Parallèle chaud", dict(drive=80, mix=35, output=-6)),
    ("Batterie qui sature", dict(drive=60, sag=40, tone=60, mix=70)), ("Voix radio", dict(drive=55, tone=70, bias=40)), ("Bus coloré", dict(drive=15, tone=50, sag=10)),
    ("Grave arrondi", dict(drive=30, tone=30, sag=60)), ("Lampe à fond", dict(drive=100, sag=70, output=-12)), ("Clavier vintage", dict(drive=45, tone=45, bias=25)),
    ("Effets saturés", dict(drive=65, tone=40, mix=50)),
]

DISTCOMP = [
    ("Batterie de pièce", dict(input=30, ratio=7, attack=0, release=8, mix=50)), ("Basse serrée", dict(input=22, ratio=4, attack=4, release=5)),
    ("Voix rap", dict(input=24, ratio=5, attack=3, release=4, detector=2)), ("Guitare saturée", dict(input=26, ratio=3, audio=1)),
    ("Synthé mordant", dict(input=20, ratio=3, audio=2, attack=5, release=3)), ("Britannique douce", dict(british=1, input=16, ratio=2)),
    ("Grave libre", dict(input=22, ratio=4, detector=1)), ("Nuke parallèle", dict(input=34, ratio=7, mix=30)), ("Couleur distordue", dict(ratio=0, audio=2, input=20)),
    ("Caisse claire qui claque", dict(input=24, ratio=4, attack=8, release=2)), ("Nappe tenue", dict(input=18, ratio=2, attack=6, release=9)),
    ("Bus britannique", dict(british=1, input=20, ratio=3, attack=5, release=6)), ("Lead en avant", dict(input=22, ratio=3, detector=3)),
    ("Grosse caisse", dict(input=20, ratio=4, attack=7, release=3)), ("Effets écrasés", dict(input=30, ratio=6, mix=60)),
]

TRANSIENT = [
    ("Grosse caisse qui claque", dict(attack=70, sustain=-20, speed=0)), ("Caisse claire sèche", dict(attack=50, sustain=-50, speed=0)), ("Toms courts", dict(attack=30, sustain=-60, speed=1)),
    ("Plus de pièce", dict(attack=0, sustain=60, speed=2)), ("Basse percutante", dict(attack=45, sustain=0, speed=1)), ("Pluck plus court", dict(attack=20, sustain=-40, speed=0)),
    ("Nappe adoucie", dict(attack=-50, sustain=20, speed=2)), ("Charley serré", dict(attack=40, sustain=-70, speed=0)), ("Attaque adoucie", dict(attack=-60, sustain=0, speed=1)),
    ("Batterie écrêtée", dict(attack=80, sustain=10, clip=1, output=-3)), ("Piano présent", dict(attack=30, sustain=10, speed=1)), ("Guitare plus nette", dict(attack=35, sustain=-15, speed=0)),
    ("Queue allongée", dict(attack=0, sustain=80, speed=2, mix=70)), ("Coup de poing", dict(attack=100, sustain=-30, speed=0, clip=1)), ("Transitoires en parallèle", dict(attack=90, sustain=-40, mix=50)),
    ("Douceur générale", dict(attack=-30, sustain=-20, speed=2)),
]

DELAY = [
    ("Croche simple", dict(mode=0, division=7, feedback=30, mix=25)), ("Noire pointée", dict(mode=0, division=12, feedback=40, mix=25)),
    ("Ping-pong croche", dict(mode=2, division=7, feedback=45)), ("Triolets", dict(mode=2, division=5, feedback=40)),
    ("Échos diffus", dict(mode=1, division=9, diffusion=70, feedback=50)), ("Seau BBD", dict(echocolour=4, mode=4, division=7, feedback=50)),
    ("Effacement sur la voix", dict(mode=2, division=9, feedback=50, ducking=80)), ("Gel", dict(freeze=1, mix=40, feedback=100)),
    ("Swing", dict(mode=2, division=7, swing=60, feedback=40)), ("Étroit", dict(mode=1, division=10, width=30, feedback=35)),
]

TAPEECHO = [
    ("Tête 1 courte", dict(heads=0, speed=90, intensity=20, echo=40)), ("Tête 2 et ressort", dict(heads=1, spring=50, echo=55)), ("Trois têtes", dict(heads=6, intensity=55, echo=55)),
    ("Croche au tempo", dict(sync=1, division=5, heads=0, intensity=40)), ("Noire pointée au tempo", dict(sync=1, division=7, heads=2, intensity=45)),
    ("Dub qui s'emballe", dict(heads=4, intensity=95, wear=50, drive=40)), ("Bande neuve", dict(wear=0, drive=10)), ("Ressort et écho doux", dict(spring=70, echo=35, intensity=30)),
    ("Sombre", dict(treble=-10, bass=3, intensity=50)), ("Brillant", dict(treble=4, bass=-4, intensity=45)), ("Bande usée sale", dict(wear=90, drive=60, intensity=55)),
    ("Têtes 1 et 3", dict(heads=5, intensity=50)), ("Slapback rockabilly", dict(heads=0, speed=120, intensity=5, echo=50)), ("Nappe en écho", dict(heads=6, intensity=70, echo=45, treble=-6)),
]

REVERB = [
    ("Petite chambre", dict(size=25, decay=0.8, predelay=5)), ("Studio boisé", dict(size=35, decay=1.1, brightness=5000, character=60)),
    ("Grande salle sombre", dict(size=80, decay=3.5, brightness=3500)), ("Arène", dict(size=95, decay=5, predelay=40)),
    ("Voix lointaine", dict(size=60, decay=2.5, distance=85, predelay=60)), ("Batterie en salle", dict(size=50, decay=1.4, lowcut=200)),
    ("Grave dégagé", dict(size=55, decay=2, lowcut=400)), ("Brillante courte", dict(size=30, decay=0.9, brightness=12000)),
    ("Nappe immense", dict(size=100, decay=12, mix=40)), ("Étroite", dict(size=45, decay=1.6, width=30)), ("Très large", dict(size=60, decay=2.4, width=150)),
    ("Gel", dict(freeze=1, mix=50)), ("Caverne", dict(size=100, decay=8, brightness=2500, predelay=80)),
]

VINTVERB = [
    ("Plaque brillante 80", dict(mode=1, color=1, decay=1.8, damping=20)), ("Chambre 70", dict(mode=2, color=0, decay=1.5)), ("Concert actuel", dict(mode=3, color=2, decay=4)),
    ("Ambiance courte", dict(mode=4, decay=0.7)), ("Lumineuse longue", dict(mode=5, decay=8, modulation=60)), ("Salle sombre 70", dict(mode=0, color=0, decay=3, damping=70)),
    ("Caisse claire plaque", dict(mode=1, decay=1.2, predelay=20, lowcut=200)), ("Voix 80", dict(mode=1, color=1, decay=2.2, predelay=40)), ("Gel lumineux", dict(mode=5, freeze=1)),
    ("Petite salle", dict(mode=0, size=30, decay=1.2)), ("Nappe modulée", dict(mode=3, decay=6, modulation=80)), ("Grave tenu", dict(mode=0, bass=90, decay=3)),
    ("Large 80", dict(mode=0, color=1, width=150, decay=2.5)),
]

PLATE = [
    ("Plaque moyenne", dict(decay=1.6)), ("Plaque courte", dict(decay=0.7, brightness=70)), ("Plaque longue", dict(decay=4.5)), ("Plaque sombre", dict(decay=2.2, brightness=25)),
    ("Plaque très brillante", dict(decay=1.8, brightness=95)), ("Voix avec retard", dict(decay=2, predelay=60)), ("Batterie serrée", dict(decay=0.9, lowcut=300, mix=20)),
    ("Grave dégagé", dict(decay=2, lowcut=400)), ("Étroite", dict(decay=1.6, width=40)), ("Très large", dict(decay=2, width=150)), ("Nappe de plaque", dict(decay=5.5, brightness=50, mix=40)),
    ("Guitare", dict(decay=1.4, predelay=25)), ("Piano", dict(decay=2.2, brightness=55)), ("Synthé lead", dict(decay=1.8, predelay=40, mix=25)), ("Caisse claire 80", dict(decay=2.5, brightness=80, mix=40)),
    ("Plaque discrète", dict(decay=1.2, mix=15)),
]

SPRING = [
    ("Un ressort", dict(springs=1, decay=45)), ("Trois ressorts", dict(springs=3, decay=60)), ("Ressort court", dict(decay=25)), ("Ressort long", dict(decay=85)),
    ("Très gouttelant", dict(drip=90)), ("Sans goutte", dict(drip=0)), ("Tendu", dict(tension=90)), ("Détendu", dict(tension=10)), ("Sombre", dict(tone=20)), ("Brillant", dict(tone=85)),
    ("Guitare surf", dict(springs=3, drip=80, decay=60, mix=45)), ("Orgue", dict(springs=2, decay=50, tone=40, mix=25)), ("Batterie dub", dict(springs=2, decay=75, drip=70, mix=40)),
    ("Voix rétro", dict(springs=2, decay=40, tone=60, mix=25)), ("Synthé", dict(springs=3, decay=55, mix=30)), ("Effet de choc", dict(springs=3, decay=90, drip=100, tension=80, mix=60)),
]

PRESTIGE = [
    ("Salle de concert", dict(mode=0, size=150, decay=3)), ("Chambre courte", dict(mode=1, size=50, decay=0.9)), ("Plaque longue", dict(mode=2, decay=3.5)),
    ("Cathédrale", dict(mode=3, size=200, decay=8)), ("Studio moyen", dict(mode=4, size=80, decay=1.2)), ("Halo court", dict(mode=5, decay=4)),
    ("Grave allongé", dict(mode=0, lowmult=1.8, decay=2.5)), ("Aigus courts", dict(mode=0, highmult=0.3)), ("Premières réflexions", dict(mode=1, early=90, decay=0.6)),
    ("Modulée", dict(mode=0, modulation=80, decay=3)), ("Retard long", dict(mode=0, predelay=120, decay=2.5)), ("Gel", dict(mode=5, freeze=1)),
    ("Étroite", dict(mode=4, width=40)), ("Infinie", dict(mode=5, decay=30, mix=40)),
]

CHORUS = [
    ("Trois voix", dict(voices=1, rate=0.5, depth=2.5)), ("Quatre voix larges", dict(voices=2, rate=0.4, depth=3.5)), ("Chorus rapide", dict(rate=2.5, depth=1.5)),
    ("Chorus lent", dict(rate=0.15, depth=4)), ("Clavier électrique", dict(rate=0.8, depth=2, mix=40)), ("Basse", dict(rate=0.6, depth=1.5, tone=4000, mix=35)),
    ("Guitare claire", dict(rate=0.9, depth=2.5, mix=45)), ("Nappe large", dict(voices=2, rate=0.25, depth=4, mix=55)), ("Flanger doux", dict(delay=2, feedback=40, depth=2, rate=0.3)),
    ("Calé sur la mesure", dict(sync=1, division=4, depth=3)), ("Calé à la noire", dict(sync=1, division=6, depth=2)), ("Sombre", dict(tone=3000, depth=3)),
    ("Étroit", dict(width=30, depth=2)), ("Réinjecté", dict(feedback=50, depth=2)), ("Vibrato rapide", dict(mix=100, rate=6, depth=1.5)), ("Vibrato lent", dict(mix=100, rate=1.5, depth=3)),
]

PHASER = [
    ("Quatre étages", dict(stages=0, rate=0.5)), ("Douze étages", dict(stages=3, rate=0.3, depth=80)), ("Rapide", dict(rate=3, depth=60)), ("Très lent", dict(rate=0.05, depth=90)),
    ("Grave", dict(center=300)), ("Aigu", dict(center=3000)), ("Réinjection forte", dict(feedback=85)), ("Réinjection inversée", dict(feedback=-70)), ("Mono", dict(stereo=0)),
    ("Croche", dict(sync=1, division=7)), ("Noire", dict(sync=1, division=6)), ("Deux mesures", dict(sync=1, division=3)), ("Doux", dict(depth=40, mix=35, feedback=10)),
    ("Guitare funk", dict(stages=0, rate=1.2, depth=70, feedback=30)), ("Nappe qui tourne", dict(stages=2, rate=0.12, depth=85, mix=60)), ("Clavier électrique", dict(stages=0, rate=0.7, feedback=20)),
    ("Effet de jet", dict(stages=3, rate=0.08, depth=100, feedback=80, mix=60)),
]

FLANGER = [
    ("Rapide", dict(rate=2, depth=50)), ("Grave", dict(manual=6, feedback=40)), ("Aigu", dict(manual=0.6)), ("Réinjection forte", dict(feedback=90)),
    ("Inversé lent", dict(mode=1, rate=0.1, feedback=-70)), ("Croche", dict(sync=1, division=7)), ("Noire", dict(sync=1, division=6)), ("Doux", dict(depth=30, mix=30, feedback=20)),
    ("Mono", dict(stereo=0)), ("Peigne grave figé", dict(mode=2, manual=8, feedback=70)), ("Batterie", dict(rate=0.15, depth=70, feedback=60, mix=40)),
    ("Guitare", dict(rate=0.4, depth=60, feedback=50)), ("Sombre", dict(tone=3500)), ("Nappe", dict(rate=0.05, depth=80, feedback=40, mix=40)),
]

PHASER2 = [
    ("Deux lents", dict(rateA=0.1, rateB=0.15)), ("Deux rapides", dict(rateA=2, rateB=2.7)), ("Parallèle grave et aigu", dict(routing=1, centerA=300, centerB=3000)),
    ("Série doux", dict(routing=0, depthA=40, depthB=40, feedbackA=10, feedbackB=10)), ("Stéréo calé", dict(routing=2, sync=1, divisionA=4, divisionB=5)),
    ("Douze étages", dict(stagesA=3, stagesB=3)), ("Quatre étages", dict(stagesA=0, stagesB=0)), ("Réinjection croisée", dict(feedbackA=80, feedbackB=-60)),
    ("Croche et noire", dict(sync=1, divisionA=7, divisionB=6)), ("Nappe", dict(rateA=0.08, rateB=0.11, depthA=90, depthB=80, mix=60)), ("Guitare", dict(rateA=0.8, rateB=1.1)),
    ("Grave seul", dict(centerA=250, centerB=400)), ("Aigus seuls", dict(centerA=2500, centerB=5000)), ("Effet de jet", dict(stagesA=3, rateA=0.06, depthA=100, feedbackA=85)),
    ("Discret", dict(depthA=30, depthB=25, mix=30)),
]

DIMENSION = [
    ("Réglage 1 sombre", dict(mode=0, tone=5000)), ("Réglage 2 étroit", dict(mode=1, width=50)), ("Réglage 3 brillant", dict(mode=2, tone=14000)),
    ("Réglage 4 en parallèle", dict(mode=3, mix=50)), ("Clavier", dict(mode=1, mix=60)), ("Basse", dict(mode=0, tone=4000, mix=50)), ("Voix", dict(mode=1, mix=40)),
    ("Guitare", dict(mode=2, mix=70)), ("Synthé lead", dict(mode=2, mix=50)), ("Nappe très large", dict(mode=3, width=100)), ("Piano", dict(mode=0, mix=60)),
    ("Discret", dict(mode=0, mix=30)), ("Mono élargi", dict(mode=3, width=100, mix=80)), ("Chœur", dict(mode=2, tone=8000, mix=70)), ("Cordes", dict(mode=3, tone=9000, mix=80)),
]

CHORUS3 = [
    ("Chorus rapide", dict(rate=2, depth=40)), ("Chorus lent", dict(rate=0.15, depth=70)), ("Ensemble lent", dict(mode=2, rate=0.3, depth=70)), ("Ensemble rapide", dict(mode=2, rate=1.5)),
    ("Vibrato profond", dict(mode=1, rate=4, depth=80)), ("Vibrato léger", dict(mode=1, rate=5, depth=25)), ("Sombre", dict(tone=3500)), ("Étroit", dict(width=30)),
    ("Calé sur la mesure", dict(sync=1, division=4)), ("Calé à la noire", dict(sync=1, division=6)), ("Nappe", dict(mode=2, rate=0.2, depth=80, mix=60)), ("Clavier", dict(rate=0.7, depth=50, mix=40)),
    ("Basse", dict(rate=0.5, depth=30, tone=3000, mix=35)), ("Guitare", dict(rate=0.9, depth=55, mix=45)), ("Chœur de voix", dict(mode=2, rate=0.4, depth=60, mix=50)), ("Discret", dict(mix=25, depth=30)),
]

STUTTER = [
    ("Roulement 1/8", dict(division=1, mode=1)), ("Roulement 1/32", dict(division=5, mode=1)), ("Roulement 1/64", dict(division=6, mode=1, length=0)), ("Triolets", dict(division=2, mode=1)),
    ("Hasard 1/16", dict(division=3, mode=2, chance=40)), ("Hasard rare", dict(division=4, mode=2, chance=15)), ("Qui descend", dict(division=3, mode=1, pitch=-12)),
    ("Qui s'éteint", dict(division=3, mode=1, decay=60)), ("Haché", dict(division=3, mode=1, gate=50)), ("Haché serré", dict(division=5, mode=1, gate=30)),
    ("Une mesure", dict(division=3, mode=1, length=2)), ("Deux mesures", dict(division=1, mode=1, length=3)), ("Demi-mesure", dict(division=3, mode=1, length=1)),
    ("Plongée", dict(division=5, mode=1, pitch=-24, decay=40)), ("En parallèle", dict(division=3, mode=1, mix=50)), ("Hasard qui s'éteint", dict(division=4, mode=2, chance=50, decay=50)),
    ("Arrêt", dict(mode=0)),
]

FILTER = [
    ("Passe-bas lent", dict(type=0, cutoff=800, lfodepth=60, division=3)), ("Passe-haut qui monte", dict(type=4, cutoff=200, lfodepth=50, division=2)),
    ("Bande qui chante", dict(type=2, cutoff=1000, reso=70, lfodepth=60, division=5)), ("Auto-wah rapide", dict(type=2, cutoff=600, reso=60, envdepth=80, envspeed=30)),
    ("Wah lent", dict(type=1, cutoff=500, reso=50, envdepth=60, envspeed=300)), ("Croches filtrées", dict(type=0, cutoff=600, reso=40, lfodepth=60, division=7)),
    ("Doubles croches", dict(type=0, cutoff=900, reso=50, lfodepth=50, division=8)), ("Passe-bas fermé", dict(type=0, cutoff=400, reso=30)),
    ("Passe-haut téléphone", dict(type=3, cutoff=800)), ("Résonance saturée", dict(type=0, cutoff=700, reso=85, drive=12)), ("Passe-bas 12 doux", dict(type=1, cutoff=2000, reso=10)),
    ("Balayage 4 mesures", dict(type=0, cutoff=300, lfodepth=90, division=2)), ("Triolets", dict(type=1, cutoff=800, reso=40, lfodepth=55, division=11)),
    ("Libre lent", dict(sync=0, rate=0.2, type=0, cutoff=900, lfodepth=60)), ("Bande étroite", dict(type=2, cutoff=1500, reso=85)), ("En parallèle", dict(type=0, cutoff=500, reso=60, mix=50)),
]

MDELAY = [
    ("Numérique croche", dict(mode=0, division=7, feedback=35)), ("Bande noire", dict(mode=1, division=10, feedback=45)), ("Seau croche pointée", dict(mode=2, division=9, feedback=50)),
    ("Lo-fi noire", dict(mode=3, division=10, grit=60)), ("Inverse croche", dict(mode=4, division=7, feedback=40)), ("Filtre lent", dict(mode=5, modspeed=0.2, moddepth=60)),
    ("Double croche", dict(mode=6, division=7)), ("Ping-pong triolets", dict(mode=7, division=8)), ("Effacement sur la voix", dict(mode=0, division=9, ducking=80)),
    ("Modulé", dict(mode=1, moddepth=60, modspeed=1.2)), ("Sombre", dict(mode=2, tone=1500)), ("Brillant", dict(mode=0, tone=14000)),
]

STEPFX = [
    ("Roulements de fin", {"s13": 10, "s14": 10, "s15": 9, "s16": 9}), ("Inverse au contretemps", {"s3": 2, "s7": 2, "s11": 2, "s15": 2}),
    ("Filtres alternés", {"s1": 19, "s5": 20, "s9": 21, "s13": 22}), ("Écrase sur les temps", {"s1": 25, "s5": 25, "s9": 25, "s13": 25}),
    ("Hachure en 1/32", {"s9": 32, "s10": 32, "s11": 32, "s12": 32}), ("Échos de fin", {"s14": 36, "s16": 37}), ("Ralentis", {"s5": 40, "s13": 41}),
    ("Fondus", {"s1": 44, "s16": 43}), ("Pompe", {f"s{k}": 46 for k in (1, 5, 9, 13)}), ("Arrêt de bande final", {"s16": 29}),
]

SCULPTOR = [
    ("Pompe 1/8", dict(s1on=1, s1shape=5, s1len=1, s1amt=70)), ("Porte 1/16", dict(s1on=1, s1shape=7, s1len=4, s1amt=80)), ("Escalier", dict(s1on=1, s1shape=9, s1len=4, s1amt=60)),
    ("Rebond", dict(s1on=1, s1shape=10, s1len=4, s1amt=60)), ("Grave qui monte", dict(s1on=1, s1shape=12, s1len=5, s1amt=70, s1band=1)), ("Aigus pointés", dict(s1on=1, s1shape=8, s1len=4, s1amt=70, s1band=3)),
    ("Pompe douce 1/4", dict(s1on=1, s1shape=6, s1len=2, s1amt=60)), ("Hasard sur le médium", dict(s1on=1, s1shape=11, s1len=0, s1amt=60, s1band=2)),
]

LOOPER = [
    ("Deux mesures calées", dict(length=2, start=1)), ("Huit mesures calées", dict(length=4, start=1)), ("Boucle à double vitesse", dict(speed=2)), ("Boucle à demi-vitesse", dict(speed=0)),
    ("Boucle à l'envers", dict(reverse=1)), ("Surcouches qui durent", dict(keep=95)), ("Surcouches rapides", dict(keep=60)), ("Boucle discrète", dict(level=-9)),
    ("Une mesure à l'envers", dict(length=1, start=1, reverse=1)), ("Deux mesures sans direct", dict(length=2, start=1, dry=0)), ("Demi-vitesse à l'envers", dict(speed=0, reverse=1)),
    ("Quatre mesures, surcouches qui s'effacent", dict(length=3, start=1, keep=75)), ("Libre à l'envers", dict(reverse=1, start=0)), ("Boucle au premier plan", dict(level=3, dry=60)),
    ("Une mesure double vitesse", dict(length=1, start=1, speed=2)),
]

FX = {
    "eq": EQ, "opto": OPTO, "fet": FET, "bus": BUS, "comp": COMP, "limiter": LIMITER, "master": MASTER, "gate": GATE, "mband": MBAND, "varimu": VARIMU,
    "tubeeq": TUBEEQ, "consoleeq": CONSOLEEQ, "dist": DIST, "grinder": GRINDER, "tapesat": TAPESAT, "tubesat": TUBESAT, "distcomp": DISTCOMP,
    "transient": TRANSIENT, "delay": DELAY, "tapeecho": TAPEECHO, "reverb": REVERB, "vintverb": VINTVERB, "plate": PLATE, "spring": SPRING,
    "prestige": PRESTIGE, "chorus": CHORUS, "phaser": PHASER, "flanger": FLANGER, "phaser2": PHASER2, "dimension": DIMENSION, "chorus3": CHORUS3,
    "stutter": STUTTER, "filter": FILTER, "mdelay": MDELAY, "stepfx": STEPFX, "sculptor": SCULPTOR, "looper": LOOPER,
}
