# Nappes (4 oscillateurs en unisson, 2 filtres, chorus, LFO, matrice) : 50 sons.
# Formes : 0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion, 5 bruit, 6 impulsion étroite, 7 tri-scie.
# Filtres : 0 PB12, 1 PB24, 2 bande, 3 passe-haut, 4 contourné.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

BASE = dict(awave=2, aunison=5, adetune=30, bwave=2, boct=0, bunison=3, bdetune=20, blevel=50, facutoff=3500, fbcutoff=3500,
            aa=600, ad=1500, as_=85, ar=2500, ma=900, md=2500, ms=40, mr=2000, modcutoffa=25, modcutoffb=25, chorusmix=35)
NAPPE = mix(BASE)
SUPER = mix(BASE, aunison=8, adetune=40, bunison=8, bdetune=30, facutoff=9000, fbcutoff=9000, modcutoffa=10, modcutoffb=10, aa=20, ar=900, chorusmix=25)
PLK = mix(BASE, aunison=6, aa=1, ad=300, as_=0, ar=400, ma=1, md=250, ms=0, mr=300, modcutoffa=70, modcutoffb=70,
          facutoff=700, fbcutoff=700, chorusmix=20)
BASSE = mix(BASE, vmode=2, glide=20, awave=2, aunison=1, aoct=-1, bwave=3, boct=-1, bunison=1, blevel=60, facutoff=700, fbcutoff=700,
            fatype=1, fbtype=1, aa=1, ad=600, as_=85, ar=150, ma=1, md=280, ms=10, mr=200, modcutoffa=55, modcutoffb=55, chorusmix=0, width=40)
LEAD_ = mix(BASE, voices=1, vmode=2, glide=50, aunison=5, adetune=22, bwave=3, bunison=3, facutoff=5000, fbcutoff=5000, fareso=20,
            aa=2, ad=800, as_=100, ar=300, ma=1, md=600, ms=70, mr=400, modcutoffa=20, modcutoffb=20, chorusmix=20, l1rate=5.5)
CLAV = mix(BASE, aunison=2, adetune=12, bwave=1, bunison=1, aa=1, ad=1500, as_=35, ar=500, ma=1, md=900, ms=25, mr=500,
           facutoff=2500, fbcutoff=2500, modcutoffa=35, modcutoffb=35, velamp=50, chorusmix=30)


