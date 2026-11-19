# Micro Tonnerre — fiche d'architecture

Batterie de GOLEM ajoutée après la 1.1 (sortie prévue avec la 1.2). Elle reprend le **principe** des
générateurs de rythmes logiciels à synthèse universelle : une seule architecture de son, simple,
utilisée par chacun des 8 canaux. Elle imite une grosse caisse aussi bien qu'une cymbale, un clap ou
une percussion métallique. Seule l'architecture est reprise : ni le nom, ni le dessin, ni les sons,
ni les motifs d'origine.

- Code : `src/instruments/MicroTonnerre.h` (type `microton`, famille Batteries).
- Kits d'usine : `src/instruments/mt/MtKits.h`, écrit par `tools/presets/kits_microton.py`.
- Façade : `ui/src/screens/faces/MtPanel.tsx` et la disposition `microton` dans `layouts.ts`.

## Un canal

```
Oscillateur ─┐
(sinus, triangle, scie ; modulation de hauteur ; env. A/D)
             ├─ Mélange osc./bruit ─ Égaliseur en cloche ─ Distorsion ─ 2 inserts ─ Niveau / pan ─ sortie
Bruit ───────┘                                                                    └─ Envois A, B
(bruit blanc → passe-bas / bande / haut, résonance ; mono ou stéréo ; env. expo / linéaire / claquée)
```

| Partie | Réglages | Plages |
|---|---|---|
| Mélange | Oscillateur / bruit | 0 à 100 % (au milieu, les deux sont à plein) |
| | Fréquence et gain de l'égaliseur | 20 Hz à 20 kHz, ±40 dB |
| | Distorsion | 0 à 100 %, courbe quadratique (douce au début), crête gardée |
| | Niveau, panoramique | −48 à +12 dB |
| | Groupe d'étouffement | Aucun, groupes 1 à 4 (effacement en 4 ms) |
| Oscillateur | Forme d'onde | Sinus, triangle, dent de scie (à bande limitée) |
| | Fréquence | 20 Hz à 20 kHz |
| | Modulation de hauteur | Déclin, sinus, aléatoire |
| | Quantité | ±96 demi-tons |
| | Durée (déclin), vitesse (sinus, aléatoire) | 1 ms à 10 s · 0,1 Hz à 2 kHz |
| | Attaque, déclin | 0 à 10 s · 10 ms à 10 s |
| Bruit | Filtre | Passe-bas, passe-bande, passe-haut |
| | Fréquence, résonance | 20 Hz à 20 kHz · Q de 0,5 à 100 (le niveau est compensé) |
| | Stéréo | Deux bruits différents à gauche et à droite |
| | Enveloppe | Exponentielle, linéaire, claquée (trois salves dans l'attaque) |
| | Attaque, déclin | 0 à 10 s · 10 ms à 10 s |
| Vélocité | Part sur l'oscillateur, le bruit, la modulation | 0 à 100 % |
| Sortie | Envois A et B | vers les retours partagés |

Réglage du kit : **Volume du kit** (−24 à +6 dB).

## Comportements

- **Relance** : deux voix par canal. Quand on relance un canal qui sonne encore, l'ancien coup
  s'efface en 3 ms pendant que le nouveau part du début. Pas de clic, et l'attaque est toujours
  la même.
- **Départ des formes d'onde** : chaque forme part d'un passage à zéro (sinus 0, triangle ¼, scie ½).
- **Accent** du séquenceur : environ +3,5 dB sur le coup.
- **Pads 9 à 16** : ils rejouent les canaux 1 à 8, pour les contrôleurs à 16 pads.
- **Morphing** entre deux états du kit : avec les scènes et le crossfader de GOLEM. Tous les
  réglages sont verrouillables.
- **Inserts et envois** par canal, comme les autres batteries.
- **Sons d'un canal** enregistrés à part, dans le dossier « Canaux Micro Tonnerre ».

## Kits d'usine (64)

Les rôles des canaux sont fixes : 1 grosse caisse, 2 caisse claire, 3 charley fermé, 4 charley
ouvert, 5 clap, 6 tom, 7 percussion, 8 cymbale. Les charleys sont dans le même groupe
d'étouffement.

| Banque | Kits |
|---|---|
| Classiques | 7 |
| Électro | 7 |
| Techno | 7 |
| House | 7 |
| Hip-hop | 7 |
| Minimal | 7 |
| Rave 96 | 8 (esprit des boîtes de groove de la fin des années 90) |
| Dur | 7 (gabber, frenchcore, hardstyle, tekno, terreur…) |
| Expérimental | 7 |

Chaque son vient d'une recette classique de synthèse de batterie : balayage de hauteur des grosses
caisses, bruit filtré des caisses claires et des charleys, salves des claps, FM à fréquence audio
des métaux, bruit très résonant pour le deuxième partiel des cloches.

Les **niveaux sont égalisés par rôle**, sur la crête mesurée, aux mêmes valeurs que les autres
batteries de GOLEM :

| Rôle | Crête visée |
|---|---|
| Grosse caisse | −6 dB |
| Caisse claire | −7 dB |
| Clap | −6,5 dB |
| Tom | −7,5 dB |
| Percussion | −10 dB |
| Charley ouvert | −13 dB |
| Charley fermé | −14 dB |
| Cymbale | −15 dB |

Pour refaire le calibrage :

```
g++ -std=c++20 -O2 -Isrc tools/MtCalib.cpp -o mtcalib && ./mtcalib mesures.json
python3 tools/presets/kits_microton.py --calibrate mesures.json
```

## Mesures (après la 1.1)

- Tests : `testMicroTonnerre`, réussi sous `ulimit -s 1024`. Il vérifie :
  - chaque forme d'onde et chaque mode de modulation ;
  - chaque filtre et chaque enveloppe du bruit, résonance au maximum ;
  - la distorsion et l'égaliseur au maximum ;
  - la relance sans saut et l'étouffement ;
  - les 64 kits × 8 canaux : chacun sonne, s'éteint et ne donne aucune valeur invalide.
- Charge : 1,9 % d'un cœur au pire (8 canaux relancés à chaque double-croche à 120 BPM, tous
  les kits essayés).
- Écart des niveaux après calibrage : 0 dB sur toutes les crêtes visées.
