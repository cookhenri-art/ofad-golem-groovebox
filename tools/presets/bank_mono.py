# Basse mono (grand monophonique à deux oscillateurs, filtre en échelle, réinjection, 2 bus) : 50 sons.
# Formes continues : 0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion.
from banks_common import Bank, mix, BASS, LEAD, PLUCK, ARP, FX, INIT

# archétypes
BASSE = dict(o1shape=2, o2shape=2, o2fine=6, mix1=80, mix2=55, mixsub=45, cutoff=480, reso=18, envamt=45,
             fa=1, fd=260, fs=25, fr=150, aa=1, ad=700, as_=80, ar=120, drive=6)
LEAD_ = dict(o1oct=1, o2oct=1, o1shape=2, o2shape=2, o2fine=8, mix1=80, mix2=70, mixsub=0, cutoff=1900, reso=25,
             envamt=35, fd=500, fs=55, aa=2, ad=900, as_=100, ar=220, glide=40, glidemode=1, drive=8,
             mb1_src=7, mb1_rate=5.5, mb1_pitch=0.4)       # vibrato à la molette
PLK = dict(as_=0, ad=280, ar=160, fd=160, fs=0, envamt=65, cutoff=380, reso=35, mixsub=30)
VIB = dict(mb2_src=7, mb2_rate=5.2, mb2_pitch=0.15, mb2_ctrl=2, mb2_gdst=0)  # vibrato léger permanent


