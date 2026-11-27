![OFAD GOLEM for MPC & Force 1.2.0](https://github.com/cookhenri-art/ofad-golem-groovebox/releases/download/mpc-1.2.0/OFAD-GOLEM-MPC-1.2.0-preview-EN.jpg)

**35 instruments and 58 effects** from the OFAD GOLEM groovebox, inspired by the best synths and effects around today, hardware and software, as plugins for **MPC and Force standalone** (32-bit Force / MPC Live / X / One and 64-bit MPC Live II / One+ / Key / X SE). Free and open source (AGPL-3.0-or-later). Every plugin comes in **French and English** versions.

## Update, 27 November: Good Doctor 2 screens laid out again

After tests on a Force, **Good Doctor 2** has been laid out again by hand:
- **one key per oscillator** (OSC A, OSC B, OSC C), with the **same three screens** for each: main settings and tuning; warps 1 and 2, unison and grains; spectral. No more panels cut across two screens;
- MIX (levels, pans and volume, then sub-oscillator and noise), **one screen per filter**, envelopes and LFOs in pairs, effects two per screen with their order, and **all 32 matrix slots**;
- bigger, even knobs and larger text, and the **16 Q-Links follow each screen**.

Only the screens changed: the sound engine is the same. To update, replace the `OFAD - VST - Poly - Good Doctor 2` folder, then **restart the MPC / Force** (it keeps the old Q-Links until it restarts).

## What's new in 1.2.0: a real sample browser, waveforms and bigger text

This update comes straight from tests on a Force. Most of the work went into the samplers. The picture at the top shows the new screens: the LOAD browser, the waveform, Cutsample's SLICE page, the Octopus 8 NOTES page and Kick Forge's LOAD page.

### A real browser to load your samples

The four samplers (Cutsample, Chromasample, Vinyl Dust, Octopus 8) and Kick Forge now have a **LOAD page** that works like the Force's own browser:
- the drives come first (internal drive, SD card, USB drive);
- touch a drive or a folder to open it, touch the top line to go back up, and use **< Page / Page >** to scroll;
- touch a WAV file to load it into the slot, track or layer chosen in **Load into**. The loaded file is marked with a star.

Projects remember the full path of each file. In Kick Forge, loading a WAV on a layer switches that layer to **My samples** and turns it on.

### See your sample

The playback screens of the samplers draw the **waveform** of the sample, as in GOLEM:
- start and end in white;
- loop start in yellow;
- slices in cyan.

The waveform follows the track or zone you are editing.

### Slicing on Cutsample and Vinyl Dust

- **Cutsample:** a **SLICE** page with *Slice into 16* (equal slices) or *Slice on transients*. One touch sets the 16 zones, and the Start / End of each zone stays editable. The GLOBAL page now comes first.
- **Vinyl Dust:** the GLOBAL page comes first, then one page per pad.

### Octopus 8: who plays what

A **NOTES** page, as in GOLEM:
- **1 pad = 1 track** (pads 1–8 play tracks T1–T8) or keyboard zones;
- a low and a high note for each track (T1 starts at C1 = 36 by default, one octave per track).

The track settings that were missing are back, on 3 screens per track (playback, sound, modulation). The LFOs and the modulation matrix are laid out in clear numbered rows (LFO1…, M1 source / target / depth). There is no step sequencer: you play with the machine's sequencer.

### Kick Forge: your own samples

Load any WAV on any of the 8 layers. Fixed: after you changed the sample, the old sound could come back until you touched a setting.

### Easier to read

- **Main screens** (consoles, front panels, hero screens): control names, values (including the dB next to the level faders), legends and the small green display are 20–30 % bigger.
- **Settings pages:** the values under the knobs are much bigger.
- **Samplers and Kick Forge:** all their text is about twice as big.

### Fixed

- Samplers: tracks 2, 3, 4… did not play the sample loaded on them.
- Samplers and Kick Forge: a new sample was sometimes not heard until you touched a setting.
- Cutsample: the zone Start / End had no effect once a slice mode was on. The zones now switch to free zones automatically.

## The main screens (from 1.1.0)

Every instrument opens on a hardware-style main screen:
- **consoles:** one strip per sound;
- **hero screens:** a big picture of the synth with its 8 macro knobs;
- **front panels:** the coloured caps follow the Q-Links.

On every screen there is one function key per function (OSC, FILTER, ENV, LFO, MATRIX, PLAY…), at most 16 controls, and the Q-Links follow the screen.

| | |
|---|---|
| ![Drum machine](https://github.com/user-attachments/assets/ecaf394e-403f-4fe9-b849-25ca41e6c349) | ![Synthonde](https://github.com/user-attachments/assets/a5768f71-640a-4b76-a402-4161c7373759) |
| ![Old Metal](https://github.com/user-attachments/assets/70483284-0348-4349-a1c8-1b1f31340f5d) | ![Orbiteur](https://github.com/user-attachments/assets/6ca4fcc4-6d36-4dce-a2f5-2f6e5bbcbac1) |
| ![Glutamake](https://github.com/user-attachments/assets/ddc2e987-81f1-4bbe-9ccb-2dbaf36951d4) | ![Acid Lover](https://github.com/user-attachments/assets/cafb3c75-a1d2-49be-8867-07b881a17764) |
| ![Fatsub](https://github.com/user-attachments/assets/76de6f71-2472-4c75-ac16-cf5a5aad011e) | ![Boum 80](https://github.com/user-attachments/assets/7bb76cf3-a164-4cb4-aad3-ba012c9a1570) |
| ![Pieuvre 8](https://github.com/user-attachments/assets/adbbe01e-26fa-4377-863f-98c2e3327311) | ![Etat de Grace](https://github.com/user-attachments/assets/6155424f-b78e-49db-ad1b-dd30a2b8256a) |
| ![Ricochet](https://github.com/user-attachments/assets/4b92482a-808c-424a-9e6c-6d95090da30f) | ![Titan Espace](https://github.com/user-attachments/assets/bfb24a27-a8e1-4d0f-a692-3c86a2d5c1e4) |

**Gamer Lover** (video game music) puts every console chip in one instrument: 8 bits, Handheld, 16-bit FM, 16-bit samples and a console drum kit, with a TV-screen look.

![Gamer Lover](https://github.com/user-attachments/assets/48f893cf-9274-4a4d-8580-eba3fe5fb262)

The package also has 4 WAV samplers, 4 vocoders (as effects: the sound of the track is the voice) and, on the drum machines, Mute / Solo per sound and a pad layout setting.

## Upgrading from 1.1.0

1. Delete the old `OFAD - VST - …` folders from `VST2 Plugins` first, then copy the new ones.
2. The plugin names have not changed, so your projects open with the new versions.
3. Sampler projects saved with 1.1.0: check the loaded samples and reload them from the LOAD page if needed.

## Download

| File | Contents |
|---|---|
| `OFAD-GOLEM-MPC-Instruments-1.2.0-EN.zip` | 35 instruments, English version |
| `OFAD-GOLEM-MPC-Effects-1.2.0-EN.zip` | 58 effects, English version |
| `OFAD-GOLEM-MPC-Instruments-1.2.0.zip` | 35 instruments, French version |
| `OFAD-GOLEM-MPC-Effects-1.2.0.zip` | 58 effects, French version |
| `OFAD-GOLEM-MPC-Screenshots-1.2.0(-EN).zip` | One screenshot per plugin |
| `OFAD-GOLEM-MPC-sources-1.2.0.tar.gz` | Source code |
| `CATALOGUE-EN.md` · `INSTALL-EN.txt` · `LISEZ-MOI-FR.txt` | Catalogue and instructions |
| `SHA256SUMS.txt` | Checksums |

## Install

1. Update the MPC or Force to firmware 3.10.1 or later.
2. Menu > Preferences > Enhancements: enable the enhancements and **External Plugin Support**.
3. On the SD card or USB drive, create a folder named exactly `VST2 Plugins`.
4. Copy the `OFAD - VST - …` folders into it, then restart. The plugins are listed under OFAD.

## Notes

- Third-party plugins run inside the MPC software: save your project first. inMusic does not support third-party plugins.
- On a 32-bit Force, use only 1 or 2 instances of the heavy plugins (see the catalogue).
- Not available on MPC / Force: GOLEM's audio input track, MIDI track and FM Player.
- The French and English versions are separate plugins: install one or both.

---

## En français

![OFAD GOLEM pour MPC et Force 1.2.0](https://github.com/cookhenri-art/ofad-golem-groovebox/releases/download/mpc-1.2.0/OFAD-GOLEM-MPC-1.2.0-apercu.jpg)

**35 instruments et 58 effets** de la groovebox OFAD GOLEM, inspirés des meilleurs synthés et effets actuels, matériels ou logiciels, en greffons pour MPC et Force, gratuits et libres. Chaque greffon existe en version française et en version anglaise.

**Mise à jour du 27 novembre : Good Doctor 2 rangé à nouveau**
- **Une touche par oscillateur** (OSC A, OSC B, OSC C), avec les **trois mêmes écrans** pour chacun : réglages de base et accord ; déformations 1 et 2, unisson et grains ; spectral. Plus aucun panneau coupé sur deux écrans.
- MIX (niveaux, pans et volume, puis sous-oscillateur et bruit), **un écran par filtre**, enveloppes et LFO par paires, effets deux par écran avec leur ordre, et **les 32 emplacements de la matrice**.
- Boutons plus grands et réguliers, textes plus gros, et les **16 Q-Links suivent chaque écran**.

Seuls les écrans changent, le son est le même. Pour mettre à jour : remplacer le dossier `OFAD - VST - Poly - Good Doctor 2`, puis **redémarrer la MPC / Force** (elle garde les anciens Q-Links jusqu'au redémarrage).

**Nouveautés de la 1.2.0**
- **Un vrai navigateur pour charger ses samples**, page **CHARGER**, sur les 4 échantillonneurs et Kick Forge. On voit d'abord les disques (interne, carte SD, clé USB). On touche un dossier pour l'ouvrir, la ligne du haut pour remonter et un fichier WAV pour le charger.
- **Forme d'onde** sur les écrans de lecture : début et fin, début de boucle, découpes.
- **Cutsample** : page DÉCOUPE (*Découper en 16* ou *sur les attaques*), le Début et la Fin des zones restent réglables, et la page GLOBAL passe en premier. **Poussière de Vinyle** : la page GLOBAL passe aussi en premier.
- **Pieuvre 8** : page NOTES (*1 pad = 1 piste* ou zones du clavier, T1 à partir de C1). Tous les réglages des pistes sont revenus, sur 3 écrans par piste, et les LFO et la matrice sont rangés proprement.
- **Kick Forge** : n'importe quel WAV sur n'importe quelle couche.
- **Textes plus gros** : écrans principaux, valeurs sous les boutons, et pages des échantillonneurs et de Kick Forge (environ deux fois plus grandes).
- **Corrigé** : les pistes 2, 3, 4… des échantillonneurs ne jouaient pas leur sample, et un nouveau sample ne s'entendait parfois qu'après avoir touché un réglage.

**Mise à jour depuis la 1.1.0** : supprimer d'abord les anciens dossiers `OFAD - VST - …`, puis copier les nouveaux. Les noms n'ont pas changé, donc les projets s'ouvrent avec les nouvelles versions. Pour les projets avec échantillonneurs, vérifier les samples chargés.