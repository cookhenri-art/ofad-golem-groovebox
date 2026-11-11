"""
Groovebox — OFAD — AGPL-3.0-or-later
1.1 : les projets de démonstration, un par style (compétence « compositeur électro »).
5 à 6 pistes au plus par démo (petits téléphones). Chaque fonction écrit une fiche de prod en code :
tempo, tonalité, rythmique, basse, harmonie, sons, effets, arrangement, mix et master.

  python3 tools/demos/make_demos.py [style…]   → tools/demos/specs/<style>.json
Puis tools/demos/build.sh fabrique les .gbx (ui/public/demos/), les .mid et les mesures.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from golemlib import *  # noqa

# Pads des batteries
K909 = dict(kick=0, snare=1, clap=2, rim=3, ch=4, oh=5, tlo=6, tmid=7, thi=8, crash=9, ride=10)
K808 = dict(kick=0, snare=1, clap=2, rim=3, ch=4, oh=5, tlo=6, tmid=7, thi=8, conga=9, cow=10, cym=11, mara=12, clav=13)


def pad(p, **kw):
    """Réglages d'un pad : pad(1, tune=-2, decay=300) → {'p01_tune': -2, …}."""
    return {f'p{p + 1:02d}_{k}': v for k, v in kw.items()}


def merge(*ds):
    out = {}
    for d in ds:
        out.update(d)
    return out


def master_chain(drive, ceiling=-1.8, clip=20, low=0.0, air=0.5, glue_thr=-10, character=40, width_high=115):
    """Bus (colle 2:1, détection filtrée ; son gain d'entrée est réglé par l'outil pour viser la sonie) →
    chaîne de mastering (dynamique 3 bandes qui n'agit que sur les crêtes, image, excitation, maximiseur)
    → limiteur final en crête vraie (1.1 : le maximiseur du Varnish est coupé ; deux étages sans anticipation
    à la suite fabriquaient 3 dB de crêtes entre échantillons)."""
    # 1.1 : passe-haut du principal en 1er insert (balayé dans les montées) — l'égaliseur de la tranche
    # est APRÈS les inserts, l'automatiser ferait passer des crêtes au-dessus du limiteur
    return [fx('consoleeq', hpf=16),
            fx('bus', threshold=glue_thr, ratio=0, attack=4, release=4, makeup=0, schpf=100),
            fx('master', lowCut=2, lowGain=low, airGain=air, thrLow=-6, thrMid=-8, thrHigh=-9, ratLow=1.6, ratMid=1.3,
               ratHigh=1.3, widthLow=0, widthMid=100, widthHigh=width_high, shine=12, warmth=15, drive=min(drive, 3),
               ceiling=ceiling + 0.3, character=character, clipAmt=clip, truePeak=1, splitLow=1, maxOn=0),
            fx('limiter', ceiling=ceiling, truePeak=1, clip=1, release=60, style=0)]


def kick_dfam(freq, vca_decay, pitch_env=55, pitch_decay=40, click=12, vcf_decay=70, cutoff=2600, level=-6):
    """Grosse caisse accordée (percussion semi-modulaire) : VCO 1 triangle sur la fréquence de la tonique,
    enveloppe de hauteur courte, bruit pour le click, séquence 8 pas à plat (hauteur fixe)."""
    p = dict(seqmode=0, seqlen=8, prange=0, pitchto=1, vcodecay=pitch_decay, vco1eg=pitch_env, vco1freq=freq, vco1wave=0,
             fm=0, vco2eg=0, mix1=100, mix2=0, mixnoise=click, vcfmode=0, cutoff=cutoff, reso=8, vcfeg=45,
             vcfdecay=vcf_decay, noisevcf=0, vcaattack=0, vcadecay=vca_decay, volume=level, velsens=25, drift=0)
    for i in range(1, 9):
        p[f'p{i}'] = 0
        p[f'v{i}'] = 100
    return p


