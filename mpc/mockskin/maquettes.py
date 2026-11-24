import os
import sys
if len(sys.argv) > 1:
    os.environ["MAQUETTES_OUT"] = sys.argv[1]
from maquettes_gen import *

T = THEMES

# ============================== A ==============================
make_a("A-Machine", "drumA", "Machine électronique", "Drum - Machine electronique", "#ff8a3d",
       kit_strips("drumA", range(1, 9), ("tune", "Accord"), ("decay", "Déclin"), ("mach",), chipname=False),
       ["Sons 1-8", "Sons 9-16", "Mixage"], "Gros bouton = boutons 1 à 8 · petit bouton = 9 à 16",
       ["Kit", "1 BD", "2 SD", "3 CP", "4 RS", "5 CH  ›"], bar=False, abfs=24, bigfs=16, smallfs=13, namecol="#c9cbd2")
make_a("A-Drum-DragonFist", "drumImp", "Dragon Fist", "Drum - Dragon Fist", "#ff6b4a",
       kit_strips("drumImp", range(1, 9), ("tune", "Accord"), ("decay", "Déclin"), ("fx",)),
       ["Sons 1-8", "Sons 9-16"], "Gros bouton = boutons 1 à 8 · petit = 9 à 16", ["Kit", "1 BD", "2 SD", "3 SD", "4 HT", "5 LT  ›"])
make_a("A-Drum-CutOfTheBeat", "drumNrv", "Cut of the Beat", "Drum - Cut of the Beat", "#57d0c3",
       kit_strips("drumNrv", range(1, 9), ("tune", "Accord"), ("decay", "Déclin"), ("fx",)),
       ["Sons 1-8", "Sons 9-16"], "Gros bouton = boutons 1 à 8 · petit = 9 à 16", ["Kit", "1 BD", "2 BD", "3 SD", "4 CP", "5 RS  ›"],
       bg="#101618", panel="#172024", line="#24313a")
make_a("A-Drum-MicroTonnerre", "microton", "Micro Tonnerre", "Drum - Micro Tonnerre", "#ffd25a",
       kit_strips("microton", range(1, 9), ("freq", "Fréquence"), ("odec", "Déclin"), ("wave",)),
       ["8 sons"], "Gros bouton = boutons 1 à 8 (fréquence) · petit = 9 à 16 (déclin)", ["Kit", "1 BD", "2 SD", "3 CH", "4 OH", "5 CP  ›"],
       bg="#15140f", panel="#1d1c16", line="#2f2d24")
make_a("A-Sampler-Cutsample", "sampler", "Cutsample", "Sampler - Cutsample", "#ff7aa8",
       kit_strips("sampler", range(1, 9), ("tune", "Accord"), ("cutoff", "Coupure"), ("mode",)),
       ["Zones 1-8", "Zones 9-16"], "Gros bouton = boutons 1 à 8 · petit = 9 à 16", ["Zones", "Samples", "Z1", "Z2", "Z3", "Z4  ›"],
       bg="#17111a", panel="#1f1724", line="#33243a")
make_a("A-Sampler-Poussiere", "sp", "Poussière de Vinyle", "Sampler - Poussiere de Vinyle", "#f0c25a",
       kit_strips("sp", range(1, 9), ("tune", "Accord"), ("cutoff", "Coupure"), ("bus",)),
       ["Pads 1-8", "Pads 9-16", "Bus"], "Gros bouton = boutons 1 à 8 · petit = 9 à 16", ["Pads", "Samples", "Bus", "P1", "P2", "P3  ›"],
       bg="#16130e", panel="#1e1a13", line="#332c20")

ly = []
for n in range(1, 9):
    k = (n + 1) // 2
    g = 1 if n <= 4 else 2
    mod = P("lyra", "p%dmod" % k)
    ly.append({"ab": "V%d" % n, "n": "paire %d-%d" % (2 * k - 1, 2 * k), "name": "Voix %d" % n, "col": PALETTE[k - 1],
               "chip": P("lyra", "p%dsrc" % k)["name"].split(" ")[0] + " · " + fmt(P("lyra", "p%dsrc" % k)),
               "chipid": "p%dsrc" % k, "chipname": P("lyra", "p%dsrc" % k)["name"].split(" ")[0], "chipval": fmt(P("lyra", "p%dsrc" % k)),
               "lid": "p%dmod" % k, "xid": "q%dpitch" % g,
               "big": kv("lyra", "t%d" % n, "Accord"), "small": kv("lyra", "p%dsharp" % k, "Forme"),
               "lvl": round(ang(mod) / 2.7), "ll": "Modulation", "lv": fmt(mod),
               "xl": "Hauteur", "xv": fmt(P("lyra", "q%dpitch" % g))})
