#!/usr/bin/env python3
"""
OFAD GOLEM — fabrique des greffons MPC (kit de greffons MPC, VST 2.4 propre) — AGPL-3.0-or-later

Pour chaque module listé dans mpc/modules.json :
  1. écrit un projet de greffon (plugin.json, src/<Nom>.cpp) dans mpc/out/<Nom>/ ;
  2. compile la version native (C++20, sources GOLEM) avec l'outil du kit (tools/build.py) ;
  3. lit la disposition des pages dans la bibliothèque native (golem_mpc_layout) → ui.json ;
  4. compile les versions arm32 (Force, MPC Live/X/One) et arm64 (Live II, One+, Key 61, X SE),
     génère l'interface tactile, valide le dossier.

Usage :
    python3 mpc/build_mpc.py --sdk <dossier du kit> [clé ...]      (sans clé : tous les modules)

Rien n'est installé sur une machine : le résultat est un dossier « OFAD - VST - <Nom> » à copier
dans le dossier « VST2 Plugins » du disque de la MPC / Force.
"""
import argparse
import ctypes
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GOLEM_SRC = os.path.normpath(os.path.join(HERE, "..", "src"))


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def ident(name):
    return re.sub(r"[^A-Za-z0-9]", "", name) or "Golem"


def version_of_golem():
    cm = open(os.path.join(HERE, "..", "CMakeLists.txt"), encoding="utf-8").read()
    m = re.search(r"project\s*\([^)]*VERSION\s+([0-9.]+)", cm)
    return m.group(1) if m else "1.0.0"


