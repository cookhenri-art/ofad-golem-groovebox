# État de Grâce (v0.47 (maj. 5 nov.), poly à deux oscillateurs à formes continues, filtre d'état continu, X-Mod) : 78 sons de plus.
# ashape / bshape : 0 = scie … 50 = triangle … 100 = carré/impulsion (formes continues) · fmode : 0 passe-bas … 50 réjection … 100 passe-haut
# keytrack : 0 arrêt, 1 moitié, 2 plein · lfoshape : 0 sinus, 1 scie, 2 scie inv., 3 carré, 4 aléatoire
from banks_common import Bank, mix, BASS, LEAD, PAD, PLUCK, KEYS, STR, BRASS, ARP, FX

BASE = dict(ashape=0, bshape=0, bfine=7, mixa=80, mixb=70, cutoff=1800, reso=20, envamt=40, fa=5, fd=700, fs=35, fr=600,
            aa=3, ad=1000, as_=80, ar=700, vintage=20)
BASSE = mix(BASE, voices=1, afreq=-12, bfreq=-12, bfine=5, cutoff=480, reso=25, envamt=55, fd=280, fs=15, fr=150, ad=700, as_=85, ar=150, mixsub=50)
LEAD_ = mix(BASE, voices=1, glide=40, cutoff=2400, reso=25, envamt=30, fd=600, fs=60, as_=100, ar=250, lforate=5.5, lfoamt=6, lfofreqa=1, lfofreqb=1)
NAPPE = mix(BASE, ashape=50, bshape=0, bfine=9, cutoff=1300, reso=15, envamt=25, fa=900, fd=2500, fs=60, fr=2200, aa=800, ad=2000, as_=90, ar=2500,
            lforate=0.3, lfoamt=8, lfocut=1, lfofreqa=0, lfofreqb=0)
PLK = mix(BASE, cutoff=700, reso=30, envamt=60, fa=0.5, fd=250, fs=0, fr=250, aa=0.5, ad=500, as_=0, ar=350)
CLAV = mix(BASE, cutoff=2200, reso=10, envamt=30, fa=1, fd=900, fs=20, fr=400, aa=1, ad=1500, as_=35, ar=450, velenv=50, velamp=50)
CORD = mix(BASE, bfine=11, cutoff=3000, reso=5, envamt=10, fa=400, fd=1500, fs=80, fr=1300, aa=350, ad=1500, as_=90, ar=1300,
           lforate=5, lfoamt=3, lfofreqa=1, lfofreqb=1)
CUIV = mix(BASE, bfine=8, cutoff=900, reso=10, envamt=55, fa=60, fd=600, fs=45, fr=300, aa=35, ad=800, as_=85, ar=300)