make_a("A-Drone-OctoLyma", "lyra", "Octo Lyma", "Drone - Octo Lyma", "#9b8cff", ly, ["Voix", "Groupes", "Retard"],
       "Une tranche par voix · la forme et la modulation sont partagées par paire", ["Voix", "Paires", "Groupes", "LFO", "Filtre", "Retard"],
       bg="#12111a", panel="#1a1824", line="#2c2840")

# ============================== B ==============================
def fm_matrix_hero():
    """Komodo Fuel : schéma de la matrice (illustration) ; la profondeur 2 → 1 est la valeur en direct."""
    h = hero_fm_cards([[1, 2], [3], [4], [5], [6]], None, "MATRICE", "")
    h["labels"] = [l for l in h["labels"] if l["t"]]
    h["labels"].append(L(368, 64, "2 → 1 :", "#e7a6ff", 15, 600, "end"))
    h["vals"] = [{"id": "m21", "x": 372, "y": 48, "w": 32, "h": 22, "fs": 15, "fw": 600, "col": "#e7a6ff", "j": "left", "jc": "flex-start", "v": fmt(P("fmx", "m21"))}]
    return h


def th(name):
    return T[name]

make_b("B-Poly-Synthonde", "morph", "Synthonde", "Poly - Synthonde", th("ink"),
       ["Tables", "Mutants", "Filtres", "Env.", "LFO", "Mix", "Arpège"], hero_table("#7fd3ff", "#ffc873"),
       "Oscillateur 1", "La position balaie la table d'ondes, les mutants la déforment.",
       ["w1mut", "f1mode"], ["w1semi", "w1fine", "w1ratio", "mix1", "w2pos", "w3pos"],
       ["w1pos", "w2pos", "w1mutamt", "f1cutoff", "f1reso", "env3pos", "l1pos", "level"], ["Jouer", "Osc 1", "Osc 2", "Filtres", "Matrice", "Arpège"])
make_b("B-Nappe-OndesBretzel", "blo", "Ondes Bretzel", "Nappe - Ondes Bretzel", th("ink"),
       ["Osc", "Mix", "Filtres", "Env.", "LFO", "Matrice", "Jeu"], hero_filter("#7fd3ff", "#ffc873"),
       "Filtre 1", "Glisser le point : coupure et résonance. Les deux filtres en série ou en parallèle.",
       ["f1type", "routing"], ["f1drive", "f1key", "f1env", "f1vel", "f2cut", "fbalance"],
       ["f1cut", "f1reso", "o1pos", "o2pos", "o3level", "e2a", "e2r", "level"], ["Jouer", "Osc", "Filtres", "Env.", "Matrice", "Jeu"])
make_b("B-Poly-Micropointe", "hybrid", "Micropointe", "Poly - Micropointe", th("steel"),
       ["Osc", "Mix", "Filtre", "Env.", "LFO", "Arpège", "Global"], hero_waves("#9fd2ff", "#ff9ec4", ["saw", "fold", "pulse"], ["Osc. 1", "Osc. 2", "Osc. 3"]),
       "Oscillateurs", "Trois sources à forme réglable, reliées en FM 3→1, 1→2, 2→3.",
       ["o1tbl", "fmode"], ["o1semi", "o2semi", "o3semi", "fm31", "fm12", "fm23"],
       ["o1shape", "o2shape", "o3shape", "cutoff", "reso", "envamt", "mixring", "level"], ["Jouer", "Osc", "Filtre", "Env.", "LFO", "Arpège"])
make_b("B-Poly-GoodDoctor", "gooddoc", "Good Doctor", "Poly - Good Doctor", th("wine"),
       ["Osc", "Sous/bruit", "Filtres", "Env.", "LFO", "Disto", "Macros"], hero_waves("#ff7aa8", "#ffd27a", ["saw", "fold", "tri"], ["Osc. A", "Osc. B", "Osc. C"]),
       "Oscillateurs A B C", "Trois tables d'ondes, deux déformations chacune.",
       ["a_tab", "f1type"], ["a_pos", "b_pos", "c_pos", "a_w1amt", "sub", "noise"],
       ["a_pos", "b_pos", "c_pos", "f1cut", "f1res", "e2a", "ddrive", "level"], ["Jouer", "Osc", "Filtres", "Env.", "Matrice", "Macros"])
