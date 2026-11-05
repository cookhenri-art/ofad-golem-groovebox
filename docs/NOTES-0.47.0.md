# OFAD GOLEM 0.47.0

*English below.*

Une grosse version : un nouveau synthé (**Glutamake**), une sécurité renforcée, l'enregistrement audio calé
à l'échantillon près, le montage audio plus complet, les **MP3** acceptés partout, et la tablette Android
enfin stable (écran fixe, menus réguliers, sons et projets gardés dans l'application).

## Mise à jour du 5 novembre 2026

Même version 0.47.0, enrichie : un nouveau synthé (**Good Doctor**), **au moins 90 sons d'usine pour chaque
synthé** (plus de 2 400 en tout), **au moins 20 réglages d'usine pour chaque effet** (plus de 770), le
séquenceur de **Pieuvre 8** qui se joue comme celui des autres synthés, des menus de pas partout et des
corrections. Les projets s'ouvrent sans changement ; l'APK s'installe par-dessus la précédente.

### Nouveau synthé : Good Doctor
- Principe des grands synthés à tables d'ondes « hybrides » : **trois oscillateurs** à tables (12 tables de
  12 ondes calculées, dont 4 pensées pour les basses : scie qui gronde, synchro balayée, carré qui se pince,
  sinus sali), position continue.
- **Deux déformations à la suite par oscillateur** (18 au choix) : synchro, courbures, pulsation, miroir,
  asymétrie de phase, quantification, FM / AM / anneau par l'oscillateur suivant, FM par le sous-oscillateur,
  repli, saturation, redressement, bits, passe-bas et passe-haut.
- **Phase de départ** réglable et part de hasard : une basse attaque toujours pareil, une nappe reste vivante.
- **Sous-oscillateur** (4 formes, une ou deux octaves sous la note) et **bruit coloré**, chacun avec sa sortie.
- **Deux filtres** (14 types : échelle 24 / 12 dB, multi en fondu, bande, réjection, agressifs, doux,
  **peigne + et −**, **formants en voyelles**, **diode**) **en série ou en parallèle** ; chaque oscillateur va
  vers les deux filtres, un seul, ou **en direct** (le grave reste propre).
- **Distorsion par voix** (douce, dure, lampe, repli, redresseur, bits, échantillonnage).
- Trois enveloppes (ampli, filtres, hauteur), deux LFO, deux enveloppes et trois LFO à pas de plus,
  **matrice de 32 routages, 8 macros**, arpège à pas ; jeu poly, mono ou **unisson jusqu'à 8 voix**.
- **91 sons d'usine**, dont 26 basses et 4 acides (reese, grognes, 808, FM, synchro, neuro…).

### Au moins 90 sons par synthé
- Toutes les banques ont été complétées : basses, leads, nappes, plucks, claviers, cordes, cuivres, arpèges,
  effets (et percussions, séquences ou voix selon l'instrument). Plus de **2 400 sons d'usine** en tout.
- Chaque son est **mesuré et mis au même niveau** que les autres (intensité perçue, crête, fin du son).

### Au moins 20 réglages d'usine par effet
- Les 38 effets ont désormais au moins 20 réglages prêts à l'emploi (plus de 500 nouveaux) : égaliseurs,
  compresseurs, saturations, échos, réverbérations, modulations, filtres, découpes…

### Pieuvre 8 : séquenceur
- **La dernière note jouée** (clavier, pads, MIDI à l'arrêt) **s'écrit sur le pas choisi**, en vue 8 lignes
  comme en vue « T en notes » ; les pas posés ensuite sur la ligne de cette piste prennent cette note
  (en mode « Zones », la note est ramenée dans la zone de la piste, comme le moteur le fait).
- **Allonger une note** : tirer le bord droit d'un pas (ou la queue d'un pas long), dans les deux vues.
  Un message rappelle que la durée s'entend quand le **Déclenchement** de la piste est sur « Tant qu'appuyé ».
- **Un pas sélectionné ouvre son menu** (note, vélocité, durée, décalage, condition, probabilité,
  répétitions, verrous) au lieu des réglages du pattern ; « Verrous » est aussi proposé pour un seul pas.

### Menus de pas pour tous les synthés et en écriture Acide
- Sur la façade des synthés, en écriture **À verrous**, un pas choisi ouvre son **menu** sous la rangée :
  note, vélocité, durée, décalage, condition, probabilité, répétitions.
- En écriture **Acide**, un **appui long** sur un pas ouvre le même menu, avec en plus des boutons
  **Accent** et **Glissé** larges (faciles à toucher au téléphone), leur valeur (« … ») et l'octave.

### Corrections
- Une touche du clavier de la façade pouvait rester enfoncée (et la note jouer sans fin) quand le doigt
  glissait hors de la touche : elle se relâche maintenant où que le doigt se lève.
- Unisson large sur un accord joué tout de suite après le choix d'un son : une note pouvait ne plus
  s'arrêter. Corrigé pour tous les synthés.

## Nouveautés de la 0.47.0

### Sécurité renforcée
- Les projets, préréglages et fichiers reçus d'ailleurs sont contrôlés plus strictement à l'ouverture.
- Le programme Windows est compilé avec des protections supplémentaires.
- Les outils de fabrication sont verrouillés sur des versions vérifiées.
- Chaque version est accompagnée d'un fichier **`SHA256SUMS.txt`** pour vérifier vos téléchargements
  (voir « Vérifier votre téléchargement » dans le README).

### Nouveau synthé : Glutamake
- Deux oscillateurs à **tables d'ondes** (8 tables de 12 ondes calculées, mélange A/B, 12 « morphs » en temps
  réel, modificateurs), sous-oscillateur, bruit, filtre à 10 types.
