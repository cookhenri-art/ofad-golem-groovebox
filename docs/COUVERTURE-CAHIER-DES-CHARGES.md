# Groovebox — couverture du cahier des charges (v0.12, 28/10/2026 ; complété en v0.46, 02/11/2026)

Référence : `P3/SPEC-PERFORMANCE-SEQUENCEUR.md` et `P3/SPEC-PISTES-EFFETS-MIXAGE.md` (projet Claude),
plus les demandes faites en conversation. Légende : **fait**, **partiel** (ce qui manque est dit),
**écarté** (avec la raison).

## 1. Formats et plateformes

| Demande | État |
|---|---|
| Application Windows et plugin VST3, même code | **fait** (C++ / JUCE 8, interface React unique) |
| Application Android pour tablette, en natif | **fait** : APK construit (arm64-v8a, armeabi-v7a), audio Oboe faible latence, appui long = clic droit, enregistrement automatique du projet. Pas encore essayé sur une vraie tablette |
| Latence la plus faible possible | **fait** : aucun effet n'ajoute de latence ; la latence est celle du tampon audio choisi |
| Sécurité | **fait** : arguments de l'interface vérifiés, tailles plafonnées, aucun accès réseau (Android : permission Internet retirée), fichiers lus ou écrits seulement par le sélecteur du système |

## 2. Instruments (« copies proches », architecture propre, aucun nom ni son d'origine)

| Demande | État |
|---|---|
| Batterie au punch Analog Rytm, type 808, type 909 | **fait** (3 kits) ; v0.7 : voix sur principes de circuits analogiques (T ponté, oscillateur balayé, métaux à 6 carrés, dérive des composants) ; réglages d'usine à valider au banc A/B |
| Batteries : réglages complets par pad (filtres…) comme sur la machine | **fait** (v0.7) : par pad, 2 réglages de circuit, filtre analogique multimode avec enveloppe, VCA attaque / maintien / déclin, vélocité, groupe d'étouffement, effet et envois |
| Basse acide type 303, basse mono type Sub 37 | **fait** ; v0.10 : architecture complète de la basse mono (battement, réinjection, DAHDSR bouclables, 2 bus de modulation, duo paraphonique, types de glisse, arpège) |
| Batterie type Rytm : machines par pad, chacune avec ses réglages | **fait** (v0.10) : 29 machines + échantillon seul, 7 réglages propres par machine, échantillon superposé, LFO par pad |
| Fonctions de pas façon 303 / 808 / 909 : glissé et accent par pas, accent total, flam, longueur par instrument | **fait** (v0.10) |
| Molette, pitch bend, pression, pédale ; arpégiateur | **fait** (v0.10) : MIDI et molettes à l'écran ; arpège de piste |
| Façades proches des vraies machines (disposition, commandes « marque de fabrique ») | **fait** (v0.10) : basse mono, basse acide, 3 batteries, poly analogique, poly à chorus, poly hybride, wave morph, nappes ; sampler de performance (v0.11) |
| Sampler façon Octatrack : machines Flex / Static / Thru / Neighbor / Pickup, découpes, étirement, enregistreurs, verrous d'échantillon, LFO dessinables, 2 effets par piste, pas sans déclenchement, glissés de réglages, pas d'enregistrement | **fait** (v0.11) ; restent : parties (4 jeux de machines / effets par banque), arrangeur, pistes MIDI, édition manuelle des découpes, trigs « une fois » et swing par pas |
| Plein écran et fenêtre redimensionnable librement | **fait** (v0.11) : bouton et F11 (application autonome), Android déjà en plein écran |
| Annuler / rétablir (Ctrl+Z / Ctrl+Y) et photo des réglages rappelée d'un bouton | **fait** (v0.11) : 50 étapes, rappel sans coupure du son, photo enregistrée avec le projet ; tempo, swing, crossfader et MIDI hors historique |
| Arpégiateurs : une quinzaine de modes au moins | **fait** (v0.11) : 20 modes sur tous les arpèges (montée, descente, aller-retour, convergent, divergent, pédales, marches, mélangé…) |
| Banques de 50 préréglages par synthé, par catégories classiques | **fait** (v0.12) : 350 sons (7 synthés), catégories Acide / Basses / Leads / Nappes / Plucks / Claviers / Cordes / Cuivres / Arpèges / Effets, niveau égalisé et vérifié automatiquement ; v0.45.2 et v0.46 : au moins 10 sons par catégorie pour chaque nouveau synthé (Old Metal, New Metal, Komodo Fuel, World Tour, Hot Sauce, Octo Lyma, Komodo Dragon : 630 sons) ; liste complète dans `CATALOGUE.md` |
| Division binaire / ternaire propre à chaque pattern de chaque piste | **fait** (v0.12) ; réglage aussi depuis la pastille du pattern (mesures, division, vitesse) |
| Façades plus grandes en plein écran | **fait** (v0.12) : agrandissement jusqu'à 1,8 dans la limite de la largeur ; les synthés à beaucoup de modules restent limités par la largeur de l'écran |
| Effets : flèches, coupure et corbeille sur chaque ligne d'insert ; préréglages d'effets | **fait** (v0.11) : outils sur la ligne ; correction des préréglages (affichage figé, niveau qui baissait) |
| Architecture de chaque synthé (commandes qui font sa marque) | **fait** (v0.10) : Wheel-Mod et révision du filtre (poly analogique), LFO manuel et maintien (poly à chorus), tables d'ondes, Shape synchro / densité, FM croisée, matrice 16 (poly hybride), osc. 3, 2 mutants, voyelle / peigne, 5 env., 5 LFO à pas, matrice 32, macros (wave morph), 4 oscillateurs, phase, commande globale des filtres (nappes) |
| Sampler complet façon Octatrack | **à faire** (demande du 28/10) |
| Polys type Prophet, type Juno, type Peak ; morphing type Hydrasynth | **fait** |
| Nappes type Sylenth | **fait** |
| Sampler (Stem Sampler Pro) : 16 tranches ou chromatique | **fait** |
| Entrée audio (voix, guitare) | **fait** |
| Validation de chaque son au banc A/B | **à faire** par Henrico (enregistrements de référence) |