make_b("B-Poly-GoodDoctor2", "gooddoc2", "Good Doctor 2", "Poly - Good Doctor 2", th("wine"),
       ["Osc", "Sources", "Filtres", "Env.", "LFO", "Effets", "Macros"], hero_waves("#ff7aa8", "#ffd27a", ["fold", "saw", "sin"], ["Osc. A", "Osc. B", "Osc. C"]),
       "Oscillateurs A B C", "Table, granulaire ou spectral selon le mode de chaque oscillateur.",
       ["a_mode", "f1type"], ["a_pos", "b_pos", "c_pos", "a_w1amt", "sub", "e1a"],
       ["a_pos", "b_pos", "c_pos", "f1cut", "f1res", "e1a", "e1r", "level"], ["Jouer", "Osc", "Filtres", "Env.", "Effets", "Macros"])
make_b("B-Nappe-Bignappe", "pads", "Bignappe One", "Nappe - Bignappe One", th("forest"),
       ["Osc A", "Osc B", "Filtres", "Ampli", "Mod.", "LFO", "Arpège"], hero_quad("#6fe0a8", "#f0c25a"),
       "Couches A et B", "Quatre oscillateurs, deux par couche, chacun avec son unisson.",
       ["awave", "fatype"], ["aunison", "adetune", "bunison", "bdetune", "chorusmix", "width"],
       ["alevel", "a2level", "blevel", "b2level", "facutoff", "fbcutoff", "chorusmix", "level"], ["Jouer", "Osc", "Filtres", "Ampli", "Arpège", "Global"])
make_b("B-FM-KomodoFuel", "fmx", "Komodo Fuel", "FM - Komodo Fuel", th("rust"),
       ["Opér.", "Matrice", "Filtre", "Env.", "LFO", "Arpège", "Global"],
       fm_matrix_hero(),
       "Opérateurs", "Six opérateurs : qui module qui, d'après la matrice FM. Toucher un opérateur pour le régler.",
       ["ftype"], ["o1_coarse", "o2_coarse", "o3_coarse", "o4_coarse", "o5_coarse", "o6_coarse"],
       ["o1_fine", "o2_fine", "o3_fine", "fcut", "fres", "fenv", "spread", "level"], ["Jouer", "Opér.", "Matrice", "Filtre", "LFO", "Arpège"])
make_b("B-FM-NewMetal", "fm4", "New Metal", "FM - New Metal", th("steel"),
       ["FM", "Env.", "Filtres", "Ampli", "LFO", "Arpège", "Global"],
       hero_fm_film(FM4_ALGOS, ["C", "A", "B1", "B2"], "algo", "feedback"),
       "Algorithme", "Quatre opérateurs C, A, B1, B2 : en haut les modulateurs, en bas les porteuses. Toucher un opérateur pour le régler.",
       ["ftype"], ["ratc", "rata", "ratb1", "ratb2", "harm", "detune"],
       ["algo", "feedback", "mix", "leva", "levb", "ffreq", "fres", "level"], ["Jouer", "FM", "Env.", "Filtres", "LFO", "Arpège"])
make_b("B-FM-OldMetal", "fm6", "Old Metal", "FM - Old Metal", th("teal"),
       ["Opér.", "Env. op.", "Hauteur", "LFO", "Molette", "Arpège", "Global"],
       hero_fm_film(FM6_ALGOS, [1, 2, 3, 4, 5, 6], "algo", "feedback"),
       "Algorithme", "En haut les modulateurs, en bas les porteuses vers la sortie. Toucher un opérateur pour le régler.",
       ["lfowave"], ["o1_coarse", "o2_coarse", "o3_coarse", "o4_coarse", "o5_coarse", "o6_coarse"],
       ["algo", "feedback", "o1_level", "o2_level", "o3_level", "o4_level", "lfospeed", "level"], ["Jouer", "Opér.", "Env.", "LFO", "Arpège", "Global"])
