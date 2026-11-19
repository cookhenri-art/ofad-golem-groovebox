# Kick Forge (GOLEM 1.2) — constructeur de kicks par couches

Nom de travail « Kick Forge » (à confirmer par Henrico). Principe des constructeurs de kicks par
couches (architecture reprise ; ni nom, ni dessin, ni sons d'origine : toute la bibliothèque est
calculée par GOLEM).

## Principe

- Jusqu'à **8 couches** empilées, chacune comme une piste : son morceau, ses réglages, muet, solo,
  polarité, à l'envers, verrou, hasard.
- **Bibliothèque calculée** (680 morceaux, 12 familles) : Punchs (8 saveurs), Crunchs (6), Rumbles (6),
  Clics (5), Percs (4), Tops de punch (3), Réverbérations de punch (3), Renforts rumble (3), Renforts
  schranz (3), Renforts top (3), Corps / sub (5), Reverse bass (3) — plus **Mes samples** (glisser un
  fichier sur une couche ; le sample est accordé sur la tonalité du kick par détection de hauteur).
- Tous les morceaux sont calculés **dans la tonalité** du kick (do 1 à si 2) ; rumbles, crunchs hachés
  et reverse bass suivent le **tempo** du projet.
- Chaîne d'une couche : morceau → à l'envers → hauteur → étirement (WSOLA, garde la hauteur) →
  longueur et fondus à courbe → passe-haut / passe-bas → égaliseur 3 bandes → transitoires →
  distorsion (10 types, suréchantillonnée) → polarité, volume → décalage.
- **Rack de 9 modules** dans l'ordre choisi : égaliseur, égaliseur M/S, distorsion, façonneur de kick
  (courbe à 8 points), transitoires (au tempo), convolution (27 réponses calculées en 7 catégories,
  courbe du mouillé à 3 points, vitesse, stéréo, ton), finaliseur (grave relevé, grave en mono,
  écrêtage, vue de la réduction), multibande (OTT 3 bandes), filtres résonants. Chaque module a son
  mélange.
- **Gain automatique** : crête à -6 dBFS (comme les grosses caisses des batteries de GOLEM).
- **Styles** (14) : Construction libre, Rumble, Reverse bass, Industriel, Psy, Schranz, Rumble propre,
  Newstyle, Rawstyle, Hard techno, Gabber, Frenchcore, Hardstyle, Techno. « Hasard » tire un kick
  complet dans le style ; en **construction libre**, il garde les morceaux choisis et varie seulement
  leurs réglages (volumes, hauteurs, étirements, décalages, fondus) ; « Garder les fondus » ; une
  couche verrouillée n'est jamais touchée. 78 sons d'usine (6 par style) + Init.
- Hauteur jouée : fixe (tonalité) ou suit la note (classe de note, à moins d'une demi-octave).

## Moteur (temps réel)

Le kick est **calculé entier** par un fil de calcul propre à l'instrument à chaque changement de
réglage, de tempo ou de sample (≈ 4 à 30 ms), en qualité maximale, puis rejoué tel quel : ce qu'on
entend est exactement ce que montre l'afficheur. Le fil audio n'alloue rien et ne bloque jamais : il
dépose la demande (verrou de séquence), lit le dernier kick publié (trois tampons tournants, jamais
réécrits pendant qu'une voix les lit), deux voix en alternance avec fondu de 3 ms. Premier coup
avant tout calcul (ouverture, rendu hors temps réel) : calcul immédiat, une seule fois. Charge
mesurée : 0,12 % d'un cœur par piste.

## Interface

- Page **Kick** : bandeau (style, tonalité, tempo, Écouter, Hasard, Garder les fondus, → Construction
  libre, Vider), grand afficheur (Couches / Somme / Après effets / Écrêtage) sur une grille en
  double-croches, réglable à la souris (corps : décalage, Ctrl aimanté ; ronds : fondus, gauche-droite
  longueur, haut-bas courbe ; barre de fin : longueur), bibliothèque en surimpression, couches en
  pistes (+ Ajouter), réglages de la couche choisie en une rangée, jeu (volume, vélocité, hauteur,
  gain auto).
- Page **Effets du kick** : rack (ordre ▲▼, retirer, + Ajouter un module), module choisi (éditeurs de
  courbe du façonneur et du mouillé de la convolution, vue après effets / réduction), afficheur.
- Pied : une octave de notes depuis la tonalité (pas de grand clavier).

## Fichiers

- `src/instruments/kf/KfParts.h` — bibliothèque calculée.
- `src/instruments/kf/KfRender.h` — calcul du kick (couches, rack, réponses impulsionnelles, gain auto).
- `src/instruments/kf/KfStyles.h` — styles, hasard.
- `src/instruments/KickForge.h` — instrument (fil de calcul, voix), réglages (329), sons d'usine, vues.
- Ponts : `getKickView`, `getKickLibrary`, `kickRandomize` (PluginEditor.cpp).
- Interface : `ui/src/screens/faces/KfPanel.tsx`, `layouts.ts` (kickforge, thème « forge »).

## Reste à valider à l'oreille (Henrico)

Caractère de chaque famille et de chaque style, niveaux relatifs des couches dans les recettes,
réponses de la convolution, écrêtage du finaliseur.
