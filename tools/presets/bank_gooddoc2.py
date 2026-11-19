# Good Doctor 2 (après la 1.1, principe des grands synthés hybrides de 2025) : 95 sons, avec une grosse part
# de basses dures (kicks gabber, frenchcore, hardstyle, reverse bass, contretemps, roulantes, reese, hoover).
# Réglages originaux construits sur les recettes de chaque catégorie (aucun préréglage d'un autre fabricant).
#
# Tables : 0 analogique (pos 0 sinus, 25 triangle, 50 scie, 75 carré, 100 impulsion), 1 voyelles, 2 cloches FM,
# 3 plis, 4 harmoniques paires, 5 granuleux, 6 cordes frottées, 7 8 bits, 8 scie qui gronde, 9 synchro balayée,
# 10 carré qui se pince, 11 sinus sali, 12 kick (sinus de plus en plus écrêté), 13 hoover, 14 sinus → carré,
# 15 voix (formants), 16-18 tables perso.
# Déformations : 1 synchro, 2 courbure +, 3 courbure −, 4 pulsation, 5 miroir, 6 asymétrie, 7 quantifier, 8 FM suiv.,
# 9 FM sous-osc., 10 AM, 11 anneau, 12 repli, 13 saturer, 14 redresser, 15 bits, 16 passe-bas, 17 passe-haut,
# 18 synchro fenêtrée, 19 phase douce, 20 retourner, 21 écraser, 22 demi-onde, 23 FM bruit, 24 anneau sous-osc.
# Filtres : 0 multi, 1 PB 24, 2 PB 12, 3 PH 24, 4 PH 12, 5 bande, 6 réjection, 7 agressif PB, 8 agressif PH, 9 doux PB,
# 10 peigne +, 11 peigne −, 12 formants, 13 diode, 14 PB 18, 15 PB 6, 16 disperseur, 17 phaseur.
# Distorsion du rack : 0 lampe, 1 douce, 2 dure, 3 diode, 4 repli linéaire, 5 repli sinus, 6 zéro-carré, 7 asymétrique,
# 8 échantillonnage, 9 bits. Formes de LFO : 0 sinus, 1 triangle, 2 scie ↑, 3 scie ↓, 4 carré, 5 pompe, 6 aspiration,
# 7 hasard tenu, 8 hasard doux. Synchro : 7 1/4, 8 1/8, 9 1/16, 13 1/8T, 16 1/8 pointée. Modes : 0 libre, 1 redéclenché,
# 2 en un coup. Sources de la matrice : 1-4 env. 1-4, 5 env. 5, 6-8 LFO 1-3, 9-10 LFO à pas 4-5, 11 vélocité,
# 13 molette, 14 pression, 18.. macros. Sources granulaires : 0 échantillon, 1 chœur, 2 souffle, 3 cloches, 4 voix,
# 5 cordes, 6 grondement. Sources spectrales : 0 table de l'osc., 1 échantillon, 2 chœur … 7 grondement.
# Délai (division) : 7 1/8, 9 1/8 pointée, 10 1/4, 12 1/4 pointée.
from banks_common import Bank, mix, ACID, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

SAW, SQR, TRI, SIN, PULSE = 50, 75, 25, 0, 100

# --- archétypes des basses dures
KICK = dict(vmode=1, voices=1, glide=0, a_rand=0, a_tab=12, a_pos=20, a_lvl=85, f1on=0, spread=0,
            pamt=48, e3a=0, e3h=0, e3d=45, e3s=0, e3r=40, e3cd=-70,
            e1a=0, e1h=8, e1d=480, e1s=0, e1r=60, e1cd=-35,
            ds_on=1, ds_ord=1, ds_type=2, ds_drive=24, ds_mix=100,
            cp_on=1, cp_ord=2, cp_mode=1, cp_depth=35,
            eq_on=1, eq_ord=3, eq_lg=2, eq_mf=2500, eq_mg=3, eq_mq=1)
