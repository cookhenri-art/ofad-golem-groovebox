# Émulateur FM-1 (lecteur de firmwares)

Les crates `fm1-isa`, `fm1-core` et `fm1-soc` viennent de **fm1-emu** de Joakim Eriksson
(https://github.com/joakimeriksson/fm1-emu, commit 95343aa, 5 novembre 2026), sous **licence MIT**
(fichier `LICENSE`, à garder avec toute copie). Ils émulent la puce du boîtier M-VAVE FM-1 (deux cœurs
pi32v2, écran, touches, boutons, sortie audio) et font tourner ses firmwares (`.fwsc`) sans modification.

`fm1-bridge` est le pont de GOLEM : une bibliothèque statique avec une interface C (`fm1_bridge.h`), adaptée
du fil d'émulation du greffon de fm1-emu (`crates/fm1-plugin/src/engine.rs`, MIT). Le FM-1 tourne dans un fil à
lui, un peu en avance sur l'audio de GOLEM ; le moteur prend ses images sans jamais attendre.

OFAD n'est pas affilié à M-VAVE. M-VAVE et FM-1 sont des marques de leurs propriétaires.
