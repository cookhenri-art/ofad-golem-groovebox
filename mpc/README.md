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
| `face_capture.py` | capture des façades GOLEM (synthés) et des pédales / racks (effets) dans l'aperçu de l'interface : panneaux, positions des réglages, boutons et tirettes animés, couleurs → `mpc/out/_facecap` |
| `mockskin/` | écrans de tête d'après les maquettes validées (consoles A, écrans héros B, façades C) : `maquettes.py` / `maquettes_main.py` (fiches, réglages existants seulement), `render.py` (gabarits → page 1280 × 628), `capture.py` (fond, bandes d'images, boutons, valeurs en direct), `build_cache.py` → `mpc/out/mockcache` ; polices sous licence OFL dans `mockskin/fonts` |
| `instruments/` | greffons propres à la MPC, pas encore dans GOLEM (`GamerLover.h` : puces de consoles de jeu 8 et 16 bits, batterie de console, sortie d'époque) ; leur fiche est écrite par `build_mpc.py` dans `catalog_mpc.json` (lue par l'habillage et les maquettes) |
| `preview_skin.py` | aperçus des pages (images 1280 × 800) |
| `make_release.py`, `release_texts.py` | dossier de release (zips, sources, captures, textes) |
| `LISEZ-MOI-FORCE.txt` | installation sur la machine |

Construire :

```
(cd ui && npm run build)                                            # l'aperçu de l'interface (ui/dist)
python3 mpc/face_capture.py mpc/out/_facecap                        # façades des instruments
python3 mpc/face_capture.py mpc/out/_facecap --fx                   # pédales et racks des effets
python3 mpc/mockskin/build_cache.py                                 # écrans de tête des maquettes (Playwright)
python3 mpc/build_mpc.py --sdk <kit de greffons MPC> [clé ...]      # tous les modules sans clé
python3 mpc/build_mpc.py --sdk <kit> --skins-only                  # refaire seulement l'habillage
python3 mpc/make_release.py --version 1.0.0 --out <dossier>
```

Les sources GOLEM ne sont jamais modifiées par la construction (copie dans `mpc/out/_src_*`, banque
d'usine du seul module compilé). Rien n'est installé sur une machine : le résultat est un dossier
« OFAD - VST - <Nom> » à copier dans « VST2 Plugins ».

Écran de tête (touche 1) : les instruments qui ont une maquette validée l'ont telle quelle — fond rendu
depuis la maquette, boutons rotatifs, tirettes et barres en bandes d'images découpées dans le rendu,
choix en boutons allumé / éteint, valeurs écrites en direct par la machine. Écrans héros : la colonne
des modules change la rangée des 8 boutons (valeur locale de l'écran) ; schéma FM en bande d'images liée
à l'algorithme ; pavé XY pour le centre de l'orbite (Orbiteur) et la bille de Glutamake (réglages
« Bille X / Y » en lecture seule, ajoutés à la fin, écrits par le moteur).

Habillage : chaque écran porte 16 réglages au plus (autant que de Q-Links, deux rangées de 8 sur la
Force) et a sa propre touche, nommée d'après ce qu'on y règle (OSC, FILTRE, ENV...). Les synthés
reprennent les panneaux de leur façade GOLEM ; les effets affichent leur pédale ou leur rack ; les
batteries ont une touche par son (1 BD, 2 SD...) avec deux écrans chacun ; le premier écran d'un son
commence par SON niveau, puis « Muet » et « Solo » (réglages ajoutés par `GolemMpc.h` : `mpc_muteNN`,
`mpc_soloNN` ; un son coupé est joué à -48 dB, son niveau réglé ne change pas). Le volume du kit, les
réglages communs et le volume du greffon sont sur la touche GLOBAL (une seule fois). Toucher : les
barres de niveau des consoles et les tirettes de façade ont une zone de toucher élargie (bords
transparents de la bande d'images) ; sur les écrans héros (B), les six cases de valeurs à droite se
touchent (Q-Link ; deux touches : la liste) et rien de dessiné n'est seulement décoratif (Hot Sauce :
plus de faux curseur « mélange A ↔ B », les niveaux A et B sont dans les cases ; New Metal / Old Metal :
toucher le schéma règle l'algorithme). Noms : la famille devant
(« Drum - Boum 80 », « Reverb - Nave ») pour que la machine les range par type. Les vocodeurs sont des
effets : le son de la piste est la voix, la porteuse joue la note et l'accord réglés.