def build():
    b = Bank("pads")
    b.add("Init", INIT, dict(awave=0, aunison=1, adetune=0, alevel=80, blevel=0, chorusmix=0, width=100, facutoff=9000, fareso=0, fbcutoff=9000, modcutoffa=0, modcutoffb=0, l2cutoff=0, aa=2, ad=1500, as_=100, ar=300))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (6)
    add("Basse épaisse", BASS, mix(BASSE))
    add("Basse unisson", BASS, mix(BASSE, aunison=4, adetune=18, width=70))
    add("Basse sinus et scie", BASS, mix(BASSE, awave=0, bwave=2, boct=0, blevel=35, facutoff=1000))
    add("Basse à la phase", BASS, mix(BASSE, aretrig=1, bretrig=1, aphase=90, modcutoffa=65, md=200))
    add("Basse saturée", BASS, mix(BASSE, fadrive=12, fbdrive=12, warmdrive=1, facutoff=900, fareso=25))
    add("Basse quatre osc.", BASS, mix(BASSE, a2level=50, a2wave=4, a2oct=-1, b2level=40, b2wave=0, b2oct=-2))

    # --- Leads (7)
    add("Lead supersaw", LEAD, mix(LEAD_, aunison=8, adetune=35, bwave=2, bunison=8))
    add("Lead carré", LEAD, mix(LEAD_, awave=3, aunison=3, bwave=3, boct=1, blevel=30))
    add("Lead vibrato", LEAD, mix(LEAD_, l1pitch=15, l1rate=5.5))
    add("Lead tri-scie", LEAD, mix(LEAD_, awave=7, bwave=7, boct=1, facutoff=6000))
    add("Lead octave", LEAD, mix(LEAD_, boct=1, blevel=60, bwave=2))
    add("Lead résonant", LEAD, mix(LEAD_, facutoff=1800, fareso=55, fbcutoff=1800, fbreso=40, modcutoffa=40, md=500))
    add("Lead chaud", LEAD, mix(LEAD_, warmdrive=1, fadrive=10, facutoff=3500, aunison=4))

    # --- Nappes (10)
    add("Nappe classique", PAD, mix(NAPPE))
    add("Nappe supersaw", PAD, mix(SUPER, aa=900, ar=2500))
    add("Nappe chœur", PAD, mix(NAPPE, awave=1, bwave=0, aunison=4, adetune=15, facutoff=2200, l1pitch=8, chorusmix=60))
    add("Nappe sombre", PAD, mix(NAPPE, facutoff=900, fbcutoff=900, fareso=20, modcutoffa=35))
    add("Nappe très large", PAD, mix(NAPPE, width=200, aspread=100, bspread=100))
    add("Nappe filtre lent", PAD, mix(NAPPE, l2cutoff=45, l2rate=0.08, facutoff=1800, fareso=30))
    add("Nappe impulsions", PAD, mix(NAPPE, awave=4, bwave=6, aunison=6))
    add("Nappe aérienne", PAD, mix(NAPPE, fatype=3, facutoff=600, fbtype=2, fbcutoff=2500, aa=2000, ar=5000))
    add("Nappe quatre osc.", PAD, mix(NAPPE, a2level=55, a2semi=7, a2unison=4, b2level=40, b2wave=1, b2oct=1, benvown=1, ba=1600, br=3500))
    add("Nappe panoramique", PAD, mix(NAPPE, l1pan=60, l1rate=0.3, l1shape=1))

    # --- Plucks (7)
    add("Pluck festif", PLUCK, mix(PLK))
    add("Pluck large", PLUCK, mix(PLK, aunison=8, adetune=40, width=180))
    add("Pluck résonant", PLUCK, mix(PLK, fareso=55, fbreso=40, facutoff=450))
    add("Pluck carré", PLUCK, mix(PLK, awave=3, bwave=3, boct=1))
    add("Pluck doux", PLUCK, mix(PLK, awave=1, bwave=0, aunison=3, modcutoffa=45, ad=500, ar=600))
    add("Pluck hauteur", PLUCK, mix(PLK, modpitch=3))
    add("Pluck en quinte", PLUCK, mix(PLK, bsemi=7, blevel=55))

    # --- Claviers (4)
    add("Piano synthétique", KEYS, mix(CLAV))
    add("Clavier électrique", KEYS, mix(CLAV, awave=0, bwave=1, boct=1, blevel=30, facutoff=3000, as_=20))
    add("Orgue", KEYS, mix(CLAV, awave=0, bwave=0, boct=1, a2level=40, a2wave=0, a2oct=2, aa=3, ad=100, as_=100, ar=100, modcutoffa=0, modcutoffb=0, facutoff=8000, fbcutoff=8000))
    add("Accords house", KEYS, mix(CLAV, awave=2, bwave=2, facutoff=1200, fbcutoff=1200, fareso=30, modcutoffa=55, md=250, ms=0, ad=350, as_=0, ar=300))

    # --- Cordes (4)
    CORD = mix(NAPPE, aunison=6, adetune=25, aa=400, ar=1400, facutoff=5000, fbcutoff=5000, modcutoffa=10, l1pitch=6, l1rate=5)
    add("Cordes", STR, mix(CORD))
    add("Cordes lentes", STR, mix(CORD, aa=1400, ar=3000))
    add("Cordes sombres", STR, mix(CORD, facutoff=2000, fbcutoff=2000))
    add("Cordes pizzicato", STR, mix(CORD, aa=1, ad=350, as_=0, ar=350, ma=1, md=250, ms=0, modcutoffa=40))

    # --- Cuivres (3)
    CUIV = mix(BASE, aunison=3, adetune=15, bunison=2, facutoff=1200, fbcutoff=1200, ma=60, md=600, ms=50, modcutoffa=55, modcutoffb=55,
               aa=30, ad=700, as_=85, ar=300, chorusmix=20)
    add("Cuivres synthétiques", BRASS, mix(CUIV))
    add("Cuivres larges", BRASS, mix(CUIV, aunison=7, adetune=30, width=180))
    add("Stab cuivré", BRASS, mix(CUIV, md=250, ms=0, ad=300, as_=0, ar=200))

    # --- Arpèges (5)
    ARPP = mix(PLK, arp_gate=50, arp_rate=3)
    add("Arpège trance", ARP, mix(ARPP, arp_mode=1, arp_oct=2))
    add("Arpège haut-bas", ARP, mix(ARPP, arp_mode=3, arp_oct=3))
    add("Arpège à pas", ARP, mix(ARPP, arp_mode=1, arp_oct=1, arp_steps=8, arp_t2=12, arp_t4=7, arp_t6=12, arp_t8=5, arp_v3=60, arp_v7=60))
    add("Arpège convergent", ARP, mix(ARPP, arp_mode=10, arp_oct=2, awave=3))
    add("Arpège triolets", ARP, mix(ARPP, arp_mode=1, arp_oct=2, arp_rate=4, arp_swing=60))

    # --- Effets (4)
    add("Souffle", FX, mix(NAPPE, awave=5, bwave=5, aunison=1, bunison=1, fatype=2, facutoff=1500, fareso=50, l2cutoff=50, l2rate=0.2))
    add("Montée", FX, mix(SUPER, ma=4000, ms=100, md=100, modcutoffa=70, modcutoffb=70, facutoff=700, fbcutoff=700, modpitch=12, aa=1500, ar=1500))
    add("Pulsation stéréo", FX, mix(NAPPE, l1pan=100, l1rate=4, l1shape=3, aa=5))
    add("Chute", FX, mix(PLK, md=900, modpitch=12, ad=1200, ar=800))

    # --- v0.47 (maj. 5 nov.) : 32 sons de plus (au moins 90 par synthé)
    for nm, kw in [
        ("Basse carrée unisson", dict(awave=3, aunison=3, adetune=15)), ("Basse sinus pure", dict(awave=0, bwave=0, facutoff=2000, modcutoffa=10)),
        ("Basse impulsion", dict(awave=4, bwave=6, facutoff=900)), ("Basse large", dict(aunison=4, adetune=20, width=80)),
        ("Basse bruitée", dict(bwave=5, blevel=20, facutoff=800)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead sinus", dict(awave=0, bwave=1, facutoff=8000)), ("Lead impulsion", dict(awave=4, bwave=6)),
        ("Lead tri-scie large", dict(awave=7, aunison=7, adetune=30)), ("Lead doux", dict(facutoff=2500, fareso=5, aa=30)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe triangle", dict(awave=1, bwave=1)), ("Nappe carrée", dict(awave=3, bwave=3, facutoff=2500)),
        ("Nappe impulsion étroite", dict(awave=6, bwave=4)), ("Nappe tri-scie", dict(awave=7, bwave=7)),
        ("Nappe bande", dict(fatype=2, fbtype=2, facutoff=1500, fbcutoff=2500)), ("Nappe éther", dict(aa=2500, ar=4500, facutoff=5000)),
        ("Nappe souffle doux", dict(bwave=5, blevel=25)), ("Nappe passe-haut", dict(fatype=3, fbtype=3, facutoff=600, fbcutoff=600)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck sinus", dict(awave=0, bwave=1)), ("Pluck tri-scie", dict(awave=7)), ("Pluck très large", dict(aunison=8, adetune=35, width=100)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [("Clavier carré", dict(awave=3, bwave=3)), ("Clavier sinus", dict(awave=0, bwave=0, facutoff=5000)), ("Orgue large", dict(awave=0, bwave=3, as_=100, ad=200, ar=200))]:
        add(nm, KEYS, mix(CLAV, **kw))
    CORD2 = mix(NAPPE, aunison=6, adetune=25, aa=400, ar=1400, facutoff=5000, fbcutoff=5000, modcutoffa=10, l1pitch=6, l1rate=5)
    add("Cordes larges", STR, mix(CORD2, aunison=8, adetune=35))
    add("Cordes triangle", STR, mix(CORD2, awave=1, bwave=7))
    add("Supersaw géante", PAD, mix(SUPER, aunison=8, adetune=55, bdetune=45))
    add("Supersaw sombre", PAD, mix(SUPER, facutoff=2500, fbcutoff=2500))
    add("Cuivres supersaw", BRASS, mix(SUPER, aa=40, facutoff=1500, fbcutoff=1500, modcutoffa=45, modcutoffb=45, ma=50, md=500, ms=40))
    add("Arpège supersaw", ARP, mix(SUPER, aa=1, ad=250, as_=0, ar=250, arp_gate=50, arp_rate=3, arp_mode=1, arp_oct=2))
    add("Arpège sinus", ARP, mix(PLK, awave=0, arp_gate=50, arp_rate=4, arp_mode=3, arp_oct=3))
    add("Nuage de bruit", FX, mix(NAPPE, awave=5, bwave=5, facutoff=1200, fareso=40))
    return b
