#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# Kits d'usine de « Micro Tonnerre » (après la 1.1) : écrit src/instruments/mt/MtKits.h.
# Chaque kit = 8 sons de canal construits à partir de recettes classiques de synthèse de batterie
# (balayage de hauteur des grosses caisses, bruit filtré des caisses claires et des charleys, salves
# des claps, FM à fréquence audio des métaux…) ; aucun son ni kit d'un autre fabricant n'est recopié.
# Rôles fixes des canaux (noms des pads) : 1 grosse caisse, 2 caisse claire, 3 charley fermé,
# 4 charley ouvert, 5 clap, 6 tom, 7 percussion, 8 cymbale.
# Les niveaux sont égalisés par rôle (crête mesurée par l'outil de calibrage) : mt_levels.json.
#   python3 tools/presets/kits_microton.py          -> écrit l'en-tête
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from banks_common import cpp_string

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "src", "instruments", "mt", "MtKits.h")
LEVELS = os.path.join(HERE, "mt_levels.json")

KEYS = ["mix", "eqf", "eqg", "dist", "level", "pan", "choke",
        "wave", "freq", "mmode", "mamt", "mtime", "mrate", "oatk", "odec",
        "nfilt", "nfreq", "nq", "nstereo", "nenv", "natk", "ndec",
        "vosc", "vnoise", "vmod", "sendA", "sendB"]
SIN, TRI, SAW = 0, 1, 2
DEC, SINE, RND = 0, 1, 2
LP, BP, HP = 0, 1, 2
EXP, LIN, CLAP = 0, 1, 2

BASE = dict(mix=0, eqf=1000, eqg=0, dist=0, level=-8, pan=0, choke=0, wave=SIN, freq=100, mmode=DEC, mamt=0, mtime=30,
            mrate=10, oatk=0, odec=200, nfilt=HP, nfreq=5000, nq=10, nstereo=0, nenv=EXP, natk=0, ndec=50,
            vosc=60, vnoise=60, vmod=20, sendA=0, sendB=0)
ROLES = ["kick", "snare", "hatc", "hato", "clap", "tom", "perc", "cym"]
# crête visée par rôle (dB), comme les autres batteries de GOLEM (Batterie 80 / 90 / Impact)
TARGET = {"kick": -6.0, "snare": -7.0, "hatc": -14.0, "hato": -13.0, "clap": -6.5, "tom": -7.5, "perc": -10.0, "cym": -15.0}


def P(**kw):
    out = dict(BASE)
    out.update(kw)
    return out


def v(p, tune=0.0, dec=1.0, ndec=None, dist=None, mix=None, **kw):
    """Variante d'un son : accord (demi-tons, oscillateur et bruit), longueur, distorsion…"""
    q = dict(p)
    k = 2 ** (tune / 12.0)
    q["freq"] = min(20000, q["freq"] * k)
    q["nfreq"] = min(20000, q["nfreq"] * k)
    q["odec"] = max(10, q["odec"] * dec)
    q["ndec"] = max(10, q["ndec"] * (dec if ndec is None else ndec))
    if dist is not None: q["dist"] = dist
    if mix is not None: q["mix"] = mix
    q.update(kw)
    return q