# =====================================================================================================
# 1. TRANCE — uplifting, 138 BPM, la mineur, i–VI–III–VII
# =====================================================================================================
def trance():
    d = Demo('trance', 'Démo Trance', 138, 50, 'la mineur')
    # Batterie (moteur 909) : grosse caisse courte et punchy, charleys en 16es, ouverts sur les contretemps
    dr = d.track('drum90', 'Batterie', color=0, params=merge(
        pad(K909['kick'], tune=0, decay=260, tone=55, snap=60, drive=3, level=-4),
        pad(K909['clap'], decay=260, tone=60, level=-9),
        pad(K909['snare'], decay=160, tone=65, snap=70, level=-10),
        pad(K909['ch'], decay=45, tone=70, level=-15, pan=10),
        pad(K909['oh'], decay=230, tone=70, level=-14, pan=-8),
        pad(K909['ride'], decay=900, tone=60, level=-17, pan=15),
        pad(K909['crash'], decay=1600, level=-14)),
        mix=dict(volume=-3, sendA=4))
    A = Pat(1)  # intro : grosse caisse, charleys
    A.grid(K909['kick'], 'x...x...x...x...', 118).grid(K909['ch'], 'oxoxoxoxoxoxoxox', 75, ghost=45).grid(K909['oh'], '..x...x...x...x.', 92)
    B = Pat(1)  # groove : + clap
    B.grid(K909['kick'], 'x...x...x...x...', 118).grid(K909['clap'], '....x.......x...', 100)
    B.grid(K909['ch'], 'oxoxoxoxoxoxoxox', 75, ghost=45).grid(K909['oh'], '..x...x...x...x.', 95)
    C = Pat(2)  # drop : + ride, crash en début de 2 mesures
    C.grid(K909['kick'], 'x...x...x...x...', 120).grid(K909['clap'], '....x.......x...', 104)
    C.grid(K909['ch'], 'oxoxoxoxoxoxoxox', 72, ghost=42).grid(K909['oh'], '..x...x...x...x.', 98)
    C.grid(K909['ride'], 'x.x.x.x.x.x.x.x.', 70).hit(K909['crash'], 0, 105)
    C.extra(K909['crash'], 0, cond=FIRST)
    E = Pat(4)  # montée : roulement de caisse claire noires → croches → 16es, sans grosse caisse
    E.roll(K909['snare'], 0, 4, 4, 45, 60).roll(K909['snare'], 16, 8, 2, 60, 80).roll(K909['snare'], 32, 16, 1, 80, 105)
    E.roll(K909['snare'], 48, 16, 1, 105, 127)
    E.grid(K909['ch'], 'oxoxoxoxoxoxoxox', 60, ghost=35)
    for p_, l in ((A, 'A'), (B, 'B'), (C, 'C'), (E, 'E')):
        d.pattern(dr, l, p_)

    prog = [('A', 'm'), ('F', ''), ('C', ''), ('G', '')]
    roots = [n('A1'), n('F1'), n('C2'), n('G1')]
    # Basse roulante : 3 doubles-croches entre les coups de grosse caisse (pas 2, 3, 4 de chaque temps)
    ba = d.track('mono', 'Basse roulante', preset='Basse grasse', color=1, params=dict(
        cutoff=1100, reso=18, envamt=55, fa=0.5, fd=110, fs=0, fr=40, aa=0.5, ad=140, **{'as': 0}, ar=35, mixsub=55, drive=8,
        glide=0, level=-6), mix=dict(volume=-6), inserts=[fx('consoleeq', hpf=32, lfgain=1.5, lffreq=80)])
    P = Pat(4)
    for bar, r in enumerate(roots):
        for beat in range(4):
            for k in (1, 2, 3):
                P.hit(0, bar * 16 + beat * 4 + k, 100 if k == 1 else 88, r if k != 2 else r + 12 if beat % 2 else r, 4)
    d.pattern(ba, 'A', P)

    # Pluck en arpège de 16es (enveloppe de filtre courte) → écho 3/16 et réverbération
    pl = d.track('pads', 'Pluck', preset='Pluck trance', color=2, mix=dict(volume=-13, sendA=18, sendB=22, pan=0),
                 inserts=[fx('consoleeq', hpf=220)])
    Q = Pat(4)
    voic = [chord('A4', 'm'), chord('F4', ''), chord('C5', ''), chord('G4', '')]
    for bar, ch in enumerate(voic):
        Q.arp(bar * 16, ch, '0 1 2 3 1 2 3 4 2 3 4 3 2 1 3 2', 16, 1, 92, 5)
    d.pattern(pl, 'A', Q)

    # Supersaw : accords tenus, pompage façon side-chain (courbe de volume à la noire)
    voic3 = [chord('A3', 'm', 1), chord('F3', '', 2), chord('C4', '', 0), chord('G3', '', 1)]
    ss = d.track('pads', 'Supersaw', preset='Supersaw', color=3, mix=dict(volume=-12, sendA=22, width=130),
                 inserts=[fx('consoleeq', hpf=180, hfgain=1.5), fx('sculptor', s7on=1, s7shape=5, s7len=2, s7amt=70)])
    S = Pat(4)
    for bar, ch in enumerate(voic3):
        S.chords(bar * 16, ch + [ch[0] + 12], 124, 100)
    d.pattern(ss, 'A', S)

    # Lead : la mélodie (la mineur harmonique), exposée filtrée dans le break, complète au drop
    ld = d.track('pads', 'Lead', preset='Lead supersaw', color=4, mix=dict(volume=-11, sendA=25, sendB=18),
                 inserts=[fx('consoleeq', hpf=250), fx('filter', type=0, cutoff=18000, reso=10)])
    L = Pat(4)
    L.line('E5 - A5 - C6 - B5 - A5 - - - E5 - - -', len8=7)
    L.line('F5 - A5 - C6 - A5 - F5 - - - E5 - - -', len8=7, at=16)
    L.line('E5 - G5 - C6 - D6 - E6 - - - D6 - C6 -', len8=7, at=32)
    L.line('B5 - D6 - B5 - G5 - B5 - - - - - - -', len8=7, at=48)
    d.pattern(ld, 'A', L)

    # Nappe large : break et drop (porte en 16es, trancegate)
    na = d.track('pads', 'Nappe', preset='Nappe large', color=5, mix=dict(volume=-15, sendA=30, width=140),
                 inserts=[fx('consoleeq', hpf=200), fx('sculptor', s7on=1, s7shape=7, s7len=0, s7amt=0)])
    N = Pat(4)
    for bar, ch in enumerate(voic3):
        N.chords(bar * 16, [c - 12 for c in ch] + [ch[1]], 126, 88)
    d.pattern(na, 'A', N)

    d.ret([fx('reverb', size=80, decay=3.6, predelay=22, brightness=7000, lowcut=300, highcut=11000, mix=100, width=130)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=38, mix=100, lowcut=300, highcut=6000, saturation=10, width=100)])
    d.master['inserts'] = master_chain(drive=7, clip=40)

    T = dict
    d.row('Intro', 8, t0='A', t1='stop', t2='stop', t3='stop', t4='stop', t5='stop')
    d.row('Intro basse', 8, t1='A')
    d.row('Groove', 16, t0='B', t2='A')
    d.row('Break', 16, t0='stop', t1='stop', t5='A', t4='A', t2='stop')
    d.row('Montée', 8, t0='E', t3='A')
    d.row('Drop', 16, t0='C', t1='A', t2='A', t3='A', t4='A', t5='A')
    d.row('Outro', 8, t0='A', t2='stop', t3='stop', t4='stop', t5='stop')
    br, mo, dp = d.bar_of('Break'), d.bar_of('Montée'), d.bar_of('Drop')
    ib = d.bar_of('Intro basse')
    d.auto(ba, 'inst', 'cutoff', [(ib, 300), (ib + 8, 1100)])  # la basse s'ouvre pendant l'intro
    d.auto(ld, 'fx2', 'cutoff', [(br, 900), (br + 16, 2500), (mo + 8 - 0.01, 9000), (dp, 18000)])  # lead filtré → ouvert
    d.auto(na, 'fx2', 's7amt', [(br, 0), (dp - 0.01, 0), (dp, 85)])  # porte de la nappe au drop seulement
    d.auto(MASTER, 'fx1', 'hpf', [(mo, 16), (mo + 7, 120), (mo + 8 - 0.01, 160), (dp, 16)])  # graves retirés dans la montée (passe-haut AVANT le limiteur)
    return d