make_b("B-Drone-HotSauce", "hots", "Hot Sauce", "Drone - Hot Sauce", th("rust"),
       ["Moteurs", "Utilit.", "Filtres", "Env.", "LFO", "Arpège", "Global"], hero_two("#ff8a4c", "#9fd2ff", "Moteur A", "Moteur B", "fold", "pulse"),
       "Moteurs A et B", "Deux moteurs face à face, B modulé en FM par A.",
       ["atype", "btype"], ["aoct", "boct", "ap1", "bp1", "sub", "noise"],
       ["ap1", "ap2", "bp1", "bp2", "fmab", "f1cut", "route", "level"], ["Jouer", "Moteurs", "Filtres", "Env.", "LFO", "Arpège"])
make_b("B-Drone-KomodoDragon", "bitm", "Komodo Dragon", "Drone - Komodo Dragon", th("forest"),
       ["Masques", "Mix", "Delta", "Filtre", "Env.", "LFO", "Arpège"], hero_mask("#6fe0a8", "#f0c25a"),
       "Masques", "Les bits masqués fabriquent l'onde ; le delta la fait évoluer.",
       ["res", "mmode"], ["o1a", "o1b", "o2a", "o2b", "damt", "denv"],
       ["o1a", "o1b", "o2a", "o2b", "damt", "fcut", "fres", "level"], ["Jouer", "Masques", "Delta", "Filtre", "LFO", "Arpège"])
make_b("B-Poly-WorldTour", "muse", "World Tour", "Poly - World Tour", th("plum"),
       ["Osc", "Mix", "Modul.", "Filtres", "Env.", "Retard", "Global"], hero_mod("#c4a7ff", "#ffcf6b"),
       "Oscillateur de modulation", "Une source de modulation audio qui tord formes, hauteur et filtres.",
       ["mowave", "route"], ["morate", "mopitch", "mofilter", "mowaveamt", "o1wave", "o2wave"],
       ["morate", "mopitch", "mofilter", "f1cut", "f1res", "dlymix", "vibamt", "level"], ["Jouer", "Osc", "Modul.", "Filtres", "Retard", "Global"])
make_b("B-Sampler-Chromasample", "keys", "Chromasample", "Sampler - Chromasample", th("teal"),
       ["Sample", "Filtre", "Env. F", "Env. A", "Global", "Fichiers", "Jeu"], hero_sample("#4fd1c5", "#ffb36b"),
       "Échantillon", "Un fichier WAV joué sur tout le clavier, avec boucle et enveloppe.",
       ["loop", "ftype"], ["start", "end", "loopstart", "loopend", "root", "glide"],
       ["start", "end", "cutoff", "reso", "aa", "ar", "tune", "level"], ["Jouer", "Sample", "Filtre", "Env.", "Fichiers", "Global"])

# ============================== C ==============================
def legend4(a, b, c, d):
    return ["1-4 " + a, "5-8 " + b, "9-12 " + c, "13-16 " + d]

t = "acid"
make_c("C-Basse-AcidLover", t, "ACID LOVER", "Basse - Acid Lover", "silver", "Basse à un oscillateur et filtre à diodes", ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "Oscillateur", "flex": 2, "cols": 2, "items": [cb(t, "wave", span=2), ck(t, "tune", 0), ck(t, "fine", 0), ck(t, "glide", 0), ck(t, "accthr", 0)]},
        {"name": "Filtre", "flex": 3, "cols": 3, "items": [ck(t, "cutoff", 1, True), ck(t, "reso", 1), ck(t, "envmod", 1), ck(t, "decay", 1), ck(t, "keytrack"), ck(t, "drive")]},
        {"name": "Accent & ampli", "flex": 2, "cols": 2, "items": [ck(t, "accent", 2), ck(t, "ampdecay", 2), ck(t, "dist", 2), ck(t, "level", 2)]}],
       legend4("oscillateur", "filtre", "accent, ampli", "jeu"), ["Façade", "Jeu", "Global", "", "", ""], accent="#ffc04d")