- **Une bille qui roule sur une surface** : inclinaison, frottement, bosse (creux ou colline), bords qui
  absorbent ou relancent ; lancée au doigt, à chaque note ou par la séquence. Ses mouvements (X, Y, distance,
  angle, chocs) modulent n'importe quel réglage.
- **Séquence de mouvement** : 4 couloirs de 16 pas (boucle, sens, division, lissage, probabilité).
- Branché comme les autres synthés : séquenceur à pas, matrice de 32 routages, 4 boutons de modulation,
  LFO à pas, arpège, 16 boutons assignables, MIDI ; 90 sons d'usine.

### Échantillons
- Les **MP3** sont acceptés partout : pads, samplers, import, Bibliothèque (en plus des WAV, AIFF, FLAC, OGG).
- Sur tablette, tout passe par l'application : un son pris par **« Échantillon… »** sur une piste est chargé
  tout de suite **et** rangé dans **Bibliothèque › Importés** ; **Réglages › Dossier perso…** copie tous les
  sons d'un dossier du téléphone (sous-dossiers compris) dans **Bibliothèque › Perso** (le refaire ajoute les
  nouveaux) ; vos prises audio sont dans **« Mes enregistrements »**.

### Enregistrement audio
- **Calée sur la lecture** : la prise démarre avec la lecture et s'arrête pile, à l'échantillon près
  (fin du morceau, ARRÊT, ou fin du tour de boucle quand on rappuie sur « ■ Audio »).
- **Boucle parfaite** : un tour d'élan (échos et compression en place), puis exactement un tour ; la jonction
  fin → début est fondue dans le fichier : la boucle se rejoue sans clic.
- **MP3** : le fichier indique son retard d'encodage ; les logiciels qui le lisent le rendent sans blanc ajouté.

### Montage audio
- Export **de la boucle ou de tout le montage**, pile à la longueur.
- L'attaque du premier son n'est plus adoucie au début d'une prise ; plus de creux au bouclage d'une prise de boucle.
- Toucher le nom d'une voie choisit tous ses clips ; Ctrl+A ; tirer un bord agit sur tous les clips choisis.

### Mixeur et séquenceurs
- **Envois A, B, C, D** en boutons, dans le mixeur et la vue live (pistes : à la place de l'égaliseur ;
  groupes : égaliseur et envois).
- **Pinceau** : appuyer sur un pas puis glisser sur ses voisins les pose (ou les retire) d'un seul geste :
  boîtes à rythmes, rangée à verrous des synthés, pads en mode Pas.
- **Appui long** (clic droit sur ordinateur) : saisie de la valeur exacte sur les envois et les valeurs des scènes.

### Tablette et téléphone (Android)
- L'écran reste fixe, sans tremblement : seuls les listes, le montage et les façades en grand défilent ;
  les menus des réglages ont toujours la même taille et leur liste défile.
