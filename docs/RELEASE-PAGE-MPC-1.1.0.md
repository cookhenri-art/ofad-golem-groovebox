# Page de release — OFAD GOLEM for MPC & Force 1.1.0

Tag : `mpc-1.1.0` · Titre : `OFAD GOLEM for MPC & Force 1.1.0` · Étiquette : aucune (1.2.0 reste « Latest »)

Fichiers à joindre (dossier `_livraisons/OFAD-GOLEM-MPC-1.1.0/`) :
`OFAD-GOLEM-MPC-Instruments-1.1.0.zip`, `OFAD-GOLEM-MPC-Effects-1.1.0.zip`, `OFAD-GOLEM-MPC-Screenshots-1.1.0.zip`,
`OFAD-GOLEM-MPC-sources-1.1.0.tar.gz`, `OFAD-GOLEM-MPC-1.1.0-apercu.jpg`, `CATALOGUE-EN.md`, `INSTALL-EN.txt`,
`LISEZ-MOI-FR.txt`, `SHA256SUMS.txt`.

---

## Texte de la page (à coller)

# OFAD GOLEM for MPC & Force 1.1.0

**35 instruments and 58 effects** from the OFAD GOLEM groovebox, as plugins for **MPC and Force standalone** (firmware 3.10.1+, *External Plugin Support*). Free and open source (AGPL-3.0-or-later). Engine: OFAD GOLEM 1.2.0.

![New screens](OFAD-GOLEM-MPC-1.1.0-apercu.jpg)

## What's new — complete graphic redesign after your feedback
- **A new main screen on the first key of every instrument**, drawn like a hardware front panel:
  - **consoles** (drum machines, samplers, Octo Lyma, Pieuvre 8): one strip per sound, big knob on Q-Links 1–8, small knob on 9–16, press the key again for sounds 9–16;
  - **hero screens** (synths): a large live diagram and a row of 8 knobs; touch a module in the left column to bring its 8 knobs into the row. The **FM algorithm diagram follows the Algorithm setting**, **Orbiteur's orbit centre can be dragged**, **Glutamake's ball moves live** on its physics pad;
  - **front panels** (basses, polys, Boum 80, Club 90, Kick Killer): knobs and faders with coloured caps showing the Q-Link groups 1–4, 5–8, 9–12, 13–16.
- **Every synth now wears its GOLEM colours and controls**: the same knobs, faders, switches and buttons, now **filling the whole screen**, with names above and values below each control and dark text on light panels.
- **Effects show their GOLEM pedal or rack** next to their controls, with its knobs and colours.
- **One function key per function** (OSC, FILTRE, ENV, LFO, MATRICE, JEU…), **12 keys at most**. When a function has several screens (OSC 1, 2, 3…), press its key again. No more lonely SORTIE page: the volume sits on an existing screen.
- At most **16 controls per screen = the 16 Q-Links** (two rows of 8 on the Force), turning knobs first.
- **No more drop-down lists**: they opened empty on the machine. Short choices are buttons on the screen; longer ones open the large rotary editor, which shows the choice name.
- **Drum machines**: one key per sound (1 BD, 2 SD, 3 CP, 4 RS, 5 CH, 6 OH…), two screens per sound.
- **Names start with the family** (Drum -, Kick -, Basse -, Poly -, Nappe -, FM -, Drone -, Sampler -, Reverb -, Delay -, Compresseur -…) so the plugin list is sorted by type.

## New plugins
- **Gamer Lover — video game music, every console chip in one instrument.** Pick the chip on the first screen and the sound *and* the retro TV on screen change with it:
  - **8 bits**: pulse with 4 widths (12.5 / 25 / 50 / 75 %), stepped 4-bit triangle, long or short noise, 16-step volume;
  - **Portable**: pulse and a free 32-step 4-bit wave (8 waves), noise;
  - **16 bits FM**: 4 operators, 8 algorithms, feedback, DAC grain;
  - **16 bits samples**: 8 calculated instruments (strings, choir, electric piano, brass, flute, bass, guitar, organ) crunched to 4 bits like the cartridges of the time, soft interpolation;
  - **Batterie**: 16 console drum sounds on the pads (notes 36–51): kicks, snares, clap, hats, cymbal, toms, bell, coin, jump, explosion.
  Tables run at the frame rate (60 or 50 Hz) like the trackers of the time: fast arpeggio chords, stepped volume, moving pulse width, pitch drops, delayed vibrato. The **Console** output stage sets bit depth, sample rate (44.1 to 11 kHz, no smoothing filter), speaker filter (open, living room, handheld, TV) and an 8-tap filtered echo. 32 factory sounds. No console ROM or sample: everything is calculated.
