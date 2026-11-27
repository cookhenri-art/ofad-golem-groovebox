"""OFAD GOLEM — dispositions faites à la main pour les greffons MPC / Force dont l'habillage automatique
range mal les écrans (trop de réglages, panneaux coupés au milieu, « (suite) »).

27 nov. 2026 (retour d'Henrico sur Good Doctor 2) : oscillateurs coupés d'un écran à l'autre, filtres
éparpillés, boutons de tailles différentes, déformations d'un seul oscillateur. Ici : une touche par
oscillateur (A, B, C) avec les MÊMES trois écrans pour chacun (réglages de base ; déformations, unisson,
grains ; spectral), un écran par filtre, enveloppes et LFO par paires, effets regroupés deux par deux.

Format : HAND_TABS[type] = [(touche, [écran, ...]), ...] ; un écran = [panneau, ...] ;
un panneau = (titre, couleur (indice TONES), [réglage, ...], rangées ou None) ;
un réglage = id, ou (id, libellé), ou (id, libellé, forme) avec forme parmi
"big" (bouton plus grand), "fader" (tirette), "toggle", "knob" (bouton même pour une liste),
ou une liste de mots courts pour les boutons à choix.
Règles : 16 réglages par écran au plus (les 16 Q-Links), 12 touches et 32 écrans en tout
(la touche JOUER de la maquette compte pour une touche et un écran)."""

MODE_W = ["Tables", "Grains", "Spectral"]
DEST_W = ["Filtres", "F1", "F2", "Direct"]
POL_W = ["Normale", "Inversée"]


def _osc(x, tone):
    X = x.upper()
    base = [
        ("Oscillateur " + X, tone, [
            (x + "_on", "Marche", "toggle"), (x + "_mode", "Mode", MODE_W), (x + "_tab", "Table"),
            (x + "_dest", "Sortie", DEST_W),
            (x + "_pos", "Position", "big"), (x + "_lvl", "Niveau", "big"), (x + "_pan", "Pan"),
        ], 2),
        ("Accord " + X, tone, [
            (x + "_oct", "Octave"), (x + "_semi", "Demi-tons"), (x + "_fine", "Accord fin"),
            (x + "_phase", "Phase"), (x + "_rand", "Hasard"), (x + "_delay", "Retard"),
            (x + "_inv", "Polarité", POL_W),
        ], 1),
    ]
    deform = [
        ("Déformations " + X, tone, [
            (x + "_w1", "Déformation 1"), (x + "_w1amt", "Intensité 1", "big"),
            (x + "_w2", "Déformation 2"), (x + "_w2amt", "Intensité 2", "big"),
        ], 2),
        ("Unisson " + X, tone, [
            (x + "_uni", "Voix"), (x + "_udet", "Désaccord"), (x + "_ublend", "Mélange"), (x + "_uwidth", "Largeur"),
        ], 2),
        ("Grains " + X, tone, [
            (x + "_gsrc", "Source"), (x + "_gwin", "Fenêtre"), (x + "_gsize", "Taille"), (x + "_gdens", "Densité"),
            (x + "_gspray", "Éparpillement"), (x + "_gscan", "Balayage"), (x + "_gpitch", "Hasard hauteur"),
            (x + "_grev", "À l'envers"),
        ], 3),
    ]
    spectral = [
        ("Spectral " + X, tone, [
            (x + "_ssrc", "Source"), (x + "_sparts", "Partiels", "big"), (x + "_sstretch", "Étirement", "big"),
            (x + "_sshift", "Décalage", "big"), (x + "_scut", "Coupure", "big"),
            (x + "_stilt", "Pente"), (x + "_sodd", "Pair / impair"), (x + "_sblur", "Flou"), (x + "_sscan", "Balayage"),
        ], 2),
    ]
    return ("OSC " + X, [base, deform, spectral])


def _filter(n, tone, with_route):
    ctl = [("f%don" % n, "Marche", "toggle"), ("f%dtype" % n, "Type")]
    if with_route:
        ctl.append(("route", "Filtres", ["Série", "Parallèle"]))
    ctl += [("f%dcut" % n, "Coupure", "big"), ("f%dres" % n, "Résonance", "big"), ("f%ddrive" % n, "Gain"),
            ("f%dshape" % n, "Forme"), ("f%dmix" % n, "Mélange"),
            ("f%denv" % n, "Env. 2"), ("f%dkey" % n, "Clavier"), ("f%dvel" % n, "Vélocité")]
    return ("Filtre %d" % n, tone, ctl, 2)