# =====================================================================================================
# 2. HARD TECHNO — 150 BPM, fa mineur phrygien, grosse caisse saturée + rumble
# =====================================================================================================
def hardtechno():
    d = Demo('hardtechno', 'Démo Hard techno', 150, 50, 'fa mineur')
    F1 = hz(n('F1'))
    kk = d.track('dfam', 'Grosse caisse', color=0, params=kick_dfam(F1 * 1.0, 270, pitch_env=60, pitch_decay=38, click=16),
                 mix=dict(volume=-3, sendC=55),
                 inserts=[fx('dist', driveA=62, typeA=2, toneA=55, mixA=100, mixB=0, out=-6, postLow=1.5),
                          fx('consoleeq', hpf=28, lmfgain=-3, lmffreq=320, hmfgain=2, hmffreq=3500)])
    K = Pat(1).grid(0, 'x...x...x...x...', 120)
    K2 = Pat(4).grid(0, 'x...x...x...x...', 120)
    for i in (60, 62):
        K2.steps.pop((0, i), None)
    K2.hit(0, 62, 100)  # avant-dernier temps décalé : respiration en fin de phrase
    d.pattern(kk, 'A', K)
    d.pattern(kk, 'B', K2)

    pc = d.track('drum90', 'Percussions', color=1, params=merge(
        pad(K909['ch'], decay=40, tone=80, level=-13, drive=4), pad(K909['oh'], decay=160, tone=75, level=-12, drive=4),
        pad(K909['clap'], decay=300, tone=55, level=-10, drive=10), pad(K909['ride'], decay=700, tone=70, level=-15),
        pad(K909['rim'], decay=40, tone=70, level=-14, pan=-30), pad(K909['tlo'], tune=-5, decay=250, level=-14, pan=25),
        pad(K909['snare'], decay=150, snap=80, level=-11, drive=8)),
        mix=dict(volume=-6, sendA=6), inserts=[fx('dist', driveA=25, typeA=0, mixA=100, mixB=0, out=-2)])
    A = Pat(1).grid(K909['ch'], 'xoxoxoxoxoxoxoxo', 95, ghost=55).grid(K909['oh'], '..x...x...x...x.', 100)
    B = Pat(2).grid(K909['ch'], 'xoxoxoxoxoxoxoxo', 95, ghost=55).grid(K909['oh'], '..x...x...x...x.', 100)
    B.grid(K909['clap'], '....x.......x...', 105).grid(K909['rim'], '...x..x....x..x.', 85).grid(K909['tlo'], '..........x..x..x.......x..x.....', 80)
    C = Pat(2)
    for l, g_, v in ((K909['ch'], 'xoxoxoxoxoxoxoxo', 95), (K909['oh'], '..x...x...x...x.', 100), (K909['clap'], '....x.......x...', 108),
                     (K909['rim'], '...x..x....x..x.', 85), (K909['ride'], '..x...x...x...x.', 90)):
        C.grid(l, g_, v, ghost=55)
    E = Pat(4).grid(K909['ch'], 'xoxoxoxoxoxoxoxo', 85, ghost=50)
    E.roll(K909['snare'], 0, 8, 4, 50, 70).roll(K909['snare'], 32, 16, 1, 70, 110).roll(K909['snare'], 48, 16, 1, 110, 127)
    for p_, l in ((A, 'A'), (B, 'B'), (C, 'C'), (E, 'E')):
        d.pattern(pc, l, p_)

    # Stab rave : fa mineur 7, court, saturé, réverbération
    st = d.track('hybrid', 'Stab', preset='Accords stab', color=2, mix=dict(volume=-13, sendA=28, sendB=12),
                 params=dict(ad=220, **{'as': 0}, ar=180), inserts=[fx('consoleeq', hpf=220), fx('dist', driveA=30, typeA=1, out=-4)])
    S = Pat(2)
    fm7 = chord('F3', 'm7')
    for s_ in (0, 3, 6, 10, 13, 16 + 0, 16 + 3, 16 + 6, 16 + 12):
        S.chords(s_, fm7, 9, 100)
    S2 = Pat(2)
    for s_, c in ((0, chord('F3', 'm7')), (3, chord('F3', 'm7')), (6, chord('F3', 'm7')), (10, chord('Gb3', 'maj7')),
                  (16, chord('F3', 'm7')), (19, chord('F3', 'm7')), (22, chord('Eb3', '')), (28, chord('Db3', 'maj7'))):
        S2.chords(s_, c, 9, 100)
    d.pattern(st, 'A', S)
    d.pattern(st, 'B', S2)

    # Acide saturé : 16 pas, accents et glissés, ouvert par le morceau
    ac = d.track('acid', 'Acide', preset='Acide saturé', color=3, params=dict(cutoff=500, reso=78, envmod=60, decay=280, drive=10, dist=45),
                 mix=dict(volume=-12, sendB=18), inserts=[fx('consoleeq', hpf=60)], accent=70)
    Q = Pat(1).line('F1> F1 F2 F1 Eb2~ F2 F1 F1> Gb1 F1 C2~ Db2 F1 F2> F1 Ab1')
    d.pattern(ac, 'A', Q)

    # Lead « trance revival » dans le break : fa mineur – ré bémol – la bémol – mi bémol
    ld = d.track('pads', 'Lead', preset='Lead supersaw', color=4, mix=dict(volume=-12, sendA=35, sendB=20),
                 inserts=[fx('consoleeq', hpf=250)])
    L = Pat(4)
    L.line('C5 - F5 - Ab5 - G5 - F5 - - - C5 - Eb5 -', len8=7)
    L.line('Db5 - F5 - Ab5 - F5 - Db5 - - - C5 - - -', len8=7, at=16)
    L.line('C5 - Eb5 - Ab5 - Bb5 - C6 - - - Bb5 - Ab5 -', len8=7, at=32)
    L.line('G5 - Bb5 - G5 - Eb5 - G5 - - - - - - -', len8=7, at=48)
    d.pattern(ld, 'A', L)

    d.ret([fx('reverb', size=85, decay=4.5, predelay=10, brightness=5000, lowcut=250, highcut=6000, mix=100, width=120)])
    d.ret([fx('delay', mode=2, sync=1, division=6, feedback=45, mix=100, lowcut=400, highcut=5000, saturation=30)])
    d.ret([])
    # Retour C = rumble : réverbération longue (100 %) → saturation → passe-bas 180 Hz → pompage par la grosse caisse
    d.returns[2] = {'inserts': [fx('reverb', size=70, decay=3.0, predelay=0, lowcut=20, highcut=1500, mix=100, width=0),
                                fx('dist', driveA=55, typeA=1, mixA=100, mixB=0, out=-4),
                                fx('filter', type=0, cutoff=180, reso=15),
                                fx('sculptor', s7on=1, s7shape=5, s7len=2, s7amt=100)], 'mix': {'volume': 0}}
    d.master['inserts'] = master_chain(drive=9, clip=50, ceiling=-0.6)

    d.row('Intro', 16, t0='A', t1='A', t2='stop', t3='stop', t4='stop')
    d.row('Groove', 16, t1='B', t2='A')
    d.row('Break', 16, t0='stop', t1='A', t2='B', t4='A')
    d.row('Montée', 8, t1='E', t2='stop')
    d.row('Drop', 16, t0='B', t1='C', t2='A', t3='A', t4='stop')
    d.row('Drop 2', 16, t2='B', t4='A')
    d.row('Outro', 8, t1='A', t2='stop', t3='stop', t4='stop')
    ib, br, mo, dp, d2 = (d.bar_of(x) for x in ('Intro', 'Break', 'Montée', 'Drop', 'Drop 2'))
    d.auto(kk, 'mix', 'sendC', [(0, 0), (8, 0), (16, 55)])                         # la rumble arrive au groove
    d.auto(ac, 'inst', 'cutoff', [(dp, 350), (dp + 16, 900), (d2 + 8, 1800), (d2 + 16, 700)])
    d.auto(MASTER, 'fx1', 'hpf', [(mo, 16), (mo + 8 - 0.01, 180), (dp, 16)])
    return d