def write_project(m, manufacturer, out_root, version):
    name = m["name"]
    proj = os.path.join(out_root, name)
    os.makedirs(os.path.join(proj, "src"), exist_ok=True)
    s = slug(name)
    manifest = {
        "name": name,
        "manufacturer": manufacturer,
        "version": version,
        "format": "vst2",
        "kind": m["kind"],
        "uid": m["uid"],
        "numInputs": 0 if m["kind"] == "instrument" else 2,
        "numOutputs": 2,
        "category": m.get("category", "Synth" if m["kind"] == "instrument" else "Effect"),
        "file32": "VST/%s-arm32.so" % s,
        "file64": "VST/%s-arm64.so" % s,
        "description": "OFAD GOLEM - %s (portage MPC)" % name,
        "license": "AGPL-3.0-or-later",
    }
    with open(os.path.join(proj, "plugin.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    v = [int(x) for x in (version.split(".") + ["0", "0", "0"])[:4]]
    vnum = v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3]
    cls = ident(name) + "Plugin"
    base = "InstrumentPlugin" if m["kind"] == "instrument" else "EffectPlugin"
    uid = m["uid"]
    cpp = f'''// Généré par mpc/build_mpc.py — ne pas modifier à la main. OFAD GOLEM — AGPL-3.0-or-later
#include "GolemMpc.h"
#include "{m["header"]}"

namespace
{{
mpc::PluginInfo makeInfo()
{{
    mpc::PluginInfo i;
    i.name = "{name}";
    i.vendor = "{manufacturer}";
    i.uniqueId = MPC_FOURCC ('{uid[0]}', '{uid[1]}', '{uid[2]}', '{uid[3]}');
    i.version = {vnum};
    i.isInstrument = {"true" if m["kind"] == "instrument" else "false"};
    i.numInputs = {manifest["numInputs"]};
    i.numOutputs = 2;
    return i;
}}
}}

class {cls} : public grv::mpcport::{base}<std::decay_t<decltype (*{m["make"]})>>
{{
public:
    {cls}() : {base} (makeInfo(), {m["make"]}) {{}}
}};

MPC_PLUGIN_ENTRY ({cls})
GOLEM_MPC_LAYOUT ({cls})
'''
    with open(os.path.join(proj, "src", ident(name) + ".cpp"), "w", encoding="utf-8") as f:
        f.write(cpp)
    return proj, s


def load_sdk_build(sdk, extra_defines, src_dir):
    sys.path.insert(0, os.path.join(sdk, "tools"))
    import build as b  # outil du kit
    flags = [f for f in b.COMMON_FLAGS if not f.startswith("-std=") and f not in ("-Wall", "-Wextra")]
    b.COMMON_FLAGS[:] = ["-std=c++20"] + flags + ["-w", "-I" + src_dir, "-I" + HERE] + ["-D" + d for d in extra_defines]
    return b


def stage_sources(stage_dir):
    """Copie des sources GOLEM pour la compilation des greffons (les sources ne sont jamais modifiées)."""
    if os.path.isdir(stage_dir):
        shutil.rmtree(stage_dir)
    shutil.copytree(GOLEM_SRC, stage_dir)


BANK_LINE = re.compile(r'^\s*if \(m\.type == "([^"]+)"\) bank = banks::\w+\(\);\s*$')


def filter_banks(stage_dir, key):
    """Banque d'usine du seul module compilé : les autres ne sont pas compilées (compilation 5 à 10 fois
    plus rapide, greffon plus petit ; le son et les réglages ne changent pas)."""
    src = os.path.join(GOLEM_SRC, "instruments", "FactoryBanks.h")
    dst = os.path.join(stage_dir, "instruments", "FactoryBanks.h")
    lines = open(src, encoding="utf-8").read().split("\n")
    out = []
    for ln in lines:
        m = BANK_LINE.match(ln)
        if m and m.group(1) != key:
            continue
        out.append(ln)
    open(dst, "w", encoding="utf-8").write("\n".join(out))


sys.path.insert(0, HERE)


def skins_only(args, cfg, mods):
    """Refait l'habillage (Plugin Skins) des greffons déjà construits, sans recompiler."""
    import skins as SK
    catalog = os.path.join(HERE, "..", "ui", "src", "engine", "catalog.json")
    bad = []
    for m in mods:
        proj = os.path.join(args.out, m["name"])
        folder = os.path.join(proj, "build", "%s - VST - %s" % (cfg["manufacturer"], m["name"]))
        params_json = os.path.join(proj, "build", "params.json")
        if not os.path.isfile(params_json) or not os.path.isdir(folder):
            print("%-24s pas construit" % m["name"])
            continue
        skin_dir = os.path.join(proj, "skins")
        if os.path.isdir(skin_dir):
            shutil.rmtree(skin_dir)
        SK.build(os.path.abspath(args.sdk), m["key"], m["kind"], params_json, catalog, skin_dir, title=m["name"])
        dst = os.path.join(folder, "Plugin Skins")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(skin_dir, dst)
        r = subprocess.run([sys.executable, os.path.join(args.sdk, "tools", "validate.py"), folder], capture_output=True, text=True)
        tail = [l for l in r.stdout.splitlines() if "errors" in l]
        print("%-24s %s" % (m["name"], tail[-1] if tail else r.stdout[-200:]))
        if r.returncode != 0:
            bad.append(m["name"])
    print("Échecs : %s" % (", ".join(bad) if bad else "aucun"))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("keys", nargs="*")
    ap.add_argument("--sdk", required=True)
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--native-only", action="store_true")
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--stage", default="main", help="nom du dossier de sources copiées (compilations en parallèle)")
    ap.add_argument("--skip-built", action="store_true", help="passer les greffons déjà construits")
    ap.add_argument("--kind", choices=["instrument", "effect"], help="seulement les instruments ou les effets")
    ap.add_argument("--skins-only", action="store_true", help="refaire seulement l'habillage des greffons déjà construits")
    args = ap.parse_args()

    cfg = json.load(open(os.path.join(HERE, "modules.json"), encoding="utf-8"))
    mods = [m for m in cfg["modules"] if (not args.keys or m["key"] in args.keys) and (not args.kind or m["kind"] == args.kind)]
    if not mods:
        print("aucun module"); return 1
    if args.skins_only:
        return skins_only(args, cfg, mods)
    stage_dir = os.path.join(args.out, "_src_" + args.stage)
    stage_sources(stage_dir)
    b = load_sdk_build(os.path.abspath(args.sdk), ["GRV_MPC=1", "GRV_LOW_MEMORY=1"] + args.define, stage_dir)
    version = version_of_golem()
    failed = []
    for m in mods:
        print("\n" + "=" * 72 + "\n" + m["name"] + "\n" + "=" * 72)
        proj, s = write_project(m, cfg["manufacturer"], args.out, version)
        folder = os.path.join(proj, "build", "%s - VST - %s" % (cfg["manufacturer"], m["name"]))
        if args.skip_built and os.path.isfile(os.path.join(folder, "VST", s + "-arm64.so")) \
                and os.path.isfile(os.path.join(folder, "VST", s + "-arm32.so")):
            print("  déjà construit : passé")
            continue
        filter_banks(stage_dir, m["key"])
        if b.main([proj, "--target", "native", "--no-validate"]) != 0:
            failed.append(m["name"]); continue
        lib = ctypes.CDLL(os.path.join(proj, "build", "native", s + ".so"))
        lib.golem_mpc_layout.restype = ctypes.c_char_p
        layout = lib.golem_mpc_layout().decode("utf-8")
        with open(os.path.join(proj, "ui.json"), "w", encoding="utf-8") as f:
            f.write(layout)
        # habillage graphique dessiné à la main (skins.py), sinon pages automatiques (ui.json)
        import skins as SK
        skin_dir = os.path.join(proj, "skins")
        if os.path.isdir(skin_dir):
            shutil.rmtree(skin_dir)
        catalog = os.path.join(HERE, "..", "ui", "src", "engine", "catalog.json")
        params_json = os.path.join(proj, "build", "params.json")
        if SK.build(os.path.abspath(args.sdk), m["key"], m["kind"], params_json, catalog, skin_dir, title=m["name"]):
            print("  habillage graphique : skins.py")
        if args.native_only:
            continue
        if b.main([proj]) != 0:
            failed.append(m["name"])
    print("\nTerminé. Échecs : %s" % (", ".join(failed) if failed else "aucun"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
