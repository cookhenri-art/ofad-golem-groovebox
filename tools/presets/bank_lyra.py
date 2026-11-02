# Octo Lyma (8 voix en paires, FM croisée, tenue en bourdon, LFO hyper, retard modulé) :
# 10 sons par catégorie. Réglages originaux. Jeu : 0 plaques (voix accordées, note modulo 8),
# 1 clavier (les 8 voix jouent les notes). Source de modulation d'une paire : 0 paire voisine,
# 1 LFO, 2 retour de la sortie.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX


def tunes(*t):
    return {f"t{i + 1}": v for i, v in enumerate(t)}


def pairs(sharp=(10, 10, 10, 10), mod=(0, 0, 0, 0), src=(0, 0, 0, 0)):
    d = {}
    for k in range(4):
        d[f"p{k + 1}sharp"] = sharp[k]; d[f"p{k + 1}mod"] = mod[k]; d[f"p{k + 1}src"] = src[k]
    return d


def groups(att=(40, 40), dec=(900, 900), hold=(0, 0), pitch=(0, 0)):
    return dict(q1att=att[0], q2att=att[1], q1dec=dec[0], q2dec=dec[1], q1hold=hold[0], q2hold=hold[1], q1pitch=pitch[0], q2pitch=pitch[1])


def lfo(a=2, b=3, orm=0, link=0):
    return dict(lfoa=a, lfob=b, lfomode=orm, lfolink=link)


def dly(t1=260, t2=390, m1=0, m2=0, self_=0, fb=40, mixv=25):
    return dict(dtime1=t1, dtime2=t2, dmod1=m1, dmod2=m2, dmodsrc=self_, dfb=fb, dmix=mixv)


KEY = dict(play=1)
OPEN = tunes(0, 7, 12, 19, -5, 2, 10, 14)
MINOR = tunes(0, 3, 7, 10, -12, -5, 12, 15)
FIFTHS = tunes(0, 7, 14, 21, -12, -5, 2, 9)
CLUSTER = tunes(0, 1, 2, 5, 7, 8, 12, 13)
OCT = tunes(-12, 0, 12, 24, -12, 0, 7, 19)