# =====================================================================================================
# 3. TRIBE / HARDTEK — 175 BPM, ré mineur, basse offbeat, percussions tribales, acide
# =====================================================================================================
def tribe():
    d = Demo('tribe', 'Démo Tribe', 175, 52, 'ré mineur')
    D1 = hz(n('D2'))  # 73 Hz : grosse caisse ronde accordée sur le ré
    kk = d.track('dfam', 'Grosse caisse', color=0, params=kick_dfam(hz(n('D1')) * 1.25, 190, pitch_env=58, pitch_decay=35, click=14),
                 mix=dict(volume=-3), inserts=[fx('dist', driveA=40, typeA=1, mixA=100, mixB=0, out=-4), fx('consoleeq', hpf=30)])
    K = Pat(1).grid(0, 'x...x...x...x...', 120)
    R = Pat(4).roll(0, 0, 8, 2, 90, 110).roll(0, 16, 16, 1, 90, 118).roll(0, 32, 16, 1, 100, 124).roll(0, 48, 16, 1, 110, 127)
    d.pattern(kk, 'A', K)
    d.pattern(kk, 'R', R)

    # Basse offbeat : une note par contretemps, courte (< 1/8 = 171 ms), motif sur 2 mesures
    ba = d.track('mono', 'Basse offbeat', preset='Basse saturée', color=1,
                 params=dict(cutoff=1400, reso=15, envamt=40, fd=90, fs=0, aa=0.5, ad=120, **{'as': 0}, ar=30, mixsub=60, drive=12, level=-6),
                 mix=dict(volume=-5), inserts=[fx('consoleeq', hpf=35)])
    B = Pat(2).line('. . D2 . . . D2 . . . D2 . . . F2 . . . D2 . . . D2 . . . C2 . . . A1 .', len8=6)
    d.pattern(ba, 'A', B)

    # Percussions tribales (boîte analogique 80) : toms accordés, conga, maracas, clap, charleys
    pc = d.track('drum80', 'Percussions', color=2, params=merge(
        pad(K808['tlo'], tune=-3, decay=420, level=-11, pan=-35), pad(K808['tmid'], tune=0, decay=360, level=-12, pan=10),
        pad(K808['thi'], tune=2, decay=300, level=-13, pan=40), pad(K808['conga'], decay=200, level=-13, pan=-20),
        pad(K808['mara'], decay=60, level=-17, pan=25), pad(K808['clap'], decay=250, level=-11),
        pad(K808['ch'], decay=50, level=-16), pad(K808['oh'], decay=260, level=-15)),
        mix=dict(volume=-5, sendA=10, sendB=6))
    A = Pat(2)
    A.grid(K808['tlo'], '..x...x.x..x..x...x...x...x..x..', 100).grid(K808['thi'], '...x.....x.x...x.......x.x.x...x', 92)
    A.grid(K808['conga'], '.x..x.x..x..x.x..x..x.x..xx.x.x.', 85).grid(K808['mara'], 'xxxxxxxxxxxxxxxx', 70, ghost=40)
    A.grid(K808['oh'], '..x...x...x...x.', 85)
    Bp = Pat(2)
    for k_, v in A.steps.items():
        Bp.steps[k_] = list(v)
    Bp.grid(K808['clap'], '....x.......x...', 100).grid(K808['tmid'], '.....x.......x......x....x...x..', 88)
    d.pattern(pc, 'A', A)
    d.pattern(pc, 'B', Bp)

    # Acide : rôle central, résonance haute, écho dub
    ac = d.track('acid', 'Acide', preset='Acide tribal', color=3, params=dict(cutoff=650, reso=80, envmod=62, decay=320, drive=8, dist=35),
                 mix=dict(volume=-11, sendB=25, sendA=8), accent=75, inserts=[fx('consoleeq', hpf=70)])
    Q = Pat(2).line('D2> D2 D3 D2 . D2 C3~ D3 D2 . F2> D2 A2~ C3 D2 D3> D2 D2 D3 D2 . D2 F3~ E3 D2 . C3> D2 A2 . D2 G2')
    d.pattern(ac, 'A', Q)

    # Nappe sombre (break)
    na = d.track('polyj', 'Nappe', preset='Nappe sombre', color=4, mix=dict(volume=-14, sendA=35), inserts=[fx('consoleeq', hpf=180)])
    N = Pat(4)
    for bar, c in enumerate([chord('D3', 'm'), chord('Bb2', ''), chord('C3', ''), chord('A2', 'm')]):
        N.chords(bar * 16, c, 126, 90)
    d.pattern(na, 'A', N)

    d.ret([fx('reverb', size=55, decay=1.8, predelay=8, lowcut=250, highcut=9000, mix=100)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=62, mix=100, lowcut=350, highcut=3500, saturation=35, echocolour=2)])
    d.master['inserts'] = master_chain(drive=8.5, clip=45, ceiling=-0.6)

    d.row('Intro', 16, t0='A', t1='stop', t2='A', t3='stop', t4='stop')
    d.row('Groove', 16, t1='A', t2='B')
    d.row('Acide', 16, t3='A')
    d.row('Break', 16, t0='stop', t1='stop', t2='A', t4='A')
    d.row('Montée', 4, t0='R', t4='stop')
    d.row('Drop', 16, t0='A', t1='A', t2='B', t3='A', t4='A')
    d.row('Outro', 8, t1='stop', t3='stop', t4='stop', t2='A')
    br, mo, dp = d.bar_of('Break'), d.bar_of('Montée'), d.bar_of('Drop')
    ab = d.bar_of('Acide')
    d.auto(ac, 'inst', 'cutoff', [(ab, 350), (ab + 16, 1200), (br, 600), (br + 16, 2800), (dp, 900), (dp + 16, 1600)])
    d.auto(ac, 'mix', 'sendB', [(br, 25), (br + 12, 55), (dp, 25)])
    d.auto(MASTER, 'fx1', 'hpf', [(mo, 16), (mo + 4 - 0.01, 150), (dp, 16)])
    return d


# =====================================================================================================
# 4. HARDCORE — mainstream, 175 BPM, fa mineur, grosse caisse accordée = la basse
# =====================================================================================================
def hardcore():
    d = Demo('hardcore', 'Démo Hardcore', 175, 50, 'fa mineur')
    F1 = hz(n('F1'))  # 43,7 Hz : queue de la grosse caisse
    kk = d.track('dfam', 'Grosse caisse', color=0, params=kick_dfam(F1, 380, pitch_env=70, pitch_decay=45, click=18, vcf_decay=90, cutoff=3200),
                 mix=dict(volume=-2),
                 inserts=[fx('dist', driveA=70, typeA=2, toneA=60, driveB=45, typeB=10, mixB=100, routing=0, out=-9, postLow=2),
                          fx('consoleeq', hpf=30, lmfgain=-4, lmffreq=280, hmfgain=3, hmffreq=2800, drive=20),
                          fx('dist', driveA=35, typeA=13, mixA=100, mixB=0, out=-3)])
    K = Pat(1).grid(0, 'x...x...x...x...', 124)
    Kf = Pat(4).grid(0, 'x...x...x...x...', 124)
    Kf.hit(0, 62, 110)  # kick doublé en fin de phrase
    R = Pat(4).roll(0, 0, 8, 2, 100, 112).roll(0, 16, 8, 2, 105, 116).roll(0, 32, 16, 1, 108, 122).roll(0, 48, 16, 1, 112, 127)
    d.pattern(kk, 'A', K)
    d.pattern(kk, 'B', Kf)
    d.pattern(kk, 'R', R)

    dr = d.track('drum90', 'Batterie', color=1, params=merge(
        pad(K909['clap'], decay=220, tone=60, level=-9, drive=6), pad(K909['oh'], decay=180, tone=75, level=-13),
        pad(K909['crash'], decay=1800, level=-13), pad(K909['ch'], decay=40, level=-17)),
        mix=dict(volume=-7, sendA=6))
    A = Pat(1).grid(K909['oh'], '..x...x...x...x.', 95).grid(K909['ch'], 'x.xxx.xxx.xxx.xx', 60)
    B = Pat(4).grid(K909['oh'], '..x...x...x...x.', 98).grid(K909['clap'], '....x.......x...', 104).hit(K909['crash'], 0, 110)
    B.extra(K909['crash'], 0, cond=FIRST)
    d.pattern(dr, 'A', A)
    d.pattern(dr, 'B', B)

    # Screech : riff court très résonnant et saturé
    sc = d.track('ms20', 'Screech', preset='Lead hurleur', color=2, mix=dict(volume=-14, sendA=15, sendB=18),
                 inserts=[fx('consoleeq', hpf=300), fx('dist', driveA=50, typeA=6, out=-8)])
    S = Pat(2).line('F4> . F4 Ab4 . F4 C5~ . F4> . Eb4 F4 . Ab4 G4 . F4> . F4 Ab4 . F4 Db5~ . C5> . Bb4 Ab4 . G4 F4 .', len8=5)
    d.pattern(sc, 'A', S)

    # Supersaw : la mélodie (break filtré, drop 2 complet)
    ss = d.track('pads', 'Supersaw', preset='Supersaw géante', color=3, mix=dict(volume=-12, sendA=28, sendB=15, width=130),
                 inserts=[fx('consoleeq', hpf=200), fx('sculptor', s7on=1, s7shape=5, s7len=2, s7amt=0)])
    M = Pat(4)
    M.line('C5 - F5 - Ab5 - C6 - Bb5 - Ab5 - G5 - F5 -', len8=7)
    M.line('Db5 - F5 - Ab5 - Db6 - C6 - - - Ab5 - - -', len8=7, at=16)
    M.line('Eb5 - G5 - Bb5 - Eb6 - Db6 - C6 - Bb5 - G5 -', len8=7, at=32)
    M.line('C5 - E5 - G5 - C6 - - - Bb5 - G5 - E5 -', len8=7, at=48)
    for bar, c in enumerate([chord('F3', 'm'), chord('Db3', ''), chord('Eb3', ''), chord('C3', '')]):
        for i, nt in enumerate(c):
            M.hit(1 + i, bar * 16, 82, nt, 126)
    d.pattern(ss, 'A', M)

    # Chœur / nappe du break
    ch_ = d.track('pads', 'Choeur', preset='Nappe chœur', color=4, mix=dict(volume=-15, sendA=40), inserts=[fx('consoleeq', hpf=200)])
    C = Pat(4)
    for bar, c in enumerate([chord('F3', 'm'), chord('Db3', ''), chord('Eb3', ''), chord('C3', '')]):
        C.chords(bar * 16, c, 126, 85)
    d.pattern(ch_, 'A', C)

    d.ret([fx('reverb', size=85, decay=3.2, predelay=15, lowcut=350, highcut=9000, mix=100, width=130)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=35, mix=100, lowcut=400, highcut=6000)])
    d.master['inserts'] = master_chain(drive=10, clip=55, ceiling=-0.5, character=30)

    d.row('Intro', 16, t0='A', t1='A', t2='stop', t3='stop', t4='stop')
    d.row('Drop', 16, t0='B', t1='B', t2='A')
    d.row('Break', 16, t0='stop', t1='stop', t2='stop', t3='A', t4='A')
    d.row('Montée', 8, t0='R', t1='A')
    d.row('Drop 2', 16, t0='B', t1='B', t2='A', t3='A', t4='stop')
    d.row('Outro', 8, t0='A', t1='A', t2='stop', t3='stop')
    br, mo, d2 = d.bar_of('Break'), d.bar_of('Montée'), d.bar_of('Drop 2')
    d.auto(ss, 'fx2', 's7amt', [(br, 0), (d2 - 0.01, 0), (d2, 75)])
    d.auto(kk, 'inst', 'cutoff', [(0, 900), (16 - 0.01, 3200), (mo, 1200), (mo + 8 - 0.01, 3200)])  # grosse caisse filtrée qui s'ouvre
    d.auto(MASTER, 'fx1', 'hpf', [(mo, 16), (mo + 8 - 0.01, 90), (d2, 16)])
    return d


