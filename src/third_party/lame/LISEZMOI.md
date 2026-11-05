# LAME 3.100 (encodeur MP3) — bibliothèque tierce

- Origine : sources officielles de LAME 3.100 (projet LAME, https://lame.sourceforge.io), archive `lame_3.100.orig.tar.gz`
  (SHA-256 `ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e`), sans modification.
  v0.46.1 (audit de sécurité du 4 novembre 2026) : remplace LAME 3.99.5 (2012), qui avait plusieurs failles publiées
  (CVE-2015-9101, CVE-2017-15018, CVE-2017-15045…). Avant : copie du paquet npm « lame » 1.2.4 (`deps/lame`).
- Licence : GNU LGPL 2 (voir `COPYING`). Compatible avec la licence AGPL-3.0 du Groovebox ; les sources de LAME
  restent livrées avec celles du Groovebox, ce qui permet de la remplacer ou de la recompiler.
- Compilation : chaque source passe par `unites/lame_<nom>.c`, qui inclut d'abord `config/lame_config.h` (pas de HAVE_CONFIG_H global).
- Fichiers repris : `libmp3lame/*.c` (sauf `mpglib_interface.c`, décodeur non utilisé), les en-têtes, `include/lame.h`.
  Pas d'assembleur ni de SSE (`vector/` non repris) : le même code compile sous Windows, Linux et Android.
- `config/lame_config.h` est écrit pour le Groovebox (configuration portable).
- Usage : `src/Recorder.cpp` convertit les enregistrements WAV en MP3 (320 kbit/s, qualité 2), hors du fil audio.
