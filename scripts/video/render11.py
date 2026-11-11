#!/usr/bin/env python3
# Groovebox — OFAD — AGPL-3.0-or-later
"""1.1 : montage de la vidéo « de la 0 à la 1.1 » (une langue).
Un fichier par scène (mêmes réglages : 1920 × 1080, 30 i/s, x264 CRF 18), concaténés sans réencodage,
puis le son (rendu par groovebox_render --script avec le scénario tiré de scenes11.cjs).
Usage : python3 render11.py fr|en [scène,scène…|final|son]"""
import os, subprocess, sys, json, glob, shutil

lang = sys.argv[1]
only = sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] else []
HERE = os.path.dirname(os.path.abspath(__file__))
V = "/tmp/vid11"
CAP, CARDS, OUT = f"{V}/cap-{lang}", f"{V}/cards-{lang}", f"{V}/out-{lang}"
os.makedirs(OUT, exist_ok=True)
SC = json.loads(subprocess.run(["node", f"{HERE}/scenes11.cjs", "json", lang], capture_output=True, text=True, check=True).stdout)
ENC = ["-c:v", "libx264", "-crf", "18", "-preset", "veryfast", "-pix_fmt", "yuv420p", "-r", "30", "-an"]
FADE = 0.3

def run(args, dst):
    tmp = dst[:-4] + ".tmp.mp4"
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args, *ENC, tmp])
    if r.returncode != 0:
        raise SystemExit(f"échec {dst}")
    os.replace(tmp, dst)
    print("ok", os.path.basename(dst))

def cap_in(cap, D, idx):
    # légende : entre à 0,6 s, sort 1,2 s avant la fin
    return (f";[{idx}:v]format=rgba,loop=-1:1,trim=duration={D},fade=t=in:st=0.6:d=0.5:alpha=1,"
            f"fade=t=out:st={max(1.0, D - 1.2)}:d=0.5:alpha=1[c]")

def frames_scene(s):
    name, D = s["id"], s["secs"]
    frames = sorted(glob.glob(f"{CAP}/{name}/f*.jpg"))
    if not frames:
        raise SystemExit(f"pas d'images pour {name}")
    rate = len(frames) / D
    args = ["-framerate", f"{rate:.6f}", "-i", f"{CAP}/{name}/f%04d.jpg"]
    cap = f"{CARDS}/cap-{name}.png" if s.get("cap") else None
    if s["phone"]:
        args += ["-framerate", "30", "-i", f"{CARDS}/phone-bg.png"]
        f = (f"[1:v]format=rgb24,loop=-1:1,trim=duration={D},setpts=PTS-STARTPTS[bg];"
             f"[0:v]fps=30,scale=1760:-2:flags=lanczos,setsar=1[ph];[bg][ph]overlay=(W-w)/2:28+(832-h)/2:shortest=1,trim=duration={D}[v0]")
        ci = 2
    else:
        f = f"[0:v]fps=30,scale=1920:1080:flags=lanczos,setsar=1,trim=duration={D}[v0]"
        ci = 1
    if cap:
        args += ["-framerate", "30", "-i", cap]
        f += cap_in(cap, D, ci) + f";[v0][c]overlay=0:0,fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"
    else:
        f += f";[v0]fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"
    run(args + ["-filter_complex", f, "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

def still_scene(name, png, D, zoom=True):
    z = (f"[0:v]format=rgb24,loop=-1:1,trim=duration={D},setpts=PTS-STARTPTS,scale=2112:1188,"
         f"crop=1920:1080:'(iw-1920)/2*(1-t/{D})':'(ih-1080)/2*(1-t/{D})',scale=1920:1080") if zoom else \
        f"[0:v]format=rgb24,loop=-1:1,trim=duration={D},setpts=PTS-STARTPTS,scale=1920:1080"
    f = z + f",fade=t=in:st=0:d={FADE*2},fade=t=out:st={D-FADE*2}:d={FADE*2},format=yuv420p[v]"
    run(["-framerate", "30", "-i", png, "-filter_complex", f, "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

def gallery_scene(name, pngs, D, cap):
    n = len(pngs); seg = D / n; xf = 0.6
    args = []
    for p in pngs: args += ["-framerate", "30", "-i", p]
    args += ["-framerate", "30", "-i", cap]
    parts = [f"[{i}:v]format=rgb24,loop=-1:1,trim=duration={seg + (xf if i < n - 1 else 0)},setpts=PTS-STARTPTS,scale=1920:1080,settb=AVTB[g{i}]" for i in range(n)]
    chain, last, off = [], "g0", 0.0
    for i in range(1, n):
        off += seg
        chain.append(f"[{last}][g{i}]xfade=transition=fade:duration={xf}:offset={off}[x{i}]")
        last = f"x{i}"
    f = ";".join(parts + chain) + cap_in(cap, D, n) + \
        f";[{last}]trim=duration={D}[gv];[gv][c]overlay=0:0,fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"
    run(args + ["-filter_complex", f, "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

def build(s):
    if s.get("card") == "intro": still_scene(s["id"], f"{CARDS}/s01.png", s["secs"])
    elif s.get("card") == "gallery": gallery_scene(s["id"], [f"{CARDS}/s33-{i}.png" for i in range(1, 5)], s["secs"], f"{CARDS}/cap-s33.png")
    elif s.get("card") == "plat": still_scene(s["id"], f"{CARDS}/s34.png", s["secs"])
    elif s.get("card") == "outro": still_scene(s["id"], f"{CARDS}/s35.png", s["secs"], zoom=False)
    else: frames_scene(s)

def sound():
    # scénario du son (même pour FR et EN) : une ligne par passage
    lines = ["# OFAD GOLEM 1.1 — vidéo (secondes, pistes entendues, pavé vectoriel, crossfader), tiré de scenes11.cjs"]
    for s in SC:
        for secs, a in s["audio"]:
            a = a.replace("pistes=toutes", "").replace("toutes", "")
            lines.append(f"{secs} {a}".strip())
    open(f"{V}/scenario11.txt", "w").write("\n".join(lines) + "\n")
    total = sum(s["secs"] for s in SC)
    R = "/home/claude/groovebox/build/groovebox_render_artefacts/Release/groovebox_render"
    subprocess.run([R, "--script", "tribe.gbx", "scenario11.txt", "son11.wav"], cwd=V, check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", f"{V}/son11.wav", "-af",
                    f"afade=t=in:d=1.0,afade=t=out:st={total-5}:d=5,loudnorm=I=-14:TP=-1.5", "-ar", "48000", f"{V}/son11-final.wav"], check=True)
    print("son prêt", total, "s")

if only == ["son"]:
    sound(); sys.exit(0)
for s in SC:
    if not only or s["id"] in only: build(s)
if not only or "final" in only:
    if not os.path.exists(f"{V}/son11-final.wav"): sound()
    with open(f"{OUT}/list.txt", "w") as fh:
        for s in SC: fh.write(f"file '{OUT}/{s['id']}.mp4'\n")
    final = f"{V}/OFAD-GOLEM-1.1-video-{lang.upper()}.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", f"{OUT}/list.txt", "-c", "copy", f"{OUT}/video.mp4"], check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", f"{OUT}/video.mp4", "-i", f"{V}/son11-final.wav", "-map", "0:v", "-map", "1:a",
                    "-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest", "-movflags", "+faststart", final + ".tmp.mp4"], check=True)
    os.replace(final + ".tmp.mp4", final)
    print("FINAL", final)
