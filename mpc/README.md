# OFAD GOLEM pour MPC et Force (greffons autonomes)

Portage des instruments et effets de GOLEM en greffons pour les MPC et la Force en mode autonome
(version 3.10.1 et plus, « External Plugin Support »), avec le kit de greffons MPC (VST 2.4 propre,
licence MIT, à cloner à part).

| Fichier | Rôle |
|---|---|
| `GolemMpc.h` | adaptateur : un module GOLEM devient un greffon MPC (réglages, notes, tempo, programmes) |
| `modules.json` | modules portés et leurs codes uniques PERMANENTS |
| `build_mpc.py` | construit les greffons (C++20, arm32 + arm64) avec `tools/build.py` du kit |
| `skins.py`, `skin_kit.py` | habillage de l'écran tactile (pages, boutons, enveloppes, images calculées) |
| `faces.json`, `faces_to_json.cjs` | façades GOLEM (`ui/src/screens/faces/layouts.ts`) converties pour l'habillage |
| `preview_skin.py` | aperçus des pages (images 1280 × 800) |
| `make_release.py`, `release_texts.py` | dossier de release (zips, sources, captures, textes) |
| `LISEZ-MOI-FORCE.txt` | installation sur la machine |

Construire :

```
python3 mpc/build_mpc.py --sdk <kit de greffons MPC> [clé ...]      # tous les modules sans clé
python3 mpc/build_mpc.py --sdk <kit> --skins-only                  # refaire seulement l'habillage
python3 mpc/make_release.py --version 1.0.0 --out <dossier>
```

Les sources GOLEM ne sont jamais modifiées par la construction (copie dans `mpc/out/_src_*`, banque
d'usine du seul module compilé). Rien n'est installé sur une machine : le résultat est un dossier
« OFAD - VST - <Nom> » à copier dans « VST2 Plugins ».