t = "ms20"
make_c("C-Basse-Hurleur200", t, "HURLEUR 200", "Basse - Hurleur 200", "black", "Deux oscillateurs, passe-haut et passe-bas qui crient", ("Passe-bas", fmt(P(t, "lpcut")), "lpcut"),
       [{"name": "Oscillateurs", "flex": 3, "cols": 2, "items": [cb(t, "v1wave", "Forme 1"), ck(t, "v1level", 0, short="Niveau 1"), ck(t, "v1pw", 0, short="Largeur 1"), cb(t, "v2wave", "Forme 2"), ck(t, "v2pitch", 0, short="Écart 2"), ck(t, "v2level", 0, short="Niveau 2")]},
        {"name": "Filtres", "flex": 3, "cols": 3, "items": [ck(t, "hpcut", 1, short="Passe-haut"), ck(t, "hppeak", 1, short="Pic PH"), ck(t, "hpeg2", short="PH ← env."), ck(t, "lpcut", 1, True, short="Passe-bas"), ck(t, "lppeak", 1, short="Pic PB"), ck(t, "lpeg2", short="PB ← env.")]},
        {"name": "Enveloppes", "flex": 3, "cols": 3, "items": [ck(t, "e2attack", 2), ck(t, "e2decay", 2), ck(t, "e2sustain", 2), ck(t, "e2release", 2), ck(t, "e1attack", short="Att. haut."), ck(t, "e1release", short="Rel. haut.")]},
        {"name": "Modulation", "flex": 2, "cols": 2, "items": [ck(t, "mgrate", 3, short="Vitesse"), ck(t, "mgshape", 3, short="Forme"), ck(t, "pitchmg", 3, short="Hauteur ← gén."), ck(t, "drive", 3)]}],
       legend4("oscillateurs", "filtres", "enveloppe 2", "modulation"), ["Façade", "Hauteur", "Sortie", "", "", ""], accent="#ff9a3d")
t = "sh101"
make_c("C-Basse-MonoFirst", t, "MONO FIRST", "Basse - Mono First", "grey_light", "Un oscillateur, curseurs et arpège", ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "Oscillateur", "flex": 2, "cols": 2, "items": [cb(t, "range", "Registre"), ck(t, "tune", 0), ck(t, "pw", 0), ck(t, "vcolfo", 0, short="Hauteur ← LFO")]},
        {"name": "Mélangeur", "flex": 2, "cols": 4, "items": [cf(t, "pulse", 1, 230), cf(t, "saw", 1, 230), cf(t, "sub", 1, 230), cf(t, "noise", 1, 230)]},
        {"name": "Filtre", "flex": 2, "cols": 2, "items": [ck(t, "cutoff", 2, True), ck(t, "reso", 2), ck(t, "envamt", 2), ck(t, "lfoamt", 2), ck(t, "kbd")]},
        {"name": "Enveloppe", "flex": 2, "cols": 4, "items": [cf(t, "attack", 3, 230, "A"), cf(t, "decay", 3, 230, "D"), cf(t, "sustain", 3, 230, "S"), cf(t, "release", 3, 230, "R")]}],
       legend4("oscillateur", "mélangeur", "filtre", "enveloppe"), ["Façade", "LFO", "Jeu", "Arpège", "", ""], accent="#3aa6c8")
t = "poly5"
make_c("C-Poly-PolyFive", t, "POLY FIVE", "Poly - Poly Five", "walnut_black", "Polyphonique à deux oscillateurs et Poly-Mod", ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "Oscillateurs", "flex": 3, "cols": 3, "items": [ck(t, "asemi", 0, short="Fréq. A"), ck(t, "apw", 0, short="Largeur A"), ck(t, "bsemi", 0, short="Fréq. B"), ck(t, "bfine", 0, short="Fin B"), ck(t, "bpw", short="Largeur B"), cb(t, "async", "Synchro", span=1)]},
        {"name": "Mélangeur", "flex": 2, "cols": 3, "items": [cf(t, "mixa", 1, 200), cf(t, "mixb", 1, 200), cf(t, "mixnoise", 1, 200)]},
        {"name": "Filtre", "flex": 2, "cols": 2, "items": [ck(t, "cutoff", 2, True), ck(t, "reso", 2), ck(t, "envamt", 2), ck(t, "keytrack", 2)]},
        {"name": "Enveloppes", "flex": 3, "cols": 4, "items": [ck(t, "fa", 3), ck(t, "fd", 3), ck(t, "fs", short="F maintien"), ck(t, "fr", short="F relâch."), ck(t, "aa", 3), ck(t, "ar", 3), ck(t, "pmenv", short="Poly-Mod env."), ck(t, "pmoscb", short="Poly-Mod B")]}],
       legend4("oscillateurs", "mélangeur", "filtre", "enveloppes"), ["Façade", "Poly-Mod", "LFO", "Wheel-Mod", "Arpège", "Global"], accent="#e2702a")