- **4 samplers playing your own WAV files from any folder** of the SD card, USB drive or internal drive: Cutsample, Chromasample, Poussiere de Vinyle, Pieuvre 8 (8 tracks). On the SAMPLES page, pick the folder (*Dossier N*: every folder holding WAV files is listed), then the file (*Sample N*). Projects remember the full path.
- **4 vocoders, as effects**: Robot 78, Robot Choral, Spectral Vocal, Speak Synth. Insert one on an audio track (microphone, voice, loop): the track's sound is the voice, the carrier plays the note and chord set on the PORTEUSE page.
- **Drum machines**: new *Disposition pads* setting (first screen of sound 1, for the whole kit) — "Do majeur" or "Do mineur" puts one sound on each note of the project scale (root C): the bottom-left 4×4 square plays sounds 1–16. With a chromatic layout, keep "36-51".

## Fixed and simplified
- **Arpeggiators now play** on the machine (until now they did nothing), following its tempo and transport. They are **off by default** (*Arpège en marche* = Non); factory sounds that use one switch it on.
- **Arpeggiator hold**: switching *Arp Maintien* on while notes are held keeps them at once.
- **No stuck notes**: a note played before switching the arpeggiator on always stops when you release it.
- **Drum machines**: *Type de filtre* sits on the same screen as *Coupure* and *Résonance*; *Groupe d'étouffement* moved to the first screen of each sound.
- **Kick Forge**: in *Init*, *Gain automatique* is off so each layer's volume works right away (volume -3 dB to keep the peak under 0 dBFS); factory sounds keep it on.
- **Gamer Lover**: no more crackles (smoothed volume steps, notes restart and end without clicks, sample rate set to *Pleine* by default).
- **Copying to the memory card**: the ratio buttons of *Compresseur - Cement* had a `:` in their file names (`2:1`, `4:1`, `10:1`), which SD cards and Windows refuse; they are now named `2-1`, `4-1`, `10-1`.
- **No internal sequencer pages any more**: you play with the machine's own sequencer, so the step values of the step LFOs and Glutamake's sequence lanes are no longer on the screens (they stay in GOLEM). Kick Killer keeps its 8-step sequence, which is part of its sound.

## Removed
- **FM Player**: emulating the firmware needs about two cores of a Force or MPC, while a plugin gets a single core for audio, so the sound was choppy. It stays in OFAD GOLEM on desktop and mobile.

## Upgrading from 1.0.0
Delete the old `OFAD - VST - …` folders from `VST2 Plugins` before copying the new ones (the names changed, otherwise each plugin appears twice). Projects saved with 1.0.0 may not find renamed plugins.

## Download
| File | Content |
|---|---|
| `OFAD-GOLEM-MPC-Instruments-1.1.0.zip` | 35 instruments (`VST2 Plugins` folder) |
| `OFAD-GOLEM-MPC-Effects-1.1.0.zip` | 58 effects including the 4 vocoders (`VST2 Plugins` folder) |
| `OFAD-GOLEM-MPC-Screenshots-1.1.0.zip` | screen previews of every plugin |
| `OFAD-GOLEM-MPC-sources-1.1.0.tar.gz` | source code (GOLEM engine + MPC port) |
| `CATALOGUE-EN.md` | what each plugin does, CPU class |
| `INSTALL-EN.txt` / `LISEZ-MOI-FR.txt` | installation |
| `SHA256SUMS.txt` | checksums |

## Install
1. Update to firmware **3.10.1** or later.
2. **Menu > Preferences > Enhancements**: enable *External Plugin Support*.
3. Copy the `OFAD - VST - …` folders into a folder named **`VST2 Plugins`** on the SD card or USB drive.
4. Restart: the plugins are listed under **OFAD**.