def build():
    b = Bank("mono")
    b.add("Init", INIT, dict(mix1=80, mix2=0, mixsub=0, mixnoise=0, drift=0, cutoff=9000, reso=0, drive=0, envamt=0, velfilter=0, keytrack=0, fa=1, fd=600, fs=100, fr=200, aa=1, ad=600, as_=100, ar=150, mb1_pitch=0, mb1_filt=0, mb2_filt=0))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple
    add = b.add

    # --- Basses (20)
    add("Sub profond", BASS, mix(BASSE, o1shape=0.3, mix2=0, mixsub=85, subshape=1, cutoff=260, reso=0, envamt=15, fd=300, fs=40, as_=95, ar=160, drive=2))
    add("Basse ronde", BASS, mix(BASSE))
    add("Coup de poing", BASS, mix(BASSE, o1shape=2.2, o2shape=3, o2oct=-1, mix2=50, mixsub=40, cutoff=380, reso=25, envamt=60, fd=180, fs=15, drive=10, aa=0.5, ad=400, as_=70, ar=90, oscreset=1))
    add("Basse carrée", BASS, mix(BASSE, o1shape=3, o2shape=3, o2oct=-1, mix2=50, cutoff=600, reso=10, envamt=35, fd=250, fs=30))
    add("Grosse réinjection", BASS, mix(BASSE, mixfb=60, drive=18, cutoff=450, reso=30, envamt=50, fd=300))
    add("Basse funk", BASS, mix(BASSE, o1shape=3.4, o2shape=2, cutoff=520, reso=45, envamt=70, fd=140, fs=5, velfilter=60, ad=350, as_=60, ar=80, slope=1))
    add("Basse 12 dB claquante", BASS, mix(BASSE, slope=2, cutoff=700, reso=35, envamt=55, fd=160, fs=10, drive=8))
    add("Reese battante", BASS, mix(BASSE, o2fine=0, o2beat=0.6, mix2=90, mixsub=30, cutoff=900, reso=10, envamt=10, drive=12, fs=80))
    add("Sub sinus long", BASS, mix(BASSE, mix1=0, mix2=0, mixsub=100, subshape=1, cutoff=1500, envamt=0, as_=100, ar=300, drive=4))
    add("Basse glissée", BASS, mix(BASSE, glide=120, glidemode=1, o2fine=8, cutoff=500, reso=20, envamt=45, fd=220, fs=30))
    add("Basse octaviée", BASS, mix(BASSE, o2oct=1, o2shape=3, mix2=45, cutoff=650, reso=20, envamt=50, fd=200, fs=20))
    add("Basse quinte", BASS, mix(BASSE, o2semi=7, mix2=50, cutoff=550, envamt=45, fd=200, fs=20))
    add("Basse douce", BASS, mix(BASSE, o1shape=1, o2shape=1, mixsub=50, cutoff=350, reso=5, envamt=20, fd=400, fs=50, aa=8, ar=250, drive=0))
    add("Basse saturée", BASS, mix(BASSE, drive=26, cutoff=600, reso=20, envamt=45, fd=250, fs=25, o1shape=2.5))
    add("Basse pincée", BASS, mix(BASSE, PLK, cutoff=300, reso=30, ad=450, ar=200, fd=150))
    add("Basse wobble", BASS, mix(BASSE, mb2_src=0, mb2_sync=8, mb2_filt=60, mb2_ctrl=2, cutoff=400, reso=40, envamt=10, drive=14, fs=100, as_=100))
    add("Basse résonante 24 dB", BASS, mix(BASSE, slope=0, reso=70, cutoff=300, envamt=75, fd=180, fs=0, glide=60, glidemode=1, mix2=0, mixsub=0, drive=10))
    add("Basse synchro", BASS, mix(BASSE, sync=1, o2semi=10, mix1=0, mix2=90, mb2_src=5, mb2_pitch=8, mb2_pdst=2, mb2_ctrl=2, cutoff=1200, envamt=30, fd=200))
    add("Pompe en boucle", BASS, mix(BASSE, floop=1, fsync=1, fa=0.5, fd=250, fs=0, envamt=60, reso=40, cutoff=350, as_=100))
    add("Basse granuleuse", BASS, mix(BASSE, mixnoise=15, cutoff=500, reso=15, envamt=45, fd=220, drive=8))

    # --- Leads (14)
    add("Lead classique", LEAD, mix(LEAD_))
    add("Lead carré creux", LEAD, mix(LEAD_, o1shape=3, o2shape=3, o2fine=6, cutoff=1600, reso=15, slope=2, envamt=25))
    add("Lead synchro", LEAD, mix(LEAD_, sync=1, mix1=20, mix2=90, o2semi=7, mb2_src=5, mb2_pitch=12, mb2_pdst=2, mb2_ctrl=2, cutoff=4000, reso=15, envamt=20, fd=600, fs=30, glide=30))
    add("Lead duo", LEAD, mix(LEAD_, mode=1, cutoff=2200, reso=20, glide=50))
    add("Lead flûté", LEAD, mix(LEAD_, VIB, o1shape=0.8, mix2=0, mixnoise=8, cutoff=2500, slope=3, reso=0, envamt=10, aa=30, ar=250, drive=0))
    add("Lead gras", LEAD, mix(LEAD_, drive=20, mixfb=40, o2fine=12, cutoff=1500, reso=35, envamt=40))
    add("Lead portamento long", LEAD, mix(LEAD_, glide=300, glidemode=0, cutoff=1800, reso=30))
    add("Lead résonant", LEAD, mix(LEAD_, reso=80, cutoff=900, envamt=50, fd=900, fs=40, drive=12))
    add("Lead à la pression", LEAD, mix(LEAD_, mb1_ctrl=1, mb1_src=9, mb1_pitch=0, mb1_filt=50, cutoff=1000))
    add("Lead chanté", LEAD, mix(LEAD_, VIB, aa=20, cutoff=1800, o1shape=2.6))
    add("Lead quinte", LEAD, mix(LEAD_, o2semi=7, mix2=70, cutoff=2000))
    add("Lead octave basse", LEAD, mix(LEAD_, o2oct=0, mix2=60, cutoff=1700, reso=20))
    add("Lead impulsion fine", LEAD, mix(LEAD_, o1shape=4, o2shape=3.8, cutoff=3000, reso=10, envamt=20))
    add("Lead sifflant", LEAD, mix(LEAD_, o1shape=0, mix2=0, cutoff=4000, reso=60, slope=1, keytrack=100, aa=15, drive=0))

    # --- Plucks et séquences (6)
    add("Pluck rebondissant", PLUCK, mix(BASSE, PLK, ad=220, ar=150, fd=140, envamt=70, cutoff=400, reso=50))
    add("Séquence acidulée", PLUCK, mix(BASSE, reso=60, envamt=60, fd=160, fs=0, cutoff=350, as_=60, ad=250, ar=80, mixsub=10))
    add("Pluck percussif", PLUCK, mix(BASSE, PLK, oscreset=1, ad=160, ar=120, fd=90, envamt=80, cutoff=300, reso=20, drive=10, velfilter=60))
    add("Pluck boisé", PLUCK, mix(BASSE, PLK, o1shape=1, o2shape=1, o2oct=1, mix2=40, ad=300, ar=200, fd=200, envamt=50, cutoff=500, reso=30))
    add("Séquence pulsée", PLUCK, mix(BASSE, mb2_src=1, mb2_sync=9, mb2_filt=40, mb2_ctrl=2, cutoff=500, reso=30, as_=100))
    add("Pluck en boucle", PLUCK, mix(BASSE, aloop=1, async_=1, aa=1, ad=150, as_=0, ar=100, envamt=40, fd=150, fs=0))

    # --- Arpèges (4)
    ARPB = mix(BASSE, PLK, ad=250, ar=100, fd=150, envamt=55, cutoff=450, reso=35, arp_gate=50)
    add("Arpège basse montant", ARP, mix(ARPB, arp_mode=1, arp_oct=2, arp_rate=3))
    add("Arpège convergent", ARP, mix(ARPB, arp_mode=10, arp_oct=2, arp_rate=3, reso=50))
    add("Arpège en triolets", ARP, mix(ARPB, arp_mode=3, arp_oct=1, arp_rate=4, o2oct=1))
    add("Arpège pédale grave", ARP, mix(ARPB, arp_mode=13, arp_oct=2, arp_rate=3, slope=2))

    # --- Effets (6)
    add("Sirène", FX, mix(LEAD_, mb2_src=1, mb2_rate=0.5, mb2_pitch=12, mb2_ctrl=2, cutoff=3000, mb1_pitch=0))
    add("Laser", FX, mix(LEAD_, mb2_src=5, mb2_pitch=24, mb2_ctrl=2, fa=0.5, fd=300, fs=0, cutoff=5000, as_=0, ad=400, ar=200, mb1_pitch=0))
    add("Vent filtré", FX, mix(BASSE, mix1=0, mix2=0, mixsub=0, mixnoise=100, cutoff=800, reso=70, mb2_src=7, mb2_rate=0.2, mb2_filt=50, mb2_ctrl=2, aa=400, ar=1200, as_=100, envamt=0))
    add("Ordinateur fou", FX, mix(LEAD_, mb2_src=4, mb2_sync=9, mb2_pitch=12, mb2_filt=40, mb2_ctrl=2, reso=60, cutoff=1500, mb1_pitch=0))
    add("Grésillement métallique", FX, mix(LEAD_, mb2_src=7, mb2_rate=300, mb2_pitch=3, mb2_ctrl=2, cutoff=3000, mb1_pitch=0))
    add("Larsen contrôlé", FX, mix(BASSE, mixfb=100, drive=30, reso=90, cutoff=250, envamt=80, fd=800, fs=40))
    return b