# ---------------------------------------------------------------------------------------------
# Recettes de base
# grosses caisses : sinus (ou triangle) à balayage de hauteur en déclin, petit bruit pour le clic
K = {
    "ronde":    P(freq=48, mamt=18, mtime=60, odec=900, mix=3, nfilt=LP, nfreq=3000, ndec=8, eqf=60, eqg=2, vosc=55),
    "club":     P(freq=54, mamt=30, mtime=45, odec=420, mix=14, nfilt=HP, nfreq=3500, nq=12, ndec=16, dist=18, eqf=80, eqg=4),
    "frappe":   P(freq=50, mamt=40, mtime=35, odec=380, dist=35, eqf=70, eqg=5, mix=8, nfilt=BP, nfreq=2500, nq=20, ndec=10),
    "grondement": P(freq=42, mamt=26, mtime=70, odec=1500, dist=20, eqf=50, eqg=3),
    "dure":     P(freq=55, mamt=48, mtime=55, odec=600, dist=85, eqf=150, eqg=8, vosc=30),
    "seche":    P(freq=62, mamt=24, mtime=22, odec=180, mix=6, nfilt=HP, nfreq=6000, ndec=6),
    "triangle": P(wave=TRI, freq=52, mamt=36, mtime=40, odec=350, dist=25, eqf=90, eqg=3),
    "electro":  P(wave=TRI, freq=46, mamt=30, mtime=50, odec=500, dist=40, eqf=120, eqg=6),
    "profonde": P(freq=46, mamt=20, mtime=70, odec=550, eqf=55, eqg=3, mix=4, nfilt=LP, nfreq=1500, ndec=20),
    "poussiere": P(freq=52, mamt=22, mtime=50, odec=300, dist=20, mix=18, nfilt=LP, nfreq=900, nq=30, ndec=60, eqf=120, eqg=3),
    "boum":     P(freq=44, mamt=12, mtime=90, odec=2400, dist=30, eqf=60, eqg=2, vosc=40),
    "clic":     P(freq=70, mamt=50, mtime=8, odec=140, eqf=3000, eqg=4),
    "rave":     P(freq=50, mamt=42, mtime=38, odec=320, dist=45, eqf=100, eqg=4, mix=10, nfilt=HP, nfreq=4000, ndec=12),
    "tonnerre": P(freq=40, mamt=60, mtime=110, odec=900, dist=60, eqf=80, eqg=6),
}
# caisses claires : corps (triangle ou sinus, léger balayage) + bruit filtré
S = {
    "ronde":    P(wave=TRI, freq=180, mamt=6, mtime=30, odec=120, mix=62, nfilt=HP, nfreq=1800, nq=10, ndec=200, eqf=5000, eqg=2),
    "club":     P(wave=TRI, freq=200, mamt=14, mtime=25, odec=110, mix=55, nfilt=BP, nfreq=4500, nq=8, ndec=230, dist=10, eqf=3000, eqg=3),
    "serree":   P(freq=230, mamt=18, mtime=15, odec=70, mix=70, nfilt=HP, nfreq=3500, nq=15, ndec=120),
    "grasse":   P(wave=TRI, freq=170, mamt=10, mtime=30, odec=200, mix=55, nfilt=LP, nfreq=6000, nq=15, ndec=300, dist=25, eqf=250, eqg=4),
    "bruit":    P(freq=240, odec=60, mix=90, nfilt=BP, nfreq=2200, nq=18, ndec=180),
    "rave":     P(wave=TRI, freq=210, mamt=20, mtime=20, odec=140, mix=60, nfilt=HP, nfreq=2500, ndec=260, dist=40),
    "lineaire": P(wave=TRI, freq=190, mamt=8, odec=90, mix=72, nfilt=HP, nfreq=2800, nq=12, nenv=LIN, ndec=180),
    "poussiere": P(wave=TRI, freq=190, mamt=6, odec=120, mix=50, nfilt=LP, nfreq=4000, nq=20, ndec=160, dist=45, eqf=900, eqg=4),
    "claquante": P(wave=TRI, freq=210, mamt=8, odec=110, mix=65, nfilt=HP, nfreq=2200, ndec=240, nstereo=1, eqf=6000, eqg=3),
    "metal":    P(freq=320, mmode=SINE, mamt=9, mrate=505, odec=140, mix=55, nfilt=BP, nfreq=3800, nq=25, ndec=170, dist=15),
}
# charleys fermés : bruit passe-haut court, parfois un timbre métallique (scie modulée en sinus rapide)
HC = {
    "fin":      P(mix=100, nfilt=HP, nfreq=7800, nq=18, ndec=40, nstereo=1, choke=1, pan=8),
    "metal":    P(mix=80, wave=SAW, freq=6200, mmode=SINE, mamt=30, mrate=3100, odec=30, nfilt=HP, nfreq=8500, nq=12, ndec=35, nstereo=1, choke=1),
    "tic":      P(mix=100, nfilt=BP, nfreq=9000, nq=30, ndec=18, choke=1),
    "poussiere": P(mix=100, nfilt=LP, nfreq=7000, nq=20, ndec=30, dist=30, choke=1),
    "shaker":   P(mix=100, nfilt=HP, nfreq=5500, nq=8, nenv=LIN, natk=8, ndec=70, choke=1),
    "brillant": P(mix=100, nfilt=HP, nfreq=11000, nq=6, ndec=25, nstereo=1, choke=1),
}
HO = {k: v(p, ndec=9.0, dec=6.0) for k, p in HC.items()}
HO["shaker"] = v(HC["shaker"], ndec=3.0)
# claps : enveloppe « claquée » (trois salves dans l'attaque), bruit en passe-bande, stéréo
C = {
    "rond":     P(mix=100, nfilt=BP, nfreq=1150, nq=26, nenv=CLAP, natk=24, ndec=260, nstereo=1),
    "club":     P(mix=100, nfilt=BP, nfreq=1400, nq=22, nenv=CLAP, natk=20, ndec=230, nstereo=1, dist=10),
    "serre":    P(mix=100, nfilt=BP, nfreq=1800, nq=30, nenv=CLAP, natk=12, ndec=120, nstereo=1),
    "large":    P(mix=100, nfilt=BP, nfreq=1100, nq=18, nenv=CLAP, natk=30, ndec=800, nstereo=1),
    "doigts":   P(mix=100, nfilt=BP, nfreq=2600, nq=40, nenv=CLAP, natk=6, ndec=60),
    "sale":     P(mix=100, nfilt=BP, nfreq=1300, nq=20, nenv=CLAP, natk=22, ndec=300, nstereo=1, dist=55),
}
# toms et basses percussions
T = {
    "rond":     P(freq=120, mamt=6, mtime=150, odec=420, mix=6, nfilt=LP, nfreq=3000, ndec=20, pan=-15),
    "club":     P(wave=TRI, freq=140, mamt=10, mtime=90, odec=300, mix=20, nfilt=LP, nfreq=2500, nq=20, ndec=80, pan=-15),
    "electro":  P(freq=160, mamt=24, mtime=220, odec=600, mix=15, nfilt=BP, nfreq=1200, ndec=100, pan=-15),
    "conga":    P(freq=320, mamt=5, mtime=20, odec=180, pan=20),
    "bongo":    P(freq=420, mamt=8, mtime=15, odec=120, pan=25),
    "sale":     P(wave=TRI, freq=110, mamt=14, mtime=120, odec=500, dist=50, pan=-15),
}
# percussions : cloche (triangle + bruit très résonant = deuxième partiel), rimshot, claves, bips…
PC = {
    "cloche":   P(wave=TRI, freq=562, odec=320, mix=50, nfilt=BP, nfreq=845, nq=100, ndec=300, pan=20),
    "rim":      P(freq=1650, mamt=6, mtime=8, odec=28, mix=25, nfilt=BP, nfreq=3500, nq=40, ndec=25, pan=15),
    "claves":   P(freq=2500, odec=35, mix=20, nfilt=BP, nfreq=2500, nq=80, ndec=30, pan=-20),
    "bip":      P(freq=1200, mamt=-24, mtime=30, odec=90, pan=20),
    "laser":    P(freq=2000, mamt=-48, mtime=120, odec=250, pan=-20),
    "metal":    P(freq=560, mmode=SINE, mamt=12, mrate=1040, odec=210, mix=18, nfilt=BP, nfreq=3200, nq=30, ndec=60, pan=20),
    "bois":     P(wave=TRI, freq=900, mamt=4, odec=60, mix=15, nfilt=BP, nfreq=1800, nq=50, ndec=40, pan=-15),
    "acide":    P(wave=SAW, freq=220, mamt=12, mtime=40, odec=140, dist=40, eqf=1500, eqg=6, pan=10),
    "goutte":   P(freq=900, mamt=14, mtime=40, odec=110, pan=25),
    "grain":    P(wave=SAW, freq=300, mmode=RND, mamt=24, mrate=400, odec=180, dist=30, pan=-10),
    "stab":     P(wave=SAW, freq=262, mmode=SINE, mamt=0.3, mrate=6, odec=260, oatk=2, dist=55, eqf=2000, eqg=6, pan=10),
    "sonar":    P(freq=1400, mmode=SINE, mamt=1, mrate=7, oatk=3, odec=900, pan=-25),
}
# cymbales : bruit passe-haut long + scie modulée en sinus rapide (FM métallique)
Y = {
    "ride":     P(wave=SAW, freq=4100, mmode=SINE, mamt=30, mrate=1950, odec=1200, mix=55, nfilt=HP, nfreq=7000, nq=10, ndec=1100, nstereo=1, pan=-10),
    "crash":    P(wave=SAW, freq=3300, mmode=SINE, mamt=24, mrate=1700, odec=1400, mix=85, nfilt=HP, nfreq=5000, nq=4, ndec=1800, nstereo=1, pan=-10),
    "sale":     P(wave=SAW, freq=3600, mmode=SINE, mamt=36, mrate=2300, odec=900, mix=70, nfilt=HP, nfreq=4500, nq=6, ndec=1200, nstereo=1, dist=60, pan=-10),
    "souffle":  P(mix=100, nfilt=HP, nfreq=3000, nq=6, natk=420, ndec=40, nstereo=1),
    "cloche":   P(freq=2650, mmode=SINE, mamt=7, mrate=3770, odec=1600, mix=25, nfilt=BP, nfreq=5200, nq=60, ndec=900, pan=-10),
    "pluie":    P(mix=100, nfilt=BP, nfreq=6500, nq=12, nenv=LIN, ndec=900, nstereo=1),
}