def _env(n, title, tone, extra=()):
    e = "e%d" % n
    return (title, tone, [(e + "a", "A", "fader"), (e + "h", "T", "fader"), (e + "d", "D", "fader"),
                          (e + "s", "S", "fader"), (e + "r", "R", "fader"),
                          (e + "ca", "Courbe A"), (e + "cd", "Courbe D"), (e + "cr", "Courbe R")] + list(extra), 1)


def _lfo(n, tone):
    l = "l%d" % n
    return ("LFO %d" % n, tone, [(l + "shape", "Forme"), (l + "mode", "Mode", ["Libre", "Redécl.", "1 coup"]),
                                 (l + "rate", "Vitesse", "big"), (l + "sync", "Synchro", "knob"),
                                 (l + "delay", "Retard", "knob"), (l + "smooth", "Lissage"), (l + "phase", "Phase")], 1)


def _steplfo(n, tone, rows=1):
    l = "lf%d_" % n
    return ("LFO %d" % n, tone, [(l + "shape", "Forme"), (l + "mode", "Mode"),
                                 (l + "rate", "Vitesse", "big"), (l + "sync", "Synchro", "knob"),
                                 (l + "delay", "Retard"), (l + "fade", "Fondu d'entrée"), (l + "phase", "Phase")], rows)


def _fx(title, tone, pre, ctls, rows=1):
    return (title, tone, [(pre + "on", "Marche", "toggle")] + ctls + [(pre + "ord", "Ordre")], rows)


def _matrix(a, b, tone):
    ctl = []
    for i in range(a, b + 1):
        ctl += [("mx%d_src" % i, "Source"), ("mx%d_dst" % i, "Destination"), ("mx%d_amt" % i, "Profondeur")]
    return ("Matrice %d-%d" % (a, b), tone, ctl, b - a + 1)