# =====================================================================================================
# 5. FRENCHCORE — 200 BPM, la mineur, grosse caisse courte + basse offbeat, piano et cordes au break
# =====================================================================================================
def frenchcore():
    d = Demo('frenchcore', 'Démo Frenchcore', 200, 50, 'la mineur')
    A1 = hz(n('A1'))  # 55 Hz
    kk = d.track('dfam', 'Grosse caisse', color=0, params=kick_dfam(A1, 150, pitch_env=75, pitch_decay=30, click=25, vcf_decay=55, cutoff=4000),
                 mix=dict(volume=-2),
                 inserts=[fx('dist', driveA=65, typeA=2, toneA=65, mixA=100, mixB=0, out=-7, postLow=1),
                          fx('consoleeq', hpf=32, lmfgain=-3, lmffreq=300, hmfgain=4, hmffreq=3200),
                          fx('dist', driveA=40, typeA=13, mixA=100, mixB=0, out=-3)])
    K = Pat(1).grid(0, 'x...x...x...x...', 124)
    Kf = Pat(4).grid(0, 'x...x...x...x...', 124).hit(0, 62, 115)
    R = Pat(4).roll(0, 0, 8, 2, 100, 112).roll(0, 16, 8, 2, 106, 116).roll(0, 32, 16, 1, 108, 122).roll(0, 48, 16, 1, 114, 127)
    H = Pat(1).grid(0, 'x.......x.......', 115)  # demi-tempo (intro)
    for p_, l in ((K, 'A'), (Kf, 'B'), (R, 'R'), (H, 'H')):
        d.pattern(kk, l, p_)

    # Basse offbeat : carré saturé, 75 ms après… le « tchoum-pa », suit la progression la – fa – do – sol
    ba = d.track('mono', 'Basse offbeat', preset='Basse carrée', color=1,
                 params=dict(cutoff=1800, reso=12, envamt=35, fd=80, fs=0, aa=0.5, ad=105, **{'as': 0}, ar=25, mixsub=55, drive=14, level=-6),
                 mix=dict(volume=-5), inserts=[fx('consoleeq', hpf=38), fx('dist', driveA=35, typeA=1, out=-4)])
    B = Pat(4)
    for bar, r in enumerate(['A1', 'F1', 'C2', 'G1']):
        for beat in range(4):
            B.hit(0, bar * 16 + beat * 4 + 2, 105, n(r), 6)
    d.pattern(ba, 'A', B)

    dr = d.track('drum90', 'Batterie', color=2, params=merge(
        pad(K909['clap'], decay=150, tone=65, level=-10, drive=4), pad(K909['oh'], decay=120, level=-15),
        pad(K909['ch'], decay=35, level=-18), pad(K909['crash'], decay=1500, level=-13)),
        mix=dict(volume=-7, sendA=5))
    Dp = Pat(4).grid(K909['clap'], '....x.......x...', 100).grid(K909['oh'], '..x...x...x...x.', 80).grid(K909['ch'], '.x.x.x.x.x.x.x.x', 55)
    Dp.hit(K909['crash'], 0, 105).extra(K909['crash'], 0, cond=FIRST)
    d.pattern(dr, 'A', Dp)

    melody = [('E5 - - D5 C5 - B4 - A4 - - - C5 - E5 -'), ('F5 - - E5 D5 - C5 - A4 - - - - - - -'),
              ('G5 - - F5 E5 - D5 - C5 - - - E5 - G5 -'), ('B5 - - A5 G5 - D5 - B4 - - - - - - -')]
    # Piano (break) : mélodie + accords à la main gauche
    pi = d.track('fm6', 'Piano', preset='Piano FM brillant', color=3, mix=dict(volume=-9, sendA=35), inserts=[fx('consoleeq', hpf=90)])
    P = Pat(4)
    for i, m in enumerate(melody):
        P.line(m, at=16 * i, len8=10, vel=92)
    for bar, c in enumerate([chord('A3', 'm'), chord('F3', ''), chord('C3', ''), chord('G3', '')]):
        for i, nt in enumerate(c):
            P.hit(1 + i, bar * 16, 72, nt - 12, 60)
            P.hit(1 + i, bar * 16 + 8, 64, nt - 12, 50)
    d.pattern(pi, 'A', P)

    # Lead distordu : la même mélodie au drop 2
    ld = d.track('pads', 'Lead', preset='Lead supersaw', color=4, mix=dict(volume=-12, sendA=20, sendB=18),
                 inserts=[fx('consoleeq', hpf=280), fx('dist', driveA=35, typeA=1, out=-5)])
    L = Pat(4)
    for i, m in enumerate(melody):
        L.line(m, at=16 * i, len8=7)
    d.pattern(ld, 'A', L)

    # Cordes (break)
    co = d.track('fm6', 'Cordes', preset='Violons FM', color=5, mix=dict(volume=-14, sendA=40), inserts=[fx('consoleeq', hpf=150)])
    C = Pat(4)
    for bar, c in enumerate([chord('A3', 'm', 1), chord('F3', '', 2), chord('C4', ''), chord('G3', '', 1)]):
        C.chords(bar * 16, c, 126, 85)
    d.pattern(co, 'A', C)

    d.ret([fx('reverb', size=85, decay=3.4, predelay=15, lowcut=300, highcut=10000, mix=100, width=130)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=32, mix=100, lowcut=400, highcut=6000)])
    d.master['inserts'] = master_chain(drive=10, clip=55, ceiling=-0.5, character=30)

    d.row('Intro', 8, t0='H', t1='stop', t2='stop', t3='A', t4='stop', t5='stop')
    d.row('Drop', 16, t0='B', t1='A', t2='A', t3='stop')
    d.row('Break', 16, t0='stop', t1='stop', t2='stop', t3='A', t5='A')
    d.row('Montée', 8, t0='R', t2='A')
    d.row('Drop 2', 16, t0='B', t1='A', t2='A', t3='stop', t4='A', t5='stop')
    d.row('Outro', 8, t0='A', t4='stop', t3='A', t1='stop')
    mo, d2 = d.bar_of('Montée'), d.bar_of('Drop 2')
    d.auto(MASTER, 'fx1', 'hpf', [(mo, 16), (mo + 8 - 0.01, 120), (d2, 16)])
    return d


