# OFAD GOLEM — documentation technique

Licence : AGPL-3.0-or-later. Moteur C++ / JUCE 8, interface React dans la WebView de JUCE.
Une seule base de code pour l'application autonome (Windows) et le plugin VST3.

État : **version 0.46** : 34 instruments, 38 effets, 16 pistes, 4 retours, 7 groupes, bus principal,
8 patterns par piste avec lancement façon clips, séquenceur jusqu'à 512 pas par voie, 32 scènes avec
crossfader, morceau (rangées et courbes), montage audio, modulations, macros, apprentissage MIDI.
Trois formats depuis le même code : application Windows, plugin VST3, application Android (tablette).
Chaque instrument et chaque effet reprend un **principe** de synthèse ou de traitement connu,
sur une architecture propre. Aucun nom, aucun graphisme, aucun échantillon, aucun preset tiers.

## Compiler sous Windows

Le plus simple : double-cliquer sur **`COMPILER-ET-LANCER.bat`** (à la racine). Il compile tout, lance les
tests puis ouvre l'application ; en cas d'erreur la fenêtre reste ouverte. Les fois suivantes, ouvrir
directement `build\Groovebox_artefacts\Release\Standalone\Groovebox.exe`.

Prérequis : Visual Studio avec la charge de travail « Développement Desktop en C++ », Node.js 22 ou plus.

1. Une seule fois, dans PowerShell : `powershell -ExecutionPolicy Bypass -File scripts\install-webview2.ps1`
2. Puis, dans une invite de commandes : `scripts\build-windows.bat`
   (JUCE est cherché dans `..\JUCE\juce-8.0.12-windows\JUCE` ; sinon passer son chemin en argument.)

Résultat :
- application autonome : `build\Groovebox_artefacts\Release\Standalone\Groovebox.exe`
- plugin : `build\Groovebox_artefacts\Release\VST3\Groovebox.vst3` (à copier dans `C:\Program Files\Common Files\VST3\`)
- outil de vérification : `build\groovebox_render_artefacts\Release\groovebox_render.exe`
  (`groovebox_render demo.wav 16` rend 16 secondes du projet de démonstration, après une sauvegarde puis un
  rechargement du projet ; `groovebox_render demo.wav 16 --state` affiche le projet sauvegardé au lieu de rendre)

Le script lance ensuite les tests (`ctest`). Un test en échec arrête le script.

Développement de l'interface avec rechargement à chaud : `cd ui && npm run dev`. Dans un navigateur,
l'interface tourne seule sur un faux moteur (`ui/src/engine/mock.ts`, catalogue `catalog.json`).
Une version **Debug** de l'application accepte la variable d'environnement
`GROOVEBOX_UI_DEV_URL=http://localhost:5173/` ; seule une adresse locale (localhost ou 127.0.0.1) est acceptée.
La version Release ignore cette variable.

## Compiler l'application Android (tablette)

Prérequis : Android Studio (avec, depuis le SDK Manager : NDK **28.1.13356709** et CMake **3.22.1**),
Node.js, et JUCE 8.0.12 au même endroit que pour Windows.

- Le plus simple : `scripts\build-android.bat`. Il compile l'interface, prépare ses fichiers pour
  Android (`node scripts/gen-uidata.mjs` → `android/generated/`), puis l'APK.
  Résultat : `android\Builds\Android\app\build\outputs\apk\release_\release\app-release_-release.apk`.
- Ou bien : ouvrir le dossier `android/Builds/Android` dans Android Studio, puis Build › Build APK.
  Lancer d'abord `npm run build` dans `ui/` et `node scripts/gen-uidata.mjs`.
- Le projet Android est généré par le Projucer à partir de `android/Groovebox.jucer`
  (`Projucer --resave android/Groovebox.jucer` après un ajout de fichier source).
- Installation sur la tablette : copier l'APK et l'ouvrir (autoriser les sources inconnues), ou
  `adb install -r <fichier.apk>` avec le débogage USB activé.
- **Signature (v0.46.1)** : la version « release » est signée avec la clé de publication de GOLEM si
  `%USERPROFILE%\.ofad-golem\keystore.properties` existe (créé une fois par `scripts\creer-cle-android.bat`),
  sinon avec la clé de débogage (message « ATTENTION » pendant la compilation). La clé reste hors du projet ;
  la sauvegarder (clé USB + gestionnaire de mots de passe) : sans elle, aucune mise à jour ne s'installe
  par-dessus. Le premier APK signé avec la clé de publication demande de désinstaller une fois l'ancien.
  Si le Projucer régénère `app/build.gradle`, reporter le bloc « golemRelease ».
- Sauvegarde Android : laissée active (`allowBackup` par défaut) : projets et échantillons rangés dans les
  données de l'application suivent la sauvegarde Google de l'utilisateur (son propre compte), ce qui
  permet de les retrouver en changeant de téléphone. Les enregistrements sont dans Musique/Groovebox.
- Audio : Oboe (AAudio), mode faible latence. Permission demandée : microphone (piste « Entrée audio »).
  Aucun accès réseau. Le projet en cours est enregistré automatiquement toutes les 15 secondes s'il a changé.
- Sur l'écran tactile, un appui long vaut un clic droit (modulation, apprentissage MIDI, édition d'un pas).
- Contrôleur MIDI USB : branché sur la tablette, il est ouvert automatiquement.

