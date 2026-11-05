# OFAD GOLEM 0.47.0

*English below.*

## Nouveautés

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
  LFO à pas, arpège, 16 boutons assignables, MIDI ; 13 sons d'usine.

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
- L'écran reste fixe ; les menus des réglages ont toujours la même taille et leur liste défile.
- Enregistrer et ouvrir les projets **dans l'application** (liste des projets) ; le sélecteur du système ne
  sert plus qu'à importer ou exporter une copie.
- Le clavier ne décale plus l'écran ; la saisie d'une valeur reste visible au-dessus du clavier.
- Le son n'est jamais coupé quand une liste ou un menu s'ouvre (choix de la piste, du synthé…).
- Dossiers propres à l'application : **« Mes enregistrements »** apparaît dans la Bibliothèque, et
  **Réglages → Importer…** copie une fois les échantillons du téléphone dans le dossier de l'application.
- Montage audio : glisser au doigt fait défiler les voies (de côté et vers le bas) ; toucher un clip le
  choisit, puis on le tire pour le déplacer.

### Fichiers de cette version
| Fichier | Pour |
|---|---|
| `OFAD-GOLEM-0.47.0-Windows-installateur.exe` | Windows 10 / 11 (application, plugin VST3, notice) |
| `OFAD-GOLEM-0.47.0-Android.apk` | Android (tablette, téléphone) |
| `OFAD-GOLEM-0.47.0-Mac-app.zip`, `-Mac-VST3.zip`, `-Mac-AU.zip` | macOS (non signé : voir « Sur Mac » dans le README) |
| `OFAD-GOLEM-0.47.0-sources.tar.gz` | Sources (AGPL-3.0) |
| `SHA256SUMS.txt` | Empreintes de tous les fichiers ci-dessus |

---

# OFAD GOLEM 0.47.0 (English)

## What's new

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
  16 assignable knobs, MIDI; 13 factory sounds.

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
- The screen stays put; settings menus keep the same size and scroll.
- Save and open projects **inside the app**; the system picker is only for importing or exporting a copy.
- The keyboard no longer shifts the screen.
- Sound never stops when a list or menu opens (track or synth choice…).
- App-own folders: **"Mes enregistrements"** shows in the Library, and **Settings → Import…** copies phone
  samples once into the app folder.
- Audio montage: swipe to scroll the lanes (sideways and down); tap a clip to select it, then drag it.
