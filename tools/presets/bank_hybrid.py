# Poly hybride (3 oscillateurs numériques purs, formes ou tables, Shape, FM, filtre analogique, distorsion) : 50 sons.
# Formes : 0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion (continues).
# fmode : 0 passe-bas 24, 1 passe-bas 12, 2 passe-bande, 3 passe-haut 12, 4 passe-haut 24.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

BASE = dict(o1shape=2, o2shape=2, o2fine=8, mix1=70, mix2=60, mix3=0, cutoff=2500, reso=15, envamt=35, predrive=2,
            aa=2, ad=800, as_=80, ar=500, m1a=2, m1d=500, m1s=30, m1r=400)
BASSE = mix(BASE, vmode=2, glide=30, o2semi=-12, o2fine=4, mix2=50, cutoff=600, reso=20, envamt=50, m1d=250, m1s=10, as_=85, ar=150, predrive=6)
LEAD_ = mix(BASE, vmode=2, glide=45, cutoff=3000, reso=25, envamt=30, m1d=600, m1s=60, as_=100, ar=300, l1rate=5.5, l1pitch=0, predrive=8)
NAPPE = mix(BASE, o1shape=1.8, o2shape=2.2, o2fine=10, cutoff=1600, reso=12, envamt=25, aa=1000, ad=2000, as_=90, ar=2600,
            m1a=1500, m1d=3000, m1s=60, m1r=2500, spread=70, l2rate=0.2, l2cutoff=15)
PLK = mix(BASE, cutoff=900, reso=30, envamt=60, aa=1, ad=450, as_=0, ar=350, m1a=1, m1d=220, m1s=0, m1r=250)
CLAV = mix(BASE, cutoff=2200, reso=10, envamt=30, aa=1, ad=1500, as_=35, ar=450, m1a=1, m1d=900, m1s=20, m1r=400, velfilter=45, velamp=50)