HARDB = dict(vmode=1, voices=1, glide=0, a_rand=0, a_tab=12, a_pos=55, a_lvl=80, sub_on=1, sub=40, subshape=0, suboct=-1, subdest=3,
             f1type=1, f1cut=2600, f1res=10, f1env=0, f1key=30, spread=0,
             e1a=1, e1h=0, e1d=320, e1s=70, e1r=60,
             ds_on=1, ds_ord=1, ds_type=7, ds_drive=22,
             eq_on=1, eq_ord=2, eq_mf=700, eq_mg=4, eq_mq=1.2, eq_hg=-2,
             cp_on=1, cp_ord=3, cp_mode=1, cp_depth=40)
REVERSE = mix(HARDB, e1a=170, e1ca=75, e1h=0, e1d=1, e1s=100, e1r=12, e1cr=-20)
OFFB = dict(vmode=1, voices=1, glide=0, a_rand=0, a_tab=0, a_pos=SAW, a_lvl=95, sub_on=1, sub=70, subshape=0, suboct=-1, subdest=3,
            f1type=1, f1cut=480, f1res=25, f1env=60, f1key=30, e2a=0.5, e2d=160, e2s=0, e2r=80,
            e1a=0.5, e1d=230, e1s=0, e1r=50, e1cd=-40, spread=0)
MONO = dict(vmode=1, voices=1, glide=0, a_rand=0, a_tab=0, a_pos=SAW, a_lvl=80, sub_on=1, sub=55, subshape=0, suboct=-1, subdest=3,
            f1type=1, f1cut=420, f1res=18, f1env=45, f1key=30, e2a=1, e2d=260, e2s=20, e2r=150,
            e1a=1, e1d=600, e1s=85, e1r=90, spread=0)
REESE = mix(MONO, a_uni=4, a_udet=22, a_ublend=85, a_uwidth=70, b_on=1, b_tab=0, b_pos=SAW, b_lvl=70, b_uni=3, b_udet=14, b_rand=0,
            sub=45, f1cut=900, f1res=10, f1env=15, ds_on=1, ds_type=0, ds_drive=12, ds_mix=60)
GROWL = mix(MONO, a_tab=8, a_pos=40, f1type=7, f1cut=700, f1res=45, f1env=35, sub=50, l1sync=8, l1shape=0, l1mode=1,
            ds_on=1, ds_type=0, ds_drive=16, ds_mix=70, cp_on=1, cp_depth=65)
LEADB = dict(vmode=1, voices=1, glide=45, a_tab=0, a_pos=SAW, a_lvl=75, a_uni=5, a_udet=28, a_ublend=75, a_uwidth=90,
             b_on=1, b_tab=0, b_pos=SAW, b_fine=9, b_lvl=55, a_fine=-9,
             f1type=1, f1cut=3200, f1res=18, f1env=30, f1key=60, e2d=500, e2s=45, e1a=2, e1d=800, e1s=90, e1r=220,
             l2rate=5.3, l2mode=0, mx1_src=13, mx1_dst="tune", mx1_amt=1)
PADB = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=65, a_uni=5, a_udet=20, a_uwidth=100, b_on=1, b_tab=6, b_pos=40, b_lvl=55,
            f1type=2, f1cut=2400, f1res=12, f1env=15, e2a=900, e2d=2500, e2s=40, e2r=1800,
            e1a=900, e1d=1500, e1s=85, e1r=2200, spread=80, l1rate=0.15, l1mode=0, mx1_src=6, mx1_dst="a_pos", mx1_amt=18,
            ch_on=1, ch_ord=1, ch_mix=35, rv_on=1, rv_ord=2, rv_size=70, rv_mix=30)