def build():
    b = Bank("ob6")
    add = b.add

    for nm, kw in [
        ("Basse d'état", {}), ("Basse carrée d'état", dict(ashape=100, bshape=100, apw=50, bpw=40)), ("Basse triangle", dict(ashape=50, bshape=50, cutoff=900)),
        ("Basse unisson d'état", dict(unison=1, univoices=4, unidetune=15)), ("Basse X-Mod", dict(xoscb=40, xfreqa=1, cutoff=800)),
        ("Basse synchro", dict(async_=1, bfreq=0, afreq=-12, xenv=30, xfreqa=1, cutoff=900)), ("Basse réjection", dict(fmode=45, cutoff=600, reso=40)),
        ("Basse passe-bande", dict(bandpass=1, cutoff=700, reso=35)), ("Basse impulsion", dict(ashape=100, apw=20, bshape=100, bpw=30)),
        ("Basse ronde d'état", dict(cutoff=320, reso=10, envamt=35)), ("Basse claquante", dict(fd=150, fs=0, envamt=75, reso=35)),
        ("Basse glissée d'état", dict(glide=110)), ("Basse quinte", dict(bfreq=-5)), ("Basse grave octaviée", dict(bfreq=-24, mixb=60)),
        ("Basse bruitée", dict(mixnoise=20)), ("Basse wobble d'état", dict(lfosync=8, lfoamt=40, lfocut=1, as_=100, fs=60, reso=45)),
        ("Basse vintage", dict(vintage=80, cutoff=500)), ("Basse au sous-oscillateur", dict(mixa=40, mixb=0, mixsub=100, cutoff=400)),
    ]:
        add(nm, BASS, mix(BASSE, **kw))
    for nm, kw in [
        ("Lead d'état", {}), ("Lead carré", dict(ashape=100, bshape=100)), ("Lead synchro", dict(async_=1, bfreq=12, xenv=40, xfreqa=1, mixa=0)),
        ("Lead X-Mod", dict(xoscb=35, xfreqa=1, bfreq=7)), ("Lead quinte", dict(bfreq=7)), ("Lead unisson large", dict(unison=1, univoices=6, unidetune=18)),
        ("Lead passe-bande", dict(bandpass=1, cutoff=1500, reso=40)), ("Lead doux", dict(ashape=50, bshape=50, cutoff=1500, reso=10, aa=20)),
        ("Lead à la pression", dict(atfilter=60, cutoff=1200)), ("Lead vintage", dict(vintage=90)), ("Lead impulsion", dict(ashape=100, apw=15, bshape=100, bpw=25)),
        ("Lead octave", dict(bfreq=12, mixb=50)), ("Lead bruité", dict(mixnoise=15)), ("Lead réjection", dict(fmode=50, reso=50)),
    ]:
        add(nm, LEAD, mix(LEAD_, **kw))
    for nm, kw in [
        ("Nappe d'état", {}), ("Nappe sciée", dict(ashape=0, bshape=0)), ("Nappe carrée", dict(ashape=100, bshape=100, apw=40, bpw=30, lfopw=1)),
        ("Nappe réjection", dict(fmode=50, reso=40, lfomode=1)), ("Nappe passe-haut", dict(fmode=100, cutoff=500)), ("Nappe lente", dict(aa=2200, ar=4000)),
        ("Nappe sombre d'état", dict(cutoff=600, reso=25)), ("Nappe X-Mod", dict(xoscb=20, xfreqa=1, bfreq=7)), ("Nappe vintage", dict(vintage=90, cutoff=1600)),
        ("Nappe balayée", dict(lfomode=1, lfocut=0, lfoamt=40, lforate=0.1)), ("Nappe large", dict(unison=0, bfine=18, cutoff=2000)),
    ]:
        add(nm, PAD, mix(NAPPE, **kw))
    for nm, kw in [
        ("Pluck d'état franc", {}), ("Pluck carré", dict(ashape=100, bshape=100)), ("Pluck X-Mod", dict(xoscb=50, xfreqa=1, bfreq=19)),
        ("Pluck résonant", dict(reso=60)), ("Pluck passe-bande", dict(bandpass=1, cutoff=1000)), ("Pluck triangle", dict(ashape=50, bshape=50, cutoff=1500)),
        ("Pluck synchro", dict(async_=1, bfreq=12, xenv=50, xfreqa=1)), ("Pluck sec", dict(fd=150, ad=300)),
        ("Pluck passe-haut", dict(fmode=100, cutoff=400, reso=30)), ("Pluck vintage", dict(vintage=90)),
    ]:
        add(nm, PLUCK, mix(PLK, **kw))
    for nm, kw in [
        ("Piano d'état", {}), ("Clavier carré", dict(ashape=100, bshape=100)), ("Orgue d'état", dict(ashape=50, bshape=100, bfreq=12, envamt=0, cutoff=5000, as_=100, ad=100, ar=120)),
        ("Accords d'état", dict(cutoff=1000, envamt=45, ad=600, as_=20)), ("Clavier X-Mod", dict(xoscb=25, xfreqa=1, bfreq=12)), ("Cloche d'état", dict(xoscb=60, xfreqa=1, bfreq=19, as_=0, ad=2000, ar=2000)),
    ]:
        add(nm, KEYS, mix(CLAV, **kw))
    for nm, kw in [("Cordes d'état", {}), ("Cordes lentes", dict(aa=900, ar=2200)), ("Cordes sombres", dict(cutoff=1500)), ("Cordes pincées", dict(aa=1, as_=0, ad=500, ar=400, envamt=40))]:
        add(nm, STR, mix(CORD, **kw))
    for nm, kw in [("Cuivres d'état ouverts", {}), ("Cuivres brillants", dict(cutoff=1300, envamt=60)), ("Cuivres doux", dict(cutoff=600)), ("Stab cuivré", dict(ad=300, as_=0, fd=250, fs=0))]:
        add(nm, BRASS, mix(CUIV, **kw))
    for nm, kw in [("Arpège d'état", dict(arp_mode=1, arp_oct=2)), ("Arpège carré", dict(ashape=100, bshape=100, arp_mode=3, arp_oct=2)),
                   ("Arpège X-Mod", dict(xoscb=40, xfreqa=1, bfreq=12, arp_mode=1, arp_oct=3)), ("Arpège aléatoire", dict(arp_mode=6, arp_oct=2, arp_rate=4))]:
        add(nm, ARP, mix(PLK, **kw))
    for nm, kw in [
        ("Sirène d'état", dict(lfofreqa=1, lfofreqb=1, lfoamt=60, lforate=0.5)), ("Vent d'état", dict(mixa=0, mixb=0, mixnoise=100, cutoff=900, reso=60, lfocut=1, lfoamt=30, lforate=0.2)),
        ("Cloches folles", dict(xoscb=90, xfreqa=1, bfreq=23, lfoshape=4, lforate=6, lfoamt=20, lfofreqb=1)), ("Balayage réjection", dict(fmode=50, reso=80, lfomode=1, lfoamt=60, lforate=0.15)),
    ]:
        add(nm, FX, mix(NAPPE, **kw))
    return b
