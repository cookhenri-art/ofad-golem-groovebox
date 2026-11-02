#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# v0.46 : écrit docs/CATALOGUE.md (tous les instruments et effets, avec leur vignette, leur principe
# et leurs sons d'usine par catégorie) et met à jour les tableaux du README entre les repères
# <!-- CATALOGUE:… --> à partir de ui/src/engine/catalog.json (exporté par groovebox_catalog).
#   python3 scripts/gen-docs.py
import json, os, re
from collections import Counter, OrderedDict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
cat = json.load(open(os.path.join(ROOT, "ui", "src", "engine", "catalog.json"), encoding="utf-8"))
ORDER = ["Basses", "Leads", "Nappes", "Plucks", "Claviers", "Cordes", "Cuivres", "Arpèges", "Effets", "Acide", "Voix"]


def counts(m):
    c = Counter(x for x in (m.get("presetCats") or []) if x and x != "Init")
    parts = [f"{k} {c[k]}" for k in ORDER if c.get(k)] + [f"{k} {v}" for k, v in c.items() if k not in ORDER]
    return ", ".join(parts)


def thumb(path):
    return f"<img src=\"{path}\" width=\"120\" alt=\"\">" if os.path.exists(os.path.join(ROOT, path)) else ""


def esc(s):
    return (s or "").replace("|", "\\|").replace("\n", " ")


def instruments_table(with_img, rel):
    fams = OrderedDict()
    for m in cat["instruments"]:
        fams.setdefault(m["family"], []).append(m)
    out = []
    for fam, ms in fams.items():
        out.append(f"### {fam}\n")
        out.append(("| | " if with_img else "| ") + "Instrument | Principe | Sons d'usine |")
        out.append(("|---|" if with_img else "|") + "---|---|---|")
        for m in ms:
            n = len([p for p in (m.get("presets") or []) if p != "Init"])
            img = thumb(f"ui/public/thumbs/{m['type']}.webp").replace("src=\"", f"src=\"{rel}") if with_img else ""
            sounds = (f"{n}" + (f" ({counts(m)})" if counts(m) else "")) if n else "—"
            out.append((f"| {img} " if with_img else "") + f"| **{esc(m['name'])}** | {esc(m['principle'])} | {sounds} |")
        out.append("")
    return "\n".join(out)


def effects_table(with_img, rel):
    fams = OrderedDict()
    for m in cat["effects"]:
        fams.setdefault(m["family"], []).append(m)
    out = []
    for fam, ms in fams.items():
        out.append(f"### {fam}\n")
        out.append(("| | " if with_img else "| ") + "Effet | Principe | Préréglages |")
        out.append(("|---|" if with_img else "|") + "---|---|---|")
        for m in ms:
            n = len([p for p in (m.get("presets") or []) if p != "Init"])
            img = thumb(f"ui/public/fxthumbs/{m['type']}.webp").replace("src=\"", f"src=\"{rel}") if with_img else ""
            out.append((f"| {img} " if with_img else "") + f"| **{esc(m['name'])}** | {esc(m['principle'])} | {n} |")
        out.append("")
    return "\n".join(out)


def main():
    ni, ne = len(cat["instruments"]), len(cat["effects"])
    total = sum(len([p for p in (m.get("presets") or []) if p != "Init"]) for m in cat["instruments"])
    doc = ["# Catalogue des instruments et des effets", "",
           "Fichier écrit par `scripts/gen-docs.py` à partir du catalogue du moteur (`groovebox_catalog`) : ne pas le modifier à la main.",
           "Les noms sont propres au Groovebox ; chaque module reprend le **principe** d'une famille de machines, jamais son nom,",
           "sa façade ni ses sons.", "",
           f"**{ni} instruments** ({total} sons d'usine) et **{ne} effets**.", "",
           "## Instruments", "", instruments_table(True, "../"), "## Effets", "", effects_table(True, "../")]
    open(os.path.join(ROOT, "docs", "CATALOGUE.md"), "w", encoding="utf-8").write("\n".join(doc))

    readme_p = os.path.join(ROOT, "README.md")
    r = open(readme_p, encoding="utf-8").read()
    def put(tag, body):
        nonlocal r
        pat = re.compile(rf"<!-- CATALOGUE:{tag} -->.*?<!-- /CATALOGUE:{tag} -->", re.S)
        block = f"<!-- CATALOGUE:{tag} -->\n{body}\n<!-- /CATALOGUE:{tag} -->"
        assert pat.search(r), f"repère {tag} absent du README"
        r = pat.sub(lambda _m: block, r)
    put("INSTRUMENTS", f"{ni} instruments, {total} sons d'usine rangés par catégories. Le détail (vignettes, principe, sons par catégorie) est dans [docs/CATALOGUE.md](docs/CATALOGUE.md).\n\n" + instruments_table(False, ""))
    put("EFFETS", f"{ne} effets. Le détail (vignettes, principe) est dans [docs/CATALOGUE.md](docs/CATALOGUE.md).\n\n" + effects_table(False, ""))
    # v0.46 fix9 : le README est la page de présentation ; les comptes du code sont dans docs/TECHNIQUE.md
    r = re.sub(r"Les \d+ instruments\*\* \(cliquer", f"Les {ni} instruments** (cliquer", r)
    r = re.sub(r"Les \d+ effets\*\* \(cliquer", f"Les {ne} effets** (cliquer", r)
    open(readme_p, "w", encoding="utf-8").write(r)
    tech_p = os.path.join(ROOT, "docs", "TECHNIQUE.md")
    t = open(tech_p, encoding="utf-8").read()
    t = re.sub(r"\| `src/instruments/` \| Les \d+ instruments \|", f"| `src/instruments/` | Les {ni} instruments |", t)
    t = re.sub(r"\| `src/effects/` \| Les \d+ effets \|", f"| `src/effects/` | Les {ne} effets |", t)
    open(tech_p, "w", encoding="utf-8").write(t)
    print(f"docs/CATALOGUE.md et README.md : {ni} instruments, {ne} effets, {total} sons")


if __name__ == "__main__":
    main()
