#!/usr/bin/env python3
"""OFAD GOLEM — capture de toutes les maquettes validées pour l'habillage MPC — AGPL-3.0-or-later

    python3 mpc/mockskin/build_cache.py [dossier] [clé ...]

Pour chaque maquette (maquettes.py), écrit dans <dossier>/<type d'instrument>/ :
  - les images (fond, bandes d'images, boutons) ;
  - screens.json : les écrans à poser en tête de l'habillage, avec leurs états.
États d'un écran :
  - A (console 8 tranches) : une page par banque de 8 sons (sons 1-8, 9-16), sur la même touche ;
  - B (écran héros) : un état par module de la colonne de gauche (rangée des 8 boutons), choisi
    au toucher par une valeur locale de l'écran (Local) ;
  - C (façade) : un seul écran.
"""
import copy
import json
import os
import shutil
import sys
import tempfile
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def fold(s):
    return "".join(c for c in unicodedata.normalize("NFD", s.lower()) if unicodedata.category(c) != "Mn")


# module de la colonne B → sections du catalogue (onglet, mots du nom)
NAV_RULES = [
    (("tables", "osc a"), ("osc",), ("oscillateur 1", "ondes", "table", "osc. a", "oscillateur a")),
    (("osc b",), ("osc",), ("oscillateur 2", "osc. b", "oscillateur b")),
    (("osc 1",), ("osc",), ("oscillateur 1",)),
    (("osc 2",), ("osc",), ("oscillateur 2",)),
    (("coins",), ("osc",), ("coin",)),
    (("seq",), ("mod",), ("sequence a",)),
    (("mutants",), ("osc",), ("mutant",)),
    (("moteurs", "masques", "sample", "sources", "fm", "oper", "opér", "osc"), ("osc",), ()),
    (("utilit", "sous/bruit"), (), ("sous", "bruit", "utilit")),
    (("delta",), (), ("delta",)),
    (("mix",), (), ("melang", "mix", "niveaux")),
    (("filtre",), ("filter",), ()),
    (("env. op",), ("env",), ("op",)),
    (("env. f",), ("env",), ("filtre",)),
    (("env. a", "ampli"), ("amp",), ()),
    (("env",), ("env", "modenv", "amp"), ()),
    (("lfo", "modul", "mod."), ("mod", "modlfo"), ("lfo",)),
    (("molette",), ("mod", "matrix"), ("molette", "pitch", "modulation")),
    (("matrice", "macros"), ("matrix",), ()),
    (("arpege",), ("arp",), ()),
    (("hauteur",), (), ("hauteur", "pitch")),
    (("disto", "effets", "retard"), ("fx",), ("disto", "retard", "effet", "chorus")),
    (("global", "jeu"), ("global",), ()),
    (("fichiers",), ("global", "osc"), ("fichier", "sample")),
]


def nav_macros(mod, label, taken):
    """8 réglages existants du module nommé dans la colonne B (aucun réglage inventé)."""
    lab = fold(label)
    pids = [p["id"] for p in mod["params"]]
    secs = mod.get("sections", [])
    picked = []
    for keys, tabs, words in NAV_RULES:
        if not any(lab.startswith(fold(k)) or fold(k) == lab for k in keys):
            continue
        cand = []
        for s in secs:
            nm = fold(s["name"])
            if words and any(w in nm for w in words):
                cand.append(s)
            elif not words and s.get("tab") in tabs:
                cand.append(s)
        if not cand and words and tabs:
            cand = [s for s in secs if s.get("tab") in tabs]
        for s in cand:
            for i in s["params"]:
                if 0 <= i < len(pids):
                    pid = pids[i]
                    p = mod["params"][i]
                    if pid not in picked and not (p.get("choices") and len(p["choices"]) <= 1):
                        picked.append(pid)
        if picked:
            break
    return picked[:8]