- Enregistrer et ouvrir les projets **dans l'application** (liste des projets) ; le sélecteur du système ne
  sert plus qu'à importer ou exporter une copie.
- Le clavier ne décale plus l'écran ; la saisie d'une valeur reste visible au-dessus du clavier.
- Le son n'est jamais coupé quand une liste ou un menu s'ouvre (choix de la piste, du synthé…).
- Choisir un échantillon avec le sélecteur du système ne fait plus planter l'application, et les fichiers
  choisis sont reconnus par leur vrai nom (les WAV étaient parfois refusés).
- Montage audio : glisser au doigt fait défiler les voies (de côté et vers le bas) ; toucher un clip le
  choisit, puis on le tire pour le déplacer.

### Fichiers de cette version
| Fichier | Pour |
|---|---|
| `OFAD-GOLEM-0.47.0-Windows-installateur.exe` | Windows 10 / 11 (application, plugin VST3, notice) |
| `OFAD-GOLEM-0.47.0-Android.apk` | Android (tablette, téléphone) |
| `OFAD-GOLEM-0.47.0-Mac-app.zip`, `-Mac-VST3.zip`, `-Mac-AU.zip` | macOS (non signé : voir « Sur Mac » dans le README) |
| `OFAD-GOLEM-0.47.0-iOS-non-signe.ipa` | iPhone / iPad (non signée : à installer avec AltStore ou Sideloadly) |
| `OFAD-GOLEM-0.47.0-sources.tar.gz` | Sources (AGPL-3.0) |
| `SHA256SUMS.txt` | Empreintes de tous les fichiers ci-dessus |

---

# OFAD GOLEM 0.47.0 (English)

A big release: a new synth (**Glutamake**), stronger security, sample-accurate audio recording, a fuller
audio montage, **MP3** accepted everywhere, and a stable Android tablet experience (fixed screen, consistent
menus, sounds and projects kept inside the app).

## Update of 5 November 2026

Same 0.47.0 release, extended: a new synth (**Good Doctor**), **at least 90 factory sounds for every synth**
(over 2,400 in total), **at least 20 factory settings for every effect** (over 770), a **Pieuvre 8** sequencer
that plays like the other synths, step menus everywhere, and fixes. Projects open unchanged; the APK installs
over the previous one.

### New synth: Good Doctor
- Based on the principle of large "hybrid" wavetable synths: **three wavetable oscillators** (12 computed
  tables of 12 waves, 4 of them made for basses), continuous position.
- **Two warps in a row per oscillator** (18 to choose from): sync, bends, pulse, mirror, phase asymmetry,
  quantize, FM / AM / ring from the next oscillator, FM from the sub, fold, saturate, rectify, bits, low-pass
  and high-pass.
- Adjustable **start phase** and randomness: basses hit the same every time, pads stay alive.
- **Sub oscillator** (4 shapes, one or two octaves down) and **coloured noise**, each with its own output.
- **Two filters** (14 types including **comb + / −**, **vowel formants** and **diode**) **in series or
  parallel**; each oscillator goes to both filters, one of them, or **direct**.
- **Per-voice distortion** (soft, hard, tube, fold, rectifier, bits, sample-rate).
- Three envelopes, two LFOs, plus two envelopes and three step LFOs, **32-slot matrix, 8 macros**, step
  arpeggiator; poly, mono or **unison up to 8 voices**.
- **91 factory sounds**, including 26 basses and 4 acid sounds.

### At least 90 sounds per synth
- Every bank has been completed (bass, leads, pads, plucks, keys, strings, brass, arps, effects, plus
  percussion, sequences or voices depending on the instrument): over **2,400 factory sounds** in total,
  each one measured and levelled.

### At least 20 factory settings per effect
- All 38 effects now have at least 20 ready-to-use settings (more than 500 new ones).

### Pieuvre 8 sequencer
- **The last note played** (keyboard, pads, MIDI while stopped) **is written to the selected step**, in the
  8-row view and in the "T as notes" view; steps entered afterwards on that track's row take that note.
- **Lengthen a note** by dragging the right edge of a step (or the tail of a long one), in both views.
- **Selecting one step opens its menu** (note, velocity, length, offset, condition, probability, repeats,
  locks) instead of the pattern settings.