def build():
    b = Bank("lyra")
    add = b.add

    # --- Basses (clavier)
    add("Basse organique", BASS, mix(KEY, pairs((30, 30, 30, 30)), groups((5, 5), (500, 500)), dly(mixv=0), base=36, q1pitch=-12, q2pitch=-12))
    add("Basse qui frotte", BASS, mix(KEY, pairs((60, 60, 60, 60), (40, 40, 40, 40)), groups((5, 5), (400, 400)), dly(mixv=10), q1pitch=-12, q2pitch=-12, drive=30))
    add("Basse carrée lyre", BASS, mix(KEY, pairs((90, 90, 90, 90)), groups((3, 3), (350, 350)), dly(mixv=0), q1pitch=-12, q2pitch=-12))
    add("Basse FM organique", BASS, mix(KEY, pairs((10, 10, 10, 10), (55, 55, 55, 55)), groups((3, 3), (450, 450)), dly(mixv=0), q1pitch=-12, q2pitch=-12))
    add("Basse saturée lyre", BASS, mix(KEY, pairs((50, 50, 50, 50), (20, 20, 20, 20)), groups((5, 5), (500, 500)), dly(mixv=5), q1pitch=-12, q2pitch=-12, drive=70))
    add("Basse qui vibre", BASS, mix(KEY, pairs((40, 40, 40, 40), (40, 40, 40, 40), (1, 1, 1, 1)), lfo(5, 6), groups((5, 5), (600, 600)), dly(mixv=0), q1pitch=-12, q2pitch=-12))
    add("Basse ronde lyre", BASS, mix(KEY, pairs((0, 0, 0, 0)), groups((5, 5), (700, 700)), dly(mixv=0), q1pitch=-12, q2pitch=-12))
    add("Basse en retour", BASS, mix(KEY, pairs((30, 30, 30, 30), (35, 35, 35, 35), (2, 2, 2, 2)), groups((3, 3), (400, 400)), dly(mixv=0), q1pitch=-12, q2pitch=-12))
    add("Basse sourde lyre", BASS, mix(KEY, pairs((15, 15, 15, 15)), groups((30, 30), (900, 900)), dly(mixv=0), q1pitch=-24, q2pitch=-12))
    add("Basse en écho", BASS, mix(KEY, pairs((50, 50, 50, 50), (20, 20, 20, 20)), groups((5, 5), (350, 350)), dly(250, 375, fb=30, mixv=20), q1pitch=-12, q2pitch=-12))

    # --- Leads (clavier)
    add("Lead organique", LEAD, mix(KEY, pairs((40, 40, 40, 40), (20, 20, 20, 20)), groups((20, 20), (1500, 1500)), dly(mixv=20)))
    add("Lead qui chante 2", LEAD, mix(KEY, pairs((20, 20, 20, 20), (30, 30, 30, 30), (1, 1, 1, 1)), lfo(5.5, 5.8), groups((40, 40), (2000, 2000)), dly(mixv=25)))
    add("Lead hurleur lyre", LEAD, mix(KEY, pairs((90, 90, 90, 90), (40, 40, 40, 40)), groups((10, 10), (1500, 1500)), dly(mixv=15), drive=60))
    add("Lead FM lyre", LEAD, mix(KEY, pairs((10, 10, 10, 10), (70, 70, 70, 70)), groups((10, 10), (1500, 1500)), dly(mixv=15)))
    add("Lead lointain lyre", LEAD, mix(KEY, pairs((20, 20, 20, 20)), groups((200, 200), (3000, 3000)), dly(400, 600, fb=45, mixv=40)))
    add("Lead qui grince", LEAD, mix(KEY, pairs((70, 70, 70, 70), (50, 50, 50, 50), (2, 2, 2, 2)), groups((10, 10), (1200, 1200)), dly(mixv=15)))
    add("Lead carré lyre", LEAD, mix(KEY, pairs((100, 100, 100, 100)), groups((10, 10), (1500, 1500)), dly(mixv=20), q1pitch=12, q2pitch=12))
    add("Lead d'oiseau", LEAD, mix(KEY, pairs((0, 0, 0, 0), (40, 40, 40, 40), (1, 1, 1, 1)), lfo(9, 13, 1), groups((20, 20), (1200, 1200)), dly(mixv=25), q1pitch=12, q2pitch=12))
    add("Lead en ruban", LEAD, mix(KEY, pairs((30, 30, 30, 30), (25, 25, 25, 25)), groups((80, 80), (2000, 2000)), dly(300, 450, 20, 20, fb=40, mixv=30)))
    add("Lead tordu", LEAD, mix(KEY, pairs((60, 60, 60, 60), (60, 60, 60, 60)), groups((10, 10), (1500, 1500)), dly(mixv=10), drive=50))

    # --- Nappes (plaques)
    add("Bourdon d'église", PAD, mix(OPEN, pairs((10, 10, 10, 10), (10, 10, 10, 10)), groups((1500, 1500), (4000, 5000), (60, 50)), dly(fb=50, mixv=35)))
    add("Bourdon mineur", PAD, mix(MINOR, pairs((20, 20, 20, 20), (15, 15, 15, 15)), groups((1200, 1500), (4000, 4000), (55, 50)), dly(fb=45, mixv=30)))
    add("Bourdon en quintes", PAD, mix(FIFTHS, pairs((15, 15, 15, 15), (20, 10, 20, 10)), groups((1500, 2000), (5000, 5000), (60, 55)), dly(fb=50, mixv=35)))
    add("Grappe qui bat", PAD, mix(CLUSTER, pairs((10, 10, 10, 10)), groups((1500, 1500), (4000, 4000), (50, 50)), dly(fb=45, mixv=30)))
    add("Nappe organique", PAD, mix(OPEN, pairs((30, 30, 30, 30), (25, 25, 25, 25)), groups((900, 1200), (3500, 4000), (40, 35)), dly(320, 480, 20, 20, fb=50, mixv=40)))
    add("Nappe qui respire 2", PAD, mix(OCT, pairs((20, 20, 20, 20), (30, 30, 30, 30), (1, 1, 1, 1)), lfo(0.3, 0.45), groups((1500, 1500), (4000, 4000), (50, 50)), dly(fb=45, mixv=35)))
    add("Nappe carrée lyre", PAD, mix(FIFTHS, pairs((80, 80, 80, 80)), groups((1000, 1000), (3000, 3000), (40, 40)), dly(fb=40, mixv=30)))
    add("Nappe de plaques", PAD, mix(OPEN, pairs((10, 10, 10, 10), (40, 40, 40, 40)), groups((2000, 2000), (5000, 5000), (50, 45)), dly(400, 520, 30, 30, 1, fb=50, mixv=40)))
    add("Nappe grave lyre", PAD, mix(OCT, pairs((20, 20, 20, 20)), groups((1500, 1500), (4000, 4000), (55, 50), (-12, -12)), dly(fb=40, mixv=25)))
    add("Nappe d'aurore", PAD, mix(MINOR, pairs((5, 5, 5, 5), (20, 20, 20, 20)), groups((3000, 3000), (5000, 6000), (50, 45), (12, 0)), dly(450, 600, 15, 15, fb=50, mixv=45)))

    # --- Plucks (clavier)
    PK = groups((2, 2), (300, 300))
    add("Pluck organique", PLUCK, mix(KEY, pairs((40, 40, 40, 40)), PK, dly(mixv=20)))
    add("Pluck FM lyre", PLUCK, mix(KEY, pairs((10, 10, 10, 10), (60, 60, 60, 60)), PK, dly(mixv=15)))
    add("Pluck carré lyre", PLUCK, mix(KEY, pairs((90, 90, 90, 90)), PK, dly(mixv=15)))
    add("Pluck en écho lyre", PLUCK, mix(KEY, pairs((30, 30, 30, 30)), PK, dly(250, 375, fb=45, mixv=40)))
    add("Pluck de verre lyre", PLUCK, mix(KEY, pairs((0, 0, 0, 0), (30, 30, 30, 30)), groups((2, 2), (800, 800)), dly(mixv=20), q1pitch=12, q2pitch=12))
    add("Pluck sec lyre", PLUCK, mix(KEY, pairs((50, 50, 50, 50)), groups((2, 2), (150, 150)), dly(mixv=0)))
    add("Pluck qui grince", PLUCK, mix(KEY, pairs((70, 70, 70, 70), (50, 50, 50, 50), (2, 2, 2, 2)), PK, dly(mixv=10)))
    add("Harpe organique", PLUCK, mix(KEY, pairs((10, 10, 10, 10)), groups((2, 2), (2000, 2000)), dly(mixv=25)))
    add("Pluck tremblé", PLUCK, mix(KEY, pairs((30, 30, 30, 30), (40, 40, 40, 40), (1, 1, 1, 1)), lfo(7, 9), PK, dly(mixv=15)))
    add("Pluck saturé lyre", PLUCK, mix(KEY, pairs((40, 40, 40, 40), (20, 20, 20, 20)), PK, dly(mixv=10), drive=60))

    # --- Claviers (clavier)
    add("Orgue organique", KEYS, mix(KEY, pairs((20, 20, 20, 20)), groups((5, 5), (400, 400), (0, 0)), dly(mixv=10)))
    add("Orgue carré lyre", KEYS, mix(KEY, pairs((90, 90, 90, 90)), groups((5, 5), (300, 300)), dly(mixv=10)))
    add("Piano organique", KEYS, mix(KEY, pairs((20, 20, 20, 20), (30, 30, 30, 30)), groups((3, 3), (2500, 2500)), dly(mixv=15)))
    add("Clavier FM lyre", KEYS, mix(KEY, pairs((10, 10, 10, 10), (50, 50, 50, 50)), groups((3, 3), (1500, 1500)), dly(mixv=15)))
    add("Vibraphone lyre", KEYS, mix(KEY, pairs((0, 0, 0, 0), (20, 20, 20, 20), (1, 1, 1, 1)), lfo(5, 5.2), groups((3, 3), (3000, 3000)), dly(mixv=20)))
    add("Clavinette lyre", KEYS, mix(KEY, pairs((80, 80, 80, 80), (30, 30, 30, 30)), groups((2, 2), (600, 600)), dly(mixv=5)))
    add("Célesta lyre", KEYS, mix(KEY, pairs((0, 0, 0, 0), (25, 25, 25, 25)), groups((2, 2), (2000, 2000)), dly(mixv=20), q1pitch=12, q2pitch=12))
    add("Orgue d'église lyre", KEYS, mix(KEY, pairs((30, 30, 30, 30)), groups((80, 80), (1200, 1200)), dly(450, 580, fb=50, mixv=40)))
    add("Accords organiques", KEYS, mix(KEY, pairs((50, 50, 50, 50), (20, 20, 20, 20)), groups((5, 5), (700, 700)), dly(mixv=20)))
    add("Clavier tordu", KEYS, mix(KEY, pairs((40, 40, 40, 40), (40, 40, 40, 40), (2, 2, 2, 2)), groups((3, 3), (1200, 1200)), dly(mixv=15)))

    # --- Cordes (clavier, attaques lentes)
    ST = groups((500, 500), (2500, 2500))
    add("Cordes organiques", STR, mix(KEY, pairs((30, 30, 30, 30), (15, 15, 15, 15)), ST, dly(mixv=30)))
    add("Cordes lentes lyre", STR, mix(KEY, pairs((30, 30, 30, 30), (10, 10, 10, 10)), groups((1500, 1500), (3500, 3500)), dly(fb=45, mixv=35)))
    add("Violons lyre", STR, mix(KEY, pairs((40, 40, 40, 40), (20, 20, 20, 20), (1, 1, 1, 1)), lfo(5.5, 5.7), groups((300, 300), (1500, 1500)), dly(mixv=25), q1pitch=12, q2pitch=12))
    add("Violoncelles lyre", STR, mix(KEY, pairs((40, 40, 40, 40), (20, 20, 20, 20), (1, 1, 1, 1)), lfo(5, 5.3), groups((300, 300), (1500, 1500)), dly(mixv=20), q1pitch=-12, q2pitch=-12))
    add("Pizzicato lyre", STR, mix(KEY, pairs((30, 30, 30, 30)), groups((2, 2), (300, 300)), dly(mixv=20)))
    add("Cordes brillantes lyre", STR, mix(KEY, pairs((60, 60, 60, 60), (20, 20, 20, 20)), ST, dly(mixv=25)))
    add("Cordes sombres lyre", STR, mix(KEY, pairs((10, 10, 10, 10)), ST, dly(mixv=25), q1pitch=-12, q2pitch=-12))
    add("Trémolo lyre", STR, mix(KEY, pairs((30, 30, 30, 30), (40, 40, 40, 40), (1, 1, 1, 1)), lfo(7, 7.5), ST, dly(mixv=25)))
    add("Ensemble lyre", STR, mix(KEY, pairs((30, 30, 30, 30), (20, 20, 20, 20)), ST, dly(300, 450, 30, 30, fb=40, mixv=40)))
    add("Cordes en retour", STR, mix(KEY, pairs((30, 30, 30, 30), (25, 25, 25, 25), (2, 2, 2, 2)), ST, dly(mixv=25)))

    # --- Cuivres (clavier)
    BR = groups((60, 60), (1500, 1500))
    add("Cuivres organiques", BRASS, mix(KEY, pairs((70, 70, 70, 70), (25, 25, 25, 25)), BR, dly(mixv=15)))
    add("Trompette lyre", BRASS, mix(KEY, pairs((80, 80, 80, 80), (30, 30, 30, 30)), groups((40, 40), (1200, 1200)), dly(mixv=10)))
    add("Section lyre", BRASS, mix(KEY, pairs((70, 70, 70, 70), (20, 20, 20, 20)), BR, dly(mixv=20)))
    add("Trombone lyre", BRASS, mix(KEY, pairs((70, 70, 70, 70), (25, 25, 25, 25)), BR, dly(mixv=10), q1pitch=-12, q2pitch=-12))
    add("Cuivres staccato lyre", BRASS, mix(KEY, pairs((80, 80, 80, 80), (30, 30, 30, 30)), groups((20, 20), (300, 300)), dly(mixv=10)))
    add("Cor lyre", BRASS, mix(KEY, pairs((40, 40, 40, 40), (15, 15, 15, 15)), groups((120, 120), (1800, 1800)), dly(mixv=20)))
    add("Cuivres qui grognent lyre", BRASS, mix(KEY, pairs((80, 80, 80, 80), (40, 40, 40, 40)), BR, dly(mixv=10), drive=50))
    add("Saxo lyre", BRASS, mix(KEY, pairs((60, 60, 60, 60), (35, 35, 35, 35), (1, 1, 1, 1)), lfo(5, 5.5), groups((40, 40), (1500, 1500)), dly(mixv=15)))
    add("Cuivres doux lyre", BRASS, mix(KEY, pairs((50, 50, 50, 50), (15, 15, 15, 15)), groups((150, 150), (2000, 2000)), dly(mixv=20)))
    add("Fanfare lyre", BRASS, mix(KEY, pairs((80, 80, 80, 80), (25, 25, 25, 25)), BR, dly(mixv=15), q1pitch=12))

    # --- Arpèges (clavier, la machine n'a pas d'arpégiateur : notes répétées par le retard)
    AR = groups((2, 2), (250, 250))
    add("Échos organiques", ARP, mix(KEY, pairs((40, 40, 40, 40)), AR, dly(250, 375, fb=50, mixv=50)))
    add("Échos FM lyre", ARP, mix(KEY, pairs((10, 10, 10, 10), (50, 50, 50, 50)), AR, dly(188, 375, fb=50, mixv=50)))
    add("Échos carrés", ARP, mix(KEY, pairs((90, 90, 90, 90)), AR, dly(125, 250, fb=50, mixv=45)))
    add("Échos qui tournent", ARP, mix(KEY, pairs((30, 30, 30, 30)), AR, dly(250, 375, 20, 20, fb=50, mixv=50)))
    add("Échos de verre", ARP, mix(KEY, pairs((0, 0, 0, 0), (25, 25, 25, 25)), groups((2, 2), (600, 600)), dly(188, 282, fb=50, mixv=50), q1pitch=12, q2pitch=12))
    add("Échos graves", ARP, mix(KEY, pairs((40, 40, 40, 40)), AR, dly(375, 500, fb=45, mixv=45), q1pitch=-12, q2pitch=-12))
    add("Échos tordus", ARP, mix(KEY, pairs((50, 50, 50, 50), (40, 40, 40, 40), (2, 2, 2, 2)), AR, dly(250, 333, 30, 30, 1, fb=45, mixv=45)))
    add("Échos courts", ARP, mix(KEY, pairs((40, 40, 40, 40)), groups((2, 2), (120, 120)), dly(125, 188, fb=55, mixv=50)))
    add("Échos saturés", ARP, mix(KEY, pairs((40, 40, 40, 40)), AR, dly(250, 375, fb=50, mixv=50), drive=50))
    add("Échos qui vibrent", ARP, mix(KEY, pairs((30, 30, 30, 30), (30, 30, 30, 30), (1, 1, 1, 1)), lfo(6, 8), AR, dly(250, 375, fb=50, mixv=50)))

    # --- Effets (plaques)
    add("Essaim 2", FX, mix(CLUSTER, pairs((50, 50, 50, 50), (60, 50, 60, 50), (1, 0, 1, 0)), lfo(5.5, 7.1, 0, 1), groups((500, 500), (3000, 3000), (40, 40)), dly(m1=40, m2=30, fb=45, mixv=40), drive=30))
    add("Machine organique", FX, mix(OPEN, pairs((70, 70, 70, 70), (70, 70, 70, 70)), groups((200, 200), (3000, 3000), (40, 40)), dly(60, 90, 50, 50, 1, fb=50, mixv=45), drive=40))
    add("Radio de lyre", FX, mix(FIFTHS, pairs((30, 30, 30, 30), (40, 40, 40, 40), (2, 2, 2, 2)), groups((500, 500), (3000, 3000), (40, 40)), dly(20, 33, 80, 80, 0, fb=40, mixv=50)))
    add("Tempête de lyre", FX, mix(CLUSTER, pairs((90, 90, 90, 90), (80, 80, 80, 80)), lfo(0.3, 0.7, 1, 1), groups((1500, 1500), (4000, 4000), (50, 50)), dly(400, 520, 30, 30, fb=50, mixv=45), drive=70))
    add("Signal de lyre", FX, mix(OCT, pairs((100, 100, 100, 100), (30, 30, 30, 30), (1, 1, 1, 1)), lfo(3, 4.5, 0), groups((10, 10), (2000, 2000), (30, 30)), dly(fb=40, mixv=35)))
    add("Gouttes de lyre", FX, mix(OPEN, pairs((0, 0, 0, 0)), groups((2, 2), (300, 300)), dly(300, 450, 60, 60, fb=50, mixv=55)))
    add("Cloches organiques", FX, mix(FIFTHS, pairs((10, 10, 10, 10), (70, 70, 70, 70)), groups((2, 2), (4000, 4000)), dly(fb=40, mixv=35)))
    add("Moteur de lyre", FX, mix(OCT, pairs((80, 80, 80, 80), (60, 60, 60, 60), (1, 1, 1, 1)), lfo(25, 31, 1), groups((100, 100), (3000, 3000), (40, 40)), dly(mixv=20), drive=50))
    add("Vent de lyre", FX, mix(CLUSTER, pairs((5, 5, 5, 5), (90, 90, 90, 90), (2, 2, 2, 2)), groups((2000, 2000), (5000, 5000), (50, 50)), dly(500, 650, 40, 40, 1, fb=50, mixv=45)))
    add("Rêve de lyre", FX, mix(MINOR, pairs((10, 10, 10, 10), (35, 35, 35, 35), (0, 1, 0, 1)), lfo(0.2, 0.33, 0, 1), groups((3000, 3000), (6000, 6000), (50, 45)), dly(450, 600, 20, 20, fb=55, mixv=45)))
    return b
