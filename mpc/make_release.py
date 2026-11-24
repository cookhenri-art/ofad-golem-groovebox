#!/usr/bin/env python3
"""
OFAD GOLEM for MPC & Force — dossier de release (zips, sources, empreintes, captions, textes).
Usage : python3 mpc/make_release.py --version 1.0.0 --out <dossier> [--shots <dossier des aperçus>] [--cpu <simall.json>]
Les aperçus sont faits par preview_skin.py (une image par page, comme l'écran de la machine).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import tarfile
import zipfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys_path = HERE
import sys
sys.path.insert(0, HERE)
import release_texts as T  # noqa: E402

REPO = "https://github.com/cookhenri-art/ofad-golem-groovebox"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def zip_plugins(mods, out_zip, readme_text):
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr("README.txt", readme_text)
        for m in mods:
            folder = os.path.join(HERE, "out", m["name"], "build", "OFAD - VST - " + m["name"])
            for root, _dirs, files in os.walk(folder):
                for f in sorted(files):
                    full = os.path.join(root, f)
                    rel = os.path.relpath(full, os.path.dirname(folder))
                    z.write(full, os.path.join("VST2 Plugins", rel))


def cpu_class(avg):
    if avg is None:
        return "?"
    if avg < 0.6:
        return "light"
    if avg < 1.6:
        return "medium"
    return "heavy"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--shots", default="/tmp/shots_all")
    ap.add_argument("--cpu", default="/tmp/simall.json")
    ap.add_argument("--fwsrc", help="archive des sources des firmwares (celle de la version GOLEM)")
    a = ap.parse_args()
    v = a.version
    cfg = json.load(open(os.path.join(HERE, "modules.json"), encoding="utf-8"))
    mods = cfg["modules"]
    inst = [m for m in mods if m["kind"] == "instrument"]
    fx = [m for m in mods if m["kind"] == "effect"]
    out = os.path.join(a.out, "OFAD-GOLEM-MPC-%s" % v)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    cpu = {}
    if os.path.isfile(a.cpu):
        for name, res, timing, _ in json.load(open(a.cpu)):
            mm = re.match(r"([\d.]+)%", timing or "")
            cpu[name] = float(mm.group(1)) if mm else None

    # ---- captures ------------------------------------------------------------------------
    sdir = os.path.join(out, "screenshots")
    os.makedirs(os.path.join(sdir, "instruments"))
    os.makedirs(os.path.join(sdir, "effects"))
    extra = {"kickforge": ["page_1_0.png", "page_3_0.png"], "gooddoc2": ["page_1_0.png"], "drumA": ["page_1_0.png"]}
    shots = {}
    for m in mods:
        src = os.path.join(a.shots, m["key"])
        if not os.path.isdir(src):
            continue
        pages = ["page_0_0.png"] + extra.get(m["key"], [])
        files = []
        for i, pgf in enumerate(pages):
            p = os.path.join(src, pgf)
            if not os.path.isfile(p):
                continue
            base = re.sub(r"[^A-Za-z0-9]+", "-", m["name"]).strip("-")
            name = base + ("" if i == 0 else "-%d" % (i + 1)) + ".jpg"
            dst = os.path.join(sdir, "instruments" if m["kind"] == "instrument" else "effects", name)
            Image.open(p).convert("RGB").save(dst, quality=88, optimize=True)
            files.append(os.path.relpath(dst, out))
        shots[m["key"]] = files

    # ---- textes ----------------------------------------------------------------------------
    n_i, n_f = len(inst), len(fx)
    install_en = f"""OFAD GOLEM for MPC & Force {v}
=====================================

{n_i} instruments and {n_f} effects from the OFAD GOLEM groovebox, as plugins for MPC and Force
standalone (32-bit Force / MPC Live / X / One and 64-bit MPC Live II / One+ / Key / X SE).
Free and open source (AGPL-3.0-or-later). Engine: OFAD GOLEM 1.2.0.

INSTALL
1. Update your MPC or Force to firmware 3.10.1 or later.
2. Menu > Preferences > Enhancements: enable the enhancements and "External Plugin Support".
3. On the SD card or USB drive of the machine, create a folder named exactly:  VST2 Plugins
4. Copy the "OFAD - VST - ..." folders into it (do not rename them).
5. Restart. The plugins are listed under the manufacturer OFAD.

PLAYING
- Function keys under the screen = pages. Press the same key again for the next sub-page
  (the dots under the key name). Example: Kick Forge > COUCHES (layers) > layer 1, 2, ... 8.
