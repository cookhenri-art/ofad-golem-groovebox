#!/usr/bin/env python3
"""
OFAD GOLEM for MPC & Force — zips de la version anglaise (greffons construits par build_mpc.py --lang en).
Usage : python3 mpc/make_release_en.py --version 1.1.0 --out <dossier>
Écrit OFAD-GOLEM-MPC-Instruments-<v>-EN.zip et OFAD-GOLEM-MPC-Effects-<v>-EN.zip (même rangement que les
zips français : « VST2 Plugins/OFAD - VST - <nom>/ » et un README.txt en anglais).
"""
import argparse
import json
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_mpc as B  # noqa: E402

REPO = "https://github.com/cookhenri-art/ofad-golem-groovebox"


def readme(v, n_i, n_f, engine):
    return f"""OFAD GOLEM for MPC & Force {v} - English version
================================================

{n_i} instruments and {n_f} effects from the OFAD GOLEM groovebox, as plugins for MPC and Force
standalone (32-bit Force / MPC Live / X / One and 64-bit MPC Live II / One+ / Key / X SE).
Free and open source (AGPL-3.0-or-later). Engine: OFAD GOLEM {engine}.

ENGLISH VERSION
- Everything on the machine is in English: plugin names, screens, function keys, control names,
  choices and factory preset names. The sound and the settings are exactly the same as the French
  version.
- The English plugins are separate plugins (their own names and IDs): you can install them next to
  the French ones. A project saved with a French plugin keeps using the French plugin.

INSTALL
1. Update your MPC or Force to firmware 3.10.1 or later.
2. Menu > Preferences > Enhancements: enable the enhancements and "External Plugin Support".
3. On the SD card or USB drive of the machine, create a folder named exactly:  VST2 Plugins
4. Copy the "OFAD - VST - ..." folders into it (do not rename them).
5. Restart. The plugins are listed under the manufacturer OFAD.

PLAYING
- Function keys under the screen = pages. Press the same key again for the next sub-page
  (the dots under the key name). Example: Kick Forge > LAYERS > layer 1, 2, ... 8.
- First key = the main screen of the instrument, drawn like a front panel:
  consoles (drum machines, samplers, Octo Lyma): one strip per sound, big knob = Q-Links 1-8, small
  knob = Q-Links 9-16, press the key again for sounds 9-16 (or the next bank);
  hero screens (synths): touch a module in the left column to show its 8 knobs in the bottom row (the
  first module's 8 knobs are on Q-Links 1-8); FM: the diagram follows the Algorithm setting; Orbiter:
  drag the orbit centre; Glutamake: the ball moves live on the physics pad;
  front panels (basses, polys, 80/90 drum machines): coloured caps = Q-Links 1-4, 5-8, 9-12, 13-16.
- Other screens: one function key per function (OSC, FILTER, ENV, LFO, MATRIX, PLAY...), 12 keys at most.
  When a key has several screens (OSC 1, 2, 3...), press it again for the next one.
- Q-Links: every screen has 16 controls at most and fills the 16 Q-Links (two rows of 8 on the Force),
  knobs first.
- Touch a control to select it: the big data wheel edits it.
  Double-tap: large editor. Menus: double-tap. Envelopes: touch a point and drag.
- Programs = GOLEM factory presets (program 0 "Init" = default settings).
- Drum machines: one function key per sound (the key bar scrolls), two screens per sound (press the key
  again). Notes 36 to 51 play sounds 1 to 16.
  Force: plugin tracks always get the melodic keyboard (the 16-pad drum mode is kept for the Force's own
  drum tracks). Either Shift + Note: scale Chromatic, Pad Rows Continuous, bottom-left pad on C1 (36),
  so the two bottom rows play sounds 1-8 and 9-16 (the step sequencer then shows every sound); or
  "Pad layout" (first screen of sound 1) = "C major" or "C minor", matching the project scale (root C):
  one sound per scale note from C1, the bottom-left 4x4 square plays sounds 1-16 (Oct-/Oct+ until the
  bottom-left pad plays sound 1).
- Last control "Volume" (plugin output).

GOOD TO KNOW
- Third-party plugins run inside the MPC software: a fault can freeze or restart the machine.
  Save your project first. inMusic does not support third-party plugins.
- CPU: check SHIFT + MENU > Developer > resources (this panel also opens SSH access: close it after).
  Heavy plugins: 1 or 2 instances on a 32-bit Force.

YOUR SAMPLES, VOCODERS
- Samplers (Cutsample, Chromasample, Vinyl Dust, Octopus 8): WAV files (16/24/32-bit, mono or stereo,
  up to 90 s) from ANY folder of the SD card, USB drive or internal drive. SAMPLES page, for each slot:
  "Folder N" = the folder (every folder holding WAV files is listed; the plugin's own Content/Samples
  comes first), then "Sample N" = the file. Changing the folder keeps the loaded sample ("= name") until
  you pick a new file. Projects remember the full path of each file.
- Gamer Lover (video game music): pick the chip on the first screen (8 bits, Handheld, 16-bit FM,
  16-bit samples, Drums). With "Drums", notes 36 to 51 play the 16 console drum sounds.
  The "Console" page sets the bit depth, sample rate, speaker filter and echo, for every chip.
- Arpeggiators: off by default; factory sounds that use one switch it on. It follows the tempo and the
  transport of the machine.
- Vocoders (Vocoder - Robot 78, Robot Choral, Spectral Vocal, Speak Synth) are EFFECTS: insert one on
  an audio track (microphone, voice, loop) or on the track to vocode; its sound is the voice. The carrier
  plays the note and chord set on its carrier page (effects get no MIDI notes on the machine).
- Not available on MPC / Force: the audio input track and the MIDI track of GOLEM.

Source code: {REPO}
"""


def zip_plugins(mods, out_root, out_zip, text):
    n = 0
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr("README.txt", text)
        for m in mods:
            folder = os.path.join(out_root, m["name"], "build", "OFAD - VST - " + m["name"])
            if not os.path.isdir(folder):
                raise SystemExit("pas construit : " + m["name"])
            for root, _dirs, files in os.walk(folder):
                for f in sorted(files):
                    full = os.path.join(root, f)
                    z.write(full, os.path.join("VST2 Plugins", os.path.relpath(full, os.path.dirname(folder))))
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--built", default=os.path.join(HERE, "out_en"))
    a = ap.parse_args()
    cfg = json.load(open(os.path.join(HERE, "modules.json"), encoding="utf-8"))
    mods = [B.english_module(m) for m in cfg["modules"]]
    inst = [m for m in mods if m["kind"] == "instrument"]
    fx = [m for m in mods if m["kind"] == "effect"]
    text = readme(a.version, len(inst), len(fx), B.version_of_golem())
    os.makedirs(a.out, exist_ok=True)
    zi = os.path.join(a.out, "OFAD-GOLEM-MPC-Instruments-%s-EN.zip" % a.version)
    zf = os.path.join(a.out, "OFAD-GOLEM-MPC-Effects-%s-EN.zip" % a.version)
    print("instruments", zip_plugins(inst, a.built, zi, text), os.path.getsize(zi))
    print("effets", zip_plugins(fx, a.built, zf, text), os.path.getsize(zf))
    with open(os.path.join(a.out, "INSTALL-EN-english-version.txt"), "w", encoding="utf-8") as f:
        f.write(text)


if __name__ == "__main__":
    main()