## Notes
- External plugin support is experimental on the machine side: save your project first. inMusic does not support third-party plugins.
- On a 32-bit Force, use 1–2 instances of the heavy synths.
- Not possible on MPC / Force: GOLEM's audio input track and MIDI track.
- Screen labels are in French for now.

---

### En français
**Nouvel écran principal sur la première touche de chaque instrument**, dessiné comme une façade : consoles d'une tranche par son (batteries, échantillonneurs, Octo Lyma, Pieuvre 8 ; gros bouton sur les Q-Links 1 à 8, petit sur 9 à 16), écrans héros des synthés (grand schéma en direct et 8 boutons ; toucher un module dans la colonne de gauche pour amener ses 8 boutons ; le schéma FM suit le réglage Algorithme, le centre de l'orbite de l'Orbiteur se fait glisser, la bille de Glutamake bouge en direct), façades à capuchons de couleur pour les groupes de Q-Links (basses, polys, Boum 80, Club 90, Kick Killer).
**Refonte graphique complète suite à vos retours** : chaque synthé reprend les couleurs et les réglages de sa façade GOLEM (boutons, tirettes, sélecteurs) sur tout l'écran, noms au-dessus et valeurs en dessous, texte foncé sur les panneaux clairs ; chaque effet affiche sa pédale ou son rack ; les boîtes à rythmes ont une touche par son (1 BD, 2 SD…). Une touche par fonction (OSC, FILTRE, ENV, LFO, MATRICE, JEU…), 12 au plus, on rappuie pour l'écran suivant ; 16 réglages au plus par écran = les 16 Q-Links. Plus de listes déroulantes (elles s'ouvraient vides) : boutons à l'écran ou grand éditeur rotatif. Les noms commencent par la famille (Drum -, Poly -, Reverb -…) pour que la liste soit rangée par type.
**Corrigé** : le maintien de l'arpège retient tout de suite les notes tenues ; plus de note bloquée quand on met l'arpège en marche ; dans les boîtes à rythmes, le type de filtre est sur l'écran de la coupure et de la résonance ; Gamer Lover ne craque plus ; Kick Forge démarre (Init) sans gain automatique pour que le volume des couches agisse (les sons d'usine le gardent) ; les boutons de rapport de Compresseur - Cement avaient un « : » dans leur nom de fichier, refusé par la carte mémoire (renommés 2-1, 4-1, 10-1) ; les arpèges jouent enfin sur la machine (tempo et lecture de la machine), arrêtés par défaut sauf dans les sons d'usine qui en ont un ; plus de pages de séquenceur interne (valeurs des pas des LFO, couloirs de Glutamake ; Kick Killer garde sa séquence de 8 pas).
**Nouveautés** : **Gamer Lover**, la musique de jeux vidéo dans un seul instrument — on choisit la puce (8 bits, Portable, 16 bits FM, 16 bits échantillons, Batterie) et le son comme la télé à l'écran changent ; tables à 60 ou 50 Hz (arpège rapide, volume en marches, vibrato retardé), sortie console (bits, fréquence, filtre, écho), 32 sons d'usine, tout est calculé. 4 échantillonneurs qui lisent vos WAV dans n'importe quel dossier de la carte, de la clé ou du disque interne (« Dossier N » puis « Sample N »), 4 vocodeurs en effets (le son de la piste est la voix), réglage « Disposition pads » pour les batteries (Do majeur ou Do mineur : un son par note de la gamme).
**Retiré** : FM Player, trop gourmand (environ deux cœurs, un greffon n'en a qu'un) ; il reste dans GOLEM sur ordinateur et mobile.
**Mise à jour** : supprimer les anciens dossiers « OFAD - VST - … » de `VST2 Plugins` avant de copier les nouveaux. Détails dans `LISEZ-MOI-FR.txt`.

---

## Posts

### English
OFAD GOLEM for MPC & Force 1.1.0 is out — free, open source plugins for MPC and Force standalone.