## 3. Séquenceur et performance

| Demande | État |
|---|---|
| Registre unique de tous les réglages | **fait** |
| 32 scènes en 2 banques, crossfader −100 … +100, scène muette | **fait** |
| Lancement des scènes : pad, bouton, clavier, Program Change, note MIDI (pads en mode Scènes) | **fait** |
| Scènes de lancement façon Force (rangées de clips) | **fait** : 8 patterns par piste, rangées A à H, scène de réglages liée à chaque rangée |
| Blocs de 4 mesures jusqu'à 32, pages de 16 pas | **fait** |
| Longueur propre à chaque piste (polymétrie) | **fait** (par pattern) ; v0.10 : aussi par ligne de batterie |
| Divisions 1/4, 1/8, 1/16, 1/32, 1/64 et ternaires 1/8T, 1/16T, 1/32T | **fait** |
| Vitesse par piste ×1/8 … ×2, dont ×3/4 | **fait** (par pattern) |
| Swing, micro-décalage, durée, vélocité | **fait** |
| Horloge 960 impulsions par noire | **fait autrement** : placement à l'échantillon près (plus fin que 960 PPQ) |
| Sens de lecture avant, arrière, aller-retour, aléatoire | **fait** (par ligne) |
| Enregistrement pas à pas et en direct, quantification réglable | **fait** (0 à 100 %) |
| Verrous par pas sur n'importe quel réglage, conditionnels | **fait** |
| Conditions %, A:B, FILL / NON FILL, PRÉ / NON PRÉ, VOISIN / NON VOISIN, 1ER / NON 1ER | **fait** |
| Break maintenu ou verrouillé, inversion FILL, scène Break éditable | **fait** (Maj = verrouiller ; « Éditer » sur la scène Break) |
| Break sur pad, touche, note, CC | **fait** (touche B ; note ou CC par apprentissage MIDI) |
| Contrôleur MIDI : pads, encodeurs (apprentissage), crossfader en CC | **fait** |
| Outils de séquenceur classiques : sélection multiple (Ctrl+clic, rectangle), déplacer, copier (Ctrl+glisser), coller, dupliquer, régler plusieurs notes à la fois | **fait** (v0.7), au clavier, à la souris et au doigt |
| Pads du contrôleur pour poser des pas du séquenceur | **fait** : rôle des pads « Pas » (écran et contrôleur MIDI, notes 36 à 51) : les 16 pads posent ou retirent les pas de la page affichée, sur la ligne choisie ; le pas joué s'éclaire |

## 4. Pistes, effets, mixage

