# Poly analogique (5 voix d'origine, 2 oscillateurs, poly-mod, Wheel-Mod) : 50 sons.
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX, INIT

BASE = dict(asaw=1, apulse=0, bsaw=1, btri=0, bpulse=0, mixa=80, mixb=60, bfine=6, cutoff=1600, reso=15, envamt=40,
            fa=5, fd=800, fs=30, fr=600, aa=3, ad=1200, as_=75, ar=700, wmfreqa=0, wmfreqb=0)
BASSE = mix(BASE, voices=1, asemi=-12, bsemi=-12, cutoff=500, envamt=50, fd=300, fs=10, ad=600, as_=80, ar=150, keytrack=30)
LEAD_ = mix(BASE, voices=1, glide=40, cutoff=2200, reso=25, envamt=30, fs=60, as_=100, ar=250, bfine=9, wmfreqa=1, wmfreqb=1, lforate=5.5)
NAPPE = mix(BASE, apulse=1, apw=35, cutoff=1200, reso=10, envamt=25, fa=1200, fd=2500, fs=60, fr=2500, aa=900, ad=2000, as_=90, ar=2500, bfine=8)
PLK = mix(BASE, cutoff=700, reso=25, envamt=55, fa=1, fd=280, fs=0, fr=300, aa=1, ad=600, as_=0, ar=400)
CLAV = mix(BASE, cutoff=1800, reso=10, envamt=30, fa=1, fd=900, fs=20, fr=400, aa=2, ad=1500, as_=40, ar=400, velfilter=50, velamp=50)
CUIV = mix(BASE, bfine=8, cutoff=850, reso=10, envamt=55, fa=60, fd=600, fs=40, fr=300, aa=30, ad=800, as_=85, ar=300, velfilter=40)
CORD = mix(BASE, asaw=1, bsaw=1, bfine=10, cutoff=2400, reso=5, envamt=10, fa=400, fd=1500, fs=80, fr=1200, aa=350, ad=1500, as_=90, ar=1200,
           wminit=8, wmfreqa=1, wmfreqb=0, lforate=5)


