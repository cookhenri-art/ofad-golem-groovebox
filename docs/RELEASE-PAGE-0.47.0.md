<p align="center">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/ui/public/golem/golem-512.png" width="200" alt="OFAD GOLEM">
</p>

<p align="center"><b>🇬🇧 English speakers: <a href="#english-version">full English version below ↓</a></b></p>


<h2 align="center">OFAD GOLEM — Groovebox 0.47.0</h2>

<p align="center"><b>Nouvelle version : un nouveau synthé (Glutamake), une sécurité renforcée, l'enregistrement et le montage audio au point, les MP3, et la tablette enfin stable.</b></p>

<p align="center">🆕 <b>Mise à jour du 5 novembre 2026</b> : un deuxième nouveau synthé, <b>Good Doctor</b>, <b>au moins 90 sons d'usine pour chaque synthé</b> (plus de 2 400), <b>au moins 20 réglages pour chaque effet</b>, le séquenceur de Pieuvre 8 revu, des menus de pas partout et des corrections. Mêmes noms de fichiers : retéléchargez-les.</p>

<p align="center">OFAD GOLEM, c'est une groovebox complète dans l'ordinateur ou la tablette, pensée pour <b>toutes les musiques électroniques</b> : techno, house, acid, hardtek et tribe, hardcore, drum & bass, électro, ambient, beatmaking…</p>

<p align="center">L'idée : réunir dans un seul outil <b>tout ce qui a fait ses preuves</b> sur les machines qu'on aime — à commencer par les <b>séquenceurs qu'on connaît tous</b> : pas à pas, verrous de réglages sur chaque pas, séquenceur acide (note, octave, accent, glissé), conditions (A:B, Fill…), répétitions, Fill et Break en direct.</p>

<p align="center">Le flux de travail des <b>stations de production autonomes à pads</b> (du type MPC One, Force ou Maschine) : 16 pistes, 16 pads, patterns lancés comme des clips, scènes au crossfader, morceau et montage audio. Les 36 instruments et 38 effets reprennent l'<b>architecture</b> des machines qui ont fait ces musiques — boîtes à rythmes des années 80 et 90, basse acide, synthés analogiques mono et poly, FM, tables d'ondes, drones, vocodeurs, échos à bande… — <b>recréés de zéro</b>, avec leurs propres noms, façades et sons. Logiciel libre (AGPL-3.0).</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/appareil.jpg" width="820" alt="L'appareil : écran, encodeurs, pads, transport">
</p>

### 🆕 Mise à jour du 5 novembre 2026

#### Nouveau synthé : Good Doctor
- Principe des grands synthés à tables d'ondes « hybrides » : **trois oscillateurs** à tables (12 tables de 12 ondes calculées, dont 4 pour les basses), position continue, **phase de départ** réglable.
- **Deux déformations à la suite par oscillateur** (18 au choix : synchro, courbures, pulsation, miroir, asymétrie, FM / AM / anneau, repli, saturation, bits, filtres…).
- **Sous-oscillateur** et **bruit coloré** ; **deux filtres** (14 types dont peigne, voyelles, diode) en série ou en parallèle ; chaque oscillateur vers les deux filtres, un seul, ou **en direct** ; distorsion par voix.
- 3 enveloppes, 2 LFO + 2 enveloppes et 3 LFO à pas, **matrice de 32 routages, 8 macros**, arpège ; poly, mono ou **unisson jusqu'à 8 voix** ; **91 sons d'usine** (dont 26 basses).

<p align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/good-doctor.jpg" width="820" alt="Good Doctor"><br><b>Good Doctor</b> · trois oscillateurs à tables, deux déformations chacun, deux filtres, sous-oscillateur et bruit</p>

#### Plus de sons
- **Au moins 90 sons d'usine pour chaque synthé** (plus de 2 400 en tout), chacun mesuré et mis au même niveau.
- **Au moins 20 réglages d'usine pour chacun des 38 effets** (plus de 770).

#### Séquenceurs
- **Pieuvre 8** : la dernière note jouée s'écrit sur le pas choisi ; on allonge une note en tirant le bord d'un pas ; un pas sélectionné ouvre son menu.
- **Menu du pas, en écriture Acide comme À verrous** : **clic droit** sur PC, **appui long** au doigt sur téléphone et tablette, sur la note d'un pas : note, octave, **Accent** et **Glissé** (en grand, avec leur force), vélocité, durée, décalage, condition, probabilité, répétitions — fini les petites cases au téléphone.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/menu-pas-acide.jpg" alt="Menu d'un pas en écriture Acide"><br><b>Menu du pas · écriture Acide</b><br>clic droit ou appui long sur la note : Accent et Glissé en grand, octave, note, durée, condition…</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/menu-pas-verrous.jpg" alt="Menu d'un pas en écriture À verrous"><br><b>Menu du pas · écriture À verrous</b><br>clic droit ou appui long sur le pas : note, vélocité, durée, décalage, condition, probabilité, répétitions</td>
</tr>
</table>