## Catalogue de l'interface

Si on modifie les réglages d'un instrument ou d'un effet, régénérer le catalogue de l'interface :
`build\Release\groovebox_catalog.exe > ui\src\engine\catalog.json`.

## Organisation du code

| Dossier | Contenu |
|---|---|
| `src/engine/` | Moteur : pistes, graphe audio publié sans verrou, séquenceur, scènes, modulations, catalogue, échange JSON avec l'interface |
| `src/dsp/` | Briques de traitement : oscillateurs anti-repliés, filtres, sur-échantillonneur 2×, égaliseur, fonctions rapides |
| `src/synth/` | Base des instruments mélodiques : voix, enveloppes, tables d'ondes, lecteur d'échantillon |
| `src/instruments/` | Les 38 instruments |
| `src/effects/` | Les 54 effets |
| `src/PluginProcessor.*` | Processeur JUCE : MIDI, automation par l'hôte, sauvegarde du projet |
| `src/PluginEditor.*` | Fenêtre et fonctions natives appelées par l'interface (toutes vérifiées) |
| `ui/` | Interface React (Vite, TypeScript) |
| `tests/` | Tests du moteur, des voix, du séquenceur, des instruments et des effets |
| `tools/` | Rendu de démonstration, export du catalogue |

## Instruments et effets

La liste complète, avec les vignettes, le principe et les sons d'usine de chaque module, est dans
[CATALOGUE.md](CATALOGUE.md) (et [CATALOGUE.pdf](CATALOGUE.pdf)). Les tableaux courts sont dans le [README](../README.md).

## L'appareil

| Écran | Contenu |
|---|---|
| **Pistes** | 16 pistes : choix de l'instrument, nom, couleur, preset |
| **Lancement** | 8 patterns (A à H) par piste, en grille. Toucher une case : lancer et éditer le pattern ; Maj : éditer sans lancer ; rangée ▶ : lancer la même lettre sur toutes les pistes (et la scène de réglages liée) ; ■ : arrêter une piste ; copier, effacer |
| **Séquence** | Grille de pas (batteries) ou rouleau de notes (instruments mélodiques), pattern A à H choisi en bas. Clic droit sur un pas : réglages du pas (vélocité, durée, décalage, condition, verrous) |
| **Son** | Réglages de l'instrument de la piste, par onglets |
| **Effets** | Les 4 effets d'insert du canal, dans l'ordre de la chaîne |
| **Mixeur** | Volume, panoramique, coupure, solo, 4 envois, retours A à D, maître |
| **Scènes** | Scènes A et B, crossfader (−100 à +100), 32 scènes en deux banques, scène de Break |
| **Réglages** | Projet (démonstration, vide, ouvrir, enregistrer), quantification de l'enregistrement et du lancement, gamme des pads, contrôleur MIDI |