# rôle de chaque recette (niveau visé), gardé par les variantes
for _d, _r in ((K, "kick"), (S, "snare"), (HC, "hatc"), (HO, "hato"), (C, "clap"), (T, "tom"), (PC, "perc"), (Y, "cym")):
    for _p in _d.values():
        _p["_role"] = _r

# ---------------------------------------------------------------------------------------------
# Kits : (nom, banque, [grosse caisse, caisse claire, charley fermé, charley ouvert, clap, tom, percussion, cymbale])
CL, EL, TE, HO_, HH, MI, RV, DU, EX = ("Classiques", "Électro", "Techno", "House", "Hip-hop", "Minimal", "Rave 96", "Dur", "Expérimental")
KITS = [
    # Classiques : boîtes analogiques des années 80 et 90 (principe), sons ronds
    ("Boîte ronde", CL, [K["ronde"], S["ronde"], HC["fin"], HO["fin"], C["rond"], T["rond"], PC["cloche"], Y["crash"]]),
    ("Boîte de club", CL, [K["club"], S["club"], HC["metal"], HO["metal"], C["club"], T["club"], PC["rim"], Y["ride"]]),
    ("Boîte ronde longue", CL, [v(K["ronde"], dec=1.6), v(S["ronde"], tune=-1), HC["fin"], v(HO["fin"], ndec=1.3), C["rond"], v(T["rond"], tune=-3), PC["claves"], Y["crash"]]),
    ("Boîte de club sale", CL, [v(K["club"], dist=45), v(S["club"], dist=35), HC["metal"], HO["metal"], C["sale"], T["club"], PC["rim"], Y["sale"]]),
    ("Boîte à congas", CL, [K["ronde"], S["serree"], HC["shaker"], HO["shaker"], C["doigts"], T["conga"], T["bongo"], PC["claves"]]),
    ("Boîte grave", CL, [v(K["ronde"], tune=-3), v(S["ronde"], tune=-3), v(HC["fin"], tune=-4), v(HO["fin"], tune=-4), v(C["rond"], tune=-3), v(T["rond"], tune=-5), v(PC["cloche"], tune=-3), v(Y["ride"], tune=-3)]),
    ("Boîte aiguë", CL, [v(K["club"], tune=3), v(S["club"], tune=4), v(HC["tic"], tune=2), v(HO["fin"], tune=2), v(C["serre"], tune=2), v(T["club"], tune=5), v(PC["rim"], tune=2), v(Y["ride"], tune=2)]),
    # Électro : triangles, bips, lasers, claps secs
    ("Électro de rue", EL, [K["electro"], S["bruit"], HC["tic"], HO["tic"], C["serre"], T["electro"], PC["bip"], Y["pluie"]]),
    ("Robot dansant", EL, [K["triangle"], S["lineaire"], HC["brillant"], HO["brillant"], C["club"], T["electro"], PC["laser"], Y["cloche"]]),
    ("Breakdance", EL, [v(K["electro"], dec=1.3), S["claquante"], HC["metal"], HO["metal"], C["rond"], v(T["electro"], tune=-4), PC["goutte"], Y["crash"]]),
    ("Bips et lasers", EL, [K["triangle"], S["bruit"], HC["tic"], HO["brillant"], C["serre"], v(PC["laser"], tune=-12), PC["bip"], v(PC["laser"], tune=7, dec=0.6)]),
    ("Électro froide", EL, [v(K["electro"], dist=15), v(S["metal"], dist=5), HC["fin"], HO["fin"], C["doigts"], T["conga"], PC["metal"], Y["cloche"]]),
    ("Miami", EL, [v(K["boum"], dec=0.7), S["club"], HC["brillant"], HO["fin"], C["large"], T["electro"], PC["claves"], Y["pluie"]]),
    ("Électro cassée", EL, [v(K["electro"], dist=70), v(S["bruit"], dist=40), HC["poussiere"], HO["poussiere"], C["sale"], T["sale"], PC["grain"], Y["sale"]]),
    # Techno : grosses caisses frappées, charleys métalliques, rides
    ("Entrepôt", TE, [K["frappe"], S["club"], HC["metal"], HO["metal"], C["club"], v(T["rond"], tune=-7, dec=1.5), PC["rim"], Y["ride"]]),
    ("Berlin la nuit", TE, [v(K["grondement"], dec=0.8), S["bruit"], HC["fin"], HO["fin"], C["serre"], T["sale"], PC["metal"], Y["ride"]]),
    ("Usine", TE, [v(K["frappe"], dist=60), v(S["metal"], dist=30), HC["metal"], HO["metal"], C["sale"], T["sale"], PC["grain"], Y["sale"]]),
    ("Acier", TE, [K["club"], S["metal"], HC["metal"], v(HO["metal"], tune=-2), C["serre"], v(PC["metal"], tune=-12), PC["metal"], v(Y["ride"], tune=1)]),
    ("Grondement", TE, [K["grondement"], S["serree"], HC["tic"], HO["fin"], C["club"], v(T["rond"], tune=-9, dec=2.0), PC["rim"], Y["ride"]]),
    ("Techno hypnotique", TE, [K["frappe"], S["lineaire"], HC["fin"], HO["fin"], C["doigts"], T["conga"], PC["sonar"], Y["cloche"]]),
    ("Détroit", TE, [K["club"], S["ronde"], HC["metal"], HO["fin"], C["rond"], T["club"], PC["cloche"], Y["ride"]]),
    # House : grosses caisses rondes, claps larges, shakers
    ("Maison chaude", HO_, [K["profonde"], S["ronde"], HC["shaker"], HO["fin"], C["large"], T["conga"], PC["cloche"], Y["ride"]]),
    ("Club de Chicago", HO_, [K["club"], S["club"], HC["metal"], HO["metal"], C["club"], T["club"], PC["rim"], Y["crash"]]),
    ("House profonde", HO_, [K["profonde"], S["serree"], HC["fin"], HO["fin"], C["rond"], T["bongo"], PC["bois"], Y["pluie"]]),
    ("Disco", HO_, [v(K["profonde"], dec=0.8), S["claquante"], HC["brillant"], HO["fin"], C["club"], T["club"], PC["cloche"], Y["crash"]]),
    ("Garage", HO_, [K["seche"], S["claquante"], HC["shaker"], HO["shaker"], C["serre"], T["conga"], PC["rim"], Y["ride"]]),
    ("Touche française", HO_, [v(K["club"], dist=30), v(S["club"], dist=20), HC["poussiere"], HO["poussiere"], C["sale"], T["club"], PC["stab"], Y["crash"]]),
    ("Tropicale", HO_, [K["profonde"], S["bruit"], HC["shaker"], HO["shaker"], C["doigts"], T["conga"], T["bongo"], PC["claves"]]),
    # Hip-hop : grosses caisses poussiéreuses, caisses claires grasses, longues basses
    ("Boom bap", HH, [K["poussiere"], S["grasse"], HC["poussiere"], HO["poussiere"], C["rond"], T["rond"], PC["rim"], Y["crash"]]),
    ("Trap", HH, [K["boum"], S["claquante"], HC["brillant"], HO["fin"], C["club"], v(K["boum"], tune=5), PC["cloche"], Y["pluie"]]),
    ("Vinyle", HH, [v(K["poussiere"], dist=35), S["poussiere"], HC["poussiere"], HO["poussiere"], C["sale"], T["sale"], PC["bois"], Y["sale"]]),
    ("Côte ouest", HH, [K["ronde"], S["club"], HC["fin"], HO["fin"], C["large"], T["rond"], PC["goutte"], Y["crash"]]),
    ("Basse qui glisse", HH, [v(K["boum"], dec=1.3), S["grasse"], HC["tic"], HO["brillant"], C["serre"], v(K["boum"], tune=3, dec=0.8), v(K["boum"], tune=7, dec=0.6), Y["pluie"]]),
    ("Lo-fi", HH, [K["poussiere"], v(S["poussiere"], tune=-2), v(HC["poussiere"], tune=-3), v(HO["poussiere"], tune=-3), v(C["rond"], tune=-2), T["rond"], PC["bois"], v(Y["ride"], tune=-5, dist=30)]),
    ("Drill", HH, [v(K["boum"], dec=0.9, dist=45), S["claquante"], HC["brillant"], HO["brillant"], C["serre"], T["electro"], PC["goutte"], Y["pluie"]]),
    # Minimal : courts, cliquants, beaucoup d'espace
    ("Clics", MI, [K["clic"], S["serree"], HC["tic"], HO["tic"], C["doigts"], T["bongo"], PC["goutte"], Y["cloche"]]),
    ("Microscope", MI, [K["seche"], v(S["serree"], dec=0.6), v(HC["tic"], tune=4), v(HO["tic"], tune=4), v(C["doigts"], tune=3), v(T["bongo"], tune=5), v(PC["bip"], tune=5), v(Y["cloche"], tune=3)]),
    ("Bois et métal", MI, [K["seche"], S["metal"], HC["tic"], HO["fin"], C["doigts"], PC["bois"], PC["metal"], Y["cloche"]]),
    ("Gouttes", MI, [K["clic"], S["lineaire"], HC["fin"], HO["fin"], C["serre"], v(PC["goutte"], tune=-5), PC["goutte"], v(PC["goutte"], tune=7)]),
    ("Ruban", MI, [K["seche"], S["serree"], HC["shaker"], HO["shaker"], C["doigts"], T["conga"], PC["rim"], Y["pluie"]]),
    ("Minimal sombre", MI, [v(K["seche"], tune=-3), v(S["lineaire"], tune=-3), HC["fin"], HO["fin"], C["serre"], v(T["conga"], tune=-7), PC["sonar"], v(Y["cloche"], tune=-5)]),
    ("Pointillés", MI, [K["clic"], S["bruit"], HC["brillant"], HO["brillant"], C["doigts"], T["bongo"], PC["claves"], PC["bip"]]),
    # Rave 96 : esprit des boîtes de groove de la fin des années 90 — triangles saturés, claps secs,
    # charleys métalliques, stabs et bips de rave
    ("Rave 96", RV, [K["rave"], S["rave"], HC["metal"], HO["metal"], C["club"], T["club"], PC["stab"], Y["ride"]]),
    ("Hangar", RV, [v(K["rave"], dist=60), S["rave"], HC["metal"], HO["metal"], C["sale"], T["sale"], PC["acide"], Y["sale"]]),
    ("Free party", RV, [v(K["triangle"], dist=55), v(S["rave"], dist=55), HC["poussiere"], HO["metal"], C["sale"], v(T["club"], dist=40), PC["acide"], Y["crash"]]),
    ("Néons", RV, [K["rave"], S["club"], HC["brillant"], HO["brillant"], C["club"], T["electro"], PC["bip"], Y["cloche"]]),
    ("Sirène", RV, [K["rave"], S["rave"], HC["metal"], HO["metal"], C["serre"], T["club"], v(PC["stab"], tune=5), PC["laser"]]),
    ("Groove box", RV, [K["triangle"], S["club"], HC["fin"], HO["fin"], C["club"], T["club"], PC["cloche"], Y["ride"]]),
    ("Acide 96", RV, [K["rave"], S["lineaire"], HC["metal"], HO["metal"], C["club"], v(PC["acide"], tune=-12), PC["acide"], Y["ride"]]),
    ("Petit matin", RV, [v(K["rave"], dec=1.4, dist=30), S["ronde"], HC["fin"], HO["fin"], C["large"], T["conga"], PC["sonar"], Y["crash"]]),
    # Dur : gabber, hardcore, industriel (distorsion, balayages longs)
    ("Gabber", DU, [K["dure"], v(S["rave"], dist=70), HC["metal"], HO["metal"], C["sale"], v(K["dure"], tune=7, dec=0.5), PC["acide"], Y["sale"]]),
    ("Frenchcore", DU, [v(K["dure"], dec=1.4, dist=95), v(S["grasse"], dist=60), HC["poussiere"], HO["metal"], C["sale"], T["sale"], PC["stab"], Y["sale"]]),
    ("Industriel", DU, [K["tonnerre"], S["metal"], HC["poussiere"], HO["poussiere"], C["sale"], T["sale"], PC["grain"], Y["sale"]]),
    ("Hardstyle", DU, [v(K["dure"], tune=-2, dist=75, mtime=80), S["claquante"], HC["metal"], HO["metal"], C["large"], T["electro"], PC["stab"], Y["crash"]]),
    ("Tekno", DU, [v(K["frappe"], dist=80), v(S["bruit"], dist=50), HC["metal"], HO["metal"], C["sale"], T["sale"], PC["acide"], Y["ride"]]),
    ("Marteau", DU, [v(K["tonnerre"], dec=0.6), v(S["serree"], dist=60), HC["tic"], HO["poussiere"], C["club"], v(K["tonnerre"], tune=5, dec=0.4), PC["metal"], Y["sale"]]),
    ("Terreur", DU, [v(K["dure"], tune=3, dec=0.6, dist=100), v(S["rave"], tune=3, dist=80), HC["poussiere"], HO["poussiere"], C["sale"], v(T["sale"], tune=5), PC["grain"], Y["sale"]]),
    # Expérimental : modulations aléatoires, sons inversés, cloches, sonar
    ("Laboratoire", EX, [K["clic"], v(S["metal"], mmode=RND, mrate=900), HC["tic"], HO["brillant"], C["doigts"], PC["grain"], PC["sonar"], Y["cloche"]]),
    ("Souffle inversé", EX, [K["profonde"], S["lineaire"], HC["shaker"], HO["shaker"], C["large"], T["electro"], PC["sonar"], Y["souffle"]]),
    ("Cloches", EX, [K["seche"], S["metal"], HC["tic"], HO["fin"], C["doigts"], v(Y["cloche"], tune=-12), v(PC["cloche"], tune=7), Y["cloche"]]),
    ("Glitch", EX, [v(K["clic"], dist=50), v(S["bruit"], dec=0.4), v(HC["tic"], ndec=0.6), HO["tic"], v(C["doigts"], dist=60), PC["grain"], PC["bip"], PC["laser"]]),
    ("Orage", EX, [v(K["tonnerre"], dec=2.0), S["bruit"], HC["fin"], HO["fin"], C["large"], v(T["electro"], tune=-12, dec=2.0), PC["sonar"], Y["pluie"]]),
    ("Insectes", EX, [K["seche"], v(S["bruit"], tune=12), v(HC["tic"], tune=7), HO["tic"], v(C["doigts"], tune=12), v(PC["grain"], tune=12, mrate=1500), v(PC["bip"], tune=12), Y["pluie"]]),
    ("Planète lointaine", EX, [K["grondement"], S["lineaire"], HC["fin"], HO["fin"], C["large"], v(PC["sonar"], tune=-12), PC["sonar"], Y["souffle"]]),
]


