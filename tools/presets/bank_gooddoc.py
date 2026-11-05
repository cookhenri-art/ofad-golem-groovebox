# Good Doctor (v0.47 (maj. 5 nov.), principe des grands synthés à tables d'ondes hybrides) : 90 sons, surtout des basses.
# Tables : 0 analogique (pos 0 sinus, 25 triangle, 50 scie, 75 carré, 100 impulsion), 1 voyelles, 2 cloches FM,
# 3 plis, 4 harmoniques paires, 5 granuleux, 6 cordes frottées, 7 numérique 8 bits, 8 scie qui gronde,
# 9 synchro balayée, 10 carré qui se pince, 11 sinus sali.
# Déformations : 1 synchro, 2 courbure +, 3 courbure −, 4 pulsation, 5 miroir, 6 asymétrie, 7 quantifier,
# 8 FM (osc. suivant), 9 FM (sous-osc.), 10 AM, 11 anneau, 12 repli, 13 saturer, 14 redresser, 15 bits,
# 16 passe-bas, 17 passe-haut. Sorties : 0 filtres, 1 filtre 1, 2 filtre 2, 3 direct.
# Filtres : 0 multi, 1 PB 24, 2 PB 12, 3 PH 24, 4 PH 12, 5 bande, 6 réjection, 7 agressif PB, 8 agressif PH,
# 9 doux PB, 10 peigne +, 11 peigne −, 12 formants, 13 diode. Distorsion : 1 douce, 2 dure, 3 lampe, 4 repli,
# 5 redresseur, 6 bits, 7 échantillonnage.
# Sources de la matrice : 1 env. 1, 2 env. 2, 3 env. 3, 6 LFO 1, 7 LFO 2, 11 vélocité, 13 molette, 14 pression, 18.. macros.
from banks_common import Bank, mix, ACID, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

SAW, SQR, TRI, SIN, PULSE = 50, 75, 25, 0, 100

# archétypes
MONO_BASS = dict(vmode=1, voices=1, glide=0, a_rand=0, a_tab=0, a_pos=SAW, a_lvl=80, sub=55, subshape=0, suboct=-1, subdest=3,
                 f1type=1, f1cut=420, f1res=18, f1env=45, f1key=30, e2a=1, e2d=260, e2s=20, e2r=150,
                 aa=1, ad=600, as_=85, ar=90, spread=0)
REESE = mix(MONO_BASS, b_on=1, b_tab=0, b_pos=SAW, b_lvl=80, b_fine=14, b_rand=0, a_fine=-14, sub=45, f1cut=900, f1res=10, f1env=15,
            dtype=1, ddrive=10, dmix=60)
GROWL = mix(MONO_BASS, a_tab=8, a_pos=40, f1type=7, f1cut=700, f1res=45, f1env=35, dtype=3, ddrive=14, dmix=70, sub=50,
            l1sync=8, l1shape=0)
LEADB = dict(vmode=1, voices=1, glide=45, a_tab=0, a_pos=SAW, a_lvl=75, b_on=1, b_tab=0, b_pos=SAW, b_fine=9, b_lvl=65, a_fine=-9,
             f1type=1, f1cut=2600, f1res=22, f1env=30, f1key=60, e2d=500, e2s=45, aa=2, ad=800, as_=90, ar=220, l2rate=5.3,
             mx1_src=13, mx1_dst="tune", mx1_amt=1)
PADB = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=65, b_on=1, b_tab=6, b_pos=40, b_lvl=55, b_fine=8, a_fine=-8,
            f1type=2, f1cut=2400, f1res=12, f1env=15, e2a=900, e2d=2500, e2s=40, e2r=1800,
            aa=700, ad=1500, as_=85, ar=2200, spread=80, l1rate=0.15, mx1_src=6, mx1_dst="a_pos", mx1_amt=18)
PLK = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=75, f1type=1, f1cut=380, f1res=30, f1env=70, f1key=50, e2a=0.5, e2d=180, e2s=0,
           e2r=180, aa=0.5, ad=320, as_=0, ar=260, spread=50)
KEY = dict(vmode=0, voices=8, a_tab=4, a_pos=30, a_lvl=70, f1type=2, f1cut=3000, f1res=8, f1env=20, e2d=700, e2s=30,
           aa=1, ad=1400, as_=35, ar=500, spread=40)
