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
ROOT_DIR = os.path.normpath(os.path.join(HERE, ".."))


def slug(name):
    # même règle que l'outil de construction du kit (tools/build.py : make_slug)
    return re.sub(r"[^a-z0-9]", "-", name.lower())


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
        "description": ("OFAD GOLEM - %s (MPC port)" if m.get("en_table") is not None else "OFAD GOLEM - %s (portage MPC)") % name,
        "license": "AGPL-3.0-or-later",
    }
    with open(os.path.join(proj, "plugin.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    v = [int(x) for x in (version.split(".") + ["0", "0", "0"])[:4]]
    vnum = v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3]
    cls = ident(name) + "Plugin"
    base = m.get("wrapper") or ("InstrumentPlugin" if m["kind"] == "instrument" else "EffectPlugin")
    uid = m["uid"]
    extras = m.get("extras") or {}
    ex_lines = []
    if extras:
        ex_lines.append("    grv::mpcport::Extras x;")
        if extras.get("folder"):
            ex_lines.append('    x.folder = "%s";' % extras["folder"])
            ex_lines.append("    x.exts = { %s };" % ", ".join('"%s"' % e for e in extras.get("exts", [".wav"])))
            ex_lines.append("    x.slots = %d;" % int(extras.get("slots", 1)))
            ex_lines.append('    x.label = "%s";' % extras.get("label", "Sample"))
            ex_lines.append("    x.firmware = %s;" % ("true" if extras.get("firmware") else "false"))
            ex_lines.append("    x.banks = %d;" % int(extras.get("banks", 0)))
        if extras.get("lanes"):
            ex_lines.append("    x.lanes = %d;" % int(extras["lanes"]))
            ex_lines.append("    x.laneNotes = %s;" % ("false" if extras.get("allNotes") else "true"))
        if extras.get("allNotes"):
            ex_lines.append("    x.allNotes = true;")
        if extras.get("ballOut"):
            ex_lines.append("    x.ballOut = true;")
        for label, text in extras.get("phrases", []):
            ex_lines.append('    x.phrases.push_back ({ %s, %s });' % (json.dumps(label), json.dumps(text)))
        ex_lines.append("    return x;")
    loader_code = ""
    loader_arg = ""
    if m.get("fm1"):
        loader_code = """
grv::engine::SampleBuffer* firmwareLoader (const std::string& path, const std::string& name, bool)
{
    std::vector<std::uint8_t> bytes;
    if (! mpc::readFile (path, bytes)) return nullptr;
    if (! grv::instruments::fmplayer::looksLikeFirmware (name, bytes.data(), bytes.size())) return nullptr;
    auto* s = new grv::engine::SampleBuffer();
    s->firmware = true;
    s->name = name;
    const auto err = grv::instruments::fmplayer::start (bytes, {}, s->runtime, name);
    if (! err.empty() || s->runtime == nullptr) { delete s; return nullptr; }
    return s;
}
"""
        loader_arg = ", &firmwareLoader"
    extras_fn = ("grv::mpcport::Extras makeExtras()\n{\n" + "\n".join(ex_lines) + "\n}\n") if ex_lines else ""
    ctor_args = "makeInfo(), %s" % m["make"]
    if ex_lines:
        ctor_args += ", makeExtras()" + loader_arg
    cpp = f"""// Généré par mpc/build_mpc.py — ne pas modifier à la main. OFAD GOLEM — AGPL-3.0-or-later
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
{extras_fn}{loader_code}}}

class {cls} : public grv::mpcport::{base}<std::decay_t<decltype (*{m["make"]})>>
{{
public:
    {cls}() : {base} ({ctor_args}) {{}}
}};

MPC_PLUGIN_ENTRY ({cls})
GOLEM_MPC_LAYOUT ({cls})
"""
    if m.get("en_table"):
        rows = ",\n".join("        {{ {}, {} }}".format(json.dumps(k, ensure_ascii=False), json.dumps(v, ensure_ascii=False))
                           for k, v in sorted(m["en_table"].items()))
        cpp += f"""
// Version anglaise : traduction des textes du module (mpc/i18n_en.py : ui/src/i18n/en.json + mpc/i18n_en_extra.json)
#include <unordered_map>
const char* golemEn (const std::string& fr)
{{
    static const std::unordered_map<std::string, const char*> table {{
{rows}
    }};
    const auto it = table.find (fr);
    return it == table.end() ? nullptr : it->second;
}}
"""
    with open(os.path.join(proj, "src", ident(name) + ".cpp"), "w", encoding="utf-8") as f:
        f.write(cpp)
    # fichiers livrés avec le greffon (Content/), copiés depuis le dépôt GOLEM
    for sub, src in (m.get("content") or {}).items():
        dst = os.path.join(proj, "Content", sub)
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(os.path.join(ROOT_DIR, src), dst)
    # émulateur FM (licence MIT) : sa notice accompagne le greffon
    if m.get("fm1"):
        os.makedirs(os.path.join(proj, "Content", "Licences"), exist_ok=True)
        shutil.copy(os.path.join(FM1_DIR, "LICENSE"), os.path.join(proj, "Content", "Licences", "fm1emu-MIT.txt"))
    # dossier des fichiers de l'utilisateur, avec sa notice
    folder_x = (m.get("extras") or {}).get("folder")
    if folder_x:
        os.makedirs(os.path.join(proj, "Content", folder_x), exist_ok=True)
        what = "firmwares .fwsc" if (m.get("extras") or {}).get("firmware") else "fichiers WAV (16, 24 ou 32 bits, mono ou stéréo)"
        with open(os.path.join(proj, "Content", folder_x, "LISEZ-MOI.txt"), "w", encoding="utf-8") as f:
            f.write("Copiez ici vos %s, puis redémarrez la machine (ou rechargez le greffon).\n"
                    "Copy your %s here, then restart the machine (or reload the plugin).\n"
                    % (what, "firmware .fwsc files" if (m.get("extras") or {}).get("firmware") else "WAV files (16, 24 or 32-bit, mono or stereo)"))
    return proj, s


def load_sdk_build(sdk, extra_defines, src_dir):
    sys.path.insert(0, os.path.join(sdk, "tools"))
    import build as b  # outil du kit
    flags = [f for f in b.COMMON_FLAGS if not f.startswith("-std=") and f not in ("-Wall", "-Wextra")]
    b.COMMON_FLAGS[:] = ["-std=c++20"] + flags + ["-w", "-I" + src_dir, "-I" + HERE] + ["-D" + d for d in extra_defines]
    return b


FM1_DIR = os.path.join(ROOT_DIR, "third_party", "fm1emu")
FM1_TRIPLES = {"native": "x86_64-unknown-linux-gnu", "arm32": "armv7-unknown-linux-gnueabihf", "arm64": "aarch64-unknown-linux-gnu"}


def build_fm1(cargo_dir):
    """Émulateur FM-1 (Rust, licence MIT) compilé pour le PC et les deux processeurs des MPC."""
    libs = {}
    for target, triple in FM1_TRIPLES.items():
        r = subprocess.run(["cargo", "build", "--release", "--offline", "-p", "fm1-bridge", "--target", triple],
                           cwd=FM1_DIR, env=dict(os.environ, CARGO_TARGET_DIR=cargo_dir))
        lib = os.path.join(cargo_dir, triple, "release", "libfm1_bridge.a")
        if r.returncode != 0 or not os.path.isfile(lib):
            raise SystemExit("émulateur FM-1 : la compilation Rust a échoué (%s)" % triple)
        libs[target] = lib
    return libs


def with_extra_libs(b, libs):
    """Ajoute, cible par cible, une bibliothèque statique à l'édition des liens du kit."""
    B = b.Builder
    if not hasattr(B, "_golem_orig"):
        B._golem_orig = (B.build_native, B.build_arm)
    orig_native, orig_arm = B._golem_orig
    if not libs:
        B.build_native, B.build_arm = orig_native, orig_arm
        return

    def build_native(self, output):
        saved = self.sources
        self.sources = saved + [libs["native"], "-lpthread", "-ldl"]
        try:
            return orig_native(self, output)
        finally:
            self.sources = saved

    def build_arm(self, target, output):
        saved = self.sources
        self.sources = saved + [libs[target], "-lpthread", "-ldl"]
        try:
            return orig_arm(self, target, output)
        finally:
            self.sources = saved
    B.build_native, B.build_arm = build_native, build_arm


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

# textes de l'adaptateur (GolemMpc.h, fonction L) : traduits dans la table de chaque greffon anglais
WRAPPER_TEXTS = ["Disposition pads", "Notes 36-51", "Do majeur", "Do mineur", "Accord porteuse", "Une note", "Octave",
                 "Quinte", "Majeur", "Mineur", "Sus4", "Septieme", "Mineur 7", "Neuvieme", "Dossier", "Bille X", "Bille Y", "Note porteuse", "Piste clavier", "Muet", "Solo", "Non", "Oui",
                 "1 pad = 1 piste", "Clavier (ci-dessous)", "Disques", "Couche", "Sample",
                 "sample du greffon", "pas de sample (enregistreur)", "Piste", "Pad", "Zone"]


def _strings(x, out):
    """Tous les textes d'une fiche du catalogue (sauf identifiants techniques)."""
    if isinstance(x, str):
        out.add(x)
    elif isinstance(x, list):
        for v in x:
            _strings(v, out)
    elif isinstance(x, dict):
        for k, v in x.items():
            if k in ("id", "type", "visual", "tab", "unit", "padKinds", "form", "sampleRuntime"):
                continue
            _strings(v, out)


def english_module(m):
    """Module de la version anglaise : nom, identifiant et table de traduction propres."""
    import i18n_en as I
    cat = json.load(open(os.path.join(ROOT_DIR, "ui", "src", "engine", "catalog.json"), encoding="utf-8"))
    entry = next((e for grp in ("instruments", "effects") for e in cat[grp] if e["type"] == m["key"]), None)
    texts = set(WRAPPER_TEXTS)
    if entry:
        _strings(entry, texts)
    table = {}
    for t in texts:
        en = I.translate(t)
        if en != t:
            table[t] = en
    e = dict(m)
    e["name"] = I.plugin_name(m["name"], entry["name"] if entry else None)
    e["base_name"] = e["name"].split(" - ", 1)[1]
    e["uid"] = I.plugin_uid(m["uid"])
    e["en_table"] = table
    return e


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
        SK.build(os.path.abspath(args.sdk), m["key"], m["kind"], params_json, catalog, skin_dir, title=m.get("base_name", m["name"]), lang=args.lang)
        dst = os.path.join(folder, "Plugin Skins")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(skin_dir, dst)
        if args.prune:
            shutil.rmtree(skin_dir, ignore_errors=True)
        if args.lang == "en":
            with open(os.path.join(proj, "untranslated.txt"), "w", encoding="utf-8") as f:
                f.write("\n".join(sorted(SK.MISSING)) + ("\n" if SK.MISSING else ""))
            SK.MISSING.clear()
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
    ap.add_argument("--lang", choices=["fr", "en"], default="fr", help="langue des greffons (en : noms, réglages, sons et écrans en anglais)")
    ap.add_argument("--prune", action="store_true", help="après chaque greffon réussi, effacer la bibliothèque native et la copie de l'habillage (place disque)")
    args = ap.parse_args()

    cfg = json.load(open(os.path.join(HERE, "modules.json"), encoding="utf-8"))
    mods = [m for m in cfg["modules"] if (not args.keys or m["key"] in args.keys) and (not args.kind or m["kind"] == args.kind)]
    if args.lang == "en":
        mods = [english_module(m) for m in mods]
        if args.out == os.path.join(HERE, "out"):
            args.out = os.path.join(HERE, "out_en")
        args.define = args.define + ["GRV_MPC_EN=1"]
        # écrans de tête des maquettes capturés en anglais (mockskin/build_cache.py avec GOLEM_MOCK_LANG=en)
        os.environ.setdefault("GOLEM_MOCK_DIR", os.path.join(HERE, "out", "mockcache_en"))
        # façades capturées avec l'interface de GOLEM en anglais (face_capture.py avec GOLEM_FACE_LANG=en)
        os.environ.setdefault("GOLEM_FACE_DIR", os.path.join(HERE, "out", "_facecap_en"))
    if not mods:
        print("aucun module"); return 1
    if args.skins_only:
        return skins_only(args, cfg, mods)
    stage_dir = os.path.join(args.out, "_src_" + args.stage)
    stage_sources(stage_dir)
    b = load_sdk_build(os.path.abspath(args.sdk), ["GRV_MPC=1", "GRV_LOW_MEMORY=1"] + args.define, stage_dir)
    version = version_of_golem()
    failed = []
    fm1_libs = None
    base_flags = list(b.COMMON_FLAGS)
    for m in mods:
        print("\n" + "=" * 72 + "\n" + m["name"] + "\n" + "=" * 72)
        proj, s = write_project(m, cfg["manufacturer"], args.out, version)
        folder = os.path.join(proj, "build", "%s - VST - %s" % (cfg["manufacturer"], m["name"]))
        if args.skip_built and os.path.isfile(os.path.join(folder, "VST", s + "-arm64.so")) \
                and os.path.isfile(os.path.join(folder, "VST", s + "-arm32.so")):
            print("  déjà construit : passé")
            continue
        filter_banks(stage_dir, m["key"])
        if m.get("fm1"):
            if fm1_libs is None:
                fm1_libs = build_fm1(os.path.join(args.out, "_fm1"))
            with_extra_libs(b, fm1_libs)
            b.COMMON_FLAGS[:] = base_flags + ["-DGROOVEBOX_FM1=1", "-I" + os.path.join(FM1_DIR, "include")]
        else:
            with_extra_libs(b, None)
            b.COMMON_FLAGS[:] = base_flags
        if b.main([proj, "--target", "native", "--no-validate"]) != 0:
            failed.append(m["name"]); continue
        lib = ctypes.CDLL(os.path.join(proj, "build", "native", s + ".so"))
        lib.golem_mpc_layout.restype = ctypes.c_char_p
        layout = lib.golem_mpc_layout().decode("utf-8")
        with open(os.path.join(proj, "ui.json"), "w", encoding="utf-8") as f:
            f.write(layout)
        if m.get("mpc_only"):
            # greffon propre à la MPC (pas encore dans GOLEM) : sa fiche rejoint mpc/catalog_mpc.json
            lib.golem_mpc_catalog.restype = ctypes.c_char_p
            entry = json.loads(lib.golem_mpc_catalog().decode("utf-8"))
            cpath = os.path.join(HERE, "catalog_mpc.json")
            extra = json.load(open(cpath, encoding="utf-8")) if os.path.isfile(cpath) else {"instruments": [], "effects": []}
            grp = "instruments" if m["kind"] == "instrument" else "effects"
            extra[grp] = [e for e in extra.get(grp, []) if e["type"] != entry["type"]] + [entry]
            with open(cpath, "w", encoding="utf-8") as f:
                json.dump(extra, f, ensure_ascii=False, indent=1)
                f.write("\n")
        # habillage graphique dessiné à la main (skins.py), sinon pages automatiques (ui.json)
        import skins as SK
        skin_dir = os.path.join(proj, "skins")
        if os.path.isdir(skin_dir):
            shutil.rmtree(skin_dir)
        catalog = os.path.join(HERE, "..", "ui", "src", "engine", "catalog.json")
        params_json = os.path.join(proj, "build", "params.json")
        if SK.build(os.path.abspath(args.sdk), m["key"], m["kind"], params_json, catalog, skin_dir, title=m.get("base_name", m["name"]), lang=args.lang):
            print("  habillage graphique : skins.py")
        if args.lang == "en":
            # textes restés en français dans les images (à ajouter à mpc/i18n_en_extra.json)
            with open(os.path.join(proj, "untranslated.txt"), "w", encoding="utf-8") as f:
                f.write("\n".join(sorted(SK.MISSING)) + ("\n" if SK.MISSING else ""))
            SK.MISSING.clear()
        if args.native_only:
            continue
        if b.main([proj]) != 0:
            failed.append(m["name"])
        elif args.prune:
            shutil.rmtree(os.path.join(proj, "build", "native"), ignore_errors=True)
            shutil.rmtree(os.path.join(proj, "skins"), ignore_errors=True)
    print("\nTerminé. Échecs : %s" % (", ".join(failed) if failed else "aucun"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