PLK = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=95, a_uni=3, a_udet=14, f1type=1, f1cut=560, f1res=30, f1env=70, f1key=50,
           e2a=0.5, e2d=180, e2s=0, e2r=180, e1a=0.5, e1d=320, e1s=0, e1r=260, spread=50,
           dl_on=1, dl_ord=1, dl_div=9, dl_feed=30, dl_mix=20)
KEY = dict(vmode=0, voices=8, a_tab=4, a_pos=30, a_lvl=70, f1type=2, f1cut=3000, f1res=8, f1env=20, e2d=700, e2s=30,
           e1a=1, e1d=1400, e1s=35, e1r=500, spread=40, ch_on=1, ch_mix=25)
STRB = dict(vmode=0, voices=8, a_tab=6, a_pos=35, a_lvl=70, a_uni=3, a_udet=12, b_on=1, b_tab=6, b_pos=60, b_lvl=60, b_fine=10, a_fine=-10,
            f1type=2, f1cut=4200, f1res=8, f1env=10, e1a=350, e1d=900, e1s=90, e1r=1300, spread=85, l2delay=6,
            mx1_src=7, mx1_dst="tune", mx1_amt=1, rv_on=1, rv_mix=25)
BRS = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=75, b_on=1, b_tab=4, b_pos=60, b_lvl=50, b_fine=6,
           f1type=1, f1cut=900, f1res=15, f1env=55, f1key=50, e2a=60, e2d=400, e2s=55, e1a=40, e1d=600, e1s=85, e1r=250, spread=50)
ARPB = dict(vmode=0, voices=8, a_tab=0, a_pos=SQR, a_lvl=95, f1type=1, f1cut=850, f1res=35, f1env=60, e2d=200, e2s=10,
            e1a=0.5, e1d=260, e1s=20, e1r=200, arp_mode=1, arp_rate=3, spread=40, dl_on=1, dl_div=7, dl_feed=35, dl_mix=22)
GRAN = dict(vmode=0, voices=8, a_mode=1, a_gsrc=1, a_gsize=120, a_gdens=40, a_gspray=25, a_gscan=5, a_uwidth=90, a_lvl=80,
            f1type=2, f1cut=6000, f1res=8, f1env=0, e1a=600, e1d=1500, e1s=90, e1r=2500, spread=80, rv_on=1, rv_size=75, rv_mix=35)
SPEC = dict(vmode=0, voices=8, a_mode=2, a_ssrc=2, a_sparts=40, a_lvl=80, f1type=2, f1cut=8000, f1env=0,
            e1a=500, e1d=1500, e1s=85, e1r=2200, spread=60, rv_on=1, rv_mix=30)