STRB = dict(vmode=0, voices=8, a_tab=6, a_pos=35, a_lvl=70, b_on=1, b_tab=6, b_pos=60, b_lvl=60, b_fine=10, a_fine=-10,
            f1type=2, f1cut=4200, f1res=8, f1env=10, aa=350, ad=900, as_=90, ar=1300, spread=85, l2delay=6,
            mx1_src=7, mx1_dst="tune", mx1_amt=1)
BRS = dict(vmode=0, voices=8, a_tab=0, a_pos=SAW, a_lvl=75, b_on=1, b_tab=4, b_pos=60, b_lvl=50, b_fine=6,
           f1type=1, f1cut=900, f1res=15, f1env=55, f1key=50, e2a=60, e2d=400, e2s=55, aa=40, ad=600, as_=85, ar=250, spread=50)
ARPB = dict(vmode=0, voices=8, a_tab=0, a_pos=SQR, a_lvl=75, f1type=1, f1cut=600, f1res=35, f1env=60, e2d=200, e2s=10,
            aa=0.5, ad=260, as_=20, ar=200, arp_mode=1, arp_rate=3, spread=40)


# LFO 3 à pas : un motif de 16 pas
STEPS = {f"lf3_s{i + 1}": v for i, v in enumerate([100, -40, 60, -100, 80, 0, -60, 40, 100, -20, 50, -80, 70, -40, 20, -100])}