### Step menus on every synth and in Acid writing
- On a synth panel, in **Locks** writing, a selected step opens its **menu** under the row: note, velocity,
  length, offset, condition, probability, repeats.
- In **Acid** writing, a **long press** on a step opens the same menu, plus large **Accent** and **Slide**
  buttons (easy to hit on a phone), their amount ("…") and the octave.

### Fixes
- A key of the panel keyboard could stay down (and the note play forever) when the finger slid off it: it
  is now released wherever the finger lifts.
- Wide unison on a chord played right after choosing a sound could leave a note hanging. Fixed for all synths.

## What's new in 0.47.0

### Stronger security
- Projects, presets and files received from elsewhere are checked more strictly when opened.
- The Windows program is built with additional protections.
- Build tools are pinned to verified versions.
- Every release ships a **`SHA256SUMS.txt`** file to verify your downloads (see the README).

### New synth: Glutamake
- Two **wavetable** oscillators (8 computed tables of 12 waves, A/B blend, 12 real-time morphs, modifiers),
  sub-oscillator, noise, 10-type filter.
- **A ball rolling on a surface**: tilt, friction, a bump (well or hill), edges that absorb or bounce back;
  thrown by hand, on each note or by the sequence. Its motion (X, Y, distance, angle, hits) can modulate any
  setting.
- **Motion sequence**: 4 lanes of 16 steps (loop, direction, division, smoothing, probability).
- Wired like the other synths: step sequencer, 32-slot matrix, 4 mod knobs, step LFOs, arpeggiator,
  16 assignable knobs, MIDI; 90 factory sounds.

### Samples
- **MP3** files are accepted everywhere: pads, samplers, import, Library (as well as WAV, AIFF, FLAC, OGG).
- On tablets everything stays inside the app: a sound picked with **"Sample…"** on a track is loaded at once
  **and** stored in **Library › Importés**; **Settings › Personal folder…** copies every sound of a phone folder
  (subfolders included) into **Library › Perso** (do it again to add new ones); your audio takes are in
  **"Mes enregistrements"**.

### Audio recording
- **Follow playback**: the take starts with playback and stops exactly, to the sample.
- **Perfect loop**: one lead-in cycle, then exactly one cycle, with a seamless end-to-start join.
- **MP3**: the file carries its encoder delay, so players that read it add no silence.

### Audio montage
- Export **the loop or the whole montage**, exactly to length.
- Takes keep their first attack; loop takes no longer dip at the loop point.
- Tap a lane name to select all its clips; Ctrl+A; dragging an edge resizes all selected clips.

### Mixer and sequencers
- **Sends A–D** as knobs in the mixer and live view.
- **Paint**: press a step and slide across its neighbours to set (or clear) them in one gesture.
- **Long press** (right-click on desktop): exact value entry on sends and scene values.

### Tablet and phone (Android)
- The screen stays put, with no more jitter: only lists, the montage and enlarged faces scroll; settings menus
  keep the same size and scroll.
- Save and open projects **inside the app**; the system picker is only for importing or exporting a copy.
- The keyboard no longer shifts the screen.
- Sound never stops when a list or menu opens (track or synth choice…).
- Picking a sample with the system file picker no longer crashes the app, and picked files are recognised by
  their real name (WAV files were sometimes refused).
- Audio montage: swipe to scroll the lanes (sideways and down); tap a clip to select it, then drag it.

### Files in this release
| File | For |
|---|---|
| `OFAD-GOLEM-0.47.0-Windows-installateur.exe` | Windows 10 / 11 (app, VST3 plugin, manual) |
| `OFAD-GOLEM-0.47.0-Android.apk` | Android (tablet, phone) |
| `OFAD-GOLEM-0.47.0-Mac-app.zip`, `-Mac-VST3.zip`, `-Mac-AU.zip` | macOS (unsigned: see "On Mac" in the README) |
| `OFAD-GOLEM-0.47.0-iOS-non-signe.ipa` | iPhone / iPad (unsigned: install with AltStore or Sideloadly) |
| `OFAD-GOLEM-0.47.0-sources.tar.gz` | Sources (AGPL-3.0) |
| `SHA256SUMS.txt` | Checksums of all the files above |
