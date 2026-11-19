# Good Doctor 2 — fiche d'architecture

Nouvel instrument (type `gooddoc2`), à côté de Good Doctor (inchangé). Principe des grands synthés hybrides à
tables d'ondes de la génération 2025 : on reprend l'architecture, jamais le nom, le panneau, les tables ou les sons
d'origine. Tout est calculé par le programme.

## Veille (18 nov. 2026)

Sources : le « What's New » et le manuel en ligne de l'éditeur, MusicRadar, KVR, ADSR, EDMProd.
- Trois oscillateurs principaux. Modes : tables d'ondes, échantillon, multi-échantillons, granulaire, spectral.
  On garde **tables, granulaire et spectral** (choix d'Henrico). Le Groovebox a déjà ses échantillonneurs.
- Sous-oscillateur et bruit dédiés. Deux déformations (« warps ») par oscillateur.
- Deux filtres, en série ou en parallèle. 4 enveloppes, jusqu'à 10 LFO, 8 macros.
- Matrice avec source auxiliaire et courbe. Rack de 13 effets.
- Pages : Oscillateurs, Mélangeur, Effets, Matrice, Global.
- Compresseur « OTT » : 3 bandes, compression vers le bas et vers le haut, profondeur, temps,
  gains d'entrée et de sortie.
- Basses dures : kick gabber = sinus + enveloppe de hauteur très rapide (200–500 Hz → note en 10–50 ms) + écrêtage
  dur, accordé sur la tonique.
  - Hardstyle : la queue du kick et la « reverse bass » sont le même son, joué sur les temps puis entre eux.
  - La reverse bass « aspire » avant le kick : enveloppe qui monte vers le contretemps.

## Signal

```
Sous-osc ─┐
Osc A ────┤ (tables | granulaire | spectral, unisson 1–7, 2 déformations, inversion, retard)
Osc B ────┼─► sorties : Filtres / F1 / F2 / Direct ─► F1 ─► F2 (ou F1 ‖ F2) ─► ampli (env. 1)
Osc C ────┤
Bruit ────┘
                                   voix ─► somme ─► rack d'effets (10 modules, ordre réglable) ─► sortie
```

## Réglages (positions figées)

- **Global** : voix, glisse, volume, vél. ampli, accord, largeur, jeu (poly, mono, legato), qualité.
- **Oscillateur A/B/C** :
  - réglages de base : marche, mode, table, position, déformations 1 et 2 (25 types) et leurs intensités ;
  - hauteur et niveau : octave, demi-tons, fin, niveau, panoramique ;
  - phase : phase, hasard ;
  - unisson : voix (1–7), désaccord, mélange, largeur ;
  - inversion, retard (0–100 ms), sortie ;
  - granulaire : source, taille, densité, éparpillement, vitesse de balayage, hasard de hauteur,
    fenêtre, part à l'envers ;
  - spectral : source, partiels, étirement, décalage, pente, pair/impair, flou, coupure spectrale, balayage.
- **Sous-osc.** : forme, octave, niveau, pan, sortie, inversion. **Bruit** : couleur, niveau, pan, sortie.
- **Filtres 1 et 2** : les 14 types de Good Doctor + 4 (disperseur, phaseur, passe-bas 6 dB, coupe-bande
  large), coupure, résonance, gain, forme, mélange, clavier, env. 2, vélocité, pan.
- **Enveloppes 1–4** : attaque, tenue, déclin, maintien, relâchement, courbes d'attaque, de déclin et de
  relâchement.
  - Env. 1 = ampli, env. 2 = filtres, env. 3 = hauteur (profondeur en demi-tons), env. 4 libre (matrice).
- **LFO 1–3** : formes (sinus, triangle, scie ↑, scie ↓, carré, pompe, aspiration, hasard tenu, hasard doux),
  vitesse, synchro au tempo, mode (libre, redéclenché, enveloppe en un coup), retard, lissage.
  - **LFO 4–5** : à pas, dessinables (modulation commune).
- **Matrice** : 32 routages, 8 macros, arpège (modulation commune du Groovebox).
- **Rack d'effets** : chaque module a marche, ordre et ses réglages.
  - Hyper/Dimension, Distorsion (10 types, préfiltre), Flanger, Phaser, Chorus, Délai, Compresseur
    (simple ou OTT 3 bandes vers le haut et vers le bas), Filtre, Égaliseur (2 bandes en plateau + 1 en
    cloche), Réverbération.

## Tables

- 16 tables calculées : les 12 de Good Doctor et 4 nouvelles (« Kick », « Hoover », « Sinus → carré »,
  « Formants voix »).
- Plus une table personnelle par oscillateur, dans les emplacements d'échantillon 2, 3 et 4.
- L'éditeur de tables dessine des ondes de 2048 points :
  - pinceaux : ligne, courbes, rampes, sinus, bruit ;
  - formule ;
  - analyse des harmoniques ;
  - ajout et suppression d'ondes.
- Il enregistre la table comme un WAV au format courant des tables d'ondes (2048 points par onde). On peut
  aussi importer un tel WAV.
- Les tables personnelles sont limitées en bande au chargement, hors du fil audio.

## Sources granulaire et spectrale

- L'échantillon de l'emplacement 1 de la piste, quand il y en a un.
- Sinon 6 sources calculées : chœur, souffle, cloches, voix, cordes, grondement.

## Tests

- Chaque mode, chaque déformation, chaque type de filtre et chaque effet : sortie finie, non silencieuse,
  sans explosion.
- Queues < −70 dB 16 s après relâchement. Niveaux des sons d'usine ramenés à la cible.
- Charge CPU mesurée.
- Positions des réglages figées dans les tests.