def kit_values(name, chans, corr):
    out = []
    for c, p in enumerate(chans):
        q = dict(p)
        q["level"] = max(-48.0, min(12.0, q["level"] + corr.get(name, [0.0] * 8)[c]))
        out.append(q)
    return out


def num(x):
    if abs(x - round(x)) < 1e-6:
        return str(int(round(x)))
    return ("%.4f" % x).rstrip("0").rstrip(".")


def calibrate(measured_path):
    """Corrige mt_levels.json à partir des crêtes mesurées (tools/MtCalib.cpp) : crête visée par rôle."""
    corr = json.load(open(LEVELS, encoding="utf-8")) if os.path.exists(LEVELS) else {}
    meas = json.load(open(measured_path, encoding="utf-8"))
    worst = 0.0
    for name, _, chans in KITS:
        cur = corr.get(name, [0.0] * 8)
        for c, p in enumerate(chans):
            d = TARGET[p["_role"]] - meas[name][c]
            worst = max(worst, abs(d))
            cur[c] = round(cur[c] + d, 2)
        corr[name] = cur
    json.dump(corr, open(LEVELS, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("écart le plus grand avant correction : %.2f dB" % worst)


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--calibrate":
        calibrate(sys.argv[2])
    corr = json.load(open(LEVELS, encoding="utf-8")) if os.path.exists(LEVELS) else {}
    names = [k[0] for k in KITS]
    assert len(names) == len(set(names)), "nom de kit en double"
    for name, _, chans in KITS:
        assert len(chans) == 8, name
    lines = ["/*",
             "    Groovebox — OFAD — AGPL-3.0-or-later",
             "",
             "    Kits d'usine de Micro Tonnerre — FICHIER ÉCRIT PAR tools/presets/kits_microton.py (ne pas modifier).",
             "    Réglages originaux construits sur les recettes classiques de synthèse de batterie ; niveaux",
             "    égalisés par rôle (crête mesurée, mt_levels.json).",
             "*/",
             "",
             "#pragma once",
             "",
             "namespace grv::instruments::mt",
             "{",
             "    struct KitDef { const char* name; const char* bank; Patch ch[numChannels]; };",
             "",
             "    inline const std::vector<KitDef>& factoryKits()",
             "    {",
             "        static const std::vector<KitDef> kits {"]
    for name, bank, chans in KITS:
        vals = kit_values(name, chans, corr)
        lines.append("            { %s, %s, {" % (cpp_string(name), cpp_string(bank)))
        for q in vals:
            lines.append("                { " + ", ".join(num(float(q[k])) for k in KEYS) + " },")
        lines.append("            } },")
    lines += ["        };",
              "        return kits;",
              "    }",
              "",
              "    /** Ajoute les kits d'usine aux sons de l'instrument (tous les réglages des 8 canaux). */",
              "    inline void addFactoryKits (ModuleDesc& m)",
              "    {",
              "        for (const auto& k : factoryKits())",
              "        {",
              "            Preset pr { k.name, {}, k.bank };",
              "            for (int c = 0; c < numChannels; ++c)",
              "                for (int i = 0; i < numCh; ++i)",
              "                    pr.values.push_back ({ prefix (c) + chIds[i], k.ch[c].get (i) });",
              "            m.presets.push_back (std::move (pr));",
              "        }",
              "    }",
              "} // namespace grv::instruments::mt",
              ""]
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"{len(KITS)} kits -> {OUT}")


if __name__ == "__main__":
    main()