def build():
    b = Bank("hybrid")
    b.add("Init", INIT, dict(o1shape=2, mix1=70, mix2=0, mix3=0, predrive=0, drift=0, spread=0, cutoff=9000, reso=0, envamt=0, velfilter=0, keytrack=0, aa=2, ad=800, as_=100, ar=300))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (7)
    add("Basse numérique ronde", BASS, mix(BASSE))
    add("Basse FM grave", BASS, mix(BASSE, o1shape=0, o3shape=0, o3semi=12, fm31=55, mix2=0, cutoff=1500, m1d=200, m1s=0))
    add("Basse densité", BASS, mix(BASSE, o1smod=1, o1samt=60, o2smod=1, o2samt=40, cutoff=700))
    add("Basse synchro dure", BASS, mix(BASSE, o1samt=55, l1shapes=25, l1rate=0.4, cutoff=1200, mix2=30))
    add("Basse distordue", BASS, mix(BASSE, postdist=55, predrive=14, cutoff=500, reso=30))
    add("Basse table", BASS, mix(BASSE, o1tbl=1, o1shape=3.4, o2tbl=1, o2shape=3.6, o2semi=0, mix1=90, mix2=70, cutoff=1500, predrive=10))
    add("Sub numérique", BASS, mix(BASSE, o1shape=0, mix2=0, mix3=60, o3shape=1, o3semi=-12, cutoff=800, envamt=10, predrive=3))

    # --- Leads (7)
    add("Lead numérique", LEAD, mix(LEAD_))
    add("Lead mono 2 voix", LEAD, mix(LEAD_, vmode=3, unidetune=14, cutoff=3500))
    add("Lead FM perçant", LEAD, mix(LEAD_, o1shape=0, o3shape=0, o3semi=19, fm31=40, mix2=30, cutoff=5000))
    add("Lead lacéré", LEAD, mix(LEAD_, o1shape=3, o2shape=2.5, predrive=18, postdist=40, reso=35))
    add("Lead synchro balayée", LEAD, mix(LEAD_, o1samt=50, l1shapes=35, l1rate=0.3, mix2=40, cutoff=4500))
    add("Lead carré doux", LEAD, mix(LEAD_, o1shape=3, o2shape=3, fmode=1, cutoff=2000, reso=10, l1pitch=10))
    add("Lead anneau", LEAD, mix(LEAD_, mixring=45, o2semi=7, cutoff=3500))

    # --- Nappes (7)
    add("Nappe numérique", PAD, mix(NAPPE))
    add("Nappe table mouvante", PAD, mix(NAPPE, o1tbl=1, o2tbl=1, o1shape=1.5, o2shape=3, l1shapes=30, l1rate=0.25))
    add("Nappe densité", PAD, mix(NAPPE, o1smod=1, o1samt=70, o2smod=1, o2samt=70, cutoff=2000))
    add("Nappe passe-bande", PAD, mix(NAPPE, fmode=2, cutoff=1200, reso=35, l2cutoff=30))
    add("Nappe trois oscillateurs", PAD, mix(NAPPE, mix3=50, o3shape=2, o3semi=7, o3fine=-6))
    add("Nappe chaude saturée", PAD, mix(NAPPE, predrive=12, cutoff=1100))
    add("Nappe sombre", PAD, mix(NAPPE, cutoff=700, reso=25, envamt=35))

    # --- Plucks (6)
    add("Pluck numérique brillant", PLUCK, mix(PLK, cutoff=1500))
    add("Pluck FM", PLUCK, mix(PLK, o1shape=0, o3shape=0, o3semi=24, fm31=50, mix2=0, m2d=180, cutoff=3000))
    add("Pluck anneau", PLUCK, mix(PLK, o1shape=4, mixring=40))
    add("Pluck table", PLUCK, mix(PLK, o1tbl=1, o2tbl=1, o1shape=2.8, o2shape=1.2, mix1=90, mix2=80, cutoff=2000, envamt=45, predrive=8))
    add("Pluck synchro", PLUCK, mix(PLK, o1samt=60, m2shape=50, m2d=200, mix2=20))
    add("Pluck passe-haut", PLUCK, mix(PLK, fmode=3, cutoff=400, reso=40, envamt=50))

    # --- Claviers (6)
    add("Piano numérique", KEYS, mix(CLAV))
    add("Clavier FM", KEYS, mix(CLAV, o1shape=0, o2shape=0, o2semi=12, o3shape=0, o3semi=14, fm12=30, fm23=20, cutoff=5000, as_=10))
    add("Orgue numérique", KEYS, mix(CLAV, o1shape=0, o2shape=0, o2semi=12, mix3=40, o3shape=0, o3semi=19, envamt=0, cutoff=8000, aa=3, ad=100, as_=100, ar=80))
    add("Cloches cristal", KEYS, mix(CLAV, o1shape=0, o3shape=0, o3semi=24, fm31=35, mix2=0, as_=0, ad=2200, ar=2200, cutoff=6000))
    add("Clavier saturé", KEYS, mix(CLAV, predrive=14, postdist=20, cutoff=1500))
    add("Accords stab", KEYS, mix(CLAV, cutoff=1200, reso=30, envamt=55, m1d=250, m1s=0, ad=350, as_=0, ar=300))

    # --- Cordes (3)
    CORD = mix(NAPPE, aa=400, ar=1300, m1a=300, cutoff=3500, reso=5, l1pitch=6, l1rate=5)
    add("Cordes numériques", STR, mix(CORD))
    add("Cordes table", STR, mix(CORD, o1tbl=1, o1shape=2.5))
    add("Cordes pincées", STR, mix(CORD, aa=1, ad=400, as_=0, ar=400, envamt=40, m1a=1, m1d=300, m1s=0))

    # --- Cuivres (3)
    CUIV = mix(BASE, cutoff=1000, envamt=55, m1a=50, m1d=600, m1s=50, aa=25, ad=700, as_=85, ar=300, o2fine=10)
    add("Cuivres numériques", BRASS, mix(CUIV))
    add("Cuivres saturés", BRASS, mix(CUIV, predrive=12, cutoff=1300))
    add("Stab cuivré", BRASS, mix(CUIV, m1d=250, m1s=0, ad=300, as_=0, ar=200))

    # --- Arpèges (5)
    ARPP = mix(PLK, arp_gate=50, arp_rate=3)
    add("Arpège numérique", ARP, mix(ARPP, arp_mode=1, arp_oct=2))
    add("Arpège FM", ARP, mix(ARPP, o1shape=0, o3shape=0, o3semi=24, fm31=40, mix2=0, arp_mode=3, arp_oct=2))
    add("Arpège convergent", ARP, mix(ARPP, arp_mode=10, arp_oct=2, cutoff=1300))
    add("Arpège pédale aiguë", ARP, mix(ARPP, arp_mode=14, arp_oct=2, o1tbl=1, o1shape=2.5, mix1=90, cutoff=2000, envamt=45, predrive=8))
    add("Arpège marche aléatoire", ARP, mix(ARPP, arp_mode=19, arp_oct=3, arp_rate=4))

    # --- Effets (6)
    add("Balayage de tables", FX, mix(NAPPE, o1tbl=1, o2tbl=1, l1shapes=90, l1rate=0.1, l1shape=1))
    add("Bruit filtré", FX, mix(NAPPE, mix1=0, mix2=0, mixnoise=100, fmode=2, cutoff=1500, reso=60, l2cutoff=60, l2rate=0.3))
    add("Robot FM", FX, mix(LEAD_, o1shape=0, o3shape=3, o3semi=7, fm31=90, mix2=0, l1pitch=0, cutoff=4000))
    add("Chute de hauteur", FX, mix(PLK, m2pitch=24, m2d=600, ad=1200, as_=30, ar=600, cutoff=5000, envamt=20, m1s=40))
    add("Pulsation", FX, mix(NAPPE, l2amp=80, l2rate=6, l2shape=3, aa=5))
    add("Échantillonné aléatoire", FX, mix(LEAD_, l2shape=4, l2rate=8, l2cutoff=70, reso=55, cutoff=1500))
    return b