# =====================================================================================================
# 6. MINIMAL — deep tech / rominimal, 125 BPM, swing 56 %, ré mineur (dorien)
# =====================================================================================================
def minimal():
    d = Demo('minimal', 'Démo Minimal', 125, 56, 'ré mineur')
    dr = d.track('drum90', 'Batterie', color=0, params=merge(
        pad(K909['kick'], decay=190, tone=40, snap=30, drive=1, level=-4), pad(K909['rim'], decay=35, tone=60, level=-12),
        pad(K909['clap'], decay=180, tone=50, level=-13), pad(K909['ch'], decay=35, tone=65, level=-17),
        pad(K909['oh'], decay=140, tone=60, level=-17)),
        mix=dict(volume=-3, sendA=3))
    A = Pat(1).grid(K909['kick'], 'x...x...x...x...', 118).grid(K909['ch'], '..x...x...x...x.', 80)
    A.grid(K909['ch'], '.o.o.o.o.o.o.o.o', 40, ghost=40, at=0)
    B = Pat(2).grid(K909['kick'], 'x...x...x...x...', 118).grid(K909['ch'], 'oxo.oxoooxo.oxoo', 75, ghost=38)
    B.grid(K909['rim'], '....x.......x.......x.......x..o', 85, ghost=40).grid(K909['clap'], '............x...............x...', 70)
    B.grid(K909['oh'], '......x.......x.', 55)
    C = Pat(2)
    for k_, v in B.steps.items():
        if k_[0] != K909['kick']:
            C.steps[k_] = list(v)
    for p_, l in ((A, 'A'), (B, 'B'), (C, 'C')):
        d.pattern(dr, l, p_)

    # Percussions : clicks, claves, cloche… longueurs de ligne 3, 5, 7 pas (polyrythmie), probabilités
    pc = d.track('drumA', 'Percussions', color=1, params=merge(
        pad(12, mach=28, decay=15, level=-16, pan=-25, tone=70), pad(13, mach=15, decay=40, level=-18, pan=30),
        pad(14, mach=26, decay=120, tune=5, level=-21, pan=10), pad(15, mach=18, tune=7, decay=140, level=-18, pan=-40)),
        mix=dict(volume=-6, sendA=10, sendB=8))
    P = Pat(4)
    P.grid(12, 'x..', 80, at=0).grid(13, 'x....', 70).grid(14, '..x....', 60).grid(15, '.....x.........x...x....', 72)
    P.lane_len = {12: 3, 13: 5, 14: 7}
    for st in range(0, 64, 7):
        if (14, st + 2) in P.steps:
            P.extra(14, st + 2, prob=55)
    d.pattern(pc, 'A', P)

    # Basse en 16es syncopée, ronde, verrous de coupure sur quelques pas
    ba = d.track('mono', 'Basse', preset='Basse ronde', color=2,
                 params=dict(cutoff=650, reso=22, envamt=35, fd=140, fs=10, ad=180, **{'as': 20}, ar=40, drive=6, level=-6),
                 mix=dict(volume=-4), inserts=[fx('consoleeq', hpf=30)])
    Q = Pat(2).line('. D1 D2 . . D1 . F1 . D1 D2 . . A1 . . . D1 D2 . . D1 . F1 . D1 C2 . . A1 . G1', len8=5)
    Q.lock(0, 2, **{'inst.cutoff': 1100}).lock(0, 26, **{'inst.cutoff': 1300}).lock(0, 31, **{'inst.cutoff': 900})
    d.pattern(ba, 'A', Q)

    # Stab dub : ré mineur 9, une croche, écho 3/16 qui crée le motif
    st = d.track('polyj', 'Stab dub', preset='Accords techno', color=3, params=dict(cutoff=1500, reso=25),
                 mix=dict(volume=-14, sendA=25, sendB=45), inserts=[fx('consoleeq', hpf=250, lpf=7000)])
    S = Pat(2)
    S.chords(3, chord('D3', 'm9'), 7, 95)
    S.chords(19 + 3, chord('D3', 'm9'), 7, 80)
    S.extra(0, 22, cond=AB, a=2, b=2)
    d.pattern(st, 'A', S)

    # Séquence FM : micro-motif de 7 pas, discret
    sq = d.track('fm4', 'Séquence FM', preset='Pluck FM sec', color=4, mix=dict(volume=-17, sendB=20, pan=-15),
                 inserts=[fx('consoleeq', hpf=300)])
    F = Pat(1).line('A4 . D5 . . F5 .', vel=80, len8=4)
    F.lane_len = {0: 7}
    d.pattern(sq, 'A', F)

    d.ret([fx('reverb', size=45, decay=1.6, predelay=12, lowcut=300, highcut=8000, mix=100, width=110)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=55, mix=100, lowcut=400, highcut=4000, saturation=20)])
    d.master['inserts'] = master_chain(drive=4.5, clip=20, ceiling=-1, glue_thr=-12, air=1)

    d.row('Intro', 16, t0='A', t1='stop', t2='stop', t3='stop', t4='stop')
    d.row('Groove', 16, t0='B', t1='A', t2='A')
    d.row('Groove 2', 16, t3='A')
    d.row('Break', 8, t0='C', t2='stop', t4='A')
    d.row('Groove 3', 16, t0='B', t2='A')
    d.row('Outro', 8, t0='A', t3='stop', t4='stop', t1='stop')
    g2, g3 = d.bar_of('Groove 2'), d.bar_of('Groove 3')
    d.auto(st, 'inst', 'cutoff', [(g2, 900), (g2 + 16, 2200), (g3, 1400), (g3 + 16, 2600)])
    d.auto(17, 'fx1', 'feedback', [(g2, 45), (g2 + 15, 70), (g2 + 16, 50)])  # retour B : écho qui s'emballe
    return d


