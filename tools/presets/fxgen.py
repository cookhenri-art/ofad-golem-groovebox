#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# v0.47 (maj. 5 nov.) : écrit src/effects/FxBanks.h (réglages d'usine supplémentaires des effets) à partir de fx_banks.py,
# après contrôle des réglages (identifiants connus), des noms (pas de doublon) et du nombre (au moins 20 par effet).
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from banks_common import cpp_string, cpp_number
from fx_banks import FX

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "src", "effects", "FxBanks.h")
CAT = os.path.join(ROOT, "ui", "src", "engine", "catalog.json")
MIN = 20


def main():
    cat = json.load(open(CAT, encoding="utf-8"))
    effects = {e["type"]: e for e in cat["effects"]}
    ok = True
    for t, items in FX.items():
        e = effects[t]
        ids = {p["id"] for p in e["params"]}
        old = [p["name"] if isinstance(p, dict) else p for p in e["presets"] if not (isinstance(p, str) and False)]
        # les noms déjà livrés (sans ceux de cette banque, si le catalogue a déjà été régénéré)
        mine = {n for n, _ in items}
        base = [n for n in old if n not in mine]
        names = [n for n, _ in items]
        dup = {n for n in names if names.count(n) > 1} | (set(names) & set(base))
        bad = {k for _, v in items for k in v if k not in ids}
        pm = {p["id"]: p for p in e["params"]}
        bad |= {f"{n}:{k}={x}" for n, v in items for k, x in v.items() if k in pm and pm[k].get("min") is not None
                and not (pm[k]["min"] - 1e-6 <= float(x) <= pm[k]["max"] + 1e-6)}
        if dup or bad:
            print(f"{t} : doublons {sorted(dup)} réglages inconnus {sorted(bad)}"); ok = False
        if len(base) + len(items) < MIN:
            print(f"{t} : {len(base) + len(items)} réglages (au moins {MIN})"); ok = False
    for t in effects:
        if t not in FX and len(effects[t]["presets"]) < MIN:
            print(f"{t} : sans banque, {len(effects[t]['presets'])} réglages"); ok = False
    if not ok:
        sys.exit(1)
    out = ["/*", "    Groovebox — OFAD — AGPL-3.0-or-later", "",
           "    v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires des effets (au moins 20 par effet), construits sur les usages",
           "    courants de chaque famille d'effet ; aucun préréglage d'un autre fabricant n'est recopié.", "",
           "    FICHIER ÉCRIT PAR tools/presets/fxgen.py : modifier tools/presets/fx_banks.py, pas ici.", "*/", "",
           "#pragma once", "", '#include "../engine/Params.h"', "", "#include <string>", "#include <vector>", "",
           "namespace grv::effects", "{", "", "/** Ajoute les réglages d'usine de la banque à la description de l'effet (fin de sa description). */",
           "inline void addFxBank (grv::engine::ModuleDesc& m)", "{"]
    total = 0
    for t, items in FX.items():
        out.append(f"    if (m.type == {cpp_string(t)})")
        out.append("    {")
        for name, vals in items:
            pairs = ", ".join("{ %s, %s }" % (cpp_string(k), cpp_number(v)) for k, v in vals.items())
            out.append("        m.presets.push_back ({ %s, { %s } });" % (cpp_string(name), pairs))
            total += 1
        out.append("    }")
    out += ["}", "", "} // namespace grv::effects", ""]
    open(OUT, "w", encoding="utf-8").write("\n".join(out))
    print(f"{total} réglages -> {OUT}")


if __name__ == "__main__":
    main()