def build():
    b = Bank("poly5")
    b.add("Init", INIT, dict(mixa=80, mixb=0, mixnoise=0, vintage=10, cutoff=9000, reso=0, envamt=0, velfilter=0, keytrack=0, fs=100, aa=2, ad=1200, as_=100, ar=300))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (6)
    add("Basse poly ronde", BASS, mix(BASSE))
    add("Basse unisson grasse", BASS, mix(BASSE, unison=1, unidetune=18, filterrev=0, cutoff=450, reso=30, envamt=55, fd=250))
    add("Basse impulsion", BASS, mix(BASSE, asaw=0, apulse=1, apw=25, bsaw=0, bpulse=1, bpw=40, cutoff=700, envamt=40, fd=200))
    add("Basse synchro mordante", BASS, mix(BASSE, async_=1, asemi=0, bsemi=-12, pmenv=40, pmfreqa=1, cutoff=900, reso=20, filterrev=0))
    add("Basse feutrée", BASS, mix(BASSE, asaw=0, apulse=1, apw=50, bsaw=0, btri=1, cutoff=350, reso=5, envamt=30, fd=400, fs=40))
    add("Basse pluck", BASS, mix(BASSE, asemi=0, bsemi=0, mixa=100, mixb=80, cutoff=900, fs=0, as_=0, ad=500, ar=200, fd=220, envamt=65, reso=30))

    # --- Leads (6)
    add("Lead poly-mod", LEAD, mix(LEAD_, pmoscb=35, pmfreqa=1, bsemi=7, cutoff=2600))
    add("Lead synchro", LEAD, mix(LEAD_, async_=1, bsemi=-12, mixb=0, pmenv=55, pmfreqa=1, cutoff=3500, reso=20))
    add("Lead carré chaud", LEAD, mix(LEAD_, asaw=0, apulse=1, apw=50, bsaw=0, bpulse=1, bpw=45, cutoff=1800, filterrev=0))
    add("Lead unisson", LEAD, mix(LEAD_, unison=1, unidetune=14, cutoff=2400, reso=20))
    add("Lead doux à la pression", LEAD, mix(LEAD_, cutoff=900, atfilter=60, atwheel=40, reso=15))
    add("Lead quinte", LEAD, mix(LEAD_, bsemi=7, mixb=70, cutoff=2400))

    # --- Nappes (8)
    add("Nappe chaleureuse", PAD, mix(NAPPE))
    add("Nappe largeur mouvante", PAD, mix(NAPPE, wminit=25, wmpwa=1, wmpwb=1, bsaw=0, bpulse=1, bpw=40, lforate=0.4))
    add("Nappe sombre", PAD, mix(NAPPE, cutoff=600, reso=20, envamt=35, filterrev=0))
    add("Nappe de verre", PAD, mix(NAPPE, asaw=0, apulse=0, mixa=0, btri=1, bsaw=0, bsemi=12, pmoscb=20, pmfilter=1, cutoff=3000, reso=30))
    add("Nappe filtre lent", PAD, mix(NAPPE, wminit=30, wmfilter=1, wmfreqa=0, wmfreqb=0, lforate=0.12, cutoff=900, reso=35))
    add("Nappe aérienne", PAD, mix(NAPPE, mixnoise=15, cutoff=2200, reso=10, aa=2000, ar=4000))
    add("Nappe quinte", PAD, mix(NAPPE, bsemi=7, bfine=4))
    add("Nappe vintage", PAD, mix(NAPPE, vintage=80, cutoff=1400))

    # --- Plucks (5)
    add("Pluck poly", PLUCK, mix(PLK))
    add("Pluck résonant", PLUCK, mix(PLK, reso=55, cutoff=500, envamt=65, fd=200))
    add("Pluck cristal", PLUCK, mix(PLK, asaw=0, apulse=1, apw=15, bsemi=12, cutoff=1500, fd=350, ad=900, ar=600))
    add("Pluck synchro", PLUCK, mix(PLK, async_=1, bsemi=5, pmenv=45, pmfreqa=1, mixa=100, mixb=0, cutoff=3500, envamt=35, fs=10))
    add("Pluck mat", PLUCK, mix(PLK, asaw=0, apulse=1, apw=50, cutoff=450, reso=10, filterrev=0))

    # --- Claviers (6)
    add("Clavier analogique", KEYS, mix(CLAV))
    add("Clavier électrique", KEYS, mix(CLAV, asaw=0, apulse=1, apw=30, bsaw=0, btri=1, bsemi=12, mixb=40, cutoff=1500))
    add("Orgue poly", KEYS, mix(CLAV, asaw=0, apulse=1, apw=50, bsaw=0, bpulse=1, bpw=50, bsemi=12, fs=100, as_=100, ad=100, aa=5, ar=80, envamt=0, cutoff=4000))
    add("Clavinette", KEYS, mix(CLAV, asaw=0, apulse=1, apw=12, mixb=0, cutoff=2000, reso=25, fd=180, fs=10, ad=500, as_=20, ar=150))
    add("Carillon poly-mod", KEYS, mix(CLAV, async_=1, bsemi=19, pmenv=60, pmfreqa=1, envamt=20, fs=0, as_=0, ad=1800, ar=1800))
    add("Accords house", KEYS, mix(CLAV, cutoff=1300, reso=30, envamt=45, fd=250, fs=0, ad=400, as_=0, ar=300))

    # --- Cordes (5)
    add("Cordes analogiques", STR, mix(CORD))
    add("Cordes sombres", STR, mix(CORD, cutoff=1300, aa=600))
    add("Cordes pizzicato", STR, mix(CORD, aa=1, ad=300, as_=0, ar=300, fa=1, fd=250, fs=0, envamt=40, cutoff=1500))
    add("Cordes octave", STR, mix(CORD, bsemi=12, mixb=45))
    add("Ensemble lent", STR, mix(CORD, aa=1500, ar=2500, wminit=15))

    # --- Cuivres (6)
    add("Cuivres poly", BRASS, mix(CUIV))
    add("Cuivres brillants", BRASS, mix(CUIV, cutoff=1200, envamt=65, reso=15))
    add("Cuivres doux", BRASS, mix(CUIV, cutoff=600, envamt=45, fa=150, aa=80))
    add("Cuivres synchro", BRASS, mix(CUIV, async_=1, bsemi=7, pmenv=30, pmfreqa=1))
    add("Section staccato", BRASS, mix(CUIV, fs=10, ad=300, as_=30, ar=150, fd=250))
    add("Cuivres unisson", BRASS, mix(CUIV, voices=1, unison=1, unidetune=16))

    # --- Arpèges (4)
    add("Arpège poly montant", ARP, mix(PLK, arp_mode=1, arp_oct=2, arp_rate=3, arp_gate=45))
    add("Arpège haut-bas", ARP, mix(PLK, arp_mode=3, arp_oct=2, arp_rate=3, reso=40))
    add("Arpège mélangé", ARP, mix(PLK, arp_mode=18, arp_oct=2, arp_rate=3, cutoff=900))
    add("Arpège en marches", ARP, mix(PLK, arp_mode=16, arp_oct=2, arp_rate=4, async_=1, bsemi=12, pmenv=25, pmfreqa=1))

    # --- Effets (4)
    add("Bruit de vent", FX, mix(NAPPE, mixa=0, mixb=0, mixnoise=100, cutoff=900, reso=60, wminit=40, wmfilter=1, lforate=0.2, ltri=1))
    add("Échantillonneur aléatoire", FX, mix(LEAD_, wmmix=100, wminit=60, wmfilter=1, wmfreqa=0, wmfreqb=0, cutoff=1200, reso=60, lforate=8))
    add("Sirène poly", FX, mix(LEAD_, wminit=70, wmfreqa=1, wmfreqb=1, lforate=0.6, ltri=1))
    add("Impact métallique", FX, mix(PLK, async_=1, bsemi=23, pmoscb=70, pmfreqa=1, mixa=100, cutoff=5000, reso=20, envamt=30, fs=20, ad=1500, ar=1500))

    # --- v0.47 (maj. 5 nov.) : 34 sons de plus (au moins 90 par synthé)
    for nm, kw in [
        ("Basse poly sombre", dict(cutoff=320, envamt=40)), ("Basse quinte poly", dict(bsemi=-5, cutoff=600)),
        ("Basse sciée brillante", dict(cutoff=1100, reso=10, envamt=35)), ("Basse unisson large", dict(unison=1, unidetune=30, cutoff=550)),
        ("Basse poly-mod", dict(pmoscb=30, pmfreqa=1, cutoff=700, reso=20)), ("Basse impulsion fine", dict(asaw=0, apulse=1, apw=12, cutoff=800)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead quinte ouverte", dict(bsemi=7, cutoff=2400)), ("Lead synchro douce", dict(async_=1, bsemi=12, pmenv=25, pmfreqa=1, cutoff=2000)),
        ("Lead unisson épais", dict(unison=1, unidetune=20, cutoff=2500)), ("Lead impulsion", dict(asaw=0, apulse=1, apw=20, bsaw=0, bpulse=1, bpw=35)),
        ("Lead flûte poly", dict(asaw=0, apulse=1, apw=50, btri=1, bsaw=0, mixa=90, mixb=80, cutoff=2500, reso=5, mixnoise=10, aa=40)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe sciée", dict(apulse=0, asaw=1, cutoff=1500)), ("Nappe poly-mod", dict(pmoscb=20, pmfreqa=1, cutoff=1300)),
        ("Nappe des profondeurs", dict(cutoff=600, reso=20)), ("Nappe qui respire", dict(wminit=25, wmfilter=1, lforate=0.15, ltri=1)),
        ("Nappe d'hiver", dict(cutoff=2200, reso=5, bfine=12, aa=1800, ar=3500)), ("Nappe impulsion", dict(apw=15, bsaw=0, bpulse=1, bpw=25)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck sec", dict(cutoff=500, ad=350, fd=200)), ("Pluck synchro mordant", dict(async_=1, bsemi=12, pmenv=40, pmfreqa=1)),
        ("Pluck boisé", dict(asaw=0, apulse=1, apw=30, cutoff=900, reso=15)), ("Pluck qui siffle", dict(reso=55, envamt=70)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Clavier sciée", dict(cutoff=1500)), ("Clavier impulsion", dict(asaw=0, apulse=1, apw=30, bsaw=0, bpulse=1, bpw=45)),
        ("Clavier poly-mod", dict(pmoscb=25, pmfreqa=1, cutoff=2400)), ("Orgue à impulsions", dict(asaw=0, apulse=1, apw=50, bsaw=0, btri=1, as_=100, ad=200, ar=150, envamt=0)),
    ]:
        add(nm, KEYS, mix(CLAV, **kw))
    for nm, kw in [("Cordes chaudes", dict(cutoff=1800)), ("Cordes lentes", dict(aa=900, ar=2200)), ("Cordes pincées douces", dict(aa=1, ad=400, as_=0, ar=300, envamt=40, cutoff=1500))]:
        add(nm, STR, mix(CORD, **kw))
    for nm, kw in [("Cuivres éclatants", dict(cutoff=1300, envamt=60)), ("Cuivres sourds", dict(cutoff=600)), ("Cuivres attaque lente", dict(aa=150, fa=200))]:
        add(nm, BRASS, mix(CUIV, **kw))
    add("Arpège poly impulsion", ARP, mix(PLK, asaw=0, apulse=1, apw=25, arp_mode=1, arp_oct=2, arp_rate=3))
    add("Arpège trois octaves", ARP, mix(PLK, arp_mode=3, arp_oct=3, arp_rate=3, cutoff=900))
    add("Vaisseau", FX, mix(NAPPE, pmoscb=80, pmfreqa=1, bsemi=-19, lforate=0.1, wminit=40, wmfreqa=1, cutoff=900, reso=40))
    return b
