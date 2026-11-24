# Groovebox — correspondances des noms (document personnel, à ne pas publier)

Version 0.46.0 — 02/11/2026 (familles Synthés FM et Drones). Version 0.35.1 — 31/10/2026 (noms propres des effets, en anglais). Version 0.31.3 — 30/10/2026 (noms des instruments). Pour chaque instrument et effet : le nom dans le Groovebox, ce qu'il est,
et la machine ou le logiciel dont il reprend le principe (l'architecture seulement, jamais le nom, la façade
ni les sons d'usine).

> Ce document cite des marques pour **ton** usage. Ne le joins pas à une diffusion : dans le logiciel,
> seuls les noms de la première colonne apparaissent.

## Instruments

| Nom Groovebox | Identifiant interne | Sous-titre (ce que c'est) | Principe repris de |
|---|---|---|---|
| **Machine électronique** | `drumA` | Batterie analogique à machines | Elektron Analog Rytm MKII |
| **Boum 80** | `drum80` | Boîte analogique du début des années 80 | Roland TR-808 |
| **Club 90** | `drum90` | Boîte hybride de la fin des années 80 | Roland TR-909 |
| **Dragon Fist** | `drumImp` | Boîte compacte et agressive | Arturia DrumBrute Impact |
| **Cut of the Beat** | `drumNrv` | Batterie à échantillons | Xfer Records Nerve |
| **Kick Killer** | `dfam` | Percussion à séquenceur | Moog DFAM |
| **Acid Lover** | `acid` | Basse à séquenceur (acide) | Roland TB-303 |
| **Fatsub** | `mono` | Basse monophonique | Moog Sub 37 / Subsequent 37 |
| **Hurleur 200** | `ms20` | Mono semi-modulaire | Korg MS-20 |
| **Mono First** | `sh101` | Mono à mélangeur | Roland SH-101 |
| **Poly Five** | `poly5` | Polyphonique analogique | Sequential Prophet-5 |
| **Chorus 80** | `polyj` | Polyphonique à chorus | Roland Juno-60 (et Juno-106) |
| **Micropointe** | `hybrid` | Polyphonique hybride | Novation Peak |
| **Synthonde** | `morph` | Morphing d'ondes | ASM Hydrasynth |
| **Bignappe One** | `pads` | Nappes | LennarDigital Sylenth1 |
| **Ondes Bretzel** | `blo` | Poly à tables d'ondes | Waldorf Blofeld (héritage PPG) |
| **État de Grâce** | `ob6` | Poly à filtre d'état | Oberheim OB-6 (filtre SEM) |
| **Orbiteur** | `orbit` | Synthèse vectorielle à orbite | synthés de bureau tchèques à orbite (les « orbes ») |
| **Cutsample** | `sampler` | Sampler à découpes | Stem Sampler Pro (ton logiciel) |
| **Chromasample** | `keys` | Sampler chromatique | Stem Sampler Pro (ton logiciel) |
| **Poussière de Vinyle** | `sp` | Sampler à pads lo-fi | Roland SP-404 (esprit des petits samplers hip-hop) |
| **Pieuvre 8** | `octa` | Sampler de performance à 8 pistes | Elektron Octatrack — v0.40 : réserve commune de 8 × 16 emplacements et 8 enregistreurs, attributs par emplacement, éditeur audio, façade à 8 touches de piste |
| **Robot 78** | `voc16` | Vocodeur analogique de laboratoire | vocodeur analogique 16 canaux de 1978 (Moog 16 Channel Vocoder) |
| **Robot Choral** | `vocstr` | Clavier cordes et vocodeur | claviers cordes + vocodeur (esprit Roland VP-330) |
| **Spectral Vocal** | `vocspec` | Vocodeur à algorithmes | vocodeurs logiciels à algorithmes multiples |
| **Speak Synth** | `vocsyn` | Voix de synthèse à 5 modules | processeurs vocaux à modules parallèles (esprit iZotope VocalSynth) |
| **Old Metal** | `fm6` | FM à 6 opérateurs (famille Synthés FM) | Yamaha DX7 — v0.45.1 : ensemble stéréo ; 90 sons d'usine |
| **New Metal** | `fm4` | FM à 4 opérateurs, 8 algorithmes à sorties X / Y (Synthés FM) | Elektron Digitone (moteur FM « simplifié ») |
| **Komodo Fuel** | `fmx` | FM à 6 opérateurs à matrice libre (Synthés FM) | Kodamo EssenceFM Mk2 |
| **World Tour** | `muse` | Poly analogique 8 voix, 2 filtres en échelle, retard diffusant (Polys) | Moog Muse |
| **Hot Sauce** | `hots` | Hybride à deux moteurs : analogique, table d'ondes, harmonique, modal (Drones) | Arturia Pigments |
| **Octo Lyma** | `lyra` | 8 voix organiques en paires, FM croisée, bourdons, LFO hyper, retard modulé (Drones) | SOMA Laboratory Lyra-8 |
| **Komodo Dragon** | `bitm` | Synthèse par masques de bits, enveloppe delta (Drones) | Kodamo Mask1 |
| **Entrée audio** | `input` | Entrée de la carte son | — |

### Noms ajustés par prudence (30/10/2026)

| Proposé | Retenu | Pourquoi |
|---|---|---|
| Screamer 200 | Hurleur 200 | « Tube Screamer » est une marque déposée d'Ibanez (matériel de musique) ; un plugin « The Scream » existe déjà |
| Octopus | Pieuvre 8 | Deux séquenceurs du commerce s'appellent déjà Octopus (Genoqs, VIRAM) |
| Dragon Fist Impact | Dragon Fist | « Impact » renvoie à la boîte à rythmes d'Arturia |
| Dfemme rythmique | Kick Killer | Se prononce presque comme DFAM (risque de confusion) |
| Acid Lover 03 | Acid Lover | « 03 » + acid renvoie au 303 |

## Effets

Depuis la v0.35.1, chaque effet a un **nom propre en anglais** (affiché en grand) et son **rôle** en français en sous-titre.

| Nom Groovebox | Rôle (sous-titre) | Identifiant | Forme | Principe repris de |
|---|---|---|---|---|
| Prism | Égaliseur 8 bandes | `eq` | module | égaliseurs paramétriques dynamiques modernes (esprit FabFilter Pro-Q) — v0.36 : attaque, relâchement, dynamique spectrale (sans latence) |
| Lumen | Compresseur optique | `opto` | rack | compresseur à cellule optique (esprit Teletronix LA-2A) |
| Thunder | Compresseur FET | `fet` | rack | compresseur à transistor FET (esprit UREI / Universal Audio 1176) |
| Cement | Compresseur de bus | `bus` | rack | compresseur de bus de console (esprit SSL G-Bus) |
| Scalpel | Compresseur moderne | `comp` | rack | compresseur polyvalent à styles (esprit FabFilter Pro-C 3) — v0.36 : 10 styles, caractère lampe / diode / brillant, seuil automatique |
| Ceiling | Limiteur | `limiter` | rack | limiteur de crête sans anticipation |
| Varnish | Chaîne de mastering | `master` | module | chaîne de mastering tout-en-un (esprit iZotope Ozone) |
| Sentinel | Porte et dé-esseur | `gate` | rack | porte de bruit et dé-esseur |
| Furnace | Distorsion | `dist` | module | distorsion créative à deux moteurs (esprit Arturia Dist COLDFIRE, demandé au cahier des charges) — v0.36 : filtres sur le son distordu seulement (entrée et sortie par moteur), 4 modulateurs internes |
| Grinder | Distorsion multibande | `grinder` | module | distorsion multibande, porte, dynamique et réinjection par bande (esprit Ohm Force Ohmicide) — v0.36 |
| Ferrite | Saturation à bande | `tapesat` | pédale | émulations de magnétophones de studio (bosse du grave, perte d'aigus selon la vitesse, pleurage, souffle) — v0.38 |
| Glow | Saturation à lampe | `tubesat` | pédale | étages de triode (harmoniques paires, affaissement de l'alimentation) — v0.38 |
| Bruiser | Compresseur à distorsion | `distcomp` | rack | compresseur à distorsion de studio des années 90 (esprit Empirical Labs Distressor : « Nuke », mode britannique, distorsion 2 / 3) — v0.38 |
| Snap | Sculpteur de transitoires | `transient` | pédale | « transient designers » à deux paires de suiveurs, sans seuil — v0.38 |
| Ricochet | Écho stéréo | `delay` | rack | échos à caractères (esprit Ohm Force / Toys Factory, demandé au cahier des charges) — v0.36 : couleurs d'écho (esprit EchoBoy), lo-fi, groove, gel (esprit H-Delay) |
| Nave | Réverbération | `reverb` | pédale | réverbération algorithmique moderne (esprit FabFilter Pro-R, demandé au cahier des charges) |
| Moire | Chorus | `chorus` | pédale | ensembles à lignes BBD (chorus des polyphoniques des années 80) |
| Whirl | Phaser | `phaser` | pédale | phaser à étages passe-tout |
| Contrail | Flanger | `flanger` | pédale | flangers à seau BBD des années 70 (esprit Electro-Harmonix Electric Mistress) — v0.36 |
| Twinspin | Phaser double | `phaser2` | pédale | phasers doubles à deux balayages (esprit Arturia Phaser BI-TRON) — v0.36 |
| Expanse | Chorus spatial | `dimension` | pédale | chorus spatial à 4 réglages (esprit Roland Dimension D) — v0.36 |
| Trinity | Chorus à trois voix | `chorus3` | pédale | chorus à trois lignes déphasées de 120° (esprit Eventide TriceraChorus) — v0.38 |
| Staccato | Répétition | `stutter` | pédale | stutter / répétition calée au tempo |
| Iris | Filtre | `filter` | pédale | filtre multimode à LFO et enveloppe |
| Atmosphere | Réverbération d'ambiance | `atmos` | module | grandes réverbérations « pédales d'ambiance » à 12 algorithmes et traîne (esprit Strymon BigSky) — algorithmes complétés en v0.35 |
| Comet | Échos multiples | `mdelay` | pédale | grandes pédales de délai multimode (esprit Strymon TimeLine) |
| Hopscotch | Effets à pas | `stepfx` | module | multi-effets séquencés en rythme (esprit Sugar Bytes Effectrix) — v0.39 : jusqu'à 64 pas, un effet et son réglage par pas, préréglages par effet et 4 réglages perso (esprit Sugar Bytes Looperator) |
| Infinity | Boucleur | `looper` | pédale | pédales de boucle (esprit Boss RC) |
| Quartet | Compresseur multibande | `mband` | module | multibandes à bandes flottantes (esprit FabFilter Pro-MB / Waves C6) — v0.34 |
| Velvet | Compresseur à lampes | `varimu` | rack | compresseurs vari-mu à lampes (esprit Fairchild 660/670, Manley Vari-Mu) — v0.34 |
| Ember | Égaliseur à lampes | `tubeeq` | rack | égaliseur passif à lampes (esprit Pultec EQP-1A) — v0.34 |
| Strip | Égaliseur de console | `consoleeq` | rack | tranche de console anglaise (esprit SSL 4000 E/G) — v0.34 |
| Heirloom | Réverbération vintage | `vintverb` | rack | réverbérations numériques de studio des années 70-80 (esprit Valhalla VintageVerb, Lexicon 224) — v0.35 |
| Gold Leaf | Plaque | `plate` | pédale | réverbérations à plaque (esprit EMT 140) — v0.35 |
| Spiral | Ressort | `spring` | pédale | réverbérations à ressorts d'amplis (esprit Fender, Accutronics) — v0.35 |
| Sovereign | Réverbération de prestige | `prestige` | rack | grandes réverbérations algorithmiques de studio (esprit LiquidSonics Seventh Heaven, en algorithmique pour rester sans latence) : réseau à 8 lignes, déclin par bandes, gel — v0.38 |
| Sculptor | Multi-effet à courbes | `sculptor` | module | multi-effets à courbes de modulation (esprit Cableguys ShaperBox) : 9 effets (temps, saturation, broyage, filtre, liquide, bruit, volume, panoramique, largeur), chacun sa courbe, sur tout le son ou une bande — v0.37 |
| Reel | Écho à bande à têtes | `tapeecho` | pédale | échos à bande à trois têtes avec ressort (esprit Roland RE-201 Space Echo) — v0.35 |

Les mentions « esprit de » sont des rapprochements : le code ne nomme pas ces produits.

## Modules (v0.35.2)

Un effet qui a trop de réglages pour une pédale ou un rack devient un **module** : dans le pédalier, une
unité compacte (ses réglages principaux et un bouton « Ouvrir le module ↗ ») ; dans EFFETS, le bouton
« Ouvrir le module ↗ ». L'interface dédiée suit les blocs du pack (Broyeur, Sculpteur, Mastering) :
bandeau (nom, afficheur à sélecteur ◀ ▶, diodes), grand écran, sous-panneaux avec tous les réglages.

| Module | Écran | Matière · bouton · accent |
|---|---|---|
| Furnace (`dist`) | courbes de transfert des moteurs A et B, routage, modulateurs en direct | acier sombre · chrome · orange |
| Grinder (`grinder`) | bandes entre leurs séparations : algorithme, drive, niveau ; modulateurs | tôle noire · chrome · framboise |
| Varnish (`master`) | courbe de l'égaliseur, sonie (7 segments), réductions de gain | métal noir · noir · laiton |
| Prism (`eq`) | courbe, spectre de sortie, points des bandes à faire glisser | aluminium anthracite · noir · cyan |
| Quartet (`mband`) | 4 bandes entre leurs séparations, réduction de chacune, bandes flottantes | métal noir · chrome · ambre |
| Hopscotch (`stepfx`) | 16 à 64 pas en rangées de 16, tête de lecture ; palette effet → préréglage, réglages perso | tôle noire · noir à jupe · vert |
| Atmosphere (`atmos`) | enveloppe de la queue selon l'algorithme, déclin (7 segments) | aluminium violet · chrome · violet |
| Sculptor (`sculptor`) | courbes des effets en marche sur une mesure, l'effet choisi en gras, valeurs en direct | aluminium anthracite · noir · cyan |

Le Sculpteur annoncé est devenu Sculptor (v0.37), le Broyeur est devenu Grinder (v0.36) ; la dynamique
spectrale est dans Prism (v0.36). Le lot 2 de la veille (v0.38) est fait : Ferrite, Glow, Bruiser, Snap,
Trinity, Sovereign.

## Habillage (v0.35.1)

Matières, boutons, curseurs, vis, molettes et fonds de pédales : pack graphique **Sillage — Atelier des
instruments, édition 05** (images générées pour OFAD, voir sa provenance). Correspondance instrument →
matière et pédale → fond : `ui/src/skin/skins.ts` (une ligne par instrument ou par pédale).

Polices de l'habillage : Bebas Neue, Barlow Condensed (licence SIL Open Font 1.1, fichiers de licence
dans `ui/src/skin/fonts/`).