#### Corrections
- Une touche du clavier de la façade pouvait rester enfoncée quand le doigt glissait hors de la touche.
- Unisson large juste après le choix d'un son : une note pouvait ne plus s'arrêter.

### 🆕 Nouveautés de la 0.47.0

#### Nouveau synthé : Glutamake
- Deux oscillateurs à **tables d'ondes** (8 tables de 12 ondes calculées, mélange A/B, 12 « morphs » en temps réel, modificateurs), sous-oscillateur, bruit, filtre à 10 types.
- **Une bille qui roule sur une surface** : inclinaison, frottement, bosse (creux ou colline), bords qui absorbent ou relancent. On la lance au doigt, à chaque note ou par la séquence ; ses mouvements (X, Y, distance, angle, chocs) modulent n'importe quel réglage.
- **Séquence de mouvement** : 4 couloirs de 16 pas (boucle, sens, division, lissage, probabilité).
- Branché comme les autres synthés : séquenceur à pas, matrice de 32 routages, 4 boutons de modulation, LFO à pas, arpège, 16 boutons assignables, MIDI ; 90 sons d'usine.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake.jpg" alt="Glutamake"><br><b>Glutamake</b><br>la bille, les oscillateurs à tables, le filtre</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake-physique.jpg" alt="Glutamake : physique"><br><b>La physique de la bille</b><br>surface, bosse, bords, lancement</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake-sequence.jpg" alt="Glutamake : séquence de mouvement"><br><b>Séquence de mouvement</b><br>4 couloirs de 16 pas, sources de la matrice</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/mixeur-envois.jpg" alt="Mixeur et envois A à D"><br><b>Mixeur</b><br>envois A, B, C, D en boutons</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/enregistreur.jpg" alt="Enregistrement audio"><br><b>Enregistrement audio</b><br>calé sur la lecture, boucle parfaite, WAV ou MP3</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/montage-047.jpg" alt="Montage audio"><br><b>Montage audio</b><br>export de la boucle ou de tout le montage</td>
</tr>
</table>

#### Sécurité renforcée
- Projets, préréglages et fichiers reçus d'ailleurs contrôlés plus strictement à l'ouverture.
- Programme Windows compilé avec des protections supplémentaires ; outils de fabrication verrouillés sur des versions vérifiées.
- Chaque version est accompagnée d'un fichier **`SHA256SUMS.txt`** pour vérifier vos téléchargements (voir « Vérifier votre téléchargement » dans le README).

#### Échantillons et Bibliothèque
- Les **MP3** sont acceptés partout : pads, samplers, import, **Bibliothèque** (en plus des WAV, AIFF, FLAC, OGG).
- Sur tablette, tout reste dans l'application : un son pris par **« Échantillon… »** sur une piste est chargé tout de suite **et** rangé dans **Bibliothèque › Importés**.
- **Réglages › Dossier perso…** copie tous les sons d'un dossier du téléphone (sous-dossiers compris) dans **Bibliothèque › Perso** ; le refaire ajoute les nouveaux.
- Vos prises audio sont dans **Bibliothèque › « Mes enregistrements »**.

#### Enregistrement audio
- **Prise calée sur la lecture** : elle démarre avec la lecture et s'arrête pile, à l'échantillon près (fin du morceau, ARRÊT, ou fin du tour de boucle quand on rappuie sur « ■ Audio »).
- **Boucle parfaite** : un tour d'élan (échos et compression en place), puis exactement un tour ; la jonction fin → début est fondue dans le fichier : la boucle se rejoue sans clic, dans GOLEM ou un autre logiciel.
- **MP3 sans blanc** : le fichier indique son retard d'encodage, les logiciels qui le lisent n'ajoutent pas de silence.

#### Montage audio
- Export **de la boucle ou de tout le montage**, pile à la longueur (WAV 24 bits ou MP3 320 kbit/s).
- L'attaque du premier son n'est plus adoucie au début d'une prise ; plus de creux au bouclage d'une prise de boucle.
- Toucher le nom d'une voie choisit tous ses clips ; **Ctrl+A** ; tirer un bord règle **tous les clips choisis** d'un coup.

