#!/usr/bin/env python3
# Ajoute les corrections mesurées par groovebox_presetcheck (fichier passé en argument) à levels.json.
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
LEVELS = os.path.join(HERE, "levels.json")
measured = json.load(open(sys.argv[1], encoding="utf-8"))
levels = json.load(open(LEVELS, encoding="utf-8")) if os.path.exists(LEVELS) else {}
moved = 0
for inst, m in measured.items():
    for name, db in m.items():
        if abs(db) < 0.3:
            continue
        # borné : un son hors de portée du réglage de volume est signalé par gen.py, pas poussé sans fin
        levels.setdefault(inst, {})[name] = max(-30.0, min(30.0, round(levels.get(inst, {}).get(name, 0.0) + db, 1)))
        moved += 1
json.dump(levels, open(LEVELS, "w", encoding="utf-8"), ensure_ascii=False, indent=1, sort_keys=True)
print(f"{moved} sons corrigés")