t = "polyj"
make_c("C-Poly-Chorus80", t, "CHORUS 80", "Poly - Chorus 80", "grey_light", "Un oscillateur par voix et le chorus d'époque", ("Chorus", fmt(P(t, "chorus")), "chorus"),
       [{"name": "Oscillateur", "flex": 3, "cols": 3, "items": [cb(t, "range", "Registre", span=3), cf(t, "pw", 0, 170), cf(t, "pwmdepth", 0, 170), cf(t, "sub", 0, 170), cf(t, "noise", 0, 170)]},
        {"name": "Filtre", "flex": 3, "cols": 3, "items": [cb(t, "hpf", "Passe-haut", span=3), cf(t, "cutoff", 1, 170), cf(t, "reso", 1, 170), cf(t, "envamt", 1, 170), cf(t, "lfofilter", 1, 170)]},
        {"name": "Enveloppe", "flex": 3, "cols": 4, "items": [cf(t, "ea", 2, 220), cf(t, "ed", 2, 220), cf(t, "es", 2, 220), cf(t, "er", 2, 220)]},
        {"name": "LFO & chorus", "flex": 2, "cols": 2, "items": [cf(t, "lforate", 3, 170), cf(t, "lfodelay", 3, 170), cb(t, "chorus", "Chorus", span=2)]}],
       legend4("oscillateur", "filtre", "enveloppe", "LFO"), ["Façade", "Ampli", "Arpège", "Global", "", ""], accent="#d8507a")
t = "ob6"
make_c("C-Nappe-EtatDeGrace", t, "ÉTAT DE GRÂCE", "Nappe - Etat de Grace", "navy", "Polyphonique à filtre d'état continu et X-Mod", ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "Oscillateurs", "flex": 3, "cols": 3, "items": [ck(t, "afreq", 0, short="Fréq. A"), ck(t, "ashape", 0, short="Forme A"), ck(t, "bfreq", 0, short="Fréq. B"), ck(t, "bshape", 0, short="Forme B"), ck(t, "bfine", short="Fin B"), ck(t, "apw", short="Largeur A")]},
        {"name": "Mélangeur", "flex": 2, "cols": 4, "items": [cf(t, "mixa", 1, 200, "Osc. A"), cf(t, "mixsub", 1, 200, "Sous"), cf(t, "mixb", 1, 200, "Osc. B"), cf(t, "mixnoise", 1, 200, "Bruit")]},
        {"name": "Filtre d'état", "flex": 2, "cols": 2, "items": [ck(t, "cutoff", 2, True), ck(t, "reso", 2), ck(t, "fmode", 2, short="Bas → haut"), ck(t, "envamt", 2)]},
        {"name": "Enveloppes", "flex": 3, "cols": 4, "items": [ck(t, "fa", 3), ck(t, "fd", 3), ck(t, "fs", short="F maint."), ck(t, "fr", short="F relâch."), ck(t, "aa", 3), ck(t, "ar", 3), ck(t, "xenv", short="X-Mod env."), ck(t, "xoscb", short="X-Mod B")]}],
       legend4("oscillateurs", "mélangeur", "filtre", "enveloppes"), ["Façade", "LFO", "X-Mod", "Arpège", "Global", ""], accent="#7fb3ff")
t = "dfam"
seq = [cf(t, "p%d" % i, None, 120, "H%d" % i) for i in range(1, 9)] + [cf(t, "v%d" % i, None, 120, "V%d" % i) for i in range(1, 9)]
make_c("C-Kick-KickKiller", t, "KICK KILLER", "Kick - Kick Killer", "black", "Percussion analogique à 2 VCO et séquence de 8 pas", ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "VCO", "flex": 3, "cols": 3, "items": [ck(t, "vco1freq", 0, short="VCO 1"), ck(t, "vco1eg", 0, short="VCO 1 env."), ck(t, "fm", 0), ck(t, "vco2freq", 0, short="VCO 2"), ck(t, "vco2eg", short="VCO 2 env."), ck(t, "vcodecay", short="Déclin VCO")]},
        {"name": "Mélangeur & filtre", "flex": 3, "cols": 3, "items": [ck(t, "mix1", 1, short="VCO 1"), ck(t, "mixnoise", 1, short="Bruit"), ck(t, "mix2", 1, short="VCO 2"), ck(t, "cutoff", 1), ck(t, "reso", 2), ck(t, "vcfeg", 2, short="VCF env.")]},
        {"name": "Séquence · hauteurs puis vélocités", "flex": 5, "cols": 8, "items": seq},
        {"name": "VCA", "flex": 1, "cols": 1, "items": [ck(t, "vcfdecay", 3, short="Déclin VCF"), ck(t, "vcadecay", 3, short="Déclin VCA"), ck(t, "volume", 3)]}],
       legend4("VCO", "mélange, coupure", "résonance, VCF", "déclins, volume"), ["Façade", "Séquence", "Global", "", "", ""], accent="#ff8a3d")

