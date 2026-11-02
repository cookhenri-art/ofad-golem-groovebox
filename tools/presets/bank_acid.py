# Basse acide (filtre à diodes, accent, glissé) : 50 sons.
from banks_common import Bank, ACID, BASS, LEAD, FX, INIT


def build():
    b = Bank("acid")
    b.add("Init", INIT, dict(wave=0, cutoff=900, reso=20, envmod=30, decay=400, accent=50, drive=0, dist=0, ampdecay=1500, glide=60, tune=0, keytrack=20, accthr=80))  # point de départ : un oscillateur, filtre ouvert, enveloppe simple

    def a(name, cat, wave, cutoff, reso, envmod, decay, accent, drive=3, dist=0, ampdecay=1500, glide=60, tune=0, keytrack=20, accthr=80):
        b.add(name, cat, dict(wave=wave, cutoff=cutoff, reso=reso, envmod=envmod, decay=decay, accent=accent, drive=drive,
                              dist=dist, ampdecay=ampdecay, glide=glide, tune=tune, keytrack=keytrack, accthr=accthr))

    # --- Acide : lignes à résonance, de la plus sage à la plus sauvage
    a("Ligne de minuit", ACID, 0, 380, 70, 60, 450, 65)
    a("Résonance mouillée", ACID, 0, 260, 85, 70, 380, 75, drive=6)
    a("Bulle", ACID, 0, 220, 90, 85, 260, 80)
    a("Gargouille", ACID, 0, 180, 92, 95, 300, 90, drive=10)
    a("Spirale", ACID, 0, 300, 80, 75, 700, 70, glide=120)
    a("Squelette", ACID, 0, 520, 60, 40, 200, 55, ampdecay=500)
    a("Acide carré", ACID, 1, 340, 78, 65, 420, 70)
    a("Acide sec", ACID, 0, 450, 68, 55, 180, 60, ampdecay=400)
    a("Acide gras", ACID, 0, 300, 80, 70, 500, 75, drive=14, dist=25)
    a("Acide vitreux", ACID, 0, 900, 75, 45, 300, 60, keytrack=60)
    a("Acide boueux", ACID, 1, 160, 70, 60, 650, 65, drive=8)
    a("Cri du filtre", ACID, 0, 250, 96, 90, 420, 95, drive=8)
    a("Spaghetti", ACID, 0, 280, 82, 80, 1200, 70, glide=180)
    a("Corrosif", ACID, 0, 320, 85, 75, 350, 85, drive=18, dist=55)
    a("Chewing-gum", ACID, 1, 240, 88, 85, 900, 60, glide=150)
    a("Goutte", ACID, 0, 200, 94, 100, 170, 90, ampdecay=600)
    a("Acide nocturne", ACID, 0, 210, 75, 55, 800, 50, drive=4)
    a("Acide tribal", ACID, 1, 360, 72, 70, 240, 85, accthr=65)
    a("Acide liquide", ACID, 0, 260, 88, 60, 1600, 55, glide=200)
    a("Acide piquant", ACID, 0, 650, 70, 60, 160, 80, ampdecay=350)
    a("Acide sourd", ACID, 1, 140, 60, 50, 500, 50)
    a("Acide clair", ACID, 0, 1200, 65, 40, 350, 55, keytrack=50)
    a("Fouet", ACID, 0, 230, 90, 100, 150, 100, drive=6)
    a("Ressort", ACID, 0, 280, 93, 90, 220, 75, glide=90)
    a("Élastique", ACID, 1, 260, 86, 80, 600, 70, glide=240)
    a("Vrille", ACID, 0, 200, 95, 95, 500, 85, drive=12, dist=20)
    a("Sifflet de filtre", ACID, 0, 420, 98, 70, 350, 70)
    a("Acide saturé", ACID, 0, 330, 80, 70, 400, 75, drive=24)
    a("Acide distordu", ACID, 1, 300, 82, 72, 380, 80, drive=12, dist=80)
    a("Acide lent", ACID, 0, 240, 84, 65, 2200, 45, glide=260, ampdecay=3000)

    # --- Basses : résonance basse, plus de corps
    a("Basse carrée ronde", BASS, 1, 300, 25, 35, 700, 30)
    a("Basse sourde", BASS, 0, 180, 20, 30, 900, 25, ampdecay=2000)
    a("Basse électro", BASS, 0, 420, 45, 50, 260, 50, drive=8)
    a("Basse de rue", BASS, 1, 360, 35, 45, 350, 60, drive=10, dist=15)
    a("Basse claquée", BASS, 0, 500, 40, 60, 150, 70, ampdecay=300)
    a("Basse longue", BASS, 0, 260, 30, 25, 1800, 30, ampdecay=3500)
    a("Basse feutrée", BASS, 1, 200, 15, 20, 600, 20)
    a("Basse grave carrée", BASS, 1, 150, 30, 40, 450, 35, tune=-12)
    a("Basse pompe", BASS, 0, 320, 50, 65, 200, 80, ampdecay=450)
    a("Basse tendue", BASS, 0, 380, 55, 45, 300, 55, drive=14)

    # --- Leads : une octave plus haut
    a("Lead acide aigu", LEAD, 0, 600, 85, 70, 450, 70, tune=12, keytrack=60)
    a("Lead hurleur", LEAD, 0, 450, 92, 85, 600, 85, tune=12, drive=16, dist=40)
    a("Lead piqué", LEAD, 1, 800, 70, 55, 200, 60, tune=12, ampdecay=500)
    a("Lead carré glissé", LEAD, 1, 700, 75, 50, 700, 55, tune=12, glide=160)

    # --- Effets
    a("Bulles d'eau", FX, 0, 120, 99, 100, 160, 100, tune=12)
    a("Grésillement", FX, 0, 1500, 90, 30, 250, 60, drive=24, dist=100)
    a("Moteur", FX, 1, 90, 85, 20, 2500, 20, tune=-12, drive=20, dist=60, ampdecay=4000)
    a("Larsen", FX, 0, 2500, 100, 20, 2500, 40, keytrack=100, ampdecay=4000)
    a("Robot", FX, 1, 700, 95, 90, 150, 100, drive=10, dist=45, ampdecay=250)
    a("Zap", FX, 0, 3000, 90, 100, 150, 100, tune=12, ampdecay=200)
    return b