New: **Gamer Lover**, video game music with every console chip in one instrument (8-bit, handheld, 16-bit FM, 16-bit samples, console drums).
Big visual redesign after your feedback: a new main screen for every instrument (channel-strip consoles, hero screens with live diagrams, hardware-style front panels), every synth wears its GOLEM front panel, every effect shows its pedal or rack, drum machines get one key per sound. One screen per key, 16 controls per screen, so everything is on the Q-Links.

New: 4 samplers playing WAVs from any folder of your SD card or USB drive, 4 vocoders as effects (your track is the voice), working arpeggiators. Plugin names now start with their family, so the list is sorted by type.
The FM firmware player is gone: it needed about two cores, a plugin only gets one.

35 instruments, 58 effects, firmware 3.10.1+:
https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

### Français
OFAD GOLEM pour MPC & Force 1.1.0 est sorti — des greffons gratuits et libres pour MPC et Force en autonome.

Nouveau : **Gamer Lover**, la musique de jeux vidéo avec toutes les puces de console dans un seul instrument (8 bits, portable, 16 bits FM, 16 bits échantillons, batterie de console).
Grosse refonte graphique suite à vos retours : un nouvel écran principal pour chaque instrument (consoles à tranches, écrans héros avec schémas en direct, façades façon matériel), chaque synthé reprend sa façade GOLEM, chaque effet sa pédale ou son rack, les boîtes à rythmes ont une touche par son. Un écran par touche, 16 réglages par écran : tout est sur les Q-Links.

Nouveau : 4 échantillonneurs qui jouent les WAV de n'importe quel dossier de votre carte ou clé, 4 vocodeurs en effets (votre piste devient la voix), des arpèges qui marchent. Les noms commencent par la famille, la liste est rangée par type.
Le lecteur de firmwares FM est retiré : il demandait environ deux cœurs, un greffon n'en a qu'un.

35 instruments, 58 effets, version 3.10.1 et plus :
https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

---

## Posts Facebook — refonte graphique complète

Visuels (dossier `_livraisons/OFAD-GOLEM-MPC-1.1.0-visuels/`) : `fb-1-couverture-1200x630.jpg` (aperçu du lien), `fb-2-portrait-1080x1350.jpg` (image seule dans le fil), `fb-3-ecran-01…11` (carrousel : Machine électronique, Synthonde, Old Metal, Orbiteur, Glutamake, Acid Lover, Fatsub, Boum 80, Pieuvre 8, État de Grâce, Gamer Lover), `fb-4-gamer-lover-portrait-1080x1350.jpg` et `fb-4-gamer-lover-couverture-1200x630.jpg` (post Gamer Lover).

### Français
🎛️ OFAD GOLEM pour MPC & Force 1.1.0 : refonte graphique complète.

Vous nous avez envoyé vos photos d'écran, on a tout redessiné.

➡️ Un écran principal dessiné pour chaque instrument, sur la première touche :
• les batteries et les échantillonneurs en console, une tranche par son (gros bouton sur les Q-Links 1 à 8, petit sur 9 à 16) ;
• les synthés en écran héros : un grand schéma et 8 boutons, la colonne de gauche change de module au toucher. Le schéma FM suit l'algorithme choisi, l'orbite de l'Orbiteur se déplace au doigt, la bille de Glutamake roule en direct ;
• les basses et les polys en façade, capuchons de couleur pour les groupes de Q-Links.

➡️ Partout ailleurs : une touche par fonction, 16 réglages par écran (= les 16 Q-Links), tout l'écran utilisé, plus de listes déroulantes vides.

Et un nouvel instrument : 🎮 Gamer Lover, toutes les puces de consoles de jeu dans un seul greffon.

Et aussi : 4 échantillonneurs qui vont chercher vos WAV dans n'importe quel dossier de la carte ou de la clé, des arpèges qui suivent le tempo de la machine, 4 vocodeurs en effets, les noms rangés par famille.

35 instruments, 58 effets, gratuit et libre (AGPL). Version 3.10.1 et plus, « External Plugin Support » activé.
👉 https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

Dites-nous ce que vous en pensez en commentaire, avec une photo de votre écran 📸