| Demande | État |
|---|---|
| 16 pistes, navigateur par famille, préréglages | **fait** |
| Pads des instruments mélodiques : 16 notes d'une gamme | **fait** (10 gammes, tonique au choix) |
| Pads : 16 niveaux (principe MPC) | **fait** : 16 vélocités du son choisi (écran Réglages) |
| Écran SON en onglets Global / Oscillateur / Filtre / Enveloppe / Amplitude / Modulation | **fait** |
| Préréglages perso en banques (instruments, effets, pads) | **fait** (v0.7) : enregistrer, rappeler, corbeille ; fichiers dans Documents\Groovebox\Préréglages |
| Régler toute valeur en glissant haut / bas (souris, doigt), tempo compris | **fait** (v0.7) |
| 4 inserts par piste, retours A–D, bus principal | **fait** (4 inserts aussi sur les retours et le bus) |
| Envois avant ou après le fader | **fait** |
| Un insert et des envois par pad de batterie | **fait** (effet léger par pad : saturation, passe-bas, passe-haut, compression, réduction de bits ; envois A et B) |
| Écran EFFETS (esthétique figée) | **fait**, inchangé ; ajouts discrets : analyseur de l'égaliseur, bouton « Chaînes prêtes » |
| Distorsion : 2 moteurs, 11 algorithmes, 5 routages, équilibre, courbes | **fait** ; v0.8 : 14 algorithmes, asymétrie par moteur, cartes A / B cliquables (liste des algorithmes, activer / couper), courbes des filtres, de la sortie et de la réinjection |
| Porte de bruit et dé-esseur (repris de Sillage) | **fait** (v0.8) : porte à hystérésis et maintien, filtre de détection ; dé-esseur bande haute ou large, écoute ; sans latence |
| Répétitions par pas (ratchets) et Break temporisé (repris de Sillage) | **fait** (v0.8) : 2 à 8 coups, pente de vélocité ; Break de 1 à 16 temps relâché sur un temps |
| Saisie d'une valeur exacte | **fait** (v0.8) : clic droit / appui long sur un encodeur ; liste complète pour les choix |
| Affectation de modulation compréhensible ; modulations par instrument et modulations générales (effets, volume, panoramique) | **fait** (v0.8) : mode « Affecter » qui allume les encodeurs, liste des destinations avec profondeur et retrait ; sources 1 à 4 pour le son, 5 et 6 pour les effets et le mixage ; type choisi par boutons (corrige le blocage sur « Fonction ») |
| Vue « vrai synthé » plein écran avec petit clavier (comme Sillage) | **fait** (v0.9) : façade par instrument (modules, graphiques, boutons rotatifs), page Modulation, clavier jouable ; batteries : pad détaillé, vue du kit, 16 pads |
| Synchro au tempo des LFO intégrés aux synthés | **fait** (v0.9) : Libre ou division (4 mes. à 1/32, ternaires, pointées) |
| Machines par pad (FM, métal, bruit) et export du mix en WAV (repris de Sillage) | **à faire** (après la vue synthé) |
| Distorsion : filtre d'entrée par moteur, filtre de sortie 2 bandes | **fait** |
| Distorsion : dynamique avant ou après, réinjection, 6 sources de modulation | **fait** |
| Modulation : clic droit, 7 sources, petite molette, arc coloré, « LFO1 +35 % » | **fait** ; réglage d'origine des sources comme la maquette : LFO 1, LFO 2, Fonction 3, Fonction 4, Suiveur 5, Séquenceur 6, Vélocité |
| Égaliseur 8 bandes, pentes 12 à 48 dB/oct., bandes dynamiques, mid/side, analyseur | **fait** |
| Compresseurs optique, FET, bus, moderne ; limiteur | **fait** |
| Compresseur moderne et limiteur avec anticipation | **écarté** : l'anticipation ajoute de la latence ; le limiteur a un mode crête vraie sans latence (testé : −1,33 dBTP pour un plafond de −1 dB) |
| Mesures : crête, crête vraie, RMS, LUFS instantané / court / intégré, réduction de gain | **fait** |
| Affichage des compresseurs : réduction de gain, courbe entrée → sortie | **fait** |
| Chaînes fournies : Voix, Voix rap, Basse, Grosse caisse, Bus batterie, Nappes, Guitare, Bus principal | **fait** |

## 5. Projet et fichiers

| Demande | État |
|---|---|
| Tout est enregistré avec le projet | **fait** (pistes, réglages, 8 patterns, conditions, verrous, scènes, lancement, échantillons, macros, MIDI) |
| Ouvrir / enregistrer un projet en fichier | **fait** (.gbx, sélecteur du système) |
| Échantillons : glisser-déposer, ou sélecteur de fichiers (Android compris) | **fait** |

## 6. Reste à faire (prochaines versions)

1. Essais sur la tablette et dans 2 DAW, réglage de la latence (tampon) au cas par cas.
2. Bancs A/B des instruments et des effets à l'oreille, puis réglage fin des sons.
3. « 16 niveaux » sur un autre réglage que la vélocité (accord, filtre).
4. Saisie des pas depuis les pads du contrôleur.
5. Signature de l'APK avec une clé de publication (aujourd'hui : clé de débogage).

## Ajouts v0.45 – v0.46 (02/11/2026)

| Demande | État |
|---|---|
| Famille « Synthés FM » (Old Metal, New Metal, Komodo Fuel) | **fait** (v0.45 – v0.45.2) |
| Famille « Drones » (Hot Sauce, Octo Lyma, Komodo Dragon) | **fait** (v0.46) |
| Au moins 10 sons d'usine par catégorie pour chaque nouveau synthé | **fait** (v0.45.2, v0.46) |
| Séquenceur à verrous dans l'onglet Pas des synthés et des boîtes à rythmes (au choix avec l'écriture d'origine) | **fait** (v0.45.2) |
| Analyseur de spectre du bus principal (type SPAN) | **fait** (v0.45.1) |
| Taille grande des façades garantie | **fait** (v0.45.2) |
| Images de toutes les façades d'instruments et d'effets | **fait** (v0.46) : galerie livrée dans `_livraisons/facades-v0.46.0`, script `scripts/make-gallery.cjs` |
