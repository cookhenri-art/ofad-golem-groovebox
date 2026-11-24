# Groovebox — correspondances des instruments et écarts d'architecture (état v0.9, suivi jusqu'à la v0.46)

## Suivi v0.10 (28/10/2026)

Fait en v0.10 : tous les écarts **P1** des synthés et des batteries, plus la plupart des **P2** :
- moteur : molette, pitch bend, pression, pédale (MIDI et à l'écran) ; arpège de piste ; accent, glissé, flam,
  longueur par ligne ; matrice de modulation commune (source × via → destination) dont les sources sont les
  enveloppes et LFO propres à chaque synthé, plus des enveloppes DAHDSR, des LFO à pas et des macros ;
- basse mono (Sub 37), basse acide (303), batteries 808 / 909 / Rytm (machines par pad, 7 réglages chacune) ;
- poly analogique (Prophet-5) : formes cumulables, Wheel-Mod, révision du filtre, relâchement, pression ;
- poly à chorus (Juno) : LFO auto / manuel, levier vers oscillateur et filtre, maintien, chorus I et II ;
- poly hybride (Peak) : tables d'ondes, Shape (synchro dure / densité), FM croisée, modes de voix, matrice 16 ;
- wave morph (Hydrasynth) : osc. 3, 2 mutants par oscillateur (dont pile d'ondes, phase décalée, harmonique),
  filtres voyelle et peigne, 5 enveloppes, 5 LFO dont 3 à pas, matrice 32, 8 macros, arpège à pas, unisson ;
- nappes (Sylenth1) : 4 oscillateurs, phase, inversion, panoramique, impulsion étroite, tri-scie, filtre
  contourné, commande globale des filtres, ampli B, 2e enveloppe de modulation, matrice 16, arpège à pas ;
- une façade « comme la machine » par instrument (le sampler suit avec la refonte façon Octatrack).

Écarts qui restent, assumés :
- wave morph : 16 formes calculées (la référence en a plus de 200) ; LFO à 16 pas (64 sur la référence) ; pas de
  panoramique par source avant les filtres (il doublerait le coût des filtres) ; pas de ruban ni d'aftertouch
  polyphonique (le moteur reçoit la pression de canal) ;
- poly hybride : pas de boutons « Animate » (la molette et les macros jouent ce rôle) ;
- nappes : la matrice de la référence a 8 sources à 2 destinations, ici 16 routages à une destination (même
  couverture) ;
- effets intégrés des références (delay, réverbération, phaser…) : ils passent par les inserts de la piste ;
- batteries : retrig assignable, sound lock par pas et séquence euclidienne restent à faire (P2) ;
- sampler : le « sampler de performance » (v0.11) reprend l'architecture d'une piste Octatrack : machines Flex,
  Static, Thru, Neighbor, Pickup ; lecture (hauteur, début, longueur, vitesse, redéclenchements), étirement
  Normal / Beat, découpes (grille ou attaques), enregistreur par piste, 3 LFO à pas, 2 effets (inserts 1 et 2),
  verrou d'échantillon, pas sans déclenchement, glissés de réglages, pas d'enregistrement. Écarts : Static lit
  aussi depuis la mémoire (fichiers de 3 min au plus) ; pas de parties ni d'arrangeur ; pas de pistes MIDI ;
  découpes non éditables à la main ; enregistreur de 16 s par piste.

Date : 28/10/2026. Demande d'Henrico : redonner la liste des correspondances, comparer chaque instrument
à l'architecture de sa machine de référence (voix, modulations, fonctions comme le glissé par note de la
basse acide) et lister tout ce qui manque.

**Méthode.** C'est celle de la compétence « modélisation de synthés analogiques » proposée le 27/10 :
1. architecture de la référence, sourcée (fiches constructeur, manuels, essais) ;
2. relevé de ce que le code de la v0.9 fait réellement (réglages, voix, modulations, séquenceur) ;
3. écarts classés par priorité.

Cette compétence n'est pas dans ta bibliothèque aujourd'hui (elle avait été proposée, pas enregistrée).
La méthode est donc appliquée ici à la main.

**Rappel de principe** (décision du 27/10) : on reproduit l'architecture et le comportement, jamais le nom, le
graphisme, les échantillons, les tables d'ondes ni les préréglages d'origine.

**Priorités**
- **P1** : manque qui empêche d'avoir le caractère ou le jeu typique de la machine.
- **P2** : fonction importante de la machine, mais on peut s'en passer pour jouer.
- **P3** : confort ou fidélité de détail.

---

## 1. Correspondances

| Instrument Groovebox | Machine de référence (principe repris) | Fidélité d'architecture aujourd'hui (estimation) |
|---|---|---|
| Batterie analogique (`drumA`) | Elektron Analog Rytm MKII | Bonne sur la chaîne d'une voix ; manquent les machines par pad, la couche échantillon superposée, le LFO par pad |
| Batterie 80 (`drum80`) | Roland TR-808 | Bonne sur les instruments ; manquent l'accent global et les réglages propres à chaque instrument |
| Batterie 90 (`drum90`) | Roland TR-909 | Correcte ; manquent le flam, l'accent total et la longueur par instrument |
| Basse acide (`acid`) | Roland TB-303 | Bonne sur le son ; le glissé et l'accent existent mais ne se posent pas pas par pas comme sur la machine |
| Basse mono (`mono`) | Moog Sub 37 / Subsequent 37 | Correcte ; manquent la réinjection, les enveloppes DAHDSR bouclables, les bus de modulation, l'arpégiateur |
| Poly analogique (`poly5`) | Sequential Prophet-5 | Bonne (Poly-Mod en place) ; manque le vrai Wheel-Mod (LFO / bruit, destinations séparées, molette) |
| Poly chorus (`polyj`) | Roland Juno-60 (et 106) | Très bonne sur la voix ; manque l'arpégiateur |
| Poly hybride (`hybrid`) | Novation Peak | Moyenne : 3 oscillateurs en place, mais ni tables d'ondes, ni matrice de modulation, ni 4 LFO |
| Wave morph (`morph`) | ASM Hydrasynth | Moyenne : manquent l'oscillateur 3, 5 enveloppes, 5 LFO, matrice 32 routages, mutants doubles |
| Nappes (`pads`) | LennarDigital Sylenth1 | Moyenne : 2 oscillateurs au lieu de 4, 1 enveloppe d'ampli au lieu de 2, pas de matrice, pas d'arpégiateur |
| Sampler à découpes / Sampler chromatique | Stem Sampler Pro (ton logiciel) | Bonne sur la lecture ; la séparation de pistes par IA n'est pas reprise |

**Effets** (pour mémoire, hors du périmètre « synthés ») :
- Distorsion ↔ principe Coldfire ;
- Écho ↔ OhmBoyz / EchoBoy ;
- Réverbération ↔ FabFilter Pro-R ;
- compresseur optique ↔ principe LA-2A ;
- compresseur FET ↔ principe des compresseurs à FET de studio ;
- compresseur de bus ↔ principe des compresseurs de bus de console ;
- chorus, phaser, répétition (stutter), filtre, égaliseur, limiteur, porte et dé-esseur.

---

## 2. Manques communs à plusieurs instruments (à faire une fois, tout le monde en profite)

| # | Manque | Instruments concernés | Priorité |
|---|---|---|---|
| C1 | **Molette de modulation, molette de hauteur (pitch bend), aftertouch canal** : le moteur n'exploite aujourd'hui ni le pitch bend, ni la molette, ni la pression MIDI | Tous les synthés ; indispensable pour le Wheel-Mod du Prophet-5, les bus du Sub 37, Peak, Hydrasynth, Sylenth | **P1** |
| C2 | **Arpégiateur** (haut, bas, haut-bas, ordre joué, aléatoire ; 1 à 4 octaves ; synchro au tempo ; maintien) | Juno-60 (emblématique), Sub 37, Peak, Hydrasynth, Sylenth (avec pas : hauteur, vélocité, maintien) | **P1** |
| C3 | **Fonctions de pas façon basse acide** : glissé et accent posés pas par pas, octave ±1 par pas, liaison | Basse acide (indispensable), Rytm (fonctions de pas accent et glissé) | **P1** |
| C4 | **Accent global** : une ligne d'accent dans le séquenceur, avec un niveau d'accent pour toute la batterie | 808, 909 (« total accent »), Rytm | **P1** |
| C5 | **Matrice de modulation par instrument** : N routages source(s) → destination avec profondeur. Aujourd'hui, destinations fixes plus les 4 sources du son de la piste (une seule modulation par réglage) | Peak (16 routages, 2 sources par routage), Hydrasynth (32), Sylenth (16), Sub 37 (2 bus programmables) | **P1** |
| C6 | **Enveloppes DAHDSR** (retard, maintien) **bouclables**, calées sur le tempo, avec suivi clavier et sensibilité à la vélocité, déclenchement simple ou multiple | Sub 37, Hydrasynth, Peak (simple / multiple) | **P2** |
| C7 | **Longueur par ligne de batterie** (polymétrie) : aujourd'hui la longueur est par pattern de piste | Rytm (longueur par piste), 909 (dernière étape par instrument) | **P1** pour les batteries |
| C8 | **Flam par pas** (2 coups rapprochés, écart réglable) et coups faibles | 909 | **P2** |
| C9 | **Shuffle par instrument** en plus du swing global | 909 | **P3** |
| C10 | **Modes de glissé** : temps constant, vitesse constante, exponentiel ; seulement en legato ; par oscillateur | Sub 37, Sylenth (2 modes), Hydrasynth (courbe) | **P2** |
| C11 | **Modes de voix** : mono, mono legato, paraphonique, poly, poly avec 2 oscillateurs par note… | Peak (5 modes), Sylenth (mono / legato), Sub 37 (mono / duo, déjà fait) | **P2** |
| C12 | **Changement de son par pas** (sound lock) et changement d'échantillon par pas | Rytm | **P2** |

---

## 3. Instrument par instrument

### 3.1 Batterie analogique ↔ Analog Rytm

**Référence.**
- Chaque voix a :
  - un générateur de percussion analogique et un moteur de lecture d'échantillon superposés ;
  - une saturation (overdrive) et un filtre multimode 2 pôles ;
  - un VCA avec panoramique ;
  - des enveloppes de filtre et d'ampli ;
  - deux envois d'effet et un LFO assignable avec son enveloppe d'entrée.
- En sortie : distorsion et compresseur analogiques stéréo.
- Le séquenceur offre aussi : accent, glissé, conditions, micro-décalage, changement de son ou d'échantillon par pas, retrig assignable, générateur euclidien.

**En place (v0.9).**
- Par pad :
  - voix sur principe de circuit (T ponté, oscillateur balayé, résonateurs, métaux à 6 carrés) avec dérive analogique ;
  - saturation, filtre multimode 7 modes avec enveloppe ;
  - VCA attaque / maintien / déclin, panoramique, vélocité ;
  - groupe d'étouffement ;
  - effet par pad et 2 envois.
- Séquenceur : conditions, verrous, micro-décalage, répétitions dans le pas, scènes.
- Distorsion et compresseur disponibles en insert du bus.

**Manque.**
- **P1** :
  - **choix de la machine par pad** : plusieurs modèles par famille (grosse caisse dure / classique / FM, caisse claire, métaux, bruit). C'était le lot « machines par pad » repris de Sillage, encore à faire ;
  - **couche échantillon superposée** à la voix analogique, avec son propre niveau et accord. Aujourd'hui, un échantillon sur un pad remplace la synthèse ;
  - **LFO par pad** avec enveloppe d'entrée (multi-formes, destinations au choix) ;
  - **longueur par ligne** (C7) ;
  - **accent** (C4).
- **P2** :
  - **retrig assignable** : vitesse indépendante du pas (1/32, 1/16T…), longueur sur plusieurs pas, courbe de vélocité. Les répétitions de la v0.8 ne couvrent que l'intérieur d'un pas ;
  - glissé de hauteur entre pas ;
  - **changement de son et d'échantillon par pas** (C12) ;
  - chaîne de sortie « distorsion + compresseur » posée d'office sur le bus batterie.
- **P3** :
  - générateur euclidien par ligne ;
  - boucle de page ;
  - macros de performance (en partie couvertes par les scènes).

### 3.2 Batterie 80 ↔ TR-808

**Référence.**
- Réglages par instrument :
  - grosse caisse : niveau, timbre, déclin ;
  - caisse claire : timbre, claquant (snappy) ;
  - toms et congas : accord ;
  - cymbale : timbre, déclin ;
  - charleston ouvert : déclin.
- Un niveau d'accent global, des sélecteurs tom/conga, rimshot/claves, clap/maracas, et des variations A / AB / B avec intro et fill.

**En place.**
- Les 16 sons sur 16 pads (congas, claves, maracas compris), voix à circuits en T ponté et métaux à 6 oscillateurs.
- Réglages génériques par pad (accord, déclin, timbre, claquant, etc.), filtre et VCA par pad.

**Manque.**
- **P1** :
  - **accent global** avec niveau (C4) : sur la machine, l'accent agit sur tous les instruments du pas. Ici, il n'y a que la vélocité pad par pad ;
  - **réglages fidèles à chaque instrument** : la courbe et la plage de chaque bouton doivent coller au comportement d'origine (timbre de la grosse caisse = filtre sur le clic, long déclin de la grosse caisse, timbre de la cymbale = équilibre des deux bandes). À valider au banc A/B.
- **P2** : variations A / AB / B à l'intérieur d'un pattern (on a 8 patterns et la condition FILL, ce qui suffit en pratique).

### 3.3 Batterie 90 ↔ TR-909

**Référence.**
- Réglages par instrument :
  - grosse caisse : accord, attaque, déclin ;
  - caisse claire : accord, timbre, claquant ;
  - toms : accord, déclin ;
  - charlestons et cymbales échantillonnés en basse résolution : accord, déclin.
- Séquenceur :
  - accent total et accent par instrument ;
  - shuffle ;
  - flam sur la grosse caisse, la caisse claire et les toms ;
  - dernière étape par instrument ;
  - dans les versions récentes : coups faibles et sous-pas.

**En place.**
- Grosse caisse balayée, cymbales en basse résolution.
- Répétitions dans le pas (×2 à ×8), qui couvrent les sous-pas.

**Manque.**
- **P1** : **flam par pas** (C8), **accent total** (C4), **longueur par instrument** (C7).
- **P2** : attaque de la grosse caisse fidèle (clic réglable séparément) ; coups faibles (vélocité basse en un geste).
- **P3** : shuffle par instrument (C9).

### 3.4 Basse acide ↔ TB-303

**Référence.**
- Un VCO scie ou carré.
- Filtre passe-bas résonant à **−18 dB/oct.**, qui ne s'auto-oscille pas.
- Enveloppe de filtre à déclin seul.
- **Accent** : renforce le VCA et le filtre, avec une bosse d'environ 200 ms qui s'accumule sur les accents rapprochés.
- **Glissé à temps constant**.
- Séquenceur :
  - accent, glissé, octave haut / bas **posés sur chaque pas** ;
  - notes, liaisons et silences entrés à part ;
  - shuffle.

**En place.**
- Oscillateur scie / carré, filtre à diodes 4 pôles résonant.
- Enveloppe de filtre à déclin seul, accent qui raccourcit l'enveloppe et s'accumule.
- Glissé entre notes qui se chevauchent (temps réglable, 60 ms d'origine).
- Saturation et distorsion en plus.

**Manque.**
- **P1** :
  - **glissé et accent posés pas par pas** (C3) : deux interrupteurs par pas dans le séquenceur, plus **octave ±1**. Aujourd'hui, le glissé demande d'allonger la note jusqu'au pas suivant, et l'accent demande une vélocité au-dessus du seuil. C'est ce que tu appelles le « slide par note » ;
  - **pente du filtre** : vérifier au banc qu'on a bien la réponse −18 dB/oct. de la machine et non −24 dB/oct. (notre filtre est décrit à 4 pôles). Si besoin, ajouter le comportement qui empêche l'auto-oscillation.
- **P2** :
  - forme exacte de la bosse d'accent (montée rapide, sommet arrondi, descente) à caler au banc ;
  - durée de porte de 50 % du pas, et liaison qui prolonge sans redéclencher ;
  - générateur de motifs aléatoires (« randomize ») avec annulation.
- **P3** : saisie à la manière de la machine (notes d'abord, rythme ensuite).

### 3.5 Basse mono ↔ Sub 37 / Subsequent 37

**Référence.**
- Sources :
  - 2 oscillateurs à forme continue ;
  - sous-oscillateur carré ;
  - bruit ;
  - **entrée externe / réinjection du filtre** dans le mélangeur.
- Filtre en échelle 6 à 24 dB avec multidrive.
- **2 enveloppes DAHDSR bouclables** (multi-déclenchement, remise à zéro, synchro, verrou, sensibilité à la vélocité, suivi clavier).
- **2 bus de modulation assignables** (LFO de 0,01 Hz à 1 kHz, remise à zéro par le clavier).
- Glissé de type LCR, LCT, EXP, avec modes à porte et legato, assignable à l'oscillateur 1, 2 ou aux deux.
- Mono / duo, arpégiateur et séquenceur de 64 notes.

**En place.**
- 2 oscillateurs à forme continue, sous-oscillateur carré ou sinus, bruit, synchro.
- Échelle 6 / 12 / 18 / 24 dB avec saturation.
- Deux ADSR (filtre, ampli), enveloppe de filtre vers la hauteur.
- 2 LFO à destinations fixes, synchro au tempo depuis la v0.9.
- Mono / duo, portamento « toujours / lié », redéclenchement.

**Manque.**
- **P1** :
  - **réinjection du filtre dans le mélangeur** (grognement typique) ;
  - **enveloppes DAHDSR bouclables** (C6) ;
  - **bus de modulation** : LFO 1 → hauteur / filtre / destination au choix, LFO 2 idem, avec la **molette** qui dose la modulation (C1, C5) ;
  - sources LFO triangle, carré, scie, rampe, échantillonné-bloqué.
- **P2** :
  - types de glissé LCR / LCT / EXP et choix de l'oscillateur (C10) ;
  - LFO jusqu'à la fréquence audio (1 kHz) et remise à zéro au clavier ;
  - arpégiateur (C2).

### 3.6 Poly analogique ↔ Prophet-5

**Référence.**
- 2 VCO par voix :
  - A : scie, impulsion à largeur réglable, synchro sur B ;
  - B : scie, triangle, impulsion, mode basse fréquence, suivi clavier on/off.
- Mélangeur A, B, bruit.
- **Poly-Mod** : enveloppe de filtre et oscillateur B → fréquence A, largeur A, filtre.
- **Wheel-Mod** : mélange LFO / bruit envoyé par la molette vers fréquence A, fréquence B, largeurs A et B, filtre.
- LFO à formes scie, triangle et carré, cumulables.
- Filtre 4 pôles (choix Rev1/2 ou Rev3 sur la version actuelle).
- Glissé polyphonique, unisson, bouton Vintage, aftertouch vers le filtre et la quantité de LFO.

**En place.** Les deux oscillateurs complets, mélangeur, Poly-Mod complet, filtre 24 dB, deux ADSR, unisson, Vintage, glissé, vélocité.

**Manque.**
- **P1** : **Wheel-Mod fidèle** :
  - source = mélange LFO / bruit ;
  - destinations séparées fréquence A, fréquence B, largeur A, largeur B, filtre ;
  - quantité donnée par la molette (C1).

  Aujourd'hui, le LFO va vers la hauteur des deux oscillateurs ensemble, la largeur et le filtre, sans bruit ni molette.
- **P2** :
  - LFO à formes cumulables (scie + triangle + carré) ;
  - aftertouch → filtre et quantité de LFO (C1) ;
  - choix de caractère de filtre « Rev1/2 » (plus mordant) ou « Rev3 ».
- **P3** : interrupteur « relâchement » (release on/off), plage du pitch bend par son.

### 3.7 Poly chorus ↔ Juno-60 (et 106)

**Référence.**
- Un DCO par voix : scie, impulsion à largeur modulée (manuelle, LFO, enveloppe), sous-oscillateur, bruit, registre 16' / 8' / 4'.
- Passe-haut à 4 positions.
- VCF résonant avec enveloppe à polarité inversable, LFO et suivi clavier.
- Une seule ADSR ; VCA sur enveloppe ou porte.
- LFO avec retard et déclenchement auto / manuel.
- Chorus I / II.
- **Arpégiateur** haut, haut-bas, bas, sur 1 à 3 octaves, avec maintien des notes (hold).

**En place.** Toute la voix, le LFO à retard, chorus I / II / I + II, dérive. Synchro du LFO au tempo depuis la v0.9.

**Manque.**
- **P1** : **arpégiateur avec maintien** (C2). C'est la fonction la plus typique de cette machine qui manque.
- **P2** : déclenchement manuel du LFO (bouton de jeu) ; maintien des notes (hold) au clavier.
- **P3** : mode « 106 » (phase du sous-oscillateur, différences de chorus).

### 3.8 Poly hybride ↔ Novation Peak

**Référence.**
- 8 voix, 5 modes de voix (Mono, MonoLG, Mono2, Poly, Poly2).
- Par voix :
  - 3 oscillateurs numériques (sinus, triangle, scie, carré / impulsion, plus 17 tables d'ondes de 5 formes) ;
  - bruit, modulateur en anneau ;
  - saturation avant le filtre ; filtre à variable d'état 12 / 24 dB passe-bas, passe-bande, passe-haut ; distorsion après le filtre ;
  - 1 enveloppe d'ampli et 2 de modulation ;
  - 2 LFO par voix, plus 2 globaux.
- Matrice de modulation de 16 routages (2 sources par routage), plus 16 affectations directes.
- FM linéaire entre oscillateurs, boutons « Animate », arpégiateur, chorus / delay / réverbération intégrés.

**En place.** 3 oscillateurs, anneau 1×2, bruit, FM 3 → 1, saturation avant et distorsion après filtre, filtre 5 types, 3 enveloppes, 2 LFO, étalement et dérive.

**Manque.**
- **P1** :
  - **matrice de modulation** à 16 routages, 2 sources par routage (C5) ;
  - **tables d'ondes** (17 jeux de 5 formes, à créer nous-mêmes) et **modulation de forme** par oscillateur (largeur d'impulsion, densité de scie, repli du triangle) ;
  - **FM linéaire** de n'importe quel oscillateur vers n'importe quel autre (aujourd'hui seulement 3 → 1).
- **P2** :
  - 2 LFO globaux en plus (4 en tout) ;
  - les 5 modes de voix (C11) ;
  - enveloppes à déclenchement simple / multiple ;
  - « Animate » (deux boutons qui basculent un jeu de modulations ; proche de nos scènes) ;
  - arpégiateur (C2).
- **P3** : effets intégrés (couverts par les inserts de la piste ; une chaîne prête « façon Peak » suffirait).

### 3.9 Wave morph ↔ Hydrasynth

**Référence.**
- 8 voix.
- **3 oscillateurs** : 219 formes d'un cycle, morphing de 8 formes sur les oscillateurs 1 et 2.
- **2 mutants par oscillateur** : FM linéaire, empilement de formes, synchro, compression d'impulsion, balayage d'harmoniques, distorsion de phase.
- Mélangeur 5 sources avec **panoramique par source** (3 oscillateurs, bruit, anneau).
- 2 filtres en série ou parallèle :
  - filtre 1 à 16 types (échelle, voyelles, peigne…) ;
  - filtre 2 qui balaie passe-bas → passe-bande → passe-haut.
- **5 enveloppes DAHDSR bouclables**, calées sur le tempo.
- **5 LFO** avec mode pas à pas jusqu'à 64 pas.
- **Matrice de 32 routages**, 8 macros, arpégiateur avec phrases, ruban, effets avant et après.

**En place.**
- 2 oscillateurs à balayage de tables, 1 mutant par oscillateur (FM, synchro, compression, repli).
- Anneau, bruit.
- 2 filtres série / parallèle avec balayage bas → bande → haut.
- 3 ADSR, 2 LFO.

**Manque.**
- **P1** :
  - **oscillateur 3** ;
  - **5 enveloppes DAHDSR bouclables** (C6) ;
  - **5 LFO avec mode pas à pas** ;
  - **matrice de 32 routages** (C5).
- **P2** :
  - **2 mutants par oscillateur** et les types manquants (empilement de formes, balayage d'harmoniques, distorsion de phase) ;
  - plus de types pour le filtre 1 (voyelles, peigne, échelle, etc.) ;
  - panoramique par source dans le mélangeur ;
  - réduction de résolution des oscillateurs ; LFO et enveloppes quantifiés.
- **P3** :
  - arpégiateur avec phrases (C2) ;
  - ruban (l'aftertouch polyphonique est exclu par décision du 27/10) ;
  - effets avant / après intégrés (couverts par les inserts).

### 3.10 Nappes ↔ Sylenth1

**Référence.**
- 2 parties A et B, **4 oscillateurs** en tout (2 par partie), chacun avec jusqu'à 8 voix d'unisson stéréo désaccordées, panoramique et phase réglables.
- Formes : scie, impulsion, demi-impulsion, quart d'impulsion, triangle, sinus, bruit, tri-scie.
- 2 filtres stéréo 12 / 24 dB passe-bas, passe-bande, passe-haut avec saturation.
- **4 ADSR** : 2 d'ampli (une par partie) et 2 de modulation.
- 2 LFO à 11 formes, synchronisables au tempo.
- **Matrice de 16 routages**.
- **Arpégiateur avec pas à pas** (10 modes ; hauteur, vélocité, maintien par pas).
- 2 modes de portamento.
- 7 effets intégrés : distorsion, phaser, chorus / flanger, égaliseur, delay, réverbération, compresseur.

**En place.**
- 2 parties d'un oscillateur chacune, unisson jusqu'à 8, désaccord, étalement.
- Formes sinus, triangle, scie, carré, impulsion, bruit.
- Filtre par partie (4 types) et saturation.
- 1 enveloppe d'ampli, 1 enveloppe de modulation, 2 LFO à destinations fixes, chorus.

**Manque.**
- **P1** :
  - **2ᵉ oscillateur par partie** (4 en tout) ;
  - **enveloppe d'ampli par partie** (2) et **2ᵉ enveloppe de modulation** ;
  - **matrice de 16 routages** (C5) ;
  - **arpégiateur avec pas à pas** (C2).
- **P2** :
  - formes demi-impulsion, quart d'impulsion, tri-scie ;
  - par oscillateur : phase (libre / redémarrée), inversion, panoramique ;
  - 11 formes de LFO ;
  - modes mono / legato et 2 modes de portamento (C10, C11) ;
  - section « commande des filtres » commune aux deux filtres.
- **P3** : effets intégrés (couverts par les inserts ; prévoir une chaîne prête « façon Sylenth »).

### 3.11 Samplers ↔ Stem Sampler Pro

**En place.**
- Sampler à découpes : 16 zones, découpage égal ou sur les attaques, coup unique / porte / boucle, inversé, calage de la zone sur 1/4 à 4 mesures.
- Sampler chromatique : boucles avant ou aller-retour, enveloppes d'ampli et de filtre, 8 voix.

**Manque.**
- **P2** :
  - **séparation de pistes par IA** : volontairement non reprise (modèle externe lourd). À décider : on la porte avec le modèle ONNX, ou on la laisse dans Stem Sampler Pro ;
  - **étirement temporel sans changement de hauteur**. Le calage actuel règle la durée de la zone ; à vérifier sur le code de Stem Sampler Pro pour reprendre son algorithme s'il en a un.
- **P3** : enregistrement d'échantillons depuis l'entrée audio, édition à la souris des points de début / fin sur la forme d'onde.

---

## 4. Ordre de réalisation proposé

Estimations du bureau, en jours-homme de développement assisté. Chaque lot est livrable seul.

| Lot | Contenu | Gain | Effort |
|---|---|---|---|
| **A** | Fonctions de pas façon basse acide (glissé, accent, octave ±1 par pas) + accent global des batteries + flam + longueur par ligne de batterie | Basse acide, 808, 909, Rytm | 2–3 j |
| **B** | Molette de modulation, pitch bend, aftertouch canal (MIDI et à l'écran, dans la vue synthé) + Wheel-Mod fidèle du Prophet-5 | Tous les synthés | 2 j |
| **C** | Arpégiateur commun (modes, octaves, synchro, maintien, pas à pas façon Sylenth) | Juno, Sub 37, Peak, Hydrasynth, Sylenth | 2–3 j |
| **D** | Matrice de modulation commune par instrument (16 à 32 routages, 2 sources, molette et aftertouch comme sources) | Peak, Hydrasynth, Sylenth, Sub 37 | 3–4 j |
| **E** | Enveloppes DAHDSR bouclables calées sur le tempo ; LFO supplémentaires avec mode pas à pas | Sub 37, Hydrasynth, Peak, Sylenth | 2–3 j |
| **F** | Architecture complète par synthé : réinjection du Sub 37 ; tables d'ondes et FM libre du Peak ; oscillateur 3 et mutants doubles de l'Hydrasynth ; 4 oscillateurs et 2 enveloppes d'ampli du Sylenth | Fidélité de chaque synthé | 5–8 j |
| **G** | Batteries : machines par pad, couche échantillon superposée, LFO par pad, retrig assignable, changement de son par pas | Rytm, 808, 909 | 4–6 j |
| **H** | Bancs A/B à l'oreille et réglages fins (pente du filtre de la basse acide, bosse d'accent, réglages 808 / 909) | Fidélité du son | avec enregistrements de référence |

**Point technique à décider pour D et E.** L'espace d'adresses d'une piste est plein :
- 448 réglages d'instrument ;
- 4 inserts de 64 ;
- 6 sources de modulation ;
- 64 profondeurs de modulation.

Une matrice par instrument et des LFO / enveloppes en plus demandent d'agrandir cet espace (adresse canal × 2048 au lieu de × 1024). Les projets s'enregistrent par identifiants, donc les anciens projets se rouvriraient. Mais il faut reprendre les macros MIDI et les tests. C'est la même refonte que celle évoquée pour avoir plus de 2 sources de modulation pour les effets : mieux vaut la faire une seule fois, au début du lot D.

---

## 5. Sources consultées

- Elektron, fiche Analog Rytm MKII : https://elektron.se/explore/analog-rytm-mkii
- Roland, spécifications TR-808 : https://support.roland.com/hc/en-us/articles/201963539-TR-808-Technical-Specifications
- Roland, spécifications TR-909 : https://support.roland.com/hc/en-us/articles/201921899-TR-909-Technical-Specifications
- Roland, manuel du TR-909 logiciel (accent total, flam, sous-pas) : https://www.rolandcloud.com/getmedia/a88fc05f-0b9f-4ba2-8266-b2272ba31572/TR-909-Manual-E.pdf?ext=.pdf
- MusicRadar, guide du TB-303 (accent, glissé, octaves par pas) : https://www.musicradar.com/news/producers-guide-roland-tb-303
- Synthtopia, Roland TB-303 (filtre 18 dB/oct., glissé constant, accent) : https://www.synthtopia.com/?p=387
- Spécifications du Subsequent 37 : https://www.musicmatter.co.uk/moog-subsequent-37 et https://reverb.com/item/84003611-moog-subsequent-37-synthesizer-keyboard-37-keys
- Sequential, Prophet-5 actuel : https://sequential.com/classics-reissued/prophet-5-10/
- Bristol, architecture du Prophet-5 (Wheel-Mod, Poly-Mod) : https://bristol.sourceforge.net/prophet5.html
- Roland, spécifications Juno-60 : https://support.roland.com/hc/en-us/articles/201955179-Juno-60-Technical-Specifications
- Sound On Sound, Novation Peak (LFO, matrice) : https://www.soundonsound.com/node/4920592?page=1
- Fiche Novation Peak : https://www.chicagomusicexchange.com/products/novation-peak-polyphonic-desktop-synthesizer-1054393
- ASM Hydrasynth Explorer (mutants, 5 LFO pas à pas, 5 DAHDSR, 32 routages) : https://reverb.com/uk/item/70089955-asm-hydrasynth-explorer
- LennarDigital, spécifications Sylenth1 : https://www.lennardigital.com/sylenth1/specs.php
- Vintage Synth Explorer, Sylenth1 (formes, 2 + 2 enveloppes, arpégiateur) : https://vintagesynth.com/lennar-digital/sylenth1

---

## 6. Vocodeurs (v0.25)

Famille « Vocodeurs » : quatre instruments qui reprennent le **principe** de quatre machines. Seule l'architecture est reprise : ni nom, ni façade, ni son d'usine d'origine.

| Instrument | Principe repris | Ce qui fait le caractère | Écarts volontaires |
|---|---|---|---|
| Vocodeur 16 bandes | Vocodeur analogique 16 canaux de 1978 (16 bandes de 50 à 5 080 Hz, souffle « Hiss », bourdon « Buzz », canal direct des aigus, échantillonneur-bloqueur, tableau de connexion) | 16 bandes 24 dB/oct, suiveurs très rapides (0,3 à 30 ms), souffle injecté sur les consonnes, bourdon sur les voyelles, aigus directs au-dessus de 5 080 Hz, **maintien**, **décalage des bandes** (± 8) | Le tableau de connexion libre est réduit à un décalage global des bandes. L'« équilibre » d'origine est remplacé par la « voix directe ». |
| Vocodeur à cordes | Clavier cordes + vocodeur (moteur de cordes à diviseurs, vocodeur jusqu'à 256 bandes, formants, gel, instabilité de la porteuse) | Porteuse cordes 8' + 4' avec registre violon → orgue, ensemble, crescendo / relâchement, 16 à 40 bandes, formants ± 12 demi-tons, gel, sibilantes, cordes seules mélangeables | 40 bandes au plus (filtres, sans latence) au lieu de 256 (spectral). Pas de partie solo séparée : la porteuse suffit. |
| Vocodeur spectral | Vocodeur logiciel à 22–24 algorithmes, formants, expansion, traînes réverbérantes, stéréo, synthèse des non-voisées, aigus directs, synthé à unisson et FM, gel | 8 algorithmes de bancs : analogique 12 / 20, numérique 32, haute définition 40, voyelles, inversé, cristal, velours. S'y ajoutent formants, expansion, traînes, stéréo par bandes alternées, consonnes de synthèse, aigus directs et gel. Porteuse à unisson (jusqu'à 4) et FM linéaire. | Pas de méthodes LPC ni d'analyse en composantes indépendantes. Pas de quantification de hauteur de la voix. |
| Voix de synthèse | Processeur vocal à 5 modules parallèles (vocodeur, voix d'ordinateur, boîte parlante, voix modélisée, épaississement) | Les modules sont mélangés par faders : vocodeur 12 bandes, ordinateur (vocodeur 6 bandes sur porteuse carrée, bits et échantillonnage réduits), boîte parlante (3 formants suivis en temps réel), voix modélisée (voyelle A → OU, nasalité, formants), épaississement (doublage stéréo). | La boîte parlante d'origine a 2 048 échantillons de latence ; ici elle n'en a aucune. Pas de mode « automatique » qui suit la hauteur de la voix : la porteuse se joue au clavier. |

**Modulateur (la voix), commun aux quatre.** Il y a quatre sources possibles :
- la voix interne de démonstration, une phrase synthétisée ;
- l'entrée audio ;
- la piste précédente, qui peut être retirée du mixage ;
- n'importe quelle piste, lue après ses inserts et avant son volume.

Pour tous les modèles :
- une porte de bruit protège la voix ;
- la source et son gain restent ceux de la piste quand on choisit un son d'usine ou utilisateur.

**Choix techniques.**
- Filtres à variables d'état (TPT) normalisés, d'un ou deux étages.
- Synthèse en mono sur la somme de la porteuse. La stéréo vient des bandes et des effets, ce qui divise le coût par deux.
- Aucune transformée de Fourier, donc aucune latence.
- Niveaux mesurés, avec des écarts de 1 dB au plus entre les variantes d'un même modèle.
- Charge de 3 à 7 % d'un cœur par piste.
- Sans note, la piste se tait : aigus directs, souffle et voix directe suivent le niveau de la porteuse.

**À valider au banc A/B (Henrico).**
- Intelligibilité avec un vrai micro.
- Réglage du souffle et des aigus directs.
- Couleur de l'ensemble à cordes.
- Suivi des formants de la boîte parlante sur une voix chantée.

Sources consultées (30/10/2026) :
- Vintage King, Moog 16-Channel Vocoder (16 bandes, Hiss, Buzz, Balance, mode Direct) : https://vintageking.com/products/moog-16-channel-vocoder
- RSPE Audio, Moog 16 Channel Vocoder (50 Hz – 5 080 Hz, Sample/Hold, patch externe) : https://www.rspeaudio.com/moog-16-channel-vocoder
- Polynominal, mode DIRECT au-dessus de 5 080 Hz : https://polynominal.com/moog-vocoder-bode
- Sound On Sound, Waldorf STVC (gel, matrice, contours) : https://www.soundonsound.com/reviews/waldorf-stvc?amp
- MusicRadar, Waldorf STVC (256 bandes, synthé comme porteuse en mode couche) : https://www.musicradar.com/reviews/waldorf-stvc
- Andertons, Waldorf STVC (formants, gel, jitter de la porteuse cordes) : https://www.andertons.co.uk/waldorf-stvc-string-synthesizer-and-vocoder-stvc
- Zynaptiq, nouveautés d'Orange Vocoder IV (Traces, gel, formants) : https://www.zynaptiq.com/orangevocoder/what-s-new-in-orange-vocoder-version-4
- B&H, Orange Vocoder IV (24 algorithmes, consonnes, aigus directs, latence nulle) : https://www.bhphotovideo.com/c/product/1795251-REG/zynaptiq_zyn_ov4_orange_vocoder_iv.html
- Sound On Sound, iZotope VocalSynth 2 (modules en parallèle, Biovox) : https://www.soundonsound.com/node/4919259
- iZotope, VocalSynth 2 (Biovox : nasalité, voyelles, formants) : https://www.izotope.com/en/products/vocalsynth/features

## 7. Synthés FM (v0.45) et Drones (v0.46)

**Familles.** Synthés FM : Old Metal (FM 6 opérateurs, 32 algorithmes), New Metal (FM 4 opérateurs,
8 algorithmes à sorties X / Y), Komodo Fuel (FM 6 opérateurs à matrice libre). World Tour (poly
analogique 8 voix) est rangé dans Polys. Drones : Hot Sauce, Octo Lyma, Komodo Dragon. Chaque synthé
de ces familles a au moins 10 sons d'usine par catégorie (basses, leads, nappes, plucks, claviers,
cordes, cuivres, arpèges, effets), niveaux mesurés et égalisés (−16 LUFS, −18 pour nappes, cordes, effets).

### 7.1 Hot Sauce ↔ hybride à moteurs multiples (principe Arturia Pigments)

- Repris : deux moteurs en parallèle au type au choix (analogique à deux oscillateurs, table d'ondes,
  harmonique additif, modal à résonateurs), modulation d'un moteur par l'autre, moteur utilitaire
  (sous-oscillateur, bruit), deux filtres à routage continu série → parallèle, 3 LFO, 2 enveloppes et
  16 routages, arpège.
- Écarts : pas de moteur granulaire ni d'échantillon (la Pieuvre 8 et les samplers couvrent ce besoin) ;
  tables d'ondes calculées (8 images : sinus, triangle, scie, carré, deux voyelles, métal, impulsion),
  16 partiels pour le moteur harmonique ; filtres : d'état à fondu passe-bas → bande → passe-haut et
  échelle 24 dB à saturation (pas les 19 types d'origine) ; pas d'effets ni de séquenceur internes (les
  inserts, le séquenceur et les verrous du Groovebox les remplacent).
- Modèle : tables par image et par octave (spectre limité), partiels par récurrence, résonateurs à deux
  pôles excités au départ de la note puis frottés tant qu'elle est tenue. Aucune latence.

### 7.2 Octo Lyma ↔ synthé « organismique » à 8 voix (principe SOMA Lyra-8)

- Repris : 8 voix accordées une à une, en 4 paires et 2 groupes ; triangle durci jusqu'au carré par
  paire ; modulation par paire avec sa source (paire voisine en FM dont l'effet suit l'enveloppe des
  voix qui modulent, LFO, retour de la sortie) ; tenue par groupe (bourdon) ; LFO « hyper » à deux carrés
  combinés en ET / OU, le premier pouvant moduler le second ; retard à deux lignes à réinjection
  croisée, modulé par le LFO ou par lui-même ; distorsion en fin de chaîne.
- Écarts : pas de plaques tactiles : en jeu « Plaques », une note joue la voix « note modulo 8 » à sa
  hauteur réglée ; en jeu « Clavier », les 8 voix jouent les notes reçues. La tenue fait sonner tout le
  groupe tant qu'une de ses notes est tenue (elle s'éteint ensuite avec le déclin), pour que le son
  s'arrête avec le séquenceur. Attaque et déclin par groupe (pas par voix). Pas d'arpégiateur ni de
  matrice : la machine n'en a pas.
- Ajout (v0.46, absent de la machine d'origine, qui n'a pas de filtre : le timbre s'y règle par
  la forme d'onde triangle → carré et la distorsion) : filtre d'état 12 dB passe-bas / passe-bande / passe-haut sur les voix,
  avant le retard, avec résonance et coupure modulée par le LFO hyper. Grand ouvert par défaut
  (contourné) : les sons d'usine ne changent pas.
- Ajout (v0.46) : LFO A et B et temps du retard calés au choix sur le tempo (divisions) ; la machine
  d'origine n'a que des vitesses libres. Forme d'onde : une seule, le triangle qu'on durcit jusqu'au
  carré (« Forme », par paire), comme sur la machine.
- Modèle : phase accumulée, triangle mis en forme (tanh), FM par la phase ; retards à interpolation
  linéaire, saturation douce dans la réinjection. Aucune latence.

### 7.3 Komodo Dragon ↔ synthèse par masques de bits (principe Kodamo Mask1)

- Repris : un sinus découpé en segments ; un masque de 8 bits répété le long de la période garde ou
  transforme chaque segment ; 256 masques par oscillateur, deux oscillateurs (le second transposé et
  désaccordé) et un bruit échantillonné-bloqué par voix ; passage d'un masque à l'autre (« delta »)
  réglé à la main et par une enveloppe en boucle ; filtre d'état 12 dB à 4 modes, résonant, suivi du
  clavier ; enveloppe de hauteur ; 10 voix ; 2 LFO.
- Écarts : la façon exacte dont la machine range ses 256 masques n'est pas publiée : ici, un bit à 0
  coupe, inverse ou redresse le segment (au choix), et le nombre de segments va de 8 à 256. Pas de modes
  de jeu « liés » ni de multitimbralité (une piste = une partie).
- Modèle : gain de segment adouci sur environ deux échantillons à chaque bord (moins de repliement
  qu'un découpage net) ; écran des masques dans la façade (toucher un bit l'inverse, forme d'onde des
  masques A et B). Aucune latence.

**À valider au banc A/B (Henrico).** Caractère des résonateurs du moteur modal ; FM croisée et tenue
d'Octo Lyma à l'oreille ; choix des masques et du mode « bit à zéro » de Komodo Dragon.

Sources consultées (02/11/2026) :
- Sound On Sound, essai du Lyra-8 (voix en paires, FM, LFO hyper, retard modulé) : https://www.soundonsound.com/reviews/soma-laboratory-lyra-8
- Moog Audio, description du Lyra-8 (paires, groupes, tenue, LFO ET / OU, retard à deux lignes) : https://www.moogaudio.com/en/soma-laboratory-lyra-8-orange
- Perfect Circuit, essai du Mask1 (masques de bits, 256 masques, deux oscillateurs) : https://www.perfectcircuit.com/signal/kodamo-mask1-review
- Synth Anatomy, Mask1 (2 oscillateurs, bruit, filtre d'état 12 dB à 4 modes) : https://synthanatomy.com/2023/03/kodamo-mask-1-new-polyphonic-digital-bitmask-synthesizer-in-development.html
- Plugin Boutique, Pigments 6 (deux moteurs, moteur utilitaire, filtres) : https://pluginboutique.com/products/11867
- Thomann, Pigments (moteurs, filtres, modulations) : https://www.thomann.de/gr/arturia_pigments_download.htm
