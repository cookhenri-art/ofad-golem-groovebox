# LAME 3.99.5 (encodeur MP3) — bibliothèque tierce

- Origine : sources de LAME 3.99.5 (projet LAME, https://lame.sourceforge.io), copiées depuis le paquet npm « lame » 1.2.4 (dossier `deps/lame`).
- Licence : GNU LGPL 2 (voir `COPYING`). Compatible avec la licence AGPL-3.0 du Groovebox ; les sources de LAME
  restent livrées avec celles du Groovebox, ce qui permet de la remplacer ou de la recompiler.
- Compilation : chaque source passe par `unites/lame_<nom>.c`, qui inclut d'abord `config/lame_config.h` (pas de HAVE_CONFIG_H global).
- Fichiers repris : `libmp3lame/*.c` (sauf `mpglib_interface.c`, décodeur non utilisé), les en-têtes, `include/lame.h`.
  Pas d'assembleur ni de SSE (`vector/` non repris) : le même code compile sous Windows, Linux et Android.
- `config/lame_config.h` est écrit pour le Groovebox (configuration portable).
- Usage : `src/Recorder.cpp` convertit les enregistrements WAV en MP3 (320 kbit/s, qualité 2), hors du fil audio.
