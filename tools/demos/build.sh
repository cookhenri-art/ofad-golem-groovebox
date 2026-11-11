#!/bin/sh
# 1.1 : fabrique les démos (projets .gbx dans ui/public/demos, fichiers MIDI dans docs/demos-midi) et les mesure.
# Usage : tools/demos/build.sh [style…]   (depuis la racine du dépôt, après « ninja -C build groovebox_demos »)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TOOL="$ROOT/build/groovebox_demos_artefacts/Release/groovebox_demos"
python3 "$ROOT/tools/demos/make_demos.py" "$@"
mkdir -p "$ROOT/ui/public/demos" "$ROOT/docs/demos-midi"
for spec in "$ROOT"/tools/demos/specs/*.json; do
  name=$(basename "$spec" .json)
  if [ $# -gt 0 ] && ! echo " $* " | grep -q " $name "; then continue; fi
  echo "=== $name"
  (cd "$ROOT" && "$TOOL" "tools/demos/specs/$name.json" "ui/public/demos/demo-$name.gbx" --mid "docs/demos-midi/$name.mid" ${WAV:+--wav "$WAV/$name.wav"} ${TRACKS:+--tracks} ${BENCH:+--bench})
done