for t, key, title, plugin, theme, acc in (("drum80", "C-Drum-Boum80", "BOUM 80", "Drum - Boum 80", "grey_light", "#e2702a"),
                                         ("drum90", "C-Drum-Club90", "CLUB 90", "Drum - Club 90", "silver", "#3aa6c8")):
    names = INST[t]["padNames"]
    secs = []
    for n in range(1, 9):
        q = lambda s: "p%02d_%s" % (n, s)
        secs.append({"name": abbr(names[n - 1]), "flex": 1, "cols": 1,
                     "items": [ck(t, q("tune"), 0 if n <= 4 else 1, short="Accord"), ck(t, q("decay"), 2 if n <= 4 else 3, short="Déclin", small=True),
                               ck(t, q("tone"), None, short=P(t, q("tone"))["name"], small=True), cf(t, q("level"), None, 110, "Niveau")]})
    make_c(key, t, title, plugin, theme, "Une colonne par son, comme sur la machine (sons 1 à 8)", ("1 BD · Accord", fmt(P(t, "p01_tune")), "p01_tune"),
           secs, legend4("accord 1-4", "accord 5-8", "déclin 1-4", "déclin 5-8"), ["Sons 1-8", "Sons 9-16", "1 BD", "2 SD", "3 CP", "4 RS  ›"], accent=acc)
print("ok")


# Fatsub (façade C dessinée à la main, mêmes réglages)
def ckf(t, pid, q=None, big=False, short=None):
    k = ck(t, pid, q, big, short)
    s = 120 if big else (62 if q is None else 78)
    k.update({"s": s, "i": round(s * 0.7), "ptr": round(s * 0.2)})
    return k


t = "mono"
make_c("C-Basse-Fatsub", t, "FATSUB", "Basse - Fatsub", "walnut_dark", "Basse monophonique à 2 oscillateurs et filtre échelle",
       ("Coupure", fmt(P(t, "cutoff")), "cutoff"),
       [{"name": "Oscillateurs", "flex": 290, "cols": 2, "items": [ckf(t, "o1oct", None, short="Octave 1"), ckf(t, "o1shape", 0, short="Forme 1"),
                                                                 ckf(t, "o2oct", None, short="Octave 2"), ckf(t, "o2shape", 0, short="Forme 2"),
                                                                 ckf(t, "o2semi", 0, short="Fréq. 2"), ckf(t, "o2fine", 0, short="Fin 2"),
                                                                 cb(t, "sync", "Synchro", th=CTHEMES["walnut_dark"])]},
        {"name": "Mélangeur", "flex": 250, "cols": 4, "items": [cf(t, "mix1", 1, 230, "Osc. 1"), cf(t, "mixsub", 1, 230, "Sous"),
                                                              cf(t, "mix2", 1, 230, "Osc. 2"), cf(t, "mixnoise", 1, 230, "Bruit")]},
        {"name": "Filtre", "flex": 300, "cols": 2, "items": [ckf(t, "cutoff", 2, True, "Coupure"), ckf(t, "reso", 2, short="Résonance"),
                                                           ckf(t, "drive", 2, short="Multidrive"), ckf(t, "envamt", 2, short="Quantité env."),
                                                           cb(t, "slope", "Pente", th=CTHEMES["walnut_dark"])]},
        {"name": "Enveloppes", "flex": 288, "cols": 3, "items": [ckf(t, "fa", 3, short="Att. filtre"), ckf(t, "fd", 3, short="Décl. filtre"),
                                                               ckf(t, "fs", None, short="Niveau F"), ckf(t, "aa", 3, short="Att. ampli"),
                                                               ckf(t, "ar", 3, short="Relâch. A"), ckf(t, "as", None, short="Niveau A")]}],
       legend4("oscillateurs", "mélangeur", "filtre", "enveloppes"), ["Façade", "Glisse", "Env. options", "Bus 1", "Bus 2", "Arpège"],
       scrW=240, vuW=150, btn=(44, 36, 15))