### English
🎛️ OFAD GOLEM for MPC & Force 1.1.0: a complete graphic redesign.

You sent us photos of your screens, so we redrew everything.

➡️ A main screen designed for every instrument, on the first key:
• drum machines and samplers as a console, one strip per sound (big knob on Q-Links 1–8, small one on 9–16);
• synths as a hero screen: a large diagram and 8 knobs, touch the left column to switch module. The FM diagram follows the chosen algorithm, Orbiteur's orbit moves under your finger, Glutamake's ball rolls live;
• basses and polys as a front panel, with coloured caps for the Q-Link groups.

➡️ Everywhere else: one key per function, 16 controls per screen (= the 16 Q-Links), the whole screen used, no more empty drop-down lists.

And a new instrument: 🎮 Gamer Lover, every video game console chip in one plugin.

Also new: 4 samplers that pick your WAVs from any folder of the SD card or USB drive, arpeggiators following the machine's tempo, 4 vocoders as effects, plugin names sorted by family.

35 instruments, 58 effects, free and open source (AGPL). Firmware 3.10.1+, "External Plugin Support" enabled.
👉 https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

Tell us what you think in the comments, with a photo of your screen 📸


---

## Post Facebook — Gamer Lover (à publier après le post de la refonte)

Visuels : `fb-4-gamer-lover-portrait-1080x1350.jpg` (image du post) ou `fb-4-gamer-lover-couverture-1200x630.jpg` (aperçu du lien), plus `fb-3-ecran-11-Gamer-Lover.jpg`.

### Français
🎮 Nouveau dans OFAD GOLEM pour MPC & Force : Gamer Lover.

La musique de jeux vidéo des années 85-2005, dans un seul instrument. Sur le premier écran, on choisit la puce, et le son comme la petite télé à l'écran changent avec elle :
🕹️ 8 bits : carré à 4 largeurs, triangle en escalier, bruit
📟 Portable : carré et onde libre sur 4 bits
🎛️ 16 bits FM : 4 opérateurs, 8 algorithmes, le grain du convertisseur d'époque
💾 16 bits échantillons : cordes, chœur, piano électrique, cuivres… compressés sur 4 bits comme dans les cartouches
🥁 Batterie : 16 sons de console sur les pads, jusqu'à la pièce, le saut et l'explosion

Les tables tournent à 60 ou 50 images par seconde, comme dans les éditeurs de musique de jeu : faux accords très rapides, volume en marches, vibrato qui arrive après la note. Et la sortie console règle les bits, la fréquence, le filtre du haut-parleur et l'écho.

32 sons d'usine pour démarrer (basses, leads, arpèges, nappes, batteries, effets). Aucune ROM ni aucun échantillon de console : tout est calculé.

Gratuit et libre, dans la version 1.1.0 (version 3.10.1 et plus, « External Plugin Support » activé) :
👉 https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

Montrez-nous vos morceaux de boss de fin en commentaire 👾

### English
🎮 New in OFAD GOLEM for MPC & Force: Gamer Lover.

Video game music from 1985 to 2005, in a single instrument. On the first screen, pick the chip, and both the sound and the little TV on screen change with it:
🕹️ 8-bit: pulse with 4 widths, stepped triangle, noise
📟 Handheld: pulse and a free 4-bit wave
🎛️ 16-bit FM: 4 operators, 8 algorithms, the grain of the old converters
💾 16-bit samples: strings, choir, electric piano, brass… crunched to 4 bits like on cartridges
🥁 Drums: 16 console sounds on the pads, down to the coin, the jump and the explosion

Tables run at 60 or 50 frames per second, like the trackers of the time: fast arpeggio chords, stepped volume, vibrato that comes in after the note. The console output stage sets bit depth, sample rate, speaker filter and echo.

32 factory sounds to start with (basses, leads, arpeggios, pads, drums, effects). No console ROM or sample: everything is calculated.

Free and open source, in version 1.1.0 (firmware 3.10.1+, "External Plugin Support" enabled):
👉 https://github.com/cookhenri-art/ofad-golem-groovebox/releases/tag/mpc-1.1.0

Show us your final-boss tracks in the comments 👾
