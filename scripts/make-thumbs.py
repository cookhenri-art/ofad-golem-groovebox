#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# Miniatures des façades (v0.32) — étape 2/2 : recadrage de la façade et conversion en WebP 480 × 278
# dans ui/public/thumbs/<type>.webp (≈ 10 ko chacune). Usage : python3 scripts/make-thumbs.py <dossier des captures>
import glob, os, sys
from PIL import Image
src = sys.argv[1] if len(sys.argv) > 1 else "thumbs-shots"
dst = os.path.join(os.path.dirname(__file__), "..", "ui", "public", "thumbs")
os.makedirs(dst, exist_ok=True)
for f in sorted(glob.glob(os.path.join(src, "*.png"))):
    t = os.path.splitext(os.path.basename(f))[0]
    im = Image.open(f).convert("RGB").crop((104, 72, 1816, 1064)).resize((480, 278), Image.LANCZOS)
    im.save(os.path.join(dst, f"{t}.webp"), "WEBP", quality=74, method=6)
    print(t)