Les 8 encodeurs sous l'écran prennent les réglages affichés. **Toutes les valeurs se règlent en les tirant
vers le haut ou le bas**, à la souris ou au doigt : les encodeurs, les valeurs écrites sur l'écran (Tempo,
Swing, Longueur…), le tempo de la barre du haut, les listes de valeurs, le panoramique et les envois du
mixeur. Maj = réglage fin, molette = un cran, double-clic (double tape au doigt) = valeur d'origine.

**Façades « comme la machine » (v0.10)** : la vue synthé de la basse mono, de la basse acide et des trois
batteries reprend la disposition de la famille de machines dont chacune s'inspire, sans nom ni logo : panneau
noir, sérigraphie blanche, boutons à jupe métal, joues en bois, molettes de hauteur et de modulation pour la
basse mono (bus de modulation 1 / 2 à gauche, oscillateurs, mélangeur, filtre, glisse, enveloppes, arpège) ;
aluminium brossé, rangée de gros boutons et pas de séquence (note, accent, glissé, octave ▲▼) pour la basse
acide ; pad choisi par pages (machine, échantillon, filtre, amplitude, LFO, effet) avec petit écran et 8
boutons, puis le kit et les pas, pour la batterie analogique ; colonnes de boutons par instrument et 16 touches
de pas colorées par groupes de 4 (plus le bouton d'accent total) pour les batteries 80 et 90 ; panneau noir à
joues en bois et interrupteurs crème à LED rouge pour le poly analogique (Poly-Mod, LFO, Wheel-Mod, oscillateurs
A et B, mélangeur, filtre, enveloppes) ; curseurs et bandeaux de section colorés pour le poly à chorus (arpège,
LFO, oscillateur, passe-haut, filtre, ampli, enveloppe, boutons de chorus I et II) ; gris graphite à LED corail
pour le poly hybride (un jeu de boutons pour les oscillateurs 1 / 2 / 3, enveloppes à curseurs) ; noir à LED
violettes pour le wave morph (mutants, LFO à pas dessinables, macros) ; bleu ardoise à petits écrans pour les
nappes (parties A / B). Les synthés à matrice ont une 2e page : matrice de modulation (source × via →
destination, profondeur) et pas de l'arpège.

**Synthés (v0.10)**, chacun avec les commandes qui font sa marque :
- **Poly analogique** : formes cumulables (A : scie + impulsion ; B : scie + triangle + impulsion) ; LFO à
  trois formes cumulables ; **Wheel-Mod** (mélange LFO ↔ bruit, quantité initiale, dosé par la molette et la
  pression) vers la fréquence de A, de B, les largeurs et le filtre ; **révision du filtre** ancienne (mordante,
  sans perte de grave) ou récente (douce) ; interrupteur « relâchement » ; pression vers le filtre.
- **Poly à chorus** : déclenchement du LFO automatique ou à la molette, levier de hauteur vers l'oscillateur et
  le filtre, **maintien** des notes quand l'arpège est arrêté, arpège, chorus I et II cumulables.
- **Poly hybride** : chaque oscillateur en formes analogiques ou en **table d'ondes** ; commande **Shape** à
  effet choisi (**synchro dure** ou **densité** : trois copies désaccordées) ; FM 3→1, 1→2, 2→3 ; modes poly,
  mono, mono legato, **mono 2 voix** ; 2 LFO de plus ; **matrice de 16 routages** à deux sources (enveloppes et
  LFO de l'instrument, vélocité, note, molette, pression, pitch bend, aléatoire) ; arpège.
- **Wave morph** : **oscillateur 3** ; **deux mutants** par oscillateur en chaîne (FM, synchro, largeur, repli,
  **pile d'ondes**, **phase décalée**, **harmonique**) ; filtre 1 en échelle 24 / 18 / 12 / 6 dB, bande, haut
  12 / 24, **voyelle**, **peigne ±** ; **5 enveloppes**, **5 LFO** (dont 3 à **pas dessinables**) ; **matrice de
  32 routages** ; **8 macros** ; **arpège à pas** (transposition, vélocité) ; poly, mono, **unisson** 2 à 8 voix.
- **Nappes** : **quatre oscillateurs** (A1, A2, B1, B2) avec phase libre ou fixée, inversion, panoramique,
  unisson ; formes **impulsion étroite** et **tri-scie** ; filtre par partie avec position « contourné » et
  saturation ; **commande globale** de coupure / résonance et « warm drive » ; ampli B propre ou comme l'ampli A ;
  2e enveloppe de modulation ; matrice de 16 routages ; arpège à pas ; poly, mono, mono legato.

**Vue « synthé » (v0.9)** : bouton « Ouvrir le synthé ↗ » (en haut de l'appareil et sur l'écran Son, ou
double-clic sur le nom du son). L'instrument de la piste s'ouvre en grand, comme la fenêtre d'un synthé :
tous ses réglages d'un coup en modules (oscillateurs, filtre, enveloppes…) avec leurs graphiques, une page
Modulation (LFO de l'instrument et modulations du son de la piste) et un clavier jouable (la hauteur de l'appui
donne la vélocité, glisser d'une touche à l'autre rejoue). Pour les batteries : le pad choisi en détail, une
vue d'ensemble du kit (accord, déclin, niveau des 16 pads) et 16 pads jouables. Les boutons font tout ce que
font les encodeurs (glisser, molette, double-clic, menu, modulation, verrous). Échap ou ✕ referme.

**LFO des synthés synchronisables au tempo (v0.9)** : chaque LFO intégré (basse mono, poly analogique, poly
chorus, poly hybride, wave morph, nappes) a un réglage « synchro » : Libre (vitesse en Hz) ou une division
de 4 mesures à 1/32, ternaires et pointées comprises.

**Clic droit (ou appui long) sur un encodeur** :
- saisir la valeur exacte (« 1,2k » = 1200) ou, pour un choix, piocher dans la liste complète (v0.8) ;
- choisir une source de modulation, puis régler sa profondeur avec la petite molette (l'arc montre la plage) ;
- ou confier le réglage à une des 8 macros, automatisables depuis le DAW ;
- ou « Apprendre MIDI » : le prochain bouton tourné sur le contrôleur pilote ce réglage.

**Deux familles de modulations par piste (v0.8)** : les **modulations du son** (sources 1 à 4, écran Son,
pour les réglages de l'instrument et des pads) et les **modulations des effets et du mixage** (sources 5 et 6,
écran Effets, pour les inserts, le volume, le panoramique et les envois). Le type de chaque source (LFO,
fonction, suiveur, séquenceur) se choisit par des boutons, sans jamais rester bloqué ; le LFO se cale sur
le tempo (Synchro : Tempo, puis Division) ; les 16 pas du séquenceur se règlent en tirant les barres.

**Modulation guidée (v0.8)** : onglet Modulation, choisir une source (LFO, fonction, suiveur, séquenceur),
« Affecter à un réglage… », puis toucher les encodeurs à moduler sur les écrans Son, Effets ou Mixeur de la
piste (ils s'allument à la couleur de la source ; toucher à nouveau retire). La liste des destinations de la
source montre chaque profondeur (à régler en glissant) et un bouton pour la retirer.

**Rôle des pads** : Jouer, Couper (la piste), Scènes (lancer une scène), Pas (les 16 pads posent ou retirent les pas de la page affichée, sur la ligne choisie, comme le pas à pas d'une MPC), Effacer. Le contrôleur MIDI (notes 36 à 51) suit le même rôle.

**Transport** : Enr., Lecture, Arrêt. La barre d'espace lance ou arrête la lecture.
Dans un DAW, le séquenceur suit le tempo et la lecture du DAW.

**Fill** et **Break** : maintenir le bouton. Avec Maj, le bouton reste enclenché.
Break applique la scène de Break tant qu'il est tenu.

**Sampler de performance (v0.11)**, principe des samplers de performance à 8 pistes ; chaque piste qui le
prend choisit sa **machine** :
- **Flex** : un des 16 échantillons de la piste, ou le tampon de son **enregistreur** ; **Static** : pareil, pour
  les fichiers longs ; **Thru** : l'entrée audio passe dans la piste, ouverte par les pas ; **Neighbor** : la sortie
  de la piste précédente (après ses effets) passe dans celle-ci et quitte le mixage — pour enchaîner plus de
  4 effets ; **Pickup** : boucle enregistrée en direct (1er appui : enregistrer ; 2e : fermer la boucle, calée
  sur 1 à 8 mesures ; ensuite : surcouche / lecture ; un pas relance la boucle à son début) ;
- page **Lecture** : hauteur, début, longueur, vitesse (négative = à l'envers), redéclenchements (nombre et
  écart) ; page **Réglages** : étirement Non / Normal (grains, hauteur indépendante du tempo) / Beat (une double
  croche par grain, attaques nettes), boucle (avant, aller-retour), **découpes** (grille de 2 à 64 ou attaques
  détectées au chargement), jeu au clavier chromatique ou par découpe (Do3 = découpe choisie par « Début »),
  longueur de l'échantillon en mesures (auto : 85 à 170 BPM), gain ; page **Ampli** : attaque, tenue (durée,
  note, infinie), relâchement, volume, balance ;
- **3 LFO** à 8 formes dont « Pas » (16 pas dessinables), synchro au tempo, une destination et une profondeur
  chacun (page LFO), plus 3 routages libres (page Routages) ;
- **Effet 1 / Effet 2** : les inserts 1 et 2 de la piste, réglés depuis la façade ;
- **enregistreur** : source entrée, piste précédente ou mixage principal ; durée de 1 à 64 pas ou 16 s ;
  touches Enregistrer, Arrêter, Effacer, **Garder** (le tampon devient un échantillon de la piste, enregistré
  avec le projet) ;
- le réglage **Emplacement** se verrouille par pas comme tout réglage : c'est le **verrou d'échantillon**.
Au séquenceur, trois fonctions de pas s'ajoutent : **Glissé** (les verrous du pas glissent depuis les valeurs
du moment), **Verrous seuls** (le pas pose ses verrous sans jouer), **Enregistrer** (le pas lance
l'enregistreur de la piste).

**Plein écran (v0.11)** : bouton « Plein écran » en haut de l'appareil ou touche F11 : l'application prend tout
l'écran, puis revient en fenêtre. La fenêtre s'agrandit et se réduit librement ; l'appareil suit la taille en
gardant ses proportions. Dans un logiciel hôte, c'est l'hôte qui gère la fenêtre du plugin.

**Annuler / rétablir et photo des réglages (v0.11)** : flèches ↶ ↷ en haut de l'appareil, ou Ctrl+Z et
Ctrl+Y (Ctrl+Maj+Z aussi). Une étape est rangée quand un changement est posé depuis 0,4 s (un geste sur un
bouton = une étape) ; 50 étapes au plus. L'historique couvre réglages, modulations, scènes, séquences
(notes, conditions, verrous, longueurs), instruments, effets, échantillons, noms, couleurs, macros. Le jeu en
cours (pattern joué), le tempo, le swing, la position du crossfader et les affectations MIDI n'en font pas
partie. Le retour en arrière n'écrit que ce qui change : un instrument ou un effet inchangé n'est pas
recréé, une séquence inchangée n'est pas réécrite, on peut donc annuler en jouant.
Le bouton **Photo** fige tous ces réglages d'un coup ; **Rappel** y revient d'un seul geste, et ce rappel
s'annule lui-même avec Ctrl+Z. La photo est enregistrée avec le projet (sans les fichiers d'échantillons :
une photo relue d'un fichier laisse les échantillons en place).

**Arpégiateur : 20 modes (v0.11)** sur tous les instruments qui ont un arpège : Haut, Bas, Haut-bas,
Bas-haut, Haut-bas répété et Bas-haut répété (notes extrêmes jouées deux fois), Ordre joué, Joué inversé,
Aléatoire, Mélangé (chaque note une fois par tour), Marche aléatoire (une note plus haut ou plus bas),
Accord, Convergent (de l'extérieur vers l'intérieur), Divergent, Convergent-divergent, Pédale grave
(1-2-1-3-1-4…), Pédale aiguë, Sauts (1-3-2-4…), Marches haut (1-2-3, 2-3-4…), Marches bas, Haut doublé.
Les projets enregistrés avant gardent leur mode.

**Banques d'usine (v0.12)** : 50 sons par synthé (basse acide, basse mono, poly analogique, poly à
chorus, poly hybride, wave morph, nappes), rangés par catégories : Acide, Basses, Leads, Nappes, Plucks,
Claviers, Cordes, Cuivres, Arpèges, Effets (et « Classiques » pour les sons livrés avant). Réglages
originaux construits sur les recettes de chaque catégorie, adaptés à l'architecture de chaque synthé ;
aucun préréglage d'un autre fabricant n'est recopié. Chaque son est joué et mesuré par
`groovebox_presetcheck` (réglages existants et dans leur plage, pas de silence ni de valeur invalide) et
mis au même niveau perçu : −16 LUFS instantané au plus fort (−18 pour nappes, cordes et effets), crête
vraie sous −1 dBTP. Les banques s'écrivent dans `tools/presets/` (un fichier par synthé) ;
`python3 tools/presets/gen.py` produit `src/instruments/FactoryBanks.h`, puis
`groovebox_presetcheck mesure.json` et `tools/presets/level.py mesure.json` remettent les niveaux à jour.
La poly analogique a désormais un arpège (page « Arpège » de sa façade).

**Division par pattern (v0.12)** : binaire ou ternaire se règle pour le pattern édité de la piste
seulement (les autres pistes et patterns gardent la leur). La pastille « Pattern A · 4 mes. · 1/16 · ×1 »
de l'écran Séquence s'ouvre d'un clic : mesures (1 à 32), division binaire ou ternaire, vitesse.
Les projets d'avant gardent leur division sur tous leurs patterns.

**Façades en plein écran (v0.12)** : la page de la façade s'agrandit jusqu'à 1,8 fois pour remplir la
place libre, dans la limite de la largeur.

**Échantillons** : glisser un fichier WAV, AIFF, FLAC ou OGG sur un pad, ou passer par l'écran Son.
Limites : 64 Mo par fichier, 3 minutes. L'échantillon est enregistré dans le projet.

**Clavier** : W X C V / Q S D F / A Z E R / 1 2 3 4 (AZERTY) = pads 1 à 16. Maj = accent.
Espace = lecture / arrêt. B maintenu = Break. N maintenu = Fill.

**Pads des instruments mélodiques** : 16 notes d'une gamme choisie (chromatique, majeure, mineure,
modes, pentatoniques, blues) à partir de la tonique choisie (écran Réglages).

**MIDI** :
- les notes vont à la piste affichée ;
- apprentissage : n'importe quel réglage sur un CC ; crossfader, Break, Fill et Lecture sur un CC ou une note (écran Réglages) ;
- sur une piste de batterie, les notes 36 à 51 jouent les pads 1 à 16 ;
- Program Change 0 à 31 = lancement direct des scènes 1 à 32.

## Séquenceur

- 16 pistes × 8 patterns × 16 voies × 512 pas : blocs de 4 mesures, jusqu'à 32 mesures par pattern.
- Chaque pattern a sa longueur et sa vitesse. Un pattern lancé part au début de la mesure suivante
  (ou immédiatement, au temps, toutes les 2 ou 4 mesures) et repart de son début.
- Divisions 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64 ; vitesse par pattern ; sens de lecture par voie.
- Enregistrement en direct avec quantification réglable (0 à 100 % : le reste du décalage est gardé).
- Par pas : vélocité, durée, décalage fin, condition (probabilité, A:B, FILL, PRÉ, VOISIN, 1ER), verrous de réglages,
  **répétitions (v0.8)** : 2 à 8 coups dans le pas, vélocité constante, décroissante ou croissante (swing respecté),
  **flam (v0.10)** : un coup léger puis le coup principal 8 à 60 ms plus tard (encodeur « Flam » d'un pas de batterie).
- **Fonctions de pas de la piste (v0.10)**, principe des séquenceurs de basse et de batterie des années 80 : ligne
  **Accent** (tous les sons du pas, dosé par l'encodeur « Accent » de la piste : l'accent total des boîtes à
  rythmes) et, en mélodique, ligne **Glissé** (la note tient jusqu'au pas suivant et glisse vers lui ; même note :
  liée, sans redéclencher). Sur la façade de la basse acide : note, accent, glissé et octave ▲▼ de chaque pas.
- **Longueur par pad (v0.10)** : chaque ligne d'une batterie peut boucler sur sa propre longueur (polymétrie) —
  encodeur « Longueur du pad ».
- **Break temporisé (v0.8)** : bouton « Break · N » du bloc Jeu, Break pendant 1, 2, 4, 8 ou 16 temps, relâché seul
  sur un temps (Maj + appui ou molette : changer la durée).
- **Bandeau de mode (v0.8)** : quand les encodeurs écrivent ailleurs que dans le son de base (verrous d'un pas,
  scène A / B / Break, affectation de modulation), un bandeau le dit sur tous les écrans ; Échap ou « Terminer » en sort.
- **Sélection (v0.7)**, comme dans un séquenceur de DAW :
  - Ctrl+clic ajoute ou retire un pas ; tirer depuis une case vide dessine un rectangle (Maj : ajoute) ;
  - tirer un pas sélectionné **déplace** la sélection (dans la ligne, vers un autre pad, ou vers une autre
    note en mélodique) ; **Ctrl+tirer la copie** ; conditions et verrous suivent ;
  - Ctrl+C / Ctrl+X / Ctrl+V (colle sous la souris), Ctrl+D (duplique juste après), Suppr, Ctrl+A, Échap ;
    flèches : déplacer d'un pas ou d'une ligne (Maj : d'une mesure ou d'une octave) ;
  - avec plusieurs pas sélectionnés, les encodeurs règlent tous les pas à la fois : vélocité (les écarts sont
    gardés), durée, décalage, condition, probabilité, A:B, transposition, et verrous de réglages sur tous ces pas ;
  - sur tablette : boutons Sélection (toucher sélectionne), Copier, Couper, Coller, Dupliquer, Effacer et
    « Copie au glisser ».

## Scènes (deux scènes et un crossfader)

- « Éditer A » ou « Éditer B », puis tourner les encodeurs : le réglage est enregistré dans la scène.
- Seuls les réglages enregistrés dans A ou B bougent avec le crossfader. Les réglages à crans basculent au milieu du fader.
- Lancer une scène la fait remplacer celle qu'on entend. Avec Maj, elle part de l'autre côté du fader.
- En plus des 32 scènes, une scène de Break (réglée depuis l'écran Scènes) s'applique tant que Break est tenu.

Ordre d'application d'un réglage : valeur de base → fondu des scènes A/B → Break → verrous du pas → modulations.

Tout est sauvegardé avec le projet : pistes, réglages, effets, modulations, séquences, scènes, échantillons, macros.

## Enregistrement audio (v0.32)

- Bouton **● Audio** dans la barre du haut : on choisit ce qu'on enregistre (**master**, **pistes séparées** — chaque
  piste après ses effets et son volume, plus chaque retour d'effet —, ou **les deux**) et le format (**WAV 24 bits**
  ou **MP3 320 kbit/s**), puis « Démarrer ». Tout ce qui joue est pris : boucles, scènes, mode morceau, jeu aux pads.
  Un nouvel appui (le bouton montre le temps écoulé) arrête la prise.
- Les fichiers vont dans `Documents/Groovebox/Enregistrements/<date et heure>/` (ordinateur) ou `Musique/Groovebox/`
  (Android ; sinon le dossier privé de l'application) : `Master`, `P01 <nom de la piste>`…, `Retour A`…
- Fil audio : ni allocation, ni verrou, ni disque (file sans verrou de 2 s par fichier, vidée par un fil d'écriture).
  Le MP3 est fait après l'arrêt, à partir des WAV, dans un fil à part (encodeur LAME, qualité 2).

## Changer d'instrument par les images (v0.32)

- Sur la façade d'un synthé, **⇄ Instrument** ouvre les images de toutes les façades : toucher l'une la met sur la piste.
- Écran PISTES : passer la souris sur un instrument (ou sur une piste) montre la miniature de sa façade.
- Les miniatures sont dans `ui/public/thumbs/` ; `scripts/make-thumbs.cjs` puis `scripts/make-thumbs.py` les refont.

## Latence et sécurité

- **Latence ajoutée : zéro.** Aucun effet n'annonce de latence à l'hôte. Le limiteur n'a pas d'anticipation :
  le plafond est tenu par écrêtage de sécurité. La seule latence est celle du tampon audio choisi.
- Fil audio : aucune allocation, aucun verrou. Les changements de structure passent par un graphe publié,
  l'ancien est libéré hors du fil audio après 3 secondes.
- Interface : chaque argument venant de l'interface est contrôlé (type, plage, longueur).
  La WebView ne charge que les fichiers embarqués ; ni accès disque, ni accès réseau.
- Projets chargés : tailles plafonnées (échantillons, textes, nombre d'éléments) ; un projet illisible est ignoré, un élément inconnu est sauté.
- **Audit de sécurité du 4 novembre 2026 (v0.46.1)** :
  - montage : une prise d'un projet n'est acceptée que si c'est un fichier FLAC ou WAV du dossier du projet
    ou des dossiers d'enregistrement de GOLEM (`montage::takeFileAllowed`) : jamais de chemin réseau
    `\\serveur\partage` (fuite de l'empreinte du mot de passe Windows), jamais de « .. », et rien n'est
    recopié depuis l'extérieur à l'enregistrement ;
  - projets, états de séance (logiciel hôte), préréglages et réglages : profondeur des balises XML et du
    JSON plafonnée à 64 avant la lecture (`src/ParseGuard.h`) — un fichier piégé ne fait plus planter ;
    projets plafonnés à 256 Mo sur téléphone et tablette (600 Mo sur ordinateur) ;
  - page de l'interface : politique de sécurité du contenu (CSP, ajoutée par `ui/vite.config.ts` à la
    compilation) — aucun script, cadre, objet ni connexion venant d'ailleurs que l'interface embarquée ;
  - binaires : Control Flow Guard (Windows), protection de pile et `_FORTIFY_SOURCE` (Linux), LAME 3.100 ;
  - distribution : `scripts\empreintes-release.bat` (appelé par `CREER-INSTALLATEUR.bat`) écrit
    `_livraisons\SHA256SUMS.txt`, à joindre à chaque release ; la CI Apple publie les siennes ;
  - CI : jeton GitHub en lecture seule, actions épinglées par empreinte, `npm ci` seul, commit de JUCE vérifié.
  - Rapport complet : `AUDIT-SECURITE-v0.46.md` (projet Claude).

## Tests

`ctest` (lancé par le script) :
- grosse caisse et voix de batterie ;
- séquenceur : placement à l'échantillon près, swing, ternaire, sens de lecture, conditions, synchro DAW ;
- moteur : pistes, inserts, envois, scènes, verrous, modulations, charge processeur ;
- instruments et effets : niveau de crête, extinction en fin de note, charge processeur.

- mesures : intensité (sinus de référence à -20 LUFS à 44,1, 48 et 96 kHz), crête vraie, seuil, limiteur en crête vraie.

- sécurité (v0.46.1, `groovebox_SecurityTests`) : projets piégés (balises imbriquées sans fin, prises du
  montage hors du projet, chemins réseau, « .. »), 150 projets abîmés au hasard relus sans plantage.

`groovebox_InstrumentTests <dossier>` écrit en plus un fichier WAV par instrument et par effet, pour l'écoute.
`groovebox_render --check` vérifie la sauvegarde complète du projet (patterns, lancement, MIDI, chaînes).

## Fichiers tiers

- `ui/src/juce/` : bibliothèque d'interface de JUCE 8 (AGPLv3).
- `ui/src/assets/fonts/` : police Inter (SIL Open Font License, voir `OFL-Inter.txt`).
- `src/third_party/lame/` : encodeur MP3 LAME 3.100 (GNU LGPL 2, voir `COPYING` et `LISEZMOI.md`), pour l'enregistrement audio en MP3.
