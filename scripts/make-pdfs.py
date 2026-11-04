#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
# v0.46 fix9 : met en page les documents pour l'impression et l'installateur, avec la mascotte
# OFAD GOLEM : docs/NOTICE.md -> docs/NOTICE.pdf, docs/CATALOGUE.md -> docs/CATALOGUE.pdf.
# Prérequis : python3 -m pip install markdown playwright ; python3 -m playwright install chromium
#   python3 scripts/make-pdfs.py
import os, pathlib, re, tempfile, markdown
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
GOLEM = (ROOT / "ui" / "public" / "golem" / "golem-512.png").as_uri()
JPG = pathlib.Path(tempfile.gettempdir()) / "groovebox-pdf-images"   # copies JPEG temporaires des vignettes

CSS = """
@page { size: A4; margin: 16mm 15mm 18mm 15mm; }
* { box-sizing: border-box; }
body { font-family: 'Segoe UI', 'Inter', 'Helvetica Neue', Arial, sans-serif; color: #1d2026; font-size: 10.5pt; line-height: 1.45; margin: 0; }
.cover { display: flex; align-items: center; gap: 22px; padding: 14px 20px; margin-bottom: 14px; border-radius: 14px;
  background: radial-gradient(120% 140% at 15% 50%, #3b2a1d 0%, #191b20 70%); color: #efe7dc; }
.cover img { height: GOLEM_H; width: auto; filter: drop-shadow(0 6px 10px rgba(0,0,0,.5)); }
.cover .word { font-weight: 800; font-size: 26pt; letter-spacing: .26em; }
.cover .sub { font-size: 11pt; letter-spacing: .14em; text-transform: uppercase; color: #f5a623; font-weight: 700; margin-top: 4px; }
.cover .note { font-size: 9.5pt; color: #cfc6ba; margin-top: 8px; max-width: 420px; }
h1 { display: none; }
h2 { font-size: 15pt; margin: 18px 0 8px; padding-bottom: 4px; border-bottom: 2px solid #f5a623; break-after: avoid; }
h3 { font-size: 11.5pt; margin: 14px 0 6px; color: #8a4b12; break-after: avoid; }
h4 { font-size: 10.8pt; margin: 12px 0 5px; padding-left: 7px; border-left: 3px solid #f5a623; break-after: avoid; }
p, li { margin: 4px 0; }
table { width: 100%; border-collapse: collapse; margin: 6px 0 10px; break-inside: auto; font-size: 9.6pt; }
tr { break-inside: avoid; }
th { text-align: left; background: #232631; color: #fff; padding: 5px 8px; font-weight: 700; }
td { padding: 4px 8px; border-bottom: 1px solid #e3ddd4; vertical-align: top; }
tr:nth-child(even) td { background: #faf7f2; }
td img { width: 110px; border-radius: 4px; }
strong { color: #111; }
code { font-size: 9pt; background: #f1ece4; padding: 0 3px; border-radius: 3px; }
figure { margin: 8px auto 12px; text-align: center; break-inside: avoid; }
figure img { width: 74%; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,.25); }
figcaption { font-size: 8.8pt; color: #6b6259; margin-top: 4px; font-style: italic; }
figure.wide img { width: 100%; }                 /* captures annotées (repères numérotés) */
figure.pads img { width: 46%; }
figure + ol { margin: 0 0 12px; padding-left: 26px; font-size: 9.6pt; }
figure + ol li::marker { color: #c4630c; font-weight: 800; }
figure.wide + ol, figure.wide { break-inside: avoid; }

/* notice : la section des raccourcis, bien visible */
.keys h3 { background: #232631; color: #f5a623; padding: 5px 10px; border-radius: 6px 6px 0 0; margin-bottom: 0; }
.keys table { margin-top: 0; border: 2px solid #232631; }
.keys th { display: none; }
.keys td:first-child { width: 42%; font-weight: 600; }
"""


def build(md_name, pdf_name, title, sub, note, golem_h, keys_section=None, lang="fr"):
    text = (DOCS / md_name).read_text(encoding="utf-8")
    # note pour les développeurs en tête du catalogue (« Fichier écrit par… ») : pas dans le PDF
    text = re.sub(r"^Fichier écrit par.*?\n\n", "", text, flags=re.S | re.M)
    html = markdown.markdown(text, extensions=["tables"])
    # vignettes WebP : converties en JPEG pour le PDF (le PDF les garde telles quelles, bien plus léger)
    def to_jpg(m):
        src = (DOCS / m.group(1)).resolve()
        if not src.exists() or src.suffix.lower() != ".webp": return m.group(0)
        JPG.mkdir(exist_ok=True)
        dst = JPG / (src.parent.name + "-" + src.stem + ".jpg")
        if not dst.exists():
            from PIL import Image
            Image.open(src).convert("RGB").save(dst, "JPEG", quality=80)
        return f'src="{dst.as_uri()}"'
    html = re.sub(r'src="([^"]+\.webp)"', to_jpg, html)
    if keys_section:
        # la première section (raccourcis) est encadrée pour être trouvée d'un coup d'œil
        start = html.find(f"<h2>{keys_section}")
        end = html.find("<h2>", start + 4)
        if start >= 0 and end > start:
            html = html[:start] + '<section class="keys">' + html[start:end] + "</section>" + html[end:]
    page = f"""<!doctype html><html lang="{lang}"><head><meta charset="utf-8"><base href="{DOCS.as_uri()}/">
<title>{title}</title><style>{CSS.replace("GOLEM_H", golem_h)}</style></head><body>
<div class="cover"><img src="{GOLEM}" alt=""><div><div class="word">OFAD GOLEM</div><div class="sub">{sub}</div>
<div class="note">{note}</div></div></div>{html}</body></html>"""
    tmp = DOCS / f".{pdf_name}.html"
    tmp.write_text(page, encoding="utf-8")
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page()
        pg.goto(tmp.as_uri())
        pg.wait_for_load_state("networkidle")
        pg.pdf(path=str(DOCS / pdf_name), format="A4", print_background=True, display_header_footer=True,
               header_template="<span></span>",
               footer_template=f'<div style="font-size:8px;width:100%;text-align:center;color:#888">{title} — page <span class="pageNumber"></span> / <span class="totalPages"></span></div>',
               margin={"top": "14mm", "bottom": "16mm", "left": "14mm", "right": "14mm"})
        b.close()
    tmp.unlink()
    print(f"{pdf_name} écrit")


if __name__ == "__main__":
    build("NOTICE.md", "NOTICE.pdf", "OFAD GOLEM — Notice", "Notice de prise en main · raccourcis",
          "Groovebox à 16 pistes, logiciel libre (AGPL) de One For All Distribution.", "150px",
          keys_section="Raccourcis")
    # v0.46 fix28 : notice en anglais (l'interface reste en français pour l'instant : table de correspondance en tête)
    build("NOTICE.en.md", "NOTICE-EN.pdf", "OFAD GOLEM — Manual", "Getting started · shortcuts",
          "16-track groovebox, free software (AGPL) by One For All Distribution.", "150px",
          keys_section="Keyboard and mouse shortcuts", lang="en")
    build("CATALOGUE.md", "CATALOGUE.pdf", "OFAD GOLEM — Catalogue", "Catalogue des instruments et des effets",
          "Les noms sont propres au Groovebox ; chaque module reprend le principe d'une famille de machines, jamais son nom, sa façade ni ses sons.", "110px")