- First key = the main screen of the instrument, drawn like a front panel:
  consoles (drum machines, samplers, Octo Lyma): one strip per sound, big knob = Q-Links 1-8, small
  knob = Q-Links 9-16, press the key again for sounds 9-16 (or the next bank);
  hero screens (synths): touch a module in the left column to show its 8 knobs in the bottom row (the
  first module's 8 knobs are on Q-Links 1-8); FM: the diagram follows the Algorithm setting; Orbiteur:
  drag the orbit centre; Glutamake: the ball moves live on the physics pad;
  front panels (basses, polys, 80/90 drum machines): coloured caps = Q-Links 1-4, 5-8, 9-12, 13-16.
- Other screens: one function key per function (OSC, FILTRE, ENV, LFO, MATRICE, JEU...), 12 keys at most.
  When a key has several screens (OSC 1, 2, 3...), press it again for the next one.
- Q-Links: every screen has 16 controls at most and fills the 16 Q-Links (two rows of 8 on the Force),
  knobs first.
- Touch a control to select it: the big data wheel edits it.
  Double-tap: large editor. Menus: double-tap. Envelopes: touch a point and drag.
- Programs = GOLEM factory presets (program 0 "Init" = default settings).
- Drum machines: one function key per sound (1 GROSSE CAISSE, 2 CAISSE CLAIRE...; the key bar scrolls),
  two screens per sound (press the key again). Notes 36 to 51 play sounds 1 to 16.
  Force: plugin tracks always get the melodic keyboard (the 16-pad drum mode is kept for the Force's own
  drum tracks). Either Shift + Note: scale Chromatic, Pad Rows Continuous, bottom-left pad on C1 (36),
  so the two bottom rows play sounds 1-8 and 9-16 (the step sequencer then shows every sound); or
  "Disposition pads" (first screen of sound 1) = "Do majeur" or "Do mineur", matching the
  project scale (root C): one sound per scale note from C1, the bottom-left 4x4 square plays sounds 1-16
  (Oct-/Oct+ until the bottom-left pad plays sound 1).
- Last control "Volume" (plugin output).

GOOD TO KNOW
- Third-party plugins run inside the MPC software: a fault can freeze or restart the machine.
  Save your project first. inMusic does not support third-party plugins.
- CPU: check SHIFT + MENU > Developer > resources (this panel also opens SSH access: close it after).
  "heavy" plugins in the list below: 1 or 2 instances on a 32-bit Force.

YOUR SAMPLES, VOCODERS
- Samplers (Cutsample, Chromasample, Poussiere de Vinyle, Pieuvre 8): WAV files (16/24/32-bit, mono or
  stereo, up to 90 s) from ANY folder of the SD card, USB drive or internal drive. SAMPLES page, for each
  slot: "Dossier N" = the folder (every folder holding WAV files is listed; the plugin's own
  Content/Samples comes first), then "Sample N" = the file. Changing the folder keeps the loaded sample
  ("= name") until you pick a new file. Projects remember the full path of each file.
- Gamer Lover (video game music): pick the chip on the first screen (8 bits, Portable, 16 bits FM,
  16 bits samples, Batterie). With "Batterie", notes 36 to 51 play the 16 console drum sounds.
  The "Console" page sets the bit depth, sample rate, speaker filter and echo, for every chip.
- Arpeggiators: off by default ("Arpege en marche" = Non); factory sounds that use one switch it on.
  It follows the tempo and the transport of the machine.
- Vocoders (Vocodeur - Robot 78, Robot Choral, Spectral Vocal, Speak Synth) are EFFECTS: insert one on
  an audio track (microphone, voice, loop) or on the track to vocode; its sound is the voice. The carrier
  plays the note and chord set on the PORTEUSE page (effects get no MIDI notes on the machine).
- Not available on MPC / Force: the audio input track and the MIDI track of GOLEM.

Source code: {REPO}
"""
    lisez_fr = f"""OFAD GOLEM pour MPC et Force {v}
===================================

{n_i} instruments et {n_f} effets de la groovebox OFAD GOLEM, en greffons pour les MPC et la Force
en mode autonome (Force / MPC Live / X / One en 32 bits, MPC Live II / One+ / Key / X SE en 64 bits).
Gratuit et libre (AGPL-3.0-or-later). Moteur : OFAD GOLEM 1.2.0.

INSTALLATION
1. Mettre la machine en version 3.10.1 ou plus récente.
2. Menu > Preferences > Enhancements : activer les améliorations et « External Plugin Support ».
3. Sur la carte SD ou la clé USB de la machine, créer un dossier nommé exactement :  VST2 Plugins
4. Y copier les dossiers « OFAD - VST - ... » (sans les renommer).
5. Redémarrer : les greffons apparaissent sous le fabricant OFAD.

JEU
- Touches sous l'écran = pages ; rappuyer sur la même touche = sous-page suivante (les points).
- Toucher un réglage : la grosse molette le modifie.
  Double toucher : grande fenêtre ; menus : double toucher ; enveloppes : toucher un point et glisser.
- Programmes = sons d'usine de GOLEM (programme 0 « Init » = réglages par défaut).
- Batteries : une touche par son (1 GROSSE CAISSE, 2 CAISSE CLAIRE... ; la barre des touches défile),
  deux écrans par son (rappuyer sur la touche). Les notes 36 à 51 jouent les sons 1 à 16.
  Force : les pistes de greffon ont toujours le clavier mélodique (le mode batterie à 16 pads est réservé
  aux pistes de batterie de la Force). Soit Shift + Note : gamme Chromatique, Pad Rows Continuous, pad en
  bas à gauche sur C1 (36) : les deux rangées du bas = sons 1-8 et 9-16 (le séquenceur pas à pas montre
  alors tous les sons) ; soit « Disposition pads » (premier écran du son 1) = « Do majeur » ou
  « Do mineur », selon la gamme du projet (tonique Do) : un son par note de la gamme à partir de C1,
  le carré 4x4 en bas à gauche = sons 1 à 16 (Oct-/Oct+ jusqu'à ce que le pad en bas à gauche joue le son 1).
- Première touche = l'écran principal de l'instrument, dessiné comme une façade :
  consoles (batteries, échantillonneurs, Octo Lyma) : une tranche par son, gros bouton = Q-Links 1 à 8,
  petit bouton = Q-Links 9 à 16, rappuyer sur la touche pour les sons 9 à 16 (ou la banque suivante) ;
  écrans héros (synthés) : toucher un module dans la colonne de gauche montre ses 8 boutons en bas (ceux
  du premier module sont sur les Q-Links 1 à 8) ; FM : le schéma suit le réglage Algorithme ; Orbiteur :
  faire glisser le centre de l'orbite ; Glutamake : la bille bouge en direct sur le pavé de la physique ;
  façades (basses, polys, batteries 80/90) : capuchons de couleur = Q-Links 1-4, 5-8, 9-12, 13-16.
- Autres écrans : une touche par fonction (OSC, FILTRE, ENV, LFO, MATRICE, JEU...), 12 touches au plus.
  Une touche à plusieurs écrans (OSC 1, 2, 3...) : rappuyer dessus pour passer au suivant.
- Q-Links : chaque écran a 16 réglages au plus et remplit les 16 Q-Links (deux rangées de 8 sur la Force),
  boutons rotatifs d'abord.

VOS SAMPLES, VOCODEURS
- Échantillonneurs (Cutsample, Chromasample, Poussière de Vinyle, Pieuvre 8) : des WAV (16/24/32 bits,
  mono ou stéréo, 90 s max.) pris dans N'IMPORTE QUEL dossier de la carte SD, de la clé USB ou du disque
  interne. Page SAMPLES, pour chaque case : « Dossier N » = le dossier (tous les dossiers qui contiennent
  des WAV sont proposés ; Content/Samples du greffon en premier), puis « Sample N » = le fichier.
  Changer de dossier garde le sample chargé (« = nom ») jusqu'au choix d'un nouveau fichier.
  Les projets retiennent le chemin complet de chaque fichier.
- Gamer Lover (musique de jeux vidéo) : choisir la puce sur le premier écran (8 bits, Portable, 16 bits FM,
  16 bits échantillons, Batterie). Avec « Batterie », les notes 36 à 51 jouent les 16 sons de console.
  La page « Console » règle la résolution, la fréquence, le filtre de sortie et l'écho, pour toutes les puces.
- Arpèges : arrêtés par défaut (« Arpège en marche » = Non) ; les sons d'usine qui en ont un le mettent
  en marche. Ils suivent le tempo et la lecture de la machine.
- Vocodeurs (Vocodeur - Robot 78, Robot Choral, Spectral Vocal, Speak Synth) : ce sont des EFFETS. En insert
  sur une piste audio (micro, voix, boucle) ou sur la piste à vocoder : son son est la voix. La porteuse joue
  la note et l'accord réglés sur la page PORTEUSE (un effet ne reçoit pas de notes sur la machine).
- Impossibles sur MPC / Force : la piste Entrée audio et la piste MIDI de GOLEM.

BON À SAVOIR
- Les greffons tiers tournent dans le logiciel de la machine : un défaut peut la figer ou la redémarrer.
  Sauvegarder le projet avant. inMusic n'assure pas le support des greffons tiers.
- Processeur : SHIFT + MENU > Developer > ressources (ouvre aussi l'accès SSH : le refermer après).
  Greffons « lourds » : 1 ou 2 à la fois sur une Force 32 bits.

Code source : {REPO}
"""
    open(os.path.join(out, "INSTALL-EN.txt"), "w", encoding="utf-8").write(install_en)
    open(os.path.join(out, "LISEZ-MOI-FR.txt"), "w", encoding="utf-8").write(lisez_fr)

    # catalogue (anglais)
    lines = [f"# OFAD GOLEM for MPC & Force {v} — catalogue", "",
             f"{n_i} instruments and {n_f} effects. CPU class measured on a desktop computer, for orientation only "
             "(light / medium / heavy relative to each other).", "", "## Instruments", ""]
    for m in inst:
        name, kind, desc = T.INSTRUMENTS.get(m["key"], (m["name"], "", ""))
        lines.append(f"### {m['name']} — {kind}")
        lines.append("")
        lines.append(desc)
        lines.append("")
        lines.append(f"CPU: {cpu_class(cpu.get(m['name']))}")
        for f in shots.get(m["key"], []):
            lines.append(f"![{m['name']}]({f})")
        lines.append("")
    lines += ["## Effects", ""]
    byk = {m["key"]: m for m in fx}
    for fam, keys in T.EFFECT_FAMILIES:
        lines.append(f"### {fam}")
        lines.append("")
        for k in keys:
            if k in byk:
                lines.append(f"- **{byk[k]['name']}** — {T.EFFECTS.get(k) or T.INSTRUMENTS.get(k, ("", "", ""))[2]} (CPU: {cpu_class(cpu.get(byk[k]['name']))})")
        lines.append("")
    open(os.path.join(out, "CATALOGUE-EN.md"), "w", encoding="utf-8").write("\n".join(lines))

    # ---- zips --------------------------------------------------------------------------------
    zi = os.path.join(out, "OFAD-GOLEM-MPC-Instruments-%s.zip" % v)
    zf = os.path.join(out, "OFAD-GOLEM-MPC-Effects-%s.zip" % v)
    zip_plugins(inst, zi, install_en)
    zip_plugins(fx, zf, install_en)
    zs = os.path.join(out, "OFAD-GOLEM-MPC-Screenshots-%s.zip" % v)
    with zipfile.ZipFile(zs, "w", zipfile.ZIP_STORED) as z:
        for root, _d, files in os.walk(sdir):
            for f in sorted(files):
                z.write(os.path.join(root, f), os.path.relpath(os.path.join(root, f), out))
    # sources : moteur GOLEM (src) + portage (mpc, sans les constructions)
    st = os.path.join(out, "OFAD-GOLEM-MPC-sources-%s.tar.gz" % v)
    with tarfile.open(st, "w:gz") as t:
        t.add(os.path.join(ROOT, "src"), "ofad-golem-mpc-%s/src" % v)
        for f in sorted(os.listdir(HERE)):
            if f in ("out", "__pycache__"):
                continue
            t.add(os.path.join(HERE, f), "ofad-golem-mpc-%s/mpc/%s" % (v, f),
                  filter=lambda ti: None if "__pycache__" in ti.name else ti)
        fm1 = os.path.join(ROOT, "third_party", "fm1emu")
        t.add(fm1, "ofad-golem-mpc-%s/third_party/fm1emu" % v,
              filter=lambda ti: None if ("/target" in ti.name or "/android" in ti.name) else ti)
        for f in ("LICENSE",):
            if os.path.isfile(os.path.join(ROOT, f)):
                t.add(os.path.join(ROOT, f), "ofad-golem-mpc-%s/%s" % (v, f))
    if a.fwsrc:
        shutil.copy(a.fwsrc, os.path.join(out, "OFAD-GOLEM-MPC-%s-firmwares-sources.tar.gz" % v))
    # empreintes
    files = [p for p in sorted(os.listdir(out)) if os.path.isfile(os.path.join(out, p))]
    with open(os.path.join(out, "SHA256SUMS.txt"), "w") as f:
        for p in files:
            f.write("%s  %s\n" % (sha256(os.path.join(out, p)), p))
    print(out)
    for p in sorted(os.listdir(out)):
        full = os.path.join(out, p)
        print("  %-48s %s" % (p, ("%.1f Mo" % (os.path.getsize(full) / 1e6)) if os.path.isfile(full) else "dossier"))
    json.dump({"shots": shots, "cpu": {k: cpu_class(vv) for k, vv in cpu.items()}}, open(os.path.join(a.out, "release-info.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