GOODDOC2 = [
    _osc("a", 1),
    _osc("b", 2),
    _osc("c", 7),
    ("MIX", [
        [("Mélangeur", 0, [("a_lvl", "A", "fader"), ("b_lvl", "B", "fader"), ("c_lvl", "C", "fader"),
                            ("sub", "Sous-osc.", "fader"), ("noise", "Bruit", "fader"), ("__volume__",),
                            ("a_pan", "Pan A"), ("b_pan", "Pan B"), ("c_pan", "Pan C"),
                            ("subpan", "Pan sous-osc."), ("npan", "Pan bruit")], 2)],
        [("Sous-oscillateur", 5, [("sub_on", "Marche", "toggle"), ("subshape", "Forme", "menu"), ("subdest", "Sortie", "menu"),
                                   ("suboct", "Octave", "big"), ("sub", "Niveau", "big"), ("subpan", "Pan"),
                                   ("subinv", "Polarité", POL_W)], 3),
         ("Bruit", 4, [("noise_on", "Marche", "toggle"), ("ntype", "Type"), ("ndest", "Sortie", "menu"),
                       ("ncolor", "Couleur", "big"), ("noise", "Niveau", "big"), ("npan", "Pan")], 2)],
    ]),
    ("FILTRES", [
        [_filter(1, 2, True)],
        [_filter(2, 3, False)],
    ]),
    ("ENV", [
        [_env(1, "Enveloppe 1 (ampli)", 2)],
        [_env(2, "Enveloppe 2 (filtres)", 1), _env(3, "Enveloppe 3 (hauteur)", 7, [("pamt", "Hauteur")])],
        [_env(4, "Enveloppe 4", 4),
         ("Env. 5", 6, [("me1_dly", "Retard"), ("me1_att", "Attaque"), ("me1_hld", "Tenue"), ("me1_dec", "Déclin"),
                        ("me1_sus", "Niveau"), ("me1_rel", "Relâchement"), ("me1_vel", "Vélocité"),
                        ("me1_loop", "Boucle", ["Non", "Oui"]), ("me1_sync", "Tempo", ["Libre", "Tempo"])], 1)],
    ]),
    ("LFO", [
        [_lfo(1, 1), _lfo(2, 2)],
        [_lfo(3, 7), _steplfo(4, 4)],
        [_steplfo(5, 6, 2)],
    ]),
    ("EFFETS", [
        [_fx("Distorsion", 3, "ds_", [("ds_type", "Type"), ("ds_pre", "Préfiltre"), ("ds_drive", "Poussée", "big"),
                                      ("ds_freq", "Fréquence"), ("ds_mix", "Mélange")], 3),
         _fx("Élargisseur", 1, "hy_", [("hy_rate", "Vitesse"), ("hy_depth", "Désaccord"), ("hy_dim", "Espace"),
                                       ("hy_mix", "Mélange")], 2)],
        [_fx("Flanger", 4, "fl_", [("fl_rate", "Vitesse"), ("fl_depth", "Profondeur"), ("fl_feed", "Réinjection"),
                                   ("fl_mix", "Mélange")]),
         _fx("Phaser", 6, "ph_", [("ph_rate", "Vitesse"), ("ph_depth", "Profondeur"), ("ph_freq", "Centre"),
                                  ("ph_feed", "Réinjection"), ("ph_mix", "Mélange")])],
        [_fx("Chorus", 5, "ch_", [("ch_rate", "Vitesse"), ("ch_depth", "Profondeur"), ("ch_delay", "Retard"),
                                  ("ch_feed", "Réinjection"), ("ch_mix", "Mélange")]),
         _fx("Réverbération", 2, "rv_", [("rv_size", "Taille"), ("rv_pre", "Pré-délai"), ("rv_bright", "Brillance"),
                                         ("rv_low", "Coupe-bas"), ("rv_width", "Largeur"), ("rv_mix", "Mélange")])],
        [_fx("Délai", 1, "dl_", [("dl_mode", "Mode"), ("dl_sync", "Tempo", ["Libre", "Tempo"]), ("dl_div", "Division"),
                                 ("dl_time", "Temps", "big"), ("dl_feed", "Réinjection", "big"), ("dl_mix", "Mélange", "big"),
                                 ("dl_low", "Coupe-bas"), ("dl_high", "Coupe-haut")], 2)],
        [_fx("Compresseur", 0, "cp_", [("cp_mode", "Mode", ["Simple", "3 bandes"]), ("cp_depth", "Profondeur", "big"),
                                       ("cp_time", "Temps"), ("cp_in", "Entrée"), ("cp_up", "Vers le haut"),
                                       ("cp_down", "Vers le bas"), ("cp_out", "Sortie"), ("cp_thr", "Seuil (simple)"),
                                       ("cp_ratio", "Rapport"), ("cp_low", "Grave"), ("cp_mid", "Médium"),
                                       ("cp_high", "Aigu")], 2)],
        [_fx("Filtre", 3, "fi_", [("fi_type", "Type"), ("fi_cut", "Coupure", "big"), ("fi_res", "Résonance"),
                                  ("fi_drive", "Gain"), ("fi_mix", "Mélange")]),
         _fx("Égaliseur", 5, "eq_", [("eq_lf", "Grave (Hz)"), ("eq_lg", "Grave"), ("eq_mf", "Médium (Hz)"),
                                     ("eq_mg", "Médium"), ("eq_mq", "Largeur"), ("eq_hf", "Aigu (Hz)"),
                                     ("eq_hg", "Aigu")])],
    ]),
    ("MATRICE", [
        [_matrix(1, 2, 0), _matrix(3, 4, 1), _matrix(5, 6, 2), _matrix(7, 8, 3), _matrix(9, 10, 4)],
        [_matrix(11, 12, 5), _matrix(13, 14, 6), _matrix(15, 16, 7), _matrix(17, 18, 0), _matrix(19, 20, 1)],
        [_matrix(21, 22, 2), _matrix(23, 24, 3), _matrix(25, 26, 4), _matrix(27, 28, 5), _matrix(29, 30, 6),
         _matrix(31, 32, 7)],
    ]),
    ("JEU", [
        [("Macros", 0, [("macro%d" % i, str(i)) for i in range(1, 9)], 2),
         ("Voix et sortie", 2, [("vmode", "Jeu"), ("level", "Volume"), ("voices", "Voix"), ("glide", "Glisse"),
                                ("tune", "Accord"), ("spread", "Largeur"), ("velamp", "Vél. ampli"),
                                ("pb_range", "Pitch bend")], 2)],
        [("Arpège", 6, [("arp_on", "Marche", "toggle"), ("arp_mode", "Mode"), ("arp_rate", "Division"),
                        ("arp_oct", "Octaves"), ("arp_gate", "Durée", "big"), ("arp_swing", "Swing", "big"),
                        ("arp_steps", "Pas"), ("arp_hold", "Maintien", ["Non", "Oui"])], 2)],
        [("Transposition 1-8", 1, [("arp_t%d" % i, "Transp. %d" % i) for i in range(1, 9)], 1),
         ("Transposition 9-16", 2, [("arp_t%d" % i, "Transp. %d" % i) for i in range(9, 17)], 1)],
    ]),
]

HAND_TABS = {"gooddoc2": GOODDOC2}


def check(plan):
    """Contrôle : nombre de touches, d'écrans et de réglages par écran."""
    n_scr = sum(len(s) for _, s in plan)
    out = []
    for name, scrs in plan:
        for i, scr in enumerate(scrs):
            n = sum(len(pn[2]) for pn in scr)
            if n > 18:
                out.append("%s écran %d : %d réglages" % (name, i + 1, n))
    return len(plan), n_scr, out
