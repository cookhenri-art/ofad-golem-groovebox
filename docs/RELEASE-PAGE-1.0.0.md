<p align="center">
  <img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/ui/public/golem/golem-512.png" width="200" alt="OFAD GOLEM">
</p>

<p align="center"><b>🇬🇧 English speakers: <a href="#english-version">full English version below ↓</a></b></p>


<h2 align="center">OFAD GOLEM — Groovebox 1.0.0</h2>

<p align="center"><b>La version 1.0 ! Et un nouvel instrument pas comme les autres : le <u>FM Player</u>, qui fait tourner dans GOLEM les firmwares du boîtier de synthèse M-VAVE FM-1.</b></p>

<p align="center">OFAD GOLEM, c'est une groovebox complète dans l'ordinateur, la tablette ou le téléphone, pensée pour <b>toutes les musiques électroniques</b> : techno, house, acid, hardtek et tribe, hardcore, drum & bass, électro, ambient, beatmaking… 37 instruments, 38 effets, les séquenceurs qu'on connaît tous (pas à pas, verrous, acide, conditions), 16 pistes, 16 pads, scènes, morceau et montage audio. Logiciel libre (AGPL-3.0).</p>

<p align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-ambre.jpg" width="820" alt="FM Player : la machine, l'écran de sloopDX, T1 à T4 et la rangée Acide sous le piano"></p>

### 🎛️ En vedette : le FM Player — les firmwares du FM-1 dans GOLEM

Le FM-1 de M-VAVE est un petit synthé FM dont la communauté a écrit des dizaines de **firmwares** : groovebox à boucles, synthé FM à 6 opérateurs, polys analogiques, orgue à tirettes, boîte à échantillons 12 bits, piano électrique… Le **FM Player** **émule le processeur du boîtier** : chaque firmware (fichier `.fwsc`) devient un instrument de GOLEM, avec **son écran en direct et toutes ses commandes**, joué par le séquenceur, le piano, les pads et n'importe quel contrôleur MIDI.

- **15 firmwares libres fournis**, prêts à jouer dans **Bibliothèque › Échantillons › Firmwares** : Felucca, sloopDX, SLOOP, Melodee, JIANT FM, X0X, FIZZ, MYCELIUM, GR!MM, FM-106, OberVave, FMini-1, Frodhes, B1, zp12. **Firmware…** charge n'importe quel autre `.fwsc`.
- **Quatre pistes T1 à T4**, comme sur Pieuvre 8 : chacune joue sur son canal MIDI, pour faire sonner les quatre pistes des firmwares multipistes (SLOOP, sloopDX, JIANT…). On choisit la T sous le piano, on l'écrit en **Acide** ou **À verrous**, ou dans l'écran **SÉQUENCE** (4 lignes, « T en notes ♪ »).
- **Mode Grand** pour le téléphone et la tablette : l'écran du boîtier, les boutons à la taille du doigt et le piano de GOLEM sur toute la zone de l'écran. Les boutons se règlent aussi **en tournant**. Avec **M / S** au-dessus de T1 à T4, la **rangée de pas** de la T choisie (verrous, mesures, décalage), Copier / Coller / Effacer sous l'écran et le bouton **Bibliothèque** pour changer de firmware.
- **Quatre fonds** façon pédales, au choix d'une pastille : **Ambre**, **Nuit**, **Émeraude**, **Sobre**.
- **Les 8 commandes du boîtier au contrôleur** : Knob 1 à 4, Select, Presets, Algorithm et Master se pilotent comme tous les réglages de GOLEM : **appui long** (ou clic droit) pour les affecter au contrôleur MIDI, et elles tombent sous les **8 encodeurs** (section Boîtier). Les **14 touches** du boîtier (FX, SCL, ENV… PLAY, REC, OCT − / +) s'apprennent aussi : une touche du contrôleur appuie sur celle du boîtier.
- **Des boutons qui vont de 0 à 100 %** : chaque bouton du boîtier a sa **course** (crans du firmware pour aller du minimum au maximum, réglable dans le panneau Firmware) ; à 0 % et à 100 %, GOLEM recale le firmware sur ses butées.
- **N'importe quel contrôleur** : GOLEM envoie au firmware les notes sur le bon canal, son **horloge MIDI** (les séquenceurs des firmwares suivent le tempo) et les commandes non apprises du contrôleur (par exemple un Behringer X-Touch Mini ou un M-VAVE SMC-Mixer).
- **Partout** : Windows, Mac, Android (téléphone et tablette), et **en greffon VST3 / AU dans votre logiciel de musique**.
- Économe : l'émulation se met **en pause** après 20 s sans rien jouer.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-grand-telephone.jpg" alt="Mode Grand au téléphone, fond Nuit"><br><b>Mode Grand · téléphone</b><br>écran, boutons, piano et T1 à T4 (fond Nuit)</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-sequence-t1-t4.jpg" alt="Écran Séquence : 4 lignes T1 à T4"><br><b>Séquence · T1 à T4</b><br>une ligne par canal MIDI du firmware</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-nuit.jpg" alt="FM Player, fond Nuit, JIANT FM"><br><b>JIANT FM · fond Nuit</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-emeraude.jpg" alt="FM Player, fond Émeraude, zp12"><br><b>zp12 · fond Émeraude</b></td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-pistes-t1-t4.jpg" alt="Rangée Acide de la piste T2 sous le piano"><br><b>T1 à T4 sous le piano</b><br>rangées Acide et À verrous de la T choisie</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-grand-telephone-emeraude.jpg" alt="Mode Grand, Felucca, fond Émeraude"><br><b>Felucca · mode Grand</b></td>
</tr>
</table>