#### Séquenceurs et réglages des pas
- **Menu du pas** sur la façade, en écriture **Acide** comme **À verrous** : **clic droit** (PC) ou **appui long** (doigt) sur un pas : note, octave, **Accent** et **Glissé** en grand, vélocité, durée, décalage, condition, probabilité, répétitions.
- **Valeur d'un pas** : appui long (ou clic droit) sur **Acc.** ou **Gl.** règle la **force de l'accent** (1 à 100 %) et la **durée du glissé** (1 à 100 % du pas) de ce pas seulement ; sans valeur, le pas prend celle de la piste.
- **Verrous** : un pas choisi garde ses propres réglages (coupure, résonance, effets…) en plus de sa note ; la rangée à verrous écrit la dernière note jouée.
- **Pinceau** : appuyer sur un pas puis glisser sur ses voisins les pose (ou les retire) d'un seul geste (batteries, rangée à verrous, pads en mode Pas).
- **Pieuvre 8** : la note jouée s'écrit sur le pas choisi ; on allonge une note en tirant le bord d'un pas.

#### Mixeur et scènes
- **Envois A, B, C, D** en boutons, dans le mixeur et la vue live (pistes : à la place de l'égaliseur ; groupes : égaliseur et envois).
- **Appui long** (clic droit sur ordinateur) sur un envoi ou une valeur de scène : saisie de la **valeur exacte**.

#### Tablette et téléphone (Android)
- **Écran fixe, sans tremblement** : seuls les listes, le montage et les façades en grand défilent ; les menus des réglages gardent la même taille, avec une liste qui défile.
- Projets enregistrés et ouverts **dans l'application** (liste des projets) ; le sélecteur du système ne sert plus qu'à importer ou exporter une copie.
- Le clavier ne décale plus l'écran ; la saisie d'une valeur reste visible au-dessus du clavier.
- Le son n'est jamais coupé quand une liste ou un menu s'ouvre.
- Choisir un échantillon avec le sélecteur du système ne fait plus planter l'application ; les fichiers sont reconnus par leur vrai nom.
- Montage audio au doigt : glisser fait défiler les voies, toucher choisit un clip, on le tire pour le déplacer.

### ⬇️ Télécharger (en bas de cette page, « Assets »)
| Fichier | Pour quoi |
|---|---|
| **`OFAD-GOLEM-0.47.0-Windows-installateur.exe`** | **Windows** : application, plugin VST3 (au choix), notice, catalogue et sources |
| **`OFAD-GOLEM-0.47.0-Android.apk`** | **Android** : téléphones et tablettes |
| **`Notice-OFAD-GOLEM.pdf`** | Notice de prise en main et **raccourcis**, avec captures annotées |
| **`Notice-OFAD-GOLEM-EN.pdf`** | **Manual in English** (getting started, shortcuts, annotated screenshots) |
| **`Catalogue-OFAD-GOLEM.pdf`** | Les instruments et les 38 effets, avec leurs façades (Glutamake arrive dans la prochaine édition) |
| **`OFAD-GOLEM-0.47.0-sources.tar.gz`** | Les sources de cette version (AGPL-3.0) |
| **`SHA256SUMS.txt`** | Les empreintes de tous les fichiers, pour vérifier vos téléchargements |

> **Windows** : le programme n'est pas encore signé. Si Windows affiche « Windows a protégé votre ordinateur », cliquer sur « Informations complémentaires » puis « Exécuter quand même ».

> **📱 Version Android (APK) : maintenant pour téléphones et tablettes.** L'application s'adapte à l'écran du téléphone en paysage (onglets Commandes / Pads). Le son est réglé pour les mobiles : il tourne sur les cœurs rapides du processeur, et le micro reste fermé tant qu'on ne l'ouvre pas. Le compteur CPU · MÉM montre la charge en direct.
>
> **À savoir :** le projet de démonstration est volontairement chargé (10 instruments et 14 effets). Il tourne bien sur un téléphone récent. Sur une petite tablette d'entrée de gamme, il peut craquer : commencez alors par un projet vide ou quelques pistes.

### 🍎 Version Mac et iPhone / iPad (essai)

| Fichier | Pour quoi |
|---|---|
| **`OFAD-GOLEM-0.47.0-Mac-app.zip`** | **Mac** : l'application seule, sans logiciel de musique |
| **`OFAD-GOLEM-0.47.0-Mac-AU.zip`** | **Mac** : plugin AU (Logic Pro, GarageBand, Ableton Live…) |
| **`OFAD-GOLEM-0.47.0-Mac-VST3.zip`** | **Mac** : plugin VST3 (Ableton Live, Reaper, Bitwig, Studio One…) |
| **`OFAD-GOLEM-0.47.0-iOS-non-signe.ipa`** | **iPhone / iPad** (essai) : application non signée, à installer avec AltStore ou Sideloadly |

> **Version d'essai** : compilée pour les Mac à puce Apple et Intel (macOS 11 ou plus récent), mais **pas encore testée sur un vrai Mac**. Vos retours sont les bienvenus dans l'onglet [Issues](https://github.com/cookhenri-art/ofad-golem-groovebox/issues).
>
> **Installation**
> - **Application** : décompresser, puis glisser `Groovebox.app` dans **Applications**.
> - **AU** : copier `Groovebox.component` dans `~/Bibliothèque/Audio/Plug-Ins/Components`, puis relancer le logiciel de musique.
> - **VST3** : copier `Groovebox.vst3` dans `~/Bibliothèque/Audio/Plug-Ins/VST3`.
>
> **Premier lancement** : l'appli n'est pas encore signée par Apple. Faire **clic droit › Ouvrir**, ou aller dans **Réglages Système › Confidentialité et sécurité › Ouvrir quand même**. Si macOS dit que l'appli est « endommagée », taper dans le Terminal : `xattr -cr /Applications/Groovebox.app`

### 🎛️ En images

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/facade-acide.jpg" alt="Façade de la basse et séquenceur acide"><br><b>Façade du synthé et séquenceur acide</b><br>note, octave, accent, glissé pas par pas</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/pedalier.jpg" alt="Effets en pédales"><br><b>Les effets en pédales</b><br>4 effets par piste, depuis la façade</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/sequence-reperes.jpg" alt="Écran Séquence"><br><b>Séquence</b><br>rouleau de notes, accent, glissé, conditions, pages</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/scenes-reperes.jpg" alt="Scènes et crossfader"><br><b>Scènes A / B et crossfader</b><br>enchaîner les scènes sans coupure</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/morceau.jpg" alt="Morceau"><br><b>Morceau</b><br>rangées, mouvements enregistrés en courbes</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/montage-reperes.jpg" alt="Montage audio"><br><b>Montage audio</b><br>prises en clips, édition, export WAV / MP3</td>
</tr>
</table>

### 🎬 Vue live : le jeu en direct sur un seul écran

<p align="center">Crossfader ou <b>pavé vectoriel</b> à quatre scènes (formes en mouvement, taille, hauteur, rotation), lancement des rangées, console des 16 pistes avec faders, FX et M / S, et les <b>16 boutons assignables</b> avec leurs propres modulations (LFO, fonctions…).</p>

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/vue-live-crossfader.jpg" alt="Vue live : crossfader et console"><br><b>Vue live · crossfader</b><br>scènes A / B, rangées, console des 16 pistes</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/vue-live-vecteur.jpg" alt="Vue live : pavé vectoriel et console"><br><b>Vue live · pavé vectoriel</b><br>quatre scènes mélangées par une forme en mouvement</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/scenes-vecteur-pc.jpg" alt="Écran Scènes : pavé vectoriel"><br><b>Scènes · pavé vectoriel</b><br>formes, vitesse, taille, hauteur, rotation</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/modulations-boutons.jpg" alt="Modulations des 16 boutons"><br><b>Modulations des 16 boutons</b><br>4 sources propres aux boutons, indépendantes des synthés</td>
</tr>
</table>

### 📱 Sur téléphone et tablette, avec un contrôleur MIDI

<p align="center">Toute la groovebox tient dans un <b>téléphone ou une tablette Android</b>, avec la Vue live et les 16 boutons assignables : de quoi faire un live complet avec seulement le téléphone et un contrôleur.</p>

- **Contrôleur MIDI prêt à jouer, sans réglage** : branchez votre clavier ou contrôleur **USB** sur la tablette, le téléphone (câble ou hub USB-C « OTG », même en cours de jeu) **ou le PC**, il est reconnu tout seul. En Bluetooth sur Android : Réglages › « Appairer un contrôleur Bluetooth… ».
- **Tout s'apprend** : appui long (ou clic droit) sur un réglage, un M / S, le crossfader, une rangée › **Apprendre MIDI**, puis on touche la commande du contrôleur.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/mobile-scenes-crossfader.jpg" alt="Téléphone : écran Scènes, crossfader"><br><b>Téléphone · Scènes, crossfader</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/mobile-scenes-vecteur.jpg" alt="Téléphone : écran Scènes, pavé vectoriel"><br><b>Téléphone · Scènes, pavé vectoriel</b></td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/mobile-live-crossfader.jpg" alt="Téléphone : vue live, crossfader"><br><b>Téléphone · Vue live, crossfader</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/mobile-live-vecteur.jpg" alt="Téléphone : vue live, pavé vectoriel"><br><b>Téléphone · Vue live, pavé vectoriel</b></td>
</tr>
</table>

### 🎹 Les instruments (36 avec Glutamake et Good Doctor)

<p align="center">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/instruments-1.jpg" alt="Instruments : batteries et basses">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/instruments-2.jpg" alt="Instruments : polys, nappes, sampler">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/instruments-3.jpg" alt="Instruments : synthés FM et drones">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/instruments-4.jpg" alt="Instruments : vocodeurs, Pieuvre 8, entrée audio, Glutamake, Good Doctor">
</p>

### 🎚️ Les 38 effets

<p align="center">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/effets-1.jpg" alt="Effets : égaliseurs et compresseurs">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/effets-2.jpg" alt="Effets : saturations, échos, réverbérations">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/effets-3.jpg" alt="Effets : modulations et filtres">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/galerie/effets-4.jpg" alt="Effets : multi-effet et boucleur">
</p>

### ✨ Déjà dans la groovebox (depuis la 0.46)
- **Interface en anglais** (*English interface*) : langue choisie à l'installation (Windows), au premier lancement (Android) ou dans Réglages.
- **16 rangées (A à P)** en deux pages ; −1 / +1 passent d'une page à l'autre.
- **Vue live** : crossfader ou pavé vectoriel, rangées, console des 16 pistes (faders, FX, M / S), le tout sur un écran.
- **Pavé vectoriel** : quatre scènes mélangées par une forme en mouvement, avec taille, hauteur et rotation réglables (et au doigt : glisser, pincer, tourner).
- **16 boutons assignables avec leurs propres modulations** (4 sources : LFO, fonctions…), indépendantes de celles des synthés.
- **Contrôleurs MIDI USB prêts à jouer** sur tablette, téléphone et PC ; tout s'apprend en un appui.
- **Montage audio** : prise de chaque piste et de chaque retour, découpage en clips, couper, déplacer, dupliquer, fondus, export WAV / MP3.
- **Prise de boucle** : Boucle allumée, la prise s'arrête toute seule, pile à la fin de la boucle.
- **Séquenceurs** acide et à verrous sur les façades, liés à l'écran Séquence ; **Fill** et **Break** pour les relances en direct.
- **Notice illustrée** : une capture annotée sous chaque tableau de raccourcis.
- Mascotte **OFAD GOLEM**, écran de lancement et accueil des projets.

### 🤝 Un projet qui évolue avec vous
Si on s'y met tous, on peut créer notre groovebox idéale. Une machine qui vous manque, un effet, une fonction de séquenceur ? Essayez-la et dites-moi ce qui marche, ce qui coince, quel contrôleur vous utilisez… et surtout votre **liste de courses** des fonctions que vous aimeriez, dans l'onglet **[Issues](https://github.com/cookhenri-art/ofad-golem-groovebox/issues)** : vos idées font les prochaines versions.

<sub>Les noms de marques cités (MPC, Force, Maschine…) appartiennent à leurs propriétaires et servent seulement à décrire le style de jeu. OFAD GOLEM est un projet indépendant, sans lien avec ces fabricants.</sub>

---

<a id="english-version"></a>

## English version

<p align="center"><b>OFAD GOLEM — Groovebox 0.47.0 · a new synth (Glutamake), stronger security, polished audio recording and editing, MP3 files, and a stable tablet experience</b></p>

<p align="center">🆕 <b>Update of 5 November 2026</b>: a second new synth, <b>Good Doctor</b>, <b>at least 90 factory sounds for every synth</b> (over 2,400), <b>at least 20 settings for every effect</b>, a reworked Pieuvre 8 sequencer, step menus everywhere, and fixes. Same file names: download them again.</p>

<p align="center">A complete groovebox in your computer or tablet, made for <b>all electronic music</b>: techno, house, acid, hardtek and tribe, hardcore, drum & bass, electro, ambient, beatmaking…</p>

<p align="center">The idea: bring together in a single tool <b>everything that has proven itself</b> on the machines we love — starting with the <b>sequencers we all know</b>: step sequencing, parameter locks on every step, acid sequencer (note, octave, accent, slide), trig conditions (A:B, Fill…), retrigs, live Fill and Break.</p>

<p align="center">The workflow of <b>standalone pad-based production stations</b> (in the style of MPC One, Force or Maschine): 16 tracks, 16 pads, patterns launched like clips, scenes on a crossfader, song mode and audio editing. The 36 instruments and 38 effects follow the <b>architecture</b> of the machines that made this music — 80s and 90s drum machines, acid bass, analog mono and poly synths, FM, wavetables, drones, vocoders, tape echoes… — <b>rebuilt from scratch</b>, with their own names, panels and sounds. Free software (AGPL-3.0).</p>

<p align="center">The whole interface is in <b>English</b>: choose the language when installing on Windows, at the first launch on Android (phone and tablet), or at any time in Settings › Project (<b>Français / English</b>).</p>

<p align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/main-page.jpg" width="820" alt="OFAD GOLEM in English: main page with the launch grid, the 16 pads and the transport"></p>

### 🆕 Update of 5 November 2026

#### New synth: Good Doctor
- Based on the principle of large "hybrid" wavetable synths: **three wavetable oscillators** (12 computed tables of 12 waves, 4 of them for basses), continuous position, adjustable **start phase**.
- **Two warps in a row per oscillator** (18 to choose from: sync, bends, pulse, mirror, asymmetry, FM / AM / ring, fold, saturate, bits, filters…).
- **Sub oscillator** and **coloured noise**; **two filters** (14 types including comb, vowels, diode) in series or parallel; each oscillator to both filters, one, or **direct**; per-voice distortion.
- 3 envelopes, 2 LFOs + 2 envelopes and 3 step LFOs, **32-slot matrix, 8 macros**, arpeggiator; poly, mono or **unison up to 8 voices**; **91 factory sounds** (26 basses).

<p align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/good-doctor.jpg" width="820" alt="Good Doctor"><br><b>Good Doctor</b> · three wavetable oscillators with two warps each, two filters, sub oscillator and noise</p>

#### More sounds
- **At least 90 factory sounds for every synth** (over 2,400 in total), each one measured and levelled.
- **At least 20 factory settings for each of the 38 effects** (over 770).

#### Sequencers
- **Pieuvre 8**: the last note played is written to the selected step; drag a step's edge to lengthen a note; a selected step opens its menu.
- **Step menu, in Acid and Locks writing**: **right-click** on a computer, **long press** on a phone or tablet, on a step's note: note, octave, large **Accent** and **Slide** (with their amount), velocity, length, offset, condition, probability, repeats — no more tiny boxes on a phone.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/menu-pas-acide.jpg" alt="Step menu in Acid writing"><br><b>Step menu · Acid writing</b><br>right-click or long press on the note: large Accent and Slide, octave, note, length, condition…</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/menu-pas-verrous.jpg" alt="Step menu in Locks writing"><br><b>Step menu · Locks writing</b><br>right-click or long press on the step: note, velocity, length, offset, condition, probability, repeats</td>
</tr>
</table>

#### Fixes
- A key of the panel keyboard could stay down when the finger slid off it.
- Wide unison right after choosing a sound could leave a note hanging.

### 🆕 What's new in 0.47.0

#### New synth: Glutamake
- Two **wavetable** oscillators (8 computed tables of 12 waves, A/B blend, 12 real-time morphs, modifiers), sub-oscillator, noise, 10-type filter.
- **A ball rolling on a surface**: tilt, friction, a bump (well or hill), edges that absorb or bounce back. Throw it by hand, on each note or from the sequence; its motion (X, Y, distance, angle, hits) can modulate any setting.
- **Motion sequence**: 4 lanes of 16 steps (loop, direction, division, smoothing, probability).
- Wired like the other synths: step sequencer, 32-slot matrix, 4 mod knobs, step LFOs, arpeggiator, 16 assignable knobs, MIDI; 90 factory sounds.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake.jpg" alt="Glutamake"><br><b>Glutamake</b><br>the ball, wavetable oscillators, filter</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake-physique.jpg" alt="Glutamake physics"><br><b>Ball physics</b><br>surface, bump, edges, launch</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/glutamake-sequence.jpg" alt="Motion sequence"><br><b>Motion sequence</b><br>4 lanes of 16 steps, matrix sources</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/enregistreur.jpg" alt="Audio recording"><br><b>Audio recording</b><br>follow playback, perfect loop, WAV or MP3</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/mixeur-envois.jpg" alt="Mixer and sends A to D"><br><b>Mixer</b><br>sends A, B, C, D as knobs</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v047/montage-047.jpg" alt="Audio montage"><br><b>Audio montage</b><br>export the loop or the whole montage</td>
</tr>
</table>

#### Stronger security
- Projects, presets and files received from elsewhere are checked more strictly when opened.
- The Windows program is built with additional protections; build tools are pinned to verified versions.
- Every release ships a **`SHA256SUMS.txt`** file to verify your downloads (see "Verify your download" in the README).

#### Samples and Library
- **MP3** files are accepted everywhere: pads, samplers, import, **Library** (as well as WAV, AIFF, FLAC, OGG).
- On tablets everything stays inside the app: a sound picked with **"Sample…"** on a track is loaded at once **and** stored in **Library › Importés**.
- **Settings › Personal folder…** copies every sound of a phone folder (subfolders included) into **Library › Perso**; doing it again adds the new ones.
- Your audio takes are in **Library › "Mes enregistrements"** (my recordings).

#### Audio recording
- **Locked to playback**: the take starts with playback and stops exactly, to the sample (end of the song, STOP, or end of the loop when you press "■ Audio" again).
- **Perfect loop**: one run-up loop (echoes and compression settle), then exactly one loop; the end-to-start join is crossfaded in the file, so the loop replays without a click, in GOLEM or any other software.
- **Gapless MP3**: the file carries its encoder delay, so players that read it add no silence.

#### Audio montage
- Export **the loop or the whole montage**, exactly to length (24-bit WAV or 320 kbit/s MP3).
- Takes keep their first attack; loop takes no longer dip at the loop point.
- Tap a lane name to select all its clips; **Ctrl+A**; dragging an edge resizes **all selected clips** at once.

#### Sequencers and step settings
- **Step menu** on the panel, in **Acid** and **Locks** writing: **right-click** (computer) or **long press** (touch) on a step: note, octave, large **Accent** and **Slide**, velocity, length, offset, condition, probability, repeats.
- **Step amount**: long press (or right-click) on **Acc.** or **Sl.** sets the **accent strength** (1 to 100 %) and the **slide time** (1 to 100 % of the step) of that step only; without a value, the step uses the track's.
- **Locks**: a selected step keeps its own settings (cutoff, resonance, effects…) on top of its note; the Locks row writes the last note played.
- **Paint**: press a step and slide across its neighbours to set (or clear) them in one gesture (drum machines, Locks row, pads in Step mode).
- **Pieuvre 8**: the note played is written to the selected step; drag a step's edge to lengthen a note.

#### Mixer and scenes
- **Sends A, B, C, D** as knobs, in the mixer and the live view (tracks: in place of the EQ; groups: EQ and sends).
- **Long press** (right-click on a computer) on a send or a scene value: **exact value** entry.

#### Tablet and phone (Android)
- **Fixed screen, no more jitter**: only lists, the montage and enlarged panels scroll; settings menus keep the same size, with a scrolling list.
- Projects saved and opened **inside the app** (project list); the system picker is only used to import or export a copy.
- The on-screen keyboard no longer shifts the screen; value entry stays visible above the keyboard.
- Sound never stops when a list or menu opens.
- Picking a sample with the system file picker no longer crashes the app; files are recognised by their real name.
- Audio montage by touch: swipe to scroll the lanes, tap a clip to select it, then drag it to move it.

### ⬇️ Download (at the bottom of this page, “Assets”)
| File | What for |
|---|---|
| **`OFAD-GOLEM-0.47.0-Windows-installateur.exe`** | **Windows**: application, VST3 plugin (optional), manual, catalogue and sources |
| **`OFAD-GOLEM-0.47.0-Android.apk`** | **Android**: phones and tablets |
| **`Notice-OFAD-GOLEM-EN.pdf`** | **Manual in English**: getting started and **shortcuts**, with annotated screenshots |
| **`Notice-OFAD-GOLEM.pdf`** | The same manual in French |
| **`Catalogue-OFAD-GOLEM.pdf`** | The instruments and 38 effects, with their panels (Glutamake comes in the next edition) |
| **`OFAD-GOLEM-0.47.0-sources.tar.gz`** | Source code of this release (AGPL-3.0) |
| **`SHA256SUMS.txt`** | Checksums of every file, to verify your downloads |

> **Windows**: the program is not signed yet. If Windows shows “Windows protected your PC”, click “More info”, then “Run anyway”.

> **📱 Android (APK): phones and tablets.** The app adapts to a phone screen in landscape (Controls / Pads tabs). Audio is tuned for mobile: it runs on the fast processor cores, and the microphone stays closed until you open it. The CPU · MEM meter shows the load live.
>
> **Good to know:** the demo project is deliberately heavy (10 instruments and 14 effects). It runs well on a recent phone. On a small entry-level tablet it may crackle: start with an empty project or a few tracks.

### 🍎 Mac and iPhone / iPad (preview)

| File | What for |
|---|---|
| **`OFAD-GOLEM-0.47.0-Mac-app.zip`** | **Mac**: the standalone app, no music software needed |
| **`OFAD-GOLEM-0.47.0-Mac-AU.zip`** | **Mac**: AU plugin (Logic Pro, GarageBand, Ableton Live…) |
| **`OFAD-GOLEM-0.47.0-Mac-VST3.zip`** | **Mac**: VST3 plugin (Ableton Live, Reaper, Bitwig, Studio One…) |
| **`OFAD-GOLEM-0.47.0-iOS-non-signe.ipa`** | **iPhone / iPad** (preview): unsigned app, install with AltStore or Sideloadly |

> **Preview**: built for Apple Silicon and Intel Macs (macOS 11 or later), but **not tested on a real Mac yet**. Feedback is welcome in the [Issues](https://github.com/cookhenri-art/ofad-golem-groovebox/issues) tab.
>
> **Install**
> - **App**: unzip, then drag `Groovebox.app` into **Applications**.
> - **AU**: copy `Groovebox.component` to `~/Library/Audio/Plug-Ins/Components`, then restart your music software.
> - **VST3**: copy `Groovebox.vst3` to `~/Library/Audio/Plug-Ins/VST3`.
>
> **First launch**: the app is not signed by Apple yet. **Right-click › Open**, or go to **System Settings › Privacy & Security › Open Anyway**. If macOS says the app is "damaged", type in Terminal: `xattr -cr /Applications/Groovebox.app`

### 🎛️ Screenshots

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/synth-panel.jpg" alt="Synth panel"><br><b>Synth panel</b><br>every instrument has its own front panel</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/sequence.jpg" alt="Sequence screen"><br><b>Sequence</b><br>piano roll, accent, slide, conditions, pages</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/pedalier.jpg" alt="Effects as pedals"><br><b>Effects as pedals</b><br>4 effects per track, from the panel</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/montage-reperes.jpg" alt="Audio editing"><br><b>Audio editing</b><br>takes as clips, editing, WAV / MP3 export</td>
</tr>
</table>

### 🎬 Live view: playing live on one screen

<p align="center">Crossfader or <b>vector pad</b> with four scenes (moving shapes, size, height, rotation), row launching, a console of the 16 tracks with faders, FX and M / S, and the <b>16 assignable knobs</b> with their own modulations (LFOs, functions…).</p>

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/live-view-crossfader.jpg" alt="Live view, crossfader"><br><b>Live view · Crossfader</b><br>A / B scenes, rows, console of the 16 tracks</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/live-view-vector.jpg" alt="Live view, vector pad"><br><b>Live view · Vector</b><br>four scenes blended by a moving shape</td>
</tr>
</table>

### 📱 On phone and tablet, with a MIDI controller

<p align="center">The whole groovebox fits in an <b>Android phone or tablet</b>, with the Live view and the 16 assignable knobs: enough for a full live set with just a phone and a controller.</p>

- **MIDI controller ready to play, no setup**: plug your **USB** keyboard or controller into the tablet, the phone (USB-C “OTG” cable or hub, even while playing) **or the PC**, it is detected automatically. Bluetooth on Android: Settings › “Pair a Bluetooth controller…”.
- **Everything can be learned**: long press (or right-click) a control, an M / S, the crossfader, a row › **Learn MIDI**, then touch the control on your controller.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/phone-controls.jpg" alt="Phone: Controls tab"><br><b>Phone · Controls</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/phone-pads.jpg" alt="Phone: Pads tab"><br><b>Phone · Pads</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/en/phone-live-view.jpg" alt="Phone: live view"><br><b>Phone · Live view</b></td>
</tr>
</table>

### 🎹 36 instruments and 🎚️ 38 effects

<p align="center">Drum machines, acid bass, analog mono and poly synths, pads, sampler, FM and drone synths, vocoders and audio input; EQs and compressors, saturations, echoes and reverbs, modulations and filters, multi-effect and looper. All of them are shown in the gallery above and in <b>Catalogue-OFAD-GOLEM.pdf</b>.</p>

### ✨ Already in the groovebox (since 0.46)
- **English interface**: language chosen at install (Windows), at first launch (Android) or in Settings.
- **16 rows (A to P)** on two pages; −1 / +1 move from one page to the other.
- **Live view**: crossfader or vector pad, rows, console of the 16 tracks (faders, FX, M / S), all on one screen.
- **Vector pad**: four scenes blended by a moving shape, with adjustable size, height and rotation (and by finger: drag, pinch, rotate).
- **16 assignable knobs with their own modulations** (4 sources: LFOs, functions…), independent from the synths' ones. An assigned knob follows its parameter both ways.
- **USB MIDI controllers ready to play** on tablet, phone and PC; everything is learned with one press.
- **Audio editing**: record every track and every return, split into clips, cut, move, duplicate, fades, WAV / MP3 export.
- **Loop recording**: with Loop on, recording stops by itself, exactly at the end of the loop.
- **Acid and parameter-lock sequencers** on the panels, linked to the Sequence screen; **Fill** and **Break** for live transitions.
- **Illustrated manual**: an annotated screenshot under every shortcut table.
- **OFAD GOLEM** mascot, splash screen and project home.

### 🤝 A project that grows with you
If we all get involved, we can build our ideal groovebox. A machine you miss, an effect, a sequencer feature? Try it and tell me what works, what doesn't, which controller you use… and above all your **wish list** of features, in the **[Issues](https://github.com/cookhenri-art/ofad-golem-groovebox/issues)** tab (English or French): your ideas shape the next versions.

<sub>The brand names mentioned (MPC, Force, Maschine…) belong to their owners and are only used to describe the playing style. OFAD GOLEM is an independent project, not affiliated with these manufacturers.</sub>


<p align="center">Made with ❤️ by <b>One For All Distribution (OFAD)</b></p>

