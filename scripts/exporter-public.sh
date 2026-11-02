#!/bin/bash
# Groovebox — OFAD — AGPL-3.0-or-later
# Copie publique du projet (dépôt GitHub ofad-golem-groovebox et archive des sources de l'installateur) :
# l'arbre du dernier commit, SANS les documents de travail internes qui citent des marques.
#   scripts/exporter-public.sh <dossier-de-destination>
set -euo pipefail
DEST="${1:?dossier de destination}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# documents internes : jamais publiés
EXCLURE=(
  docs/CORRESPONDANCES-NOMS.md
  docs/ECARTS-ARCHITECTURES-INSTRUMENTS.md
  docs/COUVERTURE-CAHIER-DES-CHARGES.md
)
mkdir -p "$DEST"
git -C "$ROOT" archive --format=tar HEAD | tar -x -C "$DEST"
for f in "${EXCLURE[@]}"; do
  if [ -e "$DEST/$f" ]; then mv "$DEST/$f" "$DEST/.$$.retire"; rm -f -- "$DEST/.$$.retire"; fi
done
echo "Copie publique écrite dans $DEST"