def build(out_root, only=()):
    mk = tempfile.mkdtemp(prefix="maquettes_")
    os.environ["MAQUETTES_OUT"] = mk
    import capture as CP
    import render as RD
    import maquettes  # noqa: F401  (écrit les fiches)
    import maquettes_gen as G
    try:
        import maquettes_main  # écrans faits à la main (Machine, Pieuvre, Orbiteur, Fatsub, Glutamake)
        maquettes_main.write_all(mk)
    except ImportError:
        pass
    keys = sorted(f[:-5] for f in os.listdir(mk) if f.endswith(".json"))
    index = {}
    for key in keys:
        b = json.load(open(os.path.join(mk, key + ".json"), encoding="utf-8"))
        t = b["type"]
        if only and key not in only and t not in only:
            continue
        mod = G.INST[t]
        dst = os.path.join(out_root, t)
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        cap = CP.Capture(dst, "mk")
        body = RD.screen_body(b["tpl"])
        kind = b["tpl"][0]
        D = b["D"]
        screens = []
        tab = (D.get("tabs") or [{"n": "Jouer"}])[0]["n"]
        if kind == "A":
            banks = [D] + list(D.get("_pages") or [])
            kit = D.get("_kit")
            if kit:
                kt, big, small, chip, lvl, x, cname = kit
                ids = {p["id"] for p in mod["params"]}
                pref = "p" if "p09_" + big[0] in ids else ("z" if "z09_" + big[0] in ids else None)
                if pref:
                    D2 = copy.deepcopy(D)
                    D2["strips"] = list(G.kit_strips(kt, range(9, 17), big, small, chip, lvl, x, cname))
                    for i, bk in enumerate(D2["banks"]):
                        on = i == 1
                        bk.update({"bg": D["accent"] if on else "transparent", "fg": "#131418" if on else "#c9cbd2",
                                   "border": D["accent"] if on else "#33353d"})
                    banks.append(D2)
            pages = []
            for bi, Db in enumerate(banks):
                spec = cap.screen(body, Db, "a%d" % bi)
                pages.append(spec)
            screens.append({"kind": "A", "tab": tab, "pages": pages})
        elif kind == "B":
            base = cap.screen(body, D, "b0")
            film = D.get("_film")
            hero = [w for w in base["widgets"] if w["t"] == "hero"]
            if film and hero:
                Ds = []
                for fr in film["frames"]:
                    Df = copy.deepcopy(D)
                    Df.update(fr)
                    Ds.append(Df)
                strip = cap.region_frames(body, Ds, hero[0]["box"], "b0")
                base["widgets"].append({"t": "film", "p": film["p"], "box": hero[0]["box"], "strip": strip, "frames": len(Ds)})
            states = []
            taken = [m["id"] for m in D.get("macros", [])]
            row = [w for w in base["widgets"] if w["t"] == "macrorow"]
            for i, nv in enumerate(D.get("nav", [])):
                if i == 0 or not row:
                    continue
                navm = D.get("_navmacros")
                pids = navm[i] if navm and i < len(navm) else nav_macros(mod, nv["n"], taken)
                if not pids:
                    states.append(None)
                    continue
                Di = copy.deepcopy(D)
                for j, n2 in enumerate(Di["nav"]):
                    on = j == i
                    n2.update({"bg": D["accent"] if on else D["hero"], "fg": D["bg"] if on else D["text"],
                               "border": D["accent"] if on else D["line"]})
                Di["macros"] = [dict(G.kv(t, pid), k=j + 1, foot="au toucher") for j, pid in enumerate(pids)]
                spec = cap.screen(body, Di, "b%d" % i, regions=[row[0]["box"]])
                keep = [w for w in spec["widgets"] if w.get("slot", -1) >= 0]
                ids = {w["mkid"] for w in keep}
                states.append({"nav": i, "region": spec["regions"][0], "box": row[0]["box"], "widgets": keep,
                               "labels": [lb for lb in spec["labels"] if lb["host"] in ids]})
                os.remove(os.path.join(dst, spec["bg"]))
            screens.append({"kind": "B", "tab": tab, "pages": [base], "states": states, "nav_n": len(D.get("nav", []))})
        else:
            screens.append({"kind": "C", "tab": tab, "pages": [cap.screen(body, D, "c0")]})
        cap.close()
        json.dump({"key": key, "type": t, "tpl": b["tpl"], "screens": screens},
                  open(os.path.join(dst, "screens.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        index[t] = key
        print("%-28s %-10s %d écran(s)" % (key, t, sum(len(s["pages"]) for s in screens)), flush=True)
    shutil.rmtree(mk, ignore_errors=True)
    return index


if __name__ == "__main__":
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "out", "mockcache")
    build(root, tuple(sys.argv[2:]))