# =====================================================================================================
# 7. TECHNO BERLINOISE — 132 BPM, la mineur, grosse caisse profonde + rumble, séquence, stab dub
# =====================================================================================================
def techno():
    d = Demo('techno', 'Démo Techno', 132, 51, 'la mineur')
    kk = d.track('drum90', 'Grosse caisse', color=0, params=merge(pad(K909['kick'], tune=-2, decay=380, tone=45, snap=45, drive=4, level=-3)),
                 mix=dict(volume=-3, sendC=40), inserts=[fx('consoleeq', hpf=28, lmfgain=-2, lmffreq=350)])
    d.pattern(kk, 'A', Pat(1).grid(K909['kick'], 'x...x...x...x...', 118))

    pc = d.track('drum90', 'Percussions', color=1, params=merge(
        pad(K909['ch'], decay=40, tone=70, level=-14), pad(K909['oh'], decay=150, tone=65, level=-14),
        pad(K909['ride'], decay=800, tone=55, level=-16, pan=12), pad(K909['rim'], decay=35, level=-14, pan=-20),
        pad(K909['clap'], decay=260, tone=45, level=-15)),
        mix=dict(volume=-6, sendA=8), inserts=[fx('tapesat', drive=4, output=-3)])
    A = Pat(1).grid(K909['ch'], 'xoxoxoxoxoxoxoxo', 85, ghost=45).grid(K909['oh'], '..x...x...x...x.', 70)
    B = Pat(2).grid(K909['ch'], 'xoxoxoxoxoxoxoxo', 85, ghost=45).grid(K909['oh'], '..x...x...x...x.', 78)
    B.grid(K909['rim'], '...x...x..x...x....x...x..x..x..', 75).grid(K909['clap'], '............x...', 60)
    C = Pat(2)
    for k_, v in B.steps.items():
        C.steps[k_] = list(v)
    C.grid(K909['ride'], 'x.x.x.x.x.x.x.x.', 72, ghost=50)
    for p_, l in ((A, 'A'), (B, 'B'), (C, 'C')):
        d.pattern(pc, l, p_)

    # Séquence mono en 16es : la coupure s'ouvre lentement sur 32 mesures (le morceau)
    sq = d.track('sh101', 'Séquence', preset='Séquence grave', color=2, params=dict(cutoff=500, reso=45),
                 mix=dict(volume=-11, sendB=12), inserts=[fx('consoleeq', hpf=60)], accent=55)
    Q = Pat(1).line('A1> A1 A2 A1 C2 A1 A1 E2 A1> A1 G2 A1 C2 A1 E2 D2', len8=5)
    d.pattern(sq, 'A', Q)

    # Stab dub : la mineur 7, une croche → écho dub (retour B) + réverbération
    st = d.track('polyj', 'Stab dub', preset='Accords techno', color=3, params=dict(cutoff=1300, reso=30),
                 mix=dict(volume=-14, sendA=22, sendB=50), inserts=[fx('consoleeq', hpf=220, lpf=6500)])
    S = Pat(2).chords(6, chord('A3', 'm7'), 7, 95)
    S.chords(16 + 14, chord('A3', 'm7'), 7, 80)
    d.pattern(st, 'A', S)

    na = d.track('ob6', 'Nappe', preset='Nappe sombre d\'état', color=4, mix=dict(volume=-15, sendA=40), inserts=[fx('consoleeq', hpf=180)])
    N = Pat(4)
    for bar, c in enumerate([chord('A2', 'm7'), chord('F2', 'maj7'), chord('G2', ''), chord('E2', 'm7')]):
        N.chords(bar * 16, c, 126, 85)
    d.pattern(na, 'A', N)

    d.ret([fx('reverb', size=90, decay=5.0, predelay=20, lowcut=300, highcut=5000, mix=100, width=130)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=62, mix=100, lowcut=300, highcut=3000, saturation=30, echocolour=2)])
    d.ret([fx('reverb', size=70, decay=2.8, predelay=0, lowcut=20, highcut=1200, mix=100, width=0),
           fx('dist', driveA=35, typeA=0, mixA=100, mixB=0, out=-3), fx('filter', type=0, cutoff=150, reso=10),
           fx('sculptor', s7on=1, s7shape=6, s7len=2, s7amt=100)])
    d.master['inserts'] = master_chain(drive=6, clip=30, ceiling=-0.8)

    d.row('Intro', 16, t0='A', t1='A', t2='stop', t3='stop', t4='stop')
    d.row('Groove', 16, t1='B')
    d.row('Évolution', 32, t2='A', t3='A')
    d.row('Break', 8, t0='stop', t1='A', t2='stop', t4='A')
    d.row('Retour', 16, t0='A', t1='C', t2='A', t4='stop')
    d.row('Outro', 8, t1='A', t2='stop', t3='stop')
    gr, ev, rt = d.bar_of('Groove'), d.bar_of('Évolution'), d.bar_of('Retour')
    d.auto(kk, 'mix', 'sendC', [(0, 0), (gr - 0.01, 0), (gr, 40)])
    d.auto(sq, 'inst', 'cutoff', [(ev, 300), (ev + 32, 2200), (rt, 900), (rt + 16, 3000)])
    d.auto(17, 'fx1', 'feedback', [(ev + 28, 62), (ev + 32 - 0.01, 85), (ev + 32, 62)])
    return d


# =====================================================================================================
# 8. HOUSE — deep / French touch, 123 BPM, swing 57 %, fa mineur, accords m9 qui pompent
# =====================================================================================================
def house():
    d = Demo('house', 'Démo House', 123, 57, 'fa mineur')
    dr = d.track('drum90', 'Batterie', color=0, params=merge(
        pad(K909['kick'], decay=320, tone=50, snap=50, drive=2, level=-4), pad(K909['clap'], decay=280, tone=55, level=-10),
        pad(K909['oh'], decay=210, tone=65, level=-13, pan=-10), pad(K909['ch'], decay=40, tone=70, level=-16, pan=12),
        pad(K909['rim'], decay=30, level=-16, pan=20)),
        mix=dict(volume=-3, sendA=5))
    A = Pat(1).grid(K909['kick'], 'x...x...x...x...', 116).grid(K909['oh'], '..x...x...x...x.', 92).grid(K909['ch'], 'x.xxx.xxx.xxx.xx', 70, ghost=40)
    B = Pat(2).grid(K909['kick'], 'x...x...x...x...', 116).grid(K909['oh'], '..x...x...x...x.', 95).grid(K909['clap'], '....x.......x...', 100)
    B.grid(K909['ch'], 'x.xox.xox.xox.xo', 70, ghost=38).grid(K909['rim'], '...x..x.....x..x...o..x......x..', 60, ghost=35)
    for st in (4, 12, 20, 28):
        B.steps[(K909['clap'], st)][3] = -3  # clap très légèrement en avance
    C = Pat(2)
    for k_, v in B.steps.items():
        if k_[0] != K909['kick']:
            C.steps[k_] = list(v)
    E = Pat(4)
    for k_, v in A.steps.items():
        if k_[0] != K909['kick']:
            for bar in range(4):
                E.steps[(k_[0], k_[1] + 16 * bar)] = list(v)
    E.roll(K909['clap'], 32, 8, 2, 50, 80).roll(K909['clap'], 48, 16, 1, 70, 115)
    for p_, l in ((A, 'A'), (B, 'B'), (C, 'C'), (E, 'E')):
        d.pattern(dr, l, p_)

    pc = d.track('drum80', 'Percussions', color=1, params=merge(
        pad(K808['conga'], decay=220, level=-13, pan=-30), pad(K808['tmid'], tune=4, decay=260, level=-16, pan=30),
        pad(K808['mara'], decay=55, level=-18, pan=15), pad(K808['clav'], decay=60, level=-19, pan=-15)),
        mix=dict(volume=-6, sendA=10))
    P = Pat(2).grid(K808['conga'], '.x....x..x....x..x....x..x...x.x', 85).grid(K808['mara'], 'xoxoxoxoxoxoxoxo', 70, ghost=40)
    P.grid(K808['tmid'], '.......x...........x.......x....', 72).grid(K808['clav'], '...x......x.....', 60)
    d.pattern(pc, 'A', P)

    # Basse ronde syncopée : fa mineur 9 – si bémol mineur 9
    ba = d.track('polyj', 'Basse', preset='Basse carrée house', color=2, params=dict(voices=1),
                 mix=dict(volume=-5), inserts=[fx('consoleeq', hpf=32), fx('sculptor', s7on=1, s7shape=6, s7len=2, s7amt=35)])
    Q = Pat(2).line('F1 - . F2 . . F1 . . Ab1 . F1 . . Eb2 . Bb1 - . Bb2 . . Bb1 . . Db2 . Bb1 . C2 . Eb2', len8=6)
    d.pattern(ba, 'A', Q)

    # Accords m9 syncopés : filtre « French touch » + compresseur qui pompe sur la batterie + phaser lent
    ac = d.track('polyj', 'Accords', preset='Accords house', color=3,
                 mix=dict(volume=-11, sendA=15, sendB=12),
                 inserts=[fx('filter', type=1, cutoff=18000, reso=25),
                          fx('comp', style=7, threshold=-26, ratio=6, attack=2, release=220, scsrc=1, autogain=0, makeup=2),
                          fx('phaser', rate=0.25, depth=55, mix=30), fx('consoleeq', hpf=200)])
    S = Pat(2)
    for st_ in (2, 5, 8, 12):
        S.chords(st_, chord('F3', 'm9'), 9, 92)
    for st_ in (16 + 2, 16 + 5, 16 + 8, 16 + 13):
        S.chords(st_, chord('Bb2', 'm9', 1), 9, 92)
    d.pattern(ac, 'A', S)

    # Rhodes : accords longs du break, chorus
    rh = d.track('fm6', 'Rhodes', preset='Piano électrique 81', color=4, mix=dict(volume=-10, sendA=25),
                 inserts=[fx('chorus', rate=0.5, depth=2.5, mix=40), fx('consoleeq', hpf=120)])
    R = Pat(2)
    R.chords(0, chord('F3', 'm9'), 60, 85).chords(10, chord('F3', 'm9'), 40, 70)
    R.chords(16, chord('Bb2', 'm9', 1), 60, 85).chords(26, chord('C3', '7'), 40, 70)
    d.pattern(rh, 'A', R)

    d.ret([fx('reverb', size=50, decay=1.6, predelay=18, lowcut=250, highcut=9000, mix=100)])
    d.ret([fx('delay', mode=2, sync=1, division=9, feedback=35, mix=100, lowcut=350, highcut=5000)])
    d.master['inserts'] = master_chain(drive=5.5, clip=25, ceiling=-0.8)

    d.row('Intro', 16, t0='A', t1='A', t2='stop', t3='stop', t4='stop')
    d.row('Groove', 16, t0='B', t2='A', t3='A')
    d.row('Break', 16, t0='C', t2='stop', t4='A')
    d.row('Montée', 8, t0='E', t4='stop')
    d.row('Drop', 16, t0='B', t2='A', t3='A')
    d.row('Outro', 8, t0='A', t2='stop', t3='stop', t1='A')
    gr, br, mo, dp = (d.bar_of(x) for x in ('Groove', 'Break', 'Montée', 'Drop'))
    # le filtre des accords : fermé au groove, s'ouvre, se referme au break, monte dans la montée, grand ouvert au drop
    d.auto(ac, 'fx1', 'cutoff', [(gr, 500), (gr + 16, 2500), (br, 700), (br + 16, 1100), (mo + 8 - 0.01, 9000), (dp, 18000)])
    return d