def build():
    b = Bank("gooddoc2")

    def add(name, cat, values):
        out = {}
        for k, v in values.items():
            if k.endswith("_dst") and isinstance(v, str):
                out[f"{k}@{v}"] = 0
            else:
                out[k] = v
        b.add(name, cat, out)

    # --- Basses dures : kicks (les notes de la phrase accordent la queue)
    add("Kick gabber", BASS, mix(KICK))
    add("Kick gabber long", BASS, mix(KICK, e1d=900, e1s=10, ds_drive=28))
    add("Kick frenchcore", BASS, mix(KICK, a_pos=45, pamt=60, e3d=32, e1d=950, e1s=25, ds_type=0, ds_drive=34,
                                      fi_on=1, fi_ord=4, fi_type=0, fi_cut=9000, fi_res=10))
    add("Kick frenchcore saturé", BASS, mix(KICK, a_pos=70, pamt=60, e3d=28, e1d=1000, e1s=30, ds_type=6, ds_drive=30, cp_depth=60))
    add("Kick hardstyle (queue)", BASS, mix(KICK, a_pos=60, pamt=36, e3d=60, e1d=1200, e1s=60, e1r=120, ds_type=7, ds_drive=26,
                                            eq_mf=700, eq_mg=4, eq_mq=1.2, cp_depth=50))
    add("Kick rawstyle", BASS, mix(KICK, a_pos=80, pamt=40, e3d=50, e1d=1100, e1s=55, ds_type=3, ds_drive=32,
                                    eq_mf=900, eq_mg=5, fl_on=1, fl_ord=4, fl_rate=0.1, fl_depth=30, fl_feed=40, fl_mix=20))
    add("Kick hardcore sec", BASS, mix(KICK, pamt=54, e3d=25, e1d=320, ds_type=2, ds_drive=34))
    add("Kick terror", BASS, mix(KICK, a_pos=90, pamt=66, e3d=20, e1d=260, ds_type=2, ds_drive=36, ds_mix=80, cp_depth=60))
    add("Kick techno rond", BASS, mix(KICK, a_pos=0, pamt=30, e3d=55, e1d=420, ds_type=1, ds_drive=8, cp_on=0))
    add("Kick 909 poussé", BASS, mix(KICK, a_pos=10, pamt=36, e3d=40, e1d=520, ds_type=0, ds_drive=20, noise_on=1, noise=12, ncolor=90, ndest=3))
    add("Kick-basse psy", BASS, mix(KICK, a_pos=0, pamt=28, e3d=35, e1d=260, e1s=0, ds_type=1, ds_drive=10, cp_depth=25, eq_mg=0))
    add("Kick distordu à la molette", BASS, mix(KICK, ds_drive=18, mx1_src=13, mx1_dst="ds_drive", mx1_amt=50, mx2_src=13, mx2_dst="a_pos", mx2_amt=60))
    # --- reverse bass et queues (hardstyle) : la basse joue entre les kicks
    add("Reverse bass (aspiration)", BASS, mix(REVERSE))
    add("Reverse bass grasse", BASS, mix(REVERSE, a_pos=75, ds_type=0, ds_drive=28, sub=55))
    add("Reverse bass euphorique", BASS, mix(REVERSE, a_tab=0, a_pos=SAW, a_uni=3, a_udet=10, f1cut=1800, ds_drive=16, ch_on=1, ch_ord=4, ch_mix=20))
    add("Reverse bass au LFO", BASS, mix(HARDB, e1s=100, e1d=1, l1shape=6, l1sync=7, l1mode=1, mx1_src=6, mx1_dst="level", mx1_amt=35))
    add("Aspiration (couche inversée)", BASS, mix(HARDB, b_on=1, b_tab=12, b_pos=55, b_lvl=70, b_inv=1, b_delay=9, b_rand=0, e1a=40, e1ca=60))
    add("Queue hardstyle tenue", BASS, mix(HARDB, e1d=800, e1s=80, e1r=120))
    add("Queue qui grince", BASS, mix(HARDB, a_w1=5, a_w1amt=35, f1type=8, f1cut=400, f1res=40, ph_on=1, ph_ord=4, ph_mix=30))
    add("Basse pompée (pompe 1/4)", BASS, mix(MONO, e1s=100, l1shape=5, l1sync=7, l1mode=1, mx1_src=6, mx1_dst="level", mx1_amt=40,
                                               cp_on=1, cp_depth=40))
    # --- contretemps et roulantes
    add("Basse contretemps", BASS, mix(OFFB))
    add("Basse contretemps ronde", BASS, mix(OFFB, a_pos=SQR, f1cut=260, f1res=15, sub=60))
    add("Basse roulante psy", BASS, mix(OFFB, f1cut=430, f1res=30, f1env=70, e2d=100, e1d=140))
    add("Basse roulante sale", BASS, mix(OFFB, f1cut=430, f1res=35, f1env=70, e2d=100, e1d=140, ds_on=1, ds_type=0, ds_drive=14, ds_mix=60))
    add("Basse techno au contretemps", BASS, mix(OFFB, a_tab=10, a_pos=20, f1type=13, f1cut=520, f1res=40, e2d=180))
    # --- basses classiques et modernes
    add("Sub pur", BASS, mix(MONO, a_tab=14, a_pos=0, sub_on=0, f1on=0, e1d=1, e1s=100, e1r=80))
    add("Sub sali", BASS, mix(MONO, a_tab=14, a_pos=25, sub_on=0, f1on=0, e1s=100, ds_on=1, ds_type=1, ds_drive=12, ds_mix=50))
    add("Basse scie franche", BASS, mix(MONO))
    add("Reese large", BASS, mix(REESE))
    add("Reese qui tourne", BASS, mix(REESE, l1rate=0.25, l1mode=0, mx1_src=6, mx1_dst="f1cut", mx1_amt=18, route=0, f2on=1, f2type=6, f2cut=800, f2res=40,
                                     l2rate=0.4, mx2_src=7, mx2_dst="f2cut", mx2_amt=25))
    add("Reese écrasée", BASS, mix(REESE, cp_on=1, cp_depth=80, cp_in=6, hy_on=1, hy_mix=30))
    add("Hoover", BASS, mix(MONO, vmode=2, glide=120, a_tab=13, a_pos=40, a_uni=7, a_udet=45, a_uwidth=100, sub_on=0, f1on=0,
                             pamt=4, e3a=0, e3d=220, e3s=0, e1s=95, ch_on=1, ch_mix=40, ds_on=1, ds_type=0, ds_drive=10, ds_mix=50))
    add("Hoover hardcore", BASS, mix(MONO, vmode=2, glide=90, a_tab=13, a_pos=70, a_uni=7, a_udet=55, a_uwidth=100, sub_on=1, sub=30, f1on=0,
                                      pamt=7, e3d=160, e1s=95, ds_on=1, ds_type=2, ds_drive=18, cp_on=1, cp_depth=60))
    add("Grogne à la croche", BASS, mix(GROWL, mx1_src=6, mx1_dst="a_pos", mx1_amt=50, mx2_src=6, mx2_dst="f1cut", mx2_amt=25))
    add("Grogne parlante", BASS, mix(GROWL, f1type=12, f1cut=900, f1shape=20, f1res=50, mx1_src=6, mx1_dst="f1shape", mx1_amt=70))
    add("Wobble 1/8", BASS, mix(MONO, f1type=1, f1cut=300, f1res=40, f1env=0, l1sync=8, l1shape=0, l1mode=1, mx1_src=6, mx1_dst="f1cut", mx1_amt=55,
                                 ds_on=1, ds_type=0, ds_drive=14, cp_on=1, cp_depth=50))
    add("Wobble triolet", BASS, mix(MONO, f1type=7, f1cut=400, f1res=45, f1env=0, l1sync=13, l1shape=1, l1mode=1, mx1_src=6, mx1_dst="f1cut", mx1_amt=50,
                                     ds_on=1, ds_type=6, ds_drive=12, cp_on=1, cp_depth=55))
    add("Neuro découpée", BASS, mix(GROWL, a_tab=9, a_pos=30, a_w1=8, a_w1amt=40, b_on=1, b_tab=0, b_pos=SIN, b_lvl=0, route=1, f2on=1, f2type=10, f2cut=600,
                                    f2res=55, l1sync=9, mx1_src=6, mx1_dst="a_pos", mx1_amt=60, mx2_src=2, mx2_dst="a_w1amt", mx2_amt=35, ds_type=5, ds_drive=14))
    add("808 qui glisse", BASS, mix(MONO, vmode=2, glide=80, a_tab=14, a_pos=0, sub_on=0, f1on=0, pamt=12, e3d=70, e1d=1800, e1s=0, e1r=600,
                                     ds_on=1, ds_type=1, ds_drive=10))
    add("808 saturée", BASS, mix(MONO, a_tab=11, a_pos=55, sub=40, f1on=0, pamt=10, e3d=60, e1d=1400, e1s=0, e1r=500, ds_on=1, ds_type=0, ds_drive=20))
    add("Basse acide", ACID, mix(MONO, vmode=2, glide=60, sub_on=0, f1type=13, f1cut=330, f1res=75, f1env=70, e2d=170, e2s=0, e1d=300, e1s=70,
                                  ds_on=1, ds_type=2, ds_drive=10, ds_mix=60))
    add("Acide hurlante", ACID, mix(MONO, vmode=2, glide=60, sub_on=0, f1type=13, f1cut=330, f1res=85, f1env=75, e2d=170, e2s=0, e1d=300, e1s=70,
                                     ds_on=1, ds_type=0, ds_drive=22, dl_on=1, dl_div=9, dl_mix=15))
    add("Basse FM métal", BASS, mix(MONO, a_pos=SIN, a_w1=8, a_w1amt=50, b_on=1, b_tab=0, b_pos=SIN, b_semi=7, b_lvl=0, f1cut=1500, f1env=25,
                                     mx1_src=2, mx1_dst="a_w1amt", mx1_amt=35))
    add("Basse spectrale", BASS, mix(MONO, a_mode=2, a_ssrc=0, a_tab=8, a_pos=40, a_sparts=24, a_sodd=40, f1cut=900))

    # --- Leads
    add("Lead supersaw", LEAD, mix(LEADB, a_uni=7, a_udet=32, hy_on=1, hy_mix=35, dl_on=1, dl_div=12, dl_mix=18, rv_on=1, rv_mix=15))
    add("Lead euphorique", LEAD, mix(LEADB, vmode=0, voices=8, a_uni=7, a_udet=30, b_oct=1, b_lvl=40, f1cut=6000, ch_on=1, ch_mix=30,
                                      dl_on=1, dl_div=10, dl_mix=20, rv_on=1, rv_size=75, rv_mix=25))
    add("Screech hardstyle", LEAD, mix(LEADB, a_tab=9, a_pos=60, a_w1=1, a_w1amt=55, a_uni=3, b_on=0, f1type=5, f1cut=1800, f1res=45,
                                        l1rate=6, l1mode=0, mx2_src=6, mx2_dst="a_w1amt", mx2_amt=20, ds_on=1, ds_type=2, ds_drive=22, cp_on=1, cp_depth=80,
                                        ph_on=1, ph_mix=25))
    add("Lead synchro balayée", LEAD, mix(LEADB, a_tab=9, a_pos=20, a_uni=1, b_on=0, mx2_src=2, mx2_dst="a_pos", mx2_amt=55))
    add("Lead voyelle", LEAD, mix(LEADB, a_tab=15, a_pos=30, a_uni=3, a_lvl=95, b_on=0, f1cut=5000, l1rate=0.6, l1mode=0, mx2_src=6, mx2_dst="a_pos", mx2_amt=40))
    add("Lead carré creux", LEAD, mix(LEADB, a_pos=SQR, b_pos=SQR, f1type=2, f1cut=1900))
    add("Lead FM perçant", LEAD, mix(LEADB, a_pos=SIN, a_uni=1, a_w1=8, a_w1amt=45, b_pos=SIN, b_semi=7, b_lvl=0, f1cut=5000))
    add("Lead hoover aigu", LEAD, mix(LEADB, a_tab=13, a_pos=50, a_uni=7, a_udet=40, b_on=0, pamt=3, e3d=180, ch_on=1, ch_mix=30))
    add("Lead à la pression", LEAD, mix(LEADB, f1cut=1200, mx2_src=14, mx2_dst="f1cut", mx2_amt=40))
    add("Lead granuleux", LEAD, mix(LEADB, a_mode=1, a_gsrc=4, a_gsize=60, a_gdens=60, a_gspray=5, a_gscan=0, b_on=0, f1cut=5000))
    add("Lead spectral étiré", LEAD, mix(LEADB, a_mode=2, a_ssrc=0, a_tab=0, a_pos=50, a_sstretch=18, a_sparts=48, b_on=0, f1cut=6000,
                                          dl_on=1, dl_div=9, dl_mix=20))
    add("Lead distordu au repli", LEAD, mix(LEADB, a_w1=12, a_w1amt=35, ds_on=1, ds_type=5, ds_drive=16, ds_mix=60))

    # --- Nappes (tables, granulaire, spectral)
    add("Nappe large", PAD, mix(PADB))
    add("Nappe qui respire", PAD, mix(PADB, l2rate=0.08, l2shape=8, l2mode=0, mx2_src=7, mx2_dst="f1cut", mx2_amt=20))
    add("Nappe de verre", PAD, mix(PADB, a_tab=2, a_pos=40, b_tab=1, b_pos=50, f1cut=5000, dl_on=1, dl_div=12, dl_mix=20))
    add("Nappe de grains (chœur)", PAD, mix(GRAN))
    add("Nappe de grains (cordes)", PAD, mix(GRAN, a_gsrc=5, a_gsize=180, a_gdens=30, a_gspray=40, a_grev=20))
    add("Nappe de grains (souffle)", PAD, mix(GRAN, a_gsrc=2, a_gsize=200, a_gdens=25, f1type=5, f1cut=1500, f1res=30,
                                                l1rate=0.1, l1mode=0, mx1_src=6, mx1_dst="f1cut", mx1_amt=25))
    add("Nappe spectrale (chœur)", PAD, mix(SPEC, a_sblur=60, a_sscan=6))
    add("Nappe spectrale floue", PAD, mix(SPEC, a_ssrc=6, a_sblur=85, a_sscan=3, a_stilt=-3))
    add("Nappe paire", PAD, mix(SPEC, a_ssrc=0, a_tab=0, a_pos=SAW, a_sodd=100, a_sparts=48))
    add("Nappe voix", PAD, mix(PADB, a_tab=15, a_pos=50, b_on=0, l1rate=0.07, mx1_amt=45))

    # --- Plucks
    add("Pluck à dents", PLUCK, mix(PLK))
    add("Pluck trance", PLUCK, mix(PLK, a_uni=5, a_udet=24, f1cut=500, rv_on=1, rv_mix=20))
    add("Pluck carré", PLUCK, mix(PLK, a_pos=SQR, f1cut=450))
    add("Pluck FM", PLUCK, mix(PLK, a_pos=SIN, a_w1=8, a_w1amt=40, b_on=1, b_pos=SIN, b_semi=12, b_lvl=0, mx1_src=1, mx1_dst="a_w1amt", mx1_amt=40))
    add("Pluck spectral", PLUCK, mix(PLK, a_mode=2, a_ssrc=0, a_sparts=32, a_sstretch=10))
    add("Pluck de cloches", PLUCK, mix(PLK, a_tab=2, a_pos=60, f1cut=1500, e1d=600))
    add("Pluck en grains", PLUCK, mix(PLK, a_mode=1, a_gsrc=3, a_gsize=40, a_gdens=80, a_gspray=10, f1cut=900))
    add("Pluck pompé", PLUCK, mix(PLK, cp_on=1, cp_depth=60))

    # --- Claviers, cordes, cuivres, arpèges
    add("Clavier doux", KEYS, mix(KEY))
    add("Clavier de cloches", KEYS, mix(KEY, a_tab=2, a_pos=30, e1d=2200, e1s=0, rv_on=1, rv_mix=25))
    add("Piano électrique FM", KEYS, mix(KEY, a_tab=0, a_pos=SIN, a_w1=8, a_w1amt=25, b_on=1, b_pos=SIN, b_semi=12, b_lvl=0,
                                         mx1_src=11, mx1_dst="a_w1amt", mx1_amt=30))
    add("Orgue harmonique", KEYS, mix(KEY, a_tab=4, a_pos=70, a_lvl=95, f1cut=6000, e1a=2, e1d=1, e1s=100, e1r=120, ch_on=1, ch_mix=40))
    add("Cordes", STR, mix(STRB))
    add("Cordes en grains", STR, mix(STRB, a_mode=1, a_gsrc=5, a_gsize=150, a_gdens=35, b_on=0))
    add("Cordes spectrales", STR, mix(STRB, a_mode=2, a_ssrc=6, a_sblur=40, b_on=0))
    add("Cuivres", BRASS, mix(BRS))
    add("Cuivres larges", BRASS, mix(BRS, a_uni=5, a_udet=18, ch_on=1, ch_mix=25))
    add("Cuivres sales", BRASS, mix(BRS, ds_on=1, ds_type=0, ds_drive=14, ds_mix=50))
    add("Arpège carré", ARP, mix(ARPB))
    add("Arpège scie", ARP, mix(ARPB, a_pos=SAW, a_uni=3, a_udet=15))
    add("Arpège spectral", ARP, mix(ARPB, a_mode=2, a_ssrc=0, a_sparts=24))
    add("Arpège granuleux", ARP, mix(ARPB, a_mode=1, a_gsrc=3, a_gsize=50, a_gdens=60))

    # --- Effets
    add("Montée de bruit", FX, dict(vmode=0, voices=4, a_on=0, noise_on=1, noise=80, ncolor=100, ndest=0, f1type=3, f1cut=200, f1res=30,
                                    e1a=50, e1d=1, e1s=100, e1r=800, l3shape=6, l3sync=3, l3mode=2, mx1_src=8, mx1_dst="f1cut", mx1_amt=70,
                                    rv_on=1, rv_mix=35))
    add("Chute grave", FX, mix(MONO, a_tab=14, a_pos=0, sub_on=0, f1on=0, pamt=-36, e3a=0, e3d=1500, e3s=0, e3cd=20, e1d=1800, e1s=0, e1r=500))
    add("Riser en grains", FX, mix(GRAN, a_gsrc=1, a_gdens=60, pamt=24, e3a=3000, e3ca=60, e3s=100, e1a=200))
    add("Impact gabber", FX, mix(KICK, e1d=1500, e1s=0, rv_on=1, rv_ord=4, rv_size=85, rv_mix=30))
    add("Souffle qui balaie", FX, mix(GRAN, a_gsrc=2, a_gsize=250, a_gdens=20, f1type=5, f1cut=800, f1res=50, l1rate=0.2, l1mode=0,
                                       mx1_src=6, mx1_dst="f1cut", mx1_amt=40))
    add("Cloches étirées", FX, mix(SPEC, a_ssrc=4, a_sstretch=40, a_sblur=30, dl_on=1, dl_div=12, dl_mix=25))
    add("Spectre décalé", FX, mix(SPEC, a_ssrc=0, a_tab=0, a_pos=SAW, a_sshift=120, a_sparts=48))
    add("Grondement", FX, mix(GRAN, a_gsrc=6, a_gsize=300, a_gdens=15, f1type=1, f1cut=600))
    add("Laser", FX, mix(MONO, vmode=0, voices=8, sub_on=0, a_pos=SAW, pamt=48, e3a=0, e3d=300, e3s=0, f1on=0, e1d=400, e1s=0,
                          dl_on=1, dl_div=7, dl_feed=50, dl_mix=30))
    add("Sirène hardcore", FX, mix(LEADB, a_tab=13, a_pos=60, l1rate=1.5, l1mode=0, l1shape=1, mx2_src=6, mx2_dst="tune", mx2_amt=12,
                                    ds_on=1, ds_type=2, ds_drive=16, rv_on=1, rv_mix=20))
    add("Voix granulaire", FX, mix(GRAN, a_gsrc=4, a_gsize=90, a_gdens=35, a_gpitch=0.3))
    return b