def build():
    b = Bank("gooddoc")

    def add(name, cat, values):
        # destination de matrice écrite par son réglage : mxN_dst="f1cut" -> « mxN_dst@f1cut »: 0
        out = {}
        for k, v in values.items():
            if k.endswith("_dst") and isinstance(v, str):
                out[f"{k}@{v}"] = 0
            else:
                out[k] = v
        b.add(name, cat, out)

    # --- Basses (26)
    add("Sub qui frappe", BASS, mix(MONO_BASS, a_tab=11, a_pos=15, sub=70, f1cut=300, f1env=20, pamt=12, e3d=40))
    add("Basse scie franche", BASS, mix(MONO_BASS))
    add("Basse carrée ronde", BASS, mix(MONO_BASS, a_pos=SQR, f1cut=350, f1res=10))
    add("Reese large", BASS, mix(REESE, vmode=2, univoices=3, unidetune=12, spread=60))
    add("Reese qui tourne", BASS, mix(REESE, mx2_src=6, mx2_dst="f1cut", mx2_amt=18, l1rate=0.25, route=0, f2on=1, f2type=6, f2cut=800, f2res=40,
                                     mx3_src=7, mx3_dst="f2cut", mx3_amt=25, l2rate=0.4))
    add("Grogne à la noire", BASS, mix(GROWL, l1sync=7, mx1_src=6, mx1_dst="a_pos", mx1_amt=45, mx2_src=6, mx2_dst="f1cut", mx2_amt=30))
    add("Grogne à la croche", BASS, mix(GROWL, mx1_src=6, mx1_dst="a_pos", mx1_amt=50, mx2_src=6, mx2_dst="f1cut", mx2_amt=25))
    add("Grogne parlante", BASS, mix(GROWL, f1type=12, f1cut=900, f1shape=20, f1res=50, mx1_src=6, mx1_dst="f1shape", mx1_amt=70, l1sync=8))
    add("Basse FM du dessous", BASS, mix(MONO_BASS, a_pos=SIN, a_w1=9, a_w1amt=45, sub=60, subshape=0, f1cut=900, f1env=30,
                                         mx1_src=2, mx1_dst="a_w1amt", mx1_amt=35))
    add("Basse synchro", BASS, mix(MONO_BASS, a_w1=1, a_w1amt=35, f1cut=1100, mx1_src=2, mx1_dst="a_w1amt", mx1_amt=40))
    add("Basse repliée", BASS, mix(MONO_BASS, a_tab=0, a_pos=SIN, a_w1=12, a_w1amt=40, f1cut=1400, f1env=20, mx1_src=2, mx1_dst="a_w1amt", mx1_amt=45))
    add("Basse asymétrique", BASS, mix(MONO_BASS, a_pos=SIN, a_w1=6, a_w1amt=70, f1cut=1000, f1env=25))
    add("808 longue", BASS, mix(MONO_BASS, a_tab=0, a_pos=SIN, sub=0, f1on=0, pamt=14, e3d=70, ad=1800, as_=0, ar=600, dtype=1, ddrive=8, dmix=50))
    add("808 saturée", BASS, mix(MONO_BASS, a_tab=11, a_pos=55, sub=40, f1on=0, pamt=10, e3d=60, ad=1400, as_=0, ar=500, dtype=3, ddrive=16))
    add("Basse pincée", BASS, mix(MONO_BASS, f1cut=260, f1res=35, f1env=75, e2d=150, e2s=0, ad=500, as_=40))
    add("Basse diode", BASS, mix(MONO_BASS, f1type=13, f1cut=380, f1res=45, f1env=60, e2d=180, e2s=5))
    add("Basse peigne", BASS, mix(MONO_BASS, route=0, f2on=1, f2type=10, f2cut=180, f2res=60, f2key=100, f2mix=60, f1cut=900))
    add("Basse bits", BASS, mix(MONO_BASS, a_tab=7, a_pos=40, a_w1=15, a_w1amt=40, f1cut=1200, dtype=6, ddrive=10, dmix=40))
    add("Neuro découpée", BASS, mix(GROWL, b_on=1, b_tab=9, b_pos=30, b_lvl=55, b_oct=0, route=1, f2on=1, f2type=5, f2cut=1200, f2res=50,
                                   mx1_src=6, mx1_dst="b_pos", mx1_amt=60, mx2_src=6, mx2_dst="f2cut", mx2_amt=40, l1sync=9))
    add("Basse carrée qui pince", BASS, mix(MONO_BASS, a_tab=10, a_pos=10, mx1_src=2, mx1_dst="a_pos", mx1_amt=55, f1cut=700))
    add("Basse douce", BASS, mix(MONO_BASS, a_pos=TRI, f1type=9, f1cut=600, f1env=10, sub=60, ar=180))
    add("Basse unisson", BASS, mix(MONO_BASS, vmode=2, univoices=5, unidetune=16, spread=55, f1cut=650, sub=45))
    add("Basse glissée", BASS, mix(MONO_BASS, glide=110, f1cut=520, f1res=25))
    add("Basse octaviée", BASS, mix(MONO_BASS, b_on=1, b_tab=0, b_pos=SQR, b_oct=1, b_lvl=45, b_rand=0, f1cut=700))
    add("Basse lampe", BASS, mix(MONO_BASS, dtype=3, ddrive=18, dmix=80, f1cut=600, f1res=22))
    add("Basse sale au mod 1", BASS, mix(MONO_BASS, a_tab=8, a_pos=20, macro1=40, dtype=2, ddrive=10, dmix=60,
                                        mx1_src=18, mx1_dst="a_pos", mx1_amt=70, mx2_src=18, mx2_dst="f1cut", mx2_amt=35, mx3_src=18, mx3_dst="ddrive", mx3_amt=40))

    # --- Acide (4)
    ACIDB = mix(MONO_BASS, sub=0, f1type=13, f1cut=330, f1res=72, f1env=70, e2d=170, e2s=0, glide=60, ad=300, as_=70)
    add("Acide diode", ACID, mix(ACIDB))
    add("Acide carrée", ACID, mix(ACIDB, a_pos=SQR, f1res=78))
    add("Acide hurlante", ACID, mix(ACIDB, dtype=1, ddrive=20, dmix=80, f1res=82))
    add("Acide au peigne", ACID, mix(ACIDB, route=0, f2on=1, f2type=11, f2cut=300, f2res=55, f2key=100, f2mix=45))

    # --- Leads (12)
    add("Lead scies", LEAD, mix(LEADB))
    add("Lead synchro balayée", LEAD, mix(LEADB, b_on=0, a_tab=9, a_pos=20, mx2_src=2, mx2_dst="a_pos", mx2_amt=55, e2d=700))
    add("Lead supersynchro", LEAD, mix(LEADB, vmode=2, univoices=7, unidetune=22, spread=80, a_w1=1, a_w1amt=30, b_on=0))
    add("Lead carré creux", LEAD, mix(LEADB, a_pos=SQR, b_pos=SQR, f1type=2, f1cut=1900))
    add("Lead voyelle", LEAD, mix(LEADB, a_tab=1, a_pos=30, b_on=0, mx2_src=6, mx2_dst="a_pos", mx2_amt=40, l1rate=0.6))
    add("Lead FM métal", LEAD, mix(LEADB, a_pos=SIN, a_w1=8, a_w1amt=45, b_tab=0, b_pos=SIN, b_semi=7, b_lvl=0, f1cut=4000))
    add("Lead sifflé", LEAD, mix(LEADB, a_pos=SIN, b_on=0, f1type=5, f1cut=1500, f1res=60, noise=12, aa=25))
    add("Lead à la pression", LEAD, mix(LEADB, mx2_src=14, mx2_dst="f1cut", mx2_amt=40, f1cut=1200))
    add("Lead repli", LEAD, mix(LEADB, a_pos=SIN, a_w1=12, a_w1amt=50, b_on=0, mx2_src=2, mx2_dst="a_w1amt", mx2_amt=30))
    add("Lead hurleur", LEAD, mix(LEADB, f1type=7, f1res=55, dtype=1, ddrive=14, dmix=70))
    add("Lead quinte", LEAD, mix(LEADB, b_semi=7, b_fine=3, f1cut=2200))
    add("Lead numérique", LEAD, mix(LEADB, a_tab=7, a_pos=50, b_tab=7, b_pos=80, a_w2=15, a_w2amt=30))

    # --- Nappes (12)
    add("Nappe chaude", PAD, mix(PADB))
    add("Nappe voyelles", PAD, mix(PADB, a_tab=1, a_pos=20, b_tab=1, b_pos=70, mx1_amt=40, l1rate=0.08))
    add("Nappe granuleuse", PAD, mix(PADB, a_tab=5, a_pos=30, b_tab=5, b_pos=70, f1cut=3500, noise=8, ncolor=40))
    add("Nappe verre", PAD, mix(PADB, a_tab=2, a_pos=20, b_tab=4, b_pos=50, f1type=0, f1shape=30, f1cut=3000))
    add("Nappe unisson", PAD, mix(PADB, vmode=2, univoices=7, unidetune=24, b_on=0, spread=100))
    add("Nappe peigne", PAD, mix(PADB, route=0, f2on=1, f2type=10, f2cut=400, f2res=55, f2key=100, f2mix=40))
    add("Nappe lente qui s'ouvre", PAD, mix(PADB, aa=2500, e2a=4000, f1cut=600, f1env=60, ar=3000))
    add("Nappe à pas", PAD, mix(PADB, STEPS, mx2_src=8, mx2_dst="f1cut", mx2_amt=30, lf3_sync=8, lf3_shape=7))
    add("Nappe plis", PAD, mix(PADB, a_tab=3, a_pos=10, mx1_amt=40, b_tab=6))
    add("Nappe chœur", PAD, mix(PADB, a_tab=1, a_pos=50, b_tab=1, b_pos=30, f1type=12, f1cut=1000, f1shape=40, mx2_src=6, mx2_dst="f1shape", mx2_amt=30))
    add("Nappe sombre", PAD, mix(PADB, a_pos=TRI, b_tab=0, b_pos=SAW, f1cut=900, f1res=20))
    add("Nappe cordes frottées", PAD, mix(PADB, a_tab=6, a_pos=60, b_tab=6, b_pos=20, f1cut=3200))

    # --- Plucks (8)
    add("Pluck scie", PLUCK, mix(PLK))
    add("Pluck carré", PLUCK, mix(PLK, a_pos=SQR, f1cut=450))
    add("Pluck cloche", PLUCK, mix(PLK, a_tab=2, a_pos=40, f1cut=1500, f1env=40, ad=700, ar=600))
    add("Pluck peigne", PLUCK, mix(PLK, route=0, f1cut=2000, f2on=1, f2type=10, f2cut=262, f2res=75, f2key=100, f2mix=70))
    add("Pluck synchro", PLUCK, mix(PLK, a_w1=1, a_w1amt=50, mx1_src=2, mx1_dst="a_w1amt", mx1_amt=40))
    add("Pluck house", PLUCK, mix(PLK, b_on=1, b_tab=0, b_pos=SAW, b_semi=7, b_lvl=50, f1cut=600, f1env=55))
    add("Pluck numérique", PLUCK, mix(PLK, a_tab=7, a_pos=30, f1cut=900))
    add("Pluck voyelle", PLUCK, mix(PLK, a_tab=1, a_pos=60, f1type=12, f1cut=1000, f1shape=60, mx1_src=2, mx1_dst="f1shape", mx1_amt=-50))

    # --- Claviers (7)
    add("Orgue harmoniques", KEYS, mix(KEY, as_=100, ad=100, ar=120))
    add("Piano numérique", KEYS, mix(KEY, a_tab=2, a_pos=10, b_on=1, b_tab=0, b_pos=TRI, b_lvl=50, f1cut=2600))
    add("Cloches FM", KEYS, mix(KEY, a_tab=2, a_pos=60, as_=0, ad=1800, ar=1600, f1cut=6000))
    add("Clavier électrique", KEYS, mix(KEY, a_tab=0, a_pos=SIN, a_w1=8, a_w1amt=25, b_tab=0, b_pos=SIN, b_oct=1, b_lvl=0,
                                         mx1_src=11, mx1_dst="a_w1amt", mx1_amt=30))
    add("Clavier doux", KEYS, mix(KEY, a_tab=0, a_pos=TRI, f1type=9, f1cut=2000))
    add("Clavecin", KEYS, mix(KEY, a_tab=10, a_pos=60, a_lvl=100, b_on=1, b_tab=0, b_pos=SAW, b_lvl=60, b_oct=1, f1type=4, f1cut=180, f1env=0, ad=1400, as_=0, ar=500))
    add("Accords house", KEYS, mix(KEY, a_tab=0, a_pos=SAW, b_on=1, b_tab=0, b_pos=SQR, b_lvl=50, f1type=1, f1cut=900, f1env=35, ad=500, as_=20))

    # --- Cordes (5)
    add("Cordes frottées", STR, mix(STRB))
    add("Cordes larges", STR, mix(STRB, vmode=2, univoices=5, unidetune=15, b_on=0, a_tab=0, a_pos=SAW))
    add("Cordes pincées", STR, mix(STRB, aa=2, ad=800, as_=0, ar=600, f1env=40, f1cut=1500))
    add("Violoncelles", STR, mix(STRB, a_oct=-1, b_oct=-1, f1cut=2200, aa=200))
    add("Cordes de verre", STR, mix(STRB, b_tab=2, b_pos=30, f1cut=5000))

    # --- Cuivres (5)
    add("Cuivres scies", BRASS, mix(BRS))
    add("Cuivres doux", BRASS, mix(BRS, f1cut=700, f1env=40, aa=80))
    add("Cors", BRASS, mix(BRS, a_oct=-1, b_oct=-1, f1cut=600))
    add("Cuivres synchro", BRASS, mix(BRS, a_w1=1, a_w1amt=25, mx1_src=2, mx1_dst="a_w1amt", mx1_amt=25))
    add("Fanfare", BRASS, mix(BRS, vmode=2, univoices=4, unidetune=10, f1cut=1100))

    # --- Arpèges (6)
    add("Arpège carré", ARP, mix(ARPB))
    add("Arpège scie", ARP, mix(ARPB, a_pos=SAW, f1cut=800))
    add("Arpège cloche", ARP, mix(ARPB, a_tab=2, a_pos=40, f1cut=2500, f1env=30, arp_mode=2))
    add("Arpège synchro", ARP, mix(ARPB, a_w1=1, a_w1amt=40, mx1_src=6, mx1_dst="a_w1amt", mx1_amt=40, l1sync=5))
    add("Arpège montant", ARP, mix(ARPB, arp_oct=2, arp_rate=4))
    add("Arpège à pas", ARP, mix(ARPB, STEPS, a_tab=0, a_pos=SAW, arp_steps=8, arp_t3=7, arp_t5=12, arp_t7=5, mx1_src=8, mx1_dst="f1cut", mx1_amt=25, lf3_sync=9, lf3_shape=7))

    # --- Effets (5)
    add("Riser", FX, mix(PADB, noise=80, ncolor=60, a_lvl=70, b_on=0, f1type=2, f1cut=300, f1res=30, e2a=6000, e2s=100, f1env=90, aa=4000, pamt=12, e3a=6000, e3s=100))
    add("Impact", FX, mix(MONO_BASS, a_pos=SIN, sub=60, noise=50, ndest=3, pamt=36, e3d=120, ad=1200, as_=0, ar=900, f1on=0, dtype=1, ddrive=12))
    add("Laser", FX, mix(LEADB, b_on=0, pamt=48, e3d=300, a_w1=1, a_w1amt=60))
    add("Sirène", FX, mix(LEADB, b_on=0, mx2_src=6, mx2_dst="tune", mx2_amt=40, l1rate=0.6, l1shape=1))
    add("Bruit d'échantillonnage", FX, mix(PADB, a_tab=5, dtype=7, ddrive=30, dmix=100, b_on=0))
    return b
