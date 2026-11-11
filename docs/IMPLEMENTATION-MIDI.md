# OFAD GOLEM 1.1 — Implémentation MIDI

Ce document décrit ce que GOLEM reçoit en MIDI, comment il choisit la piste qui joue, comment on
appelle un son par son numéro, ce qu'envoie la télécommande (GOLEM sur un téléphone) et ce qu'envoie une
piste MIDI vers un appareil externe (§ 8). Il complète
la notice (chapitre « Contrôleur MIDI »).

GOLEM suit les règles du MIDI comme un logiciel de musique : chaque piste peut écouter son propre canal
(1 à 16), le changement de programme choisit le son de l'instrument, et la banque (CC 0 / CC 32) passe
aux 128 sons suivants.

## 1. Ports

| Liaison | PC Windows | Mac | Android, iPhone, iPad | Remarques |
|---|---|---|---|---|
| USB | Oui | Oui | Oui (câble ou hub USB-C « OTG ») | Reconnu tout seul, branchement à chaud |
| Bluetooth MIDI | Si Windows le présente comme un port MIDI (pilote passerelle) | Oui (fenêtre d'appairage du système) | Oui (Réglages › Appairer un contrôleur Bluetooth…, ou Bluetooth… de la piste MIDI) | |
| Réseau (rtpMIDI…) | Par un pilote réseau, vu comme un appareil | Oui (réseau MIDI du système) | — | Apparaît dans la liste des appareils |
| Logiciel hôte (VST3, AU) | Oui | Oui | — | L'hôte envoie le MIDI à GOLEM et reçoit la sortie MIDI du greffon |
| Wi-Fi (télécommande) | Oui (Réglages › Accepter une télécommande Wi-Fi) | Oui | Oui | Entre deux GOLEM sur le même réseau local, voir § 7 |

La liste « Appareil » de la piste MIDI affiche le type de liaison (USB, Bluetooth, réseau, virtuel) quand
le système le donne.

## 2. Réglages (Réglages › MIDI)

| Réglage | Valeurs | Par défaut | Rôle |
|---|---|---|---|
| Canal des notes | Tous, 1 à 16 | Tous | Canal sur lequel la **piste choisie** reçoit les notes |
| Canal des commandes | Tous, 1 à 16 | Tous | Canal des commandes apprises et du lancement de scènes |
| Commandes MIDI | Actives / coupées | Actives | Commandes apprises, pads en mode Scènes / Couper / Solo / Pas, scènes par changement de programme |
| Canal propre d'une piste | Aucun, 1 à 16 | Aucun | La piste joue ce canal, qu'elle soit choisie ou non |
| Canaux des T (FM Player T1 à T4, Pieuvre 8 T1 à T8) | 1 à 16 | Suivent le canal de T1 | Un séquenceur externe joue plusieurs T à la fois |

## 3. Quelle piste reçoit un message

1. Les pistes (et les T) **réglées sur le canal** du message le reçoivent.
2. Sinon, la **piste choisie** à l'écran le reçoit, si le canal des notes l'accepte et si elle n'a pas de
   canal propre.
3. Le **changement de programme**, la banque et les CC 102 à 105 ne changent le son que d'une piste
   **réglée sur ce canal** (règle 1). Sans piste sur le canal, le changement de programme lance une scène
   (§ 5).

## 4. Messages reçus

| Message | Reçu | Effet |
|---|---|---|
| Note On / Note Off | Oui | Joue la piste ; vélocité prise en compte. Batteries : notes 36 à 51 = pads 1 à 16. Pieuvre 8 et FM Player : la T choisie sur toutes les octaves |
| Notes 36 à 51 en mode pads Scènes / Couper / Solo / Pas | Oui | Comme les pads de l'écran : lancer une scène, couper ou mettre en solo une piste, poser un pas |
| Pitch bend | Oui | Piste qui joue |
| Pression (aftertouch par canal) | Oui | Piste qui joue (selon l'instrument) |
| Aftertouch polyphonique | Non | |
| CC 1 (molette de modulation) | Oui | Piste qui joue, sauf si la commande est apprise |
| CC 64 (maintien) | Oui | Piste qui joue, sauf si la commande est apprise |
| CC 0 / CC 32 (banque) | Oui | Mémorisés par canal pour le changement de programme suivant (§ 5) |
| CC 102 à 105 | Oui | Son de l'effet des inserts 1 à 4 de la piste réglée sur ce canal (§ 6) |
| Autres CC | Oui | Commande apprise (réglage, bouton, transport, scène…) ; non apprise : envoyée à l'instrument (le FM Player la passe au firmware) |
| Changement de programme | Oui | Son de l'instrument (§ 5) ou scène |
| Horloge MIDI, Start / Stop / Continue | Non | Dans un logiciel hôte, GOLEM suit le tempo et le transport de l'hôte |
| Position de morceau (SPP) | Non | |
| SysEx | Oui | Télécommande GOLEM uniquement (§ 7) ; les autres SysEx sont ignorés. Les firmwares du FM Player reçoivent leurs propres fichiers .syx depuis l'écran du FM Player |

### Apprentissage

Appui long (clic droit) sur un réglage › **Apprendre MIDI**, puis toucher une commande du contrôleur.
Notes et CC s'apprennent sur les réglages des instruments et des effets, les valeurs du bas des écrans,
tempo, swing, crossfader, scènes A et B, pavé vectoriel, macros, les 16 boutons assignables, M / S,
rangées, patterns, Photo, Rappel et transport. Une commande apprise n'est plus envoyée à l'instrument.

## 5. Changer le son de l'instrument (banque + programme)

**Numérotation.** Pour chaque instrument :

- n° **1** = « Init » ;
- puis les sons d'usine banque par banque, dans l'ordre du panneau des préréglages : Acide, Basses,
  Leads, Nappes, Plucks, Claviers, Cordes, Cuivres, Arpèges, Effets, Classiques ;
- puis **vos sons**, à la suite (par banque perso, puis par nom).

Le numéro est affiché devant chaque son, dans les listes, dans le panneau des préréglages et à côté du
nom de l'instrument.

**Message à envoyer** sur le canal propre de la piste :

| Son | Banque (CC 0) | Banque (CC 32) | Changement de programme |
|---|---|---|---|
| n° 1 à 128 | 0 | 0 | n° − 1 (0 à 127) |
| n° 129 à 256 | 1 | 0 | n° − 129 |
| n° 257 à 384 | 2 | 0 | n° − 257 |

- La banque peut aussi être envoyée par le **CC 32 seul** (CC 0 à 0, CC 32 = 1, 2…) : GOLEM l'accepte.
  Si les deux sont différents de 0, la banque vaut CC 0 × 128 + CC 32.
- La banque reste mémorisée par canal jusqu'au prochain CC 0 / CC 32.
- Exemple : n° 140 = CC 0 à 1, CC 32 à 0, programme 11.
- Un numéro sans son ne change rien.

**Scènes.** Si aucune piste n'est réglée sur le canal, un changement de programme 0 à 31 reçu sur le canal
des commandes (commandes MIDI actives) lance la scène 1 à 32.

## 6. Changer le son d'un effet (inserts 1 à 4)

| Commande | Insert | Valeur |
|---|---|---|
| CC 102 | Insert 1 | n° du son − 1 (0 = n° 1) |
| CC 103 | Insert 2 | idem |
| CC 104 | Insert 3 | idem |
| CC 105 | Insert 4 | idem |

- Même numérotation que pour les instruments (Init = 1, sons d'usine par banque, puis vos sons), limitée
  à 128 sons par effet.
- Seulement pour une piste réglée sur ce canal, et si la commande n'est pas apprise. Sans piste sur le
  canal, ces CC sont des commandes libres comme les autres.
- Sans effet dans l'insert, rien ne change.

## 7. La télécommande (GOLEM sur un téléphone)

Le mode télécommande de l'application Android pilote :

- **GOLEM sur un PC** : en Wi-Fi (même réseau local) ou par câble USB (le téléphone passe en mode MIDI) ;
- **n'importe quel appareil MIDI** : par câble USB ou en Bluetooth MIDI.

### Avec GOLEM (Wi-Fi ou USB)

- Les écrans (Mixage, Boutons, Chaos, FM-1) pilotent directement GOLEM : volumes, coupure, solo, retours,
  groupes, 16 boutons assignables, crossfader, scènes, transport.
- Le PC renvoie au téléphone les noms et couleurs des pistes, les niveaux, les valeurs et libellés des
  boutons, le son de chaque piste (numéro et nom) : le téléphone suit ce qu'on touche sur le PC.
- Onglet FM-1 : clavier, pads, molettes et notes (Note On / Off, pitch bend, CC 1) sur le canal choisi ;
  « **Changer : instrument / effet de l'insert 1 à 4** » choisit ce que change le son. Pour l'instrument,
  le téléphone envoie banque + programme (§ 5) ; pour un insert, le CC 102 à 105 (§ 6). Au-delà de
  128 sons, un choix de série (1–128, 129–256…) apparaît.
- Échanges en SysEx propre à GOLEM (non documenté, peut changer d'une version à l'autre). En Wi-Fi :
  port UDP 47474, adresses du réseau local seulement, une télécommande à la fois.

### Avec un autre appareil (USB ou Bluetooth MIDI)

| Commande du téléphone | Envoie | Réglable |
|---|---|---|
| 16 boutons | CC | Nom, canal 1 à 16, n° de CC |
| 8 pads (onglet FM-1) | CC maintenu (gate), CC à bascule, ou note | Nom, mode, canal, n° de CC ou de note |
| Boutons de l'onglet FM-1 | CC | Nom, canal, n° de CC, ou lien vers un des 16 boutons |
| Chaos (4 coins) | 4 boutons ou 4 CC | Selon le coin |
| Clavier, molettes | Note On / Off, pitch bend, CC 1 | Canal des notes, octave |
| Son | Banque (CC 0, CC 32 à 0) puis changement de programme | Banque 0 à 127, son 1 à 128 |
| Fichier .syx | SysEx | Envoi d'un fichier choisi |

Le téléphone ne reçoit d'un appareil autre que GOLEM que pour **Apprendre** : on touche « Apprendre » à côté
d'un réglage, on tourne le bouton de l'appareil (ou on joue la note), le numéro de CC (ou de note) et le canal
se posent ; rien n'est rejoué.

## 8. Ce qu'envoie GOLEM : la piste MIDI (1.1)

Depuis la 1.1, l'instrument **Piste MIDI** (famille MIDI externe) joue un appareil externe. Chaque piste MIDI
envoie sur **son canal d'émission** (1 à 16), vers l'appareil choisi dans sa liste « Appareil » (application
PC, Mac, téléphone) ou vers la **sortie MIDI du greffon** (logiciel hôte).

| Message | Quand |
|---|---|
| Note On / Off | Notes du séquenceur, du piano, des pads, du clavier MIDI ; après les effets MIDI (arpège, accord, gamme, écho de notes, probabilité, vélocité, transposition) |
| CC 0 à 119 | Les 32 boutons (n° et canal propres à chaque bouton), à chaque changement de valeur : main, verrou de pas, scène, modulation, courbe |
| CC des pads | CC maintenu (127 / 0), CC marche / arrêt, CC par pas (2 à 8 valeurs) ; ou note |
| Banque (CC 0, puis CC 32 = 0) + changement de programme | Listes Banque et Son du bandeau, et **son du pas** : envoyés juste avant la note du pas |
| Pitch bend, CC 1 (molette), pression | Suivent le clavier MIDI qui joue la piste |
| Note Off | Toutes les notes en cours à l'arrêt, à la coupure (M) et au solo d'une autre piste |

- Au même instant, banque et programme partent **avant** les notes.
- Les numéros de CC et les canaux sont **figés** : ni scène, ni verrou, ni modulation ne les change. Les CC 120
  à 127 (modes du canal : tout couper, remise à zéro…) ne sont jamais envoyés.
- Rien n'est envoyé au chargement d'un projet (le son de l'appareil n'est pas écrasé), sauf si « Envoyer au
  départ » est coché : les valeurs partent au lancement de la lecture.
- Ce que l'appareil renvoie ne rejoue pas la piste (pas de boucle MIDI).
- GOLEM n'envoie **ni horloge MIDI, ni start / stop, ni SysEx** (hors télécommande).