# Mix : sonie de chaque piste seule par rapport à la référence (piste 1), et sonie intégrée du morceau
# (cibles club de la compétence, un peu en dessous pour garder du punch sur téléphone)
LEVELS = {
    # (sonie visée en LUFS intégrés, niveaux relatifs à la grosse caisse, atténuations par section en dB
    #  après le limiteur : un break reste 2 à 4 LU sous le drop, une montée remonte jusqu'au drop)
    'trance': (-11.5, {'Basse roulante': -3.5, 'Pluck': -7, 'Supersaw': -5, 'Lead': -4, 'Nappe': -7},
               {'Break': 2.5, 'Montée': 2}),
    'hardtechno': (-10, {'Percussions': -4, 'Stab': -5, 'Acide': -4, 'Lead': -6}, {'Break': 3, 'Montée': 1.5}),
    'tribe': (-10, {'Basse offbeat': -4, 'Percussions': -3.5, 'Acide': -4.5, 'Nappe': -8}, {'Break': 4.5, 'Montée': 1.5}),
    'hardcore': (-9.5, {'Batterie': -7, 'Screech': -4, 'Supersaw': -4, 'Choeur': -8},
                 {'Intro': 2.5, 'Break': 3.5, 'Montée': 1.5, 'Outro': 2.5}),
    'frenchcore': (-9.5, {'Basse offbeat': -4, 'Batterie': -7, 'Piano': -5, 'Lead': -4, 'Cordes': -9},
                   {'Break': 5, 'Montée': 2.5}),
    'minimal': (-12.5, {'Percussions': -8, 'Basse': -3, 'Stab dub': -7, 'Séquence FM': -10}, {}),
    'techno': (-12, {'Percussions': -6, 'Séquence': -5, 'Stab dub': -7, 'Nappe': -9}, {'Break': 3}),
    'house': (-12, {'Percussions': -7, 'Basse': -3, 'Accords': -5, 'Rhodes': -6}, {'Break': 3.5, 'Montée': 2.5}),
}


KICK_CLIP = 4.0      # dB de crêtes retirées à la grosse caisse avant le mix (le limiteur du principal respire)
CEILING_MAX = -2.0   # plafond du limiteur final (crête vraie mesurée vers -0,5 dBTP : sans anticipation, pas mieux)


def levels(name, d):
    target, rels, dips = LEVELS[name]
    d.target = target
    # grosse caisse (piste de référence) : écrêtage doux + limitation rapide en fin de chaîne
    kick = d.tracks[getattr(d, 'ref', 0) or 0]
    if len(kick.get('inserts', [])) < 4:
        kick.setdefault('inserts', []).append(fx('limiter', gain=KICK_CLIP, ceiling=-KICK_CLIP, clip=1, release=25, style=1))
    # plafond commun, maximiseur 0,3 dB au-dessus
    for ins in d.master['inserts']:
        if ins['type'] == 'limiter':
            ins['params']['ceiling'] = min(ins['params']['ceiling'], CEILING_MAX)
    for ins in d.master['inserts']:
        if ins['type'] == 'master':
            ins['params']['ceiling'] = min(ins['params']['ceiling'], CEILING_MAX + 0.3)
    # atténuations par section : volume de la tranche du principal, APRÈS le limiteur (une baisse de gain
    # ne crée aucune crête ; avant le limiteur, elle serait reprise par la limitation) ; un break est baissé
    # d'un bloc, une montée remonte en rampe jusqu'au drop
    pts, bar = [], 0
    for r in d.rows:
        dip = dips.get(r.get('label', ''), 0)
        end = bar + r['bars']
        ramp = r.get('label') == 'Montée'
        pts += [(bar, -dip), (end - 0.01, 0 if ramp else -dip)]
        bar = end
    if any(v for _, v in pts):
        # on garde les points où la courbe change (les paliers à 0 se résument à leurs bords)
        pts = [q for i, q in enumerate(pts) if not (0 < i < len(pts) - 1 and pts[i - 1][1] == q[1] == pts[i + 1][1])]
        d.auto(MASTER, 'mix', 'volume', pts)
    for t in d.tracks:
        if t['name'] in rels:
            t['rel'] = rels[t['name']]
    return d


DEMOS = dict(trance=trance, hardtechno=hardtechno, tribe=tribe, hardcore=hardcore, frenchcore=frenchcore,
             minimal=minimal, techno=techno, house=house)

if __name__ == '__main__':
    out = os.path.join(os.path.dirname(__file__), 'specs')
    os.makedirs(out, exist_ok=True)
    for name in (sys.argv[1:] or DEMOS):
        levels(name, DEMOS[name]()).save(os.path.join(out, f'{name}.json'))
        print('ok', name)