**Les firmwares fournis et leurs auteurs** — un grand merci à eux : Felucca (hugelton), sloopDX, OberVave et zp12 (zvenson), SLOOP (isod89), Melodee (keremimo), JIANT FM (juanjiant-bit), X0X (charlesvestal), FIZZ (Quixotic7), MYCELIUM (Mandalars), GR!MM (GRiMMBIENT), FM-106 (PatttF), FMini-1 (Ruppert), Frodhes (Leonardo Mirabal), B1 (Joppe Sikken / SikkenWorks). Licences GPL-3.0 ou MPL-2.0 ; leurs sources sont jointes à cette version (**`OFAD-GOLEM-1.0.0-firmwares-sources.tar.gz`**) et listées dans [FIRMWARES-LICENCES.md](https://github.com/cookhenri-art/ofad-golem-groovebox/blob/main/ui/public/firmwares/FIRMWARES-LICENCES.md). L'émulateur repose sur **fm1-emu** de Joakim Eriksson (licence MIT).

#### Aussi dans la 1.0.0
- **Un canal MIDI par T** (Pieuvre 8, FM Player) : chaque T écoute son propre canal, pour jouer plusieurs T depuis un clavier ou un séquenceur externe. Le clavier MIDI joue aussi **sur toutes ses octaves**.
- **Verrous glissés ou coupés** : sur une ligne qui a des verrous, **Glissés** fait voyager les réglages d'un verrou au suivant au lieu de sauter, et **Couper** les met en sourdine sans les effacer, pour les rallumer d'un geste en jouant.
- **Messages toujours visibles** : « copié », « collé », « effacé »… s'affichent par-dessus les façades des synthés.
- **Synthés sur téléphone** : Bretzel affiche ses deux oscillateurs et ses deux filtres l'un sous l'autre ; le détail d'un pas à verrous s'affiche à chaque fois.
- **Pads** : les pas allumés prennent la couleur de leur ligne ; le **crossfader** se tire de nouveau au doigt.
- **Android** : si l'application se ferme au lancement, elle redémarre la fois suivante **sans rouvrir la séance** (gardée de côté, jamais effacée) et affiche ce qui s'est passé.
- Toutes les nouveautés de la 0.47 (Good Doctor, Glutamake, plus de 2 400 sons d'usine, menus des pas, durée des notes visible, MP3, enregistrement et montage audio…) : voir la [page de la 0.47.0](https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/v0.47.0).

<sub>M-VAVE et FM-1 sont des marques de leurs propriétaires respectifs. OFAD GOLEM n'est ni affilié, ni approuvé, ni sponsorisé par M-VAVE ; ces noms sont cités uniquement pour indiquer la compatibilité avec les firmwares écrits pour le FM-1. Aucun logiciel d'origine de M-VAVE n'est fourni : les firmwares inclus sont des créations libres de la communauté, avec leurs auteurs et leurs sources. Behringer et X-Touch sont des marques de leur propriétaire, citées seulement comme exemple de contrôleur compatible.</sub>

### ⬇️ Télécharger (en bas de cette page, « Assets »)
| Fichier | Pour quoi |
|---|---|
| **`OFAD-GOLEM-1.0.0-Windows-installateur.exe`** | **Windows** : application, plugin VST3 (au choix), notice, catalogue et sources |
| **`OFAD-GOLEM-1.0.0-Android.apk`** | **Android** : téléphones et tablettes |
| **`Notice-OFAD-GOLEM.pdf`** | Notice de prise en main et **raccourcis**, avec captures annotées |
| **`Notice-OFAD-GOLEM-EN.pdf`** | **Manual in English** (getting started, shortcuts, annotated screenshots) |
| **`Catalogue-OFAD-GOLEM.pdf`** | Les instruments et les 38 effets, avec leurs façades  |
| **`OFAD-GOLEM-1.0.0-sources.tar.gz`** | Les sources de cette version (AGPL-3.0) |
| **`OFAD-GOLEM-1.0.0-firmwares-sources.tar.gz`** | Les sources des 15 firmwares fournis (GPL-3.0 / MPL-2.0) |
| **`SHA256SUMS.txt`** | Les empreintes de tous les fichiers, pour vérifier vos téléchargements |

> **Windows** : le programme n'est pas encore signé. Si Windows affiche « Windows a protégé votre ordinateur », cliquer sur « Informations complémentaires » puis « Exécuter quand même ».

> **📱 Version Android (APK) : maintenant pour téléphones et tablettes.** L'application s'adapte à l'écran du téléphone en paysage (onglets Commandes / Pads). Le son est réglé pour les mobiles : il tourne sur les cœurs rapides du processeur, et le micro reste fermé tant qu'on ne l'ouvre pas. Le compteur CPU · MÉM montre la charge en direct.
>
> **À savoir :** le projet de démonstration est volontairement chargé (10 instruments et 14 effets). Il tourne bien sur un téléphone récent. Sur une petite tablette d'entrée de gamme, il peut craquer : commencez alors par un projet vide ou quelques pistes.

### 🍎 Version Mac et iPhone / iPad (essai)

| Fichier | Pour quoi |
|---|---|
| **`OFAD-GOLEM-1.0.0-Mac-app.zip`** | **Mac** : l'application seule, sans logiciel de musique |
| **`OFAD-GOLEM-1.0.0-Mac-AU.zip`** | **Mac** : plugin AU (Logic Pro, GarageBand, Ableton Live…) |
| **`OFAD-GOLEM-1.0.0-Mac-VST3.zip`** | **Mac** : plugin VST3 (Ableton Live, Reaper, Bitwig, Studio One…) |
| **`OFAD-GOLEM-1.0.0-iOS-non-signe.ipa`** | **iPhone / iPad** (essai) : application non signée, à installer avec AltStore ou Sideloadly |

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

### 🎹 Les instruments (37 avec le FM Player)

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

<sub>Les noms de marques cités (MPC, Force, Maschine, M-VAVE, FM-1, Behringer…) appartiennent à leurs propriétaires et servent seulement à décrire le style de jeu. OFAD GOLEM est un projet indépendant, sans lien avec ces fabricants.</sub>

---

<a id="english-version"></a>

## English version

<p align="center"><b>OFAD GOLEM — Groovebox 1.0.0 · version 1.0, with a one-of-a-kind new instrument: the <u>FM Player</u>, which runs the firmwares of the M-VAVE FM-1 synth box inside GOLEM</b></p>

<p align="center">A complete groovebox in your computer, tablet or phone, made for <b>all electronic music</b>: techno, house, acid, hardtek and tribe, hardcore, drum & bass, electro, ambient, beatmaking… 37 instruments, 38 effects, the sequencers we all know (step, parameter locks, acid, conditions), 16 tracks, 16 pads, scenes, song mode and audio editing. Free software (AGPL-3.0).</p>

<p align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-ambre.jpg" width="820" alt="FM Player: the machine, the sloopDX screen, T1 to T4 and the Acid row under the piano"></p>

### 🎛️ Spotlight: the FM Player — FM-1 firmwares inside GOLEM

M-VAVE's FM-1 is a small FM synth for which the community has written dozens of **firmwares**: loop grooveboxes, a 6-operator FM synth, analog-style polys, a drawbar organ, a 12-bit sampler, an electric piano… The **FM Player** **emulates the box's processor**: every firmware (`.fwsc` file) becomes a GOLEM instrument, with **its live screen and all its controls**, played by the sequencer, the piano, the pads and any MIDI controller.

- **15 free firmwares included**, ready to play in **Library › Samples › Firmwares**: Felucca, sloopDX, SLOOP, Melodee, JIANT FM, X0X, FIZZ, MYCELIUM, GR!MM, FM-106, OberVave, FMini-1, Frodhes, B1, zp12. **Firmware…** loads any other `.fwsc`.
- **Four tracks T1 to T4**, as on Pieuvre 8: each one plays on its own MIDI channel, so the four tracks of multi-track firmwares (SLOOP, sloopDX, JIANT…) all sound. Pick the T under the piano, write it in **Acid** or **Locks**, or in the **SEQUENCE** screen (4 lines, "T in notes ♪").
- **Big mode** for phones and tablets: the box's screen, finger-sized knobs and GOLEM's piano on the whole screen. Knobs can also be **turned**. With **M / S** above T1 to T4, the chosen T's **step row** (locks, bars, shift), Copy / Paste / Clear under the screen and a **Library** button to switch firmware.
- **Four backgrounds** in the style of the pedals, one dot away: **Amber**, **Night**, **Emerald**, **Plain**.
- **The box's 8 controls on your controller**: Knob 1 to 4, Select, Presets, Algorithm and Master work like every GOLEM setting: **long press** (or right-click) to assign them to a MIDI controller, and they sit under the **8 encoders** (Box section). The box's **14 keys** (FX, SCL, ENV… PLAY, REC, OCT − / +) can be learned too: a controller button presses the box key.
- **Knobs from 0 to 100 %**: each box knob has its **travel** (firmware clicks from minimum to maximum, set in the Firmware panel); at 0 % and 100 %, GOLEM re-aligns the firmware on its end stops.
- **Any controller**: GOLEM sends the firmware the notes on the right channel, its **MIDI clock** (the firmwares' sequencers follow the tempo) and the controller's unlearned controls (for example a Behringer X-Touch Mini or an M-VAVE SMC-Mixer).
- **Everywhere**: Windows, Mac, Android (phone and tablet), and **as a VST3 / AU plugin in your music software**.
- Light on the battery: the emulation **pauses** after 20 s without playing.

<table>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-grand-telephone.jpg" alt="Big mode on a phone, Night background"><br><b>Big mode · phone</b><br>screen, knobs, piano and T1 to T4 (Night)</td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-sequence-t1-t4.jpg" alt="Sequence screen: 4 lines T1 to T4"><br><b>Sequence · T1 to T4</b><br>one line per firmware MIDI channel</td>
</tr>
<tr>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-nuit.jpg" alt="FM Player, Night background, JIANT FM"><br><b>JIANT FM · Night</b></td>
<td align="center"><img src="https://raw.githubusercontent.com/cookhenri-art/ofad-golem-groovebox/main/docs/img/v100/fm-player-emeraude.jpg" alt="FM Player, Emerald background, zp12"><br><b>zp12 · Emerald</b></td>
</tr>
</table>

**The bundled firmwares and their authors** — many thanks to them: Felucca (hugelton), sloopDX, OberVave and zp12 (zvenson), SLOOP (isod89), Melodee (keremimo), JIANT FM (juanjiant-bit), X0X (charlesvestal), FIZZ (Quixotic7), MYCELIUM (Mandalars), GR!MM (GRiMMBIENT), FM-106 (PatttF), FMini-1 (Ruppert), Frodhes (Leonardo Mirabal), B1 (Joppe Sikken / SikkenWorks). GPL-3.0 or MPL-2.0 licences; their sources ship with this release (**`OFAD-GOLEM-1.0.0-firmwares-sources.tar.gz`**) and are listed in [FIRMWARES-LICENCES.md](https://github.com/cookhenri-art/ofad-golem-groovebox/blob/main/ui/public/firmwares/FIRMWARES-LICENCES.md). The emulator is based on **fm1-emu** by Joakim Eriksson (MIT licence).

#### Also in 1.0.0
- **One MIDI channel per T** (Pieuvre 8, FM Player): each T listens to its own channel, to play several Ts from a keyboard or an external sequencer. The MIDI keyboard also plays **across all its octaves**.
- **Gliding or muted locks**: on a line with locks, **Glide** makes settings travel from one lock to the next instead of jumping, and **Mute** silences them without deleting them, to bring them back with one touch while playing.
- **Messages always visible**: "copied", "pasted", "cleared"… now show on top of the synth faces.
- **Synths on phones**: Bretzel shows its two oscillators and two filters one above the other; a lock step's details always show.
- **Pads**: lit steps take their line's colour; the **crossfader** can be dragged by finger again.
- **Android**: if the app closes at launch, it restarts the next time **without reopening the session** (kept aside, never deleted) and shows what happened.
- Everything new in 0.47 (Good Doctor, Glutamake, over 2,400 factory sounds, step menus, visible note lengths, MP3, audio recording and editing…): see the [0.47.0 page](https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/v0.47.0).

<sub>M-VAVE and FM-1 are trademarks of their respective owners. OFAD GOLEM is not affiliated with, endorsed or sponsored by M-VAVE; these names are used only to state compatibility with firmware written for the FM-1. No original M-VAVE software is included: the bundled firmwares are free community works, with their authors and sources credited. Behringer and X-Touch are trademarks of their owner, mentioned only as an example of a compatible controller.</sub>

### ⬇️ Download (at the bottom of this page, “Assets”)
| File | What for |
|---|---|
| **`OFAD-GOLEM-1.0.0-Windows-installateur.exe`** | **Windows**: application, VST3 plugin (optional), manual, catalogue and sources |
| **`OFAD-GOLEM-1.0.0-Android.apk`** | **Android**: phones and tablets |
| **`Notice-OFAD-GOLEM-EN.pdf`** | **Manual in English**: getting started and **shortcuts**, with annotated screenshots |
| **`Notice-OFAD-GOLEM.pdf`** | The same manual in French |
| **`Catalogue-OFAD-GOLEM.pdf`** | The instruments and 38 effects, with their panels  |
| **`OFAD-GOLEM-1.0.0-sources.tar.gz`** | Source code of this release (AGPL-3.0) |
| **`OFAD-GOLEM-1.0.0-firmwares-sources.tar.gz`** | Sources of the 15 bundled firmwares (GPL-3.0 / MPL-2.0) |
| **`SHA256SUMS.txt`** | Checksums of every file, to verify your downloads |

> **Windows**: the program is not signed yet. If Windows shows “Windows protected your PC”, click “More info”, then “Run anyway”.

> **📱 Android (APK): phones and tablets.** The app adapts to a phone screen in landscape (Controls / Pads tabs). Audio is tuned for mobile: it runs on the fast processor cores, and the microphone stays closed until you open it. The CPU · MEM meter shows the load live.
>
> **Good to know:** the demo project is deliberately heavy (10 instruments and 14 effects). It runs well on a recent phone. On a small entry-level tablet it may crackle: start with an empty project or a few tracks.

### 🍎 Mac and iPhone / iPad (preview)

| File | What for |
|---|---|
| **`OFAD-GOLEM-1.0.0-Mac-app.zip`** | **Mac**: the standalone app, no music software needed |
| **`OFAD-GOLEM-1.0.0-Mac-AU.zip`** | **Mac**: AU plugin (Logic Pro, GarageBand, Ableton Live…) |
| **`OFAD-GOLEM-1.0.0-Mac-VST3.zip`** | **Mac**: VST3 plugin (Ableton Live, Reaper, Bitwig, Studio One…) |
| **`OFAD-GOLEM-1.0.0-iOS-non-signe.ipa`** | **iPhone / iPad** (preview): unsigned app, install with AltStore or Sideloadly |

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

### 🎹 37 instruments and 🎚️ 38 effects

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

<sub>The brand names mentioned (MPC, Force, Maschine, M-VAVE, FM-1, Behringer…) belong to their owners and are only used to describe the playing style. OFAD GOLEM is an independent project, not affiliated with these manufacturers.</sub>


<p align="center">Made with ❤️ by <b>One For All Distribution (OFAD)</b></p>

