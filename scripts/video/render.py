#!/usr/bin/env python3
"""Montage de la vidéo OFAD GOLEM (une langue) : un fichier par scène, mêmes réglages, puis concat + son."""
import os, subprocess, sys, glob, shutil

lang = sys.argv[1]
only = sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] else []
V = "/tmp/vid"
CAP, CARDS, OUT = f"{V}/cap-{lang}", f"{V}/cards-{lang}", f"{V}/out-{lang}"
os.makedirs(OUT, exist_ok=True)
ENC = ["-c:v", "libx264", "-crf", "18", "-preset", "veryfast", "-pix_fmt", "yuv420p", "-r", "30", "-an"]
FADE = 0.35

def run(args, dst):
    tmp = dst[:-4] + ".tmp.mp4"
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", *args, *ENC, tmp])
    if r.returncode != 0:
        raise SystemExit(f"échec {dst}")
    os.replace(tmp, dst)
    print("ok", os.path.basename(dst))

def cap_filter(D, has_cap):
    v = f"[0:v]fps=30,scale=1920:1080:flags=lanczos,setsar=1,trim=duration={D}[v0]"
    if not has_cap:
        return v + f";[v0]fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"
    c = (f"[1:v]format=rgba,loop=-1:1,trim=duration={D},fade=t=in:st=0.7:d=0.5:alpha=1,"
         f"fade=t=out:st={D-1.3}:d=0.5:alpha=1[c]")
    return v + ";" + c + f";[v0][c]overlay=0:0,fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"

def frames_scene(name, dirs, D, cap):
    seq = f"{V}/seq-{lang}-{name}"
    shutil.rmtree(seq, ignore_errors=True); os.makedirs(seq)
    k = 0
    for d in dirs:
        for f in sorted(glob.glob(f"{CAP}/{d}/f*.jpg")):
            os.symlink(f, f"{seq}/f{k:05d}.jpg"); k += 1
    rate = k / D
    args = ["-framerate", f"{rate:.6f}", "-i", f"{seq}/f%05d.jpg"]
    if cap: args += ["-framerate", "30", "-i", cap]
    run(args + ["-filter_complex", cap_filter(D, bool(cap)), "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

def still_scene(name, png, D, cap=None, zoom=True):
    args = ["-framerate", "30", "-i", png]
    if cap: args += ["-framerate", "30", "-i", cap]
    z = (f"[0:v]format=rgb24,loop=-1:1,trim=duration={D},setpts=PTS-STARTPTS,scale=2112:1188,"
         f"crop=1920:1080:'(iw-1920)/2*(1-t/{D})':'(ih-1080)/2*(1-t/{D})',scale=1920:1080[v0]") if zoom else \
        f"[0:v]format=rgb24,loop=-1:1,trim=duration={D},setpts=PTS-STARTPTS,scale=1920:1080[v0]"
    if cap:
        f = z + (f";[1:v]format=rgba,loop=-1:1,trim=duration={D},fade=t=in:st=0.7:d=0.5:alpha=1,fade=t=out:st={D-1.3}:d=0.5:alpha=1[c]"
                 f";[v0][c]overlay=0:0,fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]")
    else:
        f = z + f";[v0]fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]"
    run(args + ["-filter_complex", f, "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

def gallery_scene(name, pngs, D, cap):
    n = len(pngs); seg = D / n; xf = 0.6
    args = []
    for p in pngs: args += ["-framerate", "30", "-i", p]
    args += ["-framerate", "30", "-i", cap]
    parts = []
    for i in range(n):
        dur = seg + (xf if i < n - 1 else 0)
        parts.append(f"[{i}:v]format=rgb24,loop=-1:1,trim=duration={dur},setpts=PTS-STARTPTS,scale=1920:1080,settb=AVTB[g{i}]")
    chain, last, off = [], "g0", 0.0
    for i in range(1, n):
        off += seg
        chain.append(f"[{last}][g{i}]xfade=transition=fade:duration={xf}:offset={off}[x{i}]")
        last = f"x{i}"
    f = ";".join(parts + chain)
    f += (f";[{n}:v]format=rgba,loop=-1:1,trim=duration={D},fade=t=in:st=0.7:d=0.5:alpha=1,fade=t=out:st={D-1.3}:d=0.5:alpha=1[c]"
          f";[{last}]trim=duration={D}[gv];[gv][c]overlay=0:0,fade=t=in:st=0:d={FADE},fade=t=out:st={D-FADE}:d={FADE},format=yuv420p[v]")
    run(args + ["-filter_complex", f, "-map", "[v]", "-t", str(D)], f"{OUT}/{name}.mp4")

SC = [
    ("s01", lambda: still_scene("s01", f"{CARDS}/s01.png", 7)),
    ("s02", lambda: frames_scene("s02", ["s02"], 12, f"{CARDS}/cap-s02.png")),
    ("s03", lambda: frames_scene("s03", ["s03"], 14, f"{CARDS}/cap-s03.png")),
    ("s04", lambda: frames_scene("s04", ["s04"], 14, f"{CARDS}/cap-s04.png")),
    ("s05", lambda: gallery_scene("s05", [f"{CARDS}/s05-{i}.png" for i in range(1, 5)], 16, f"{CARDS}/cap-s05.png")),
    ("s06", lambda: frames_scene("s06", ["s06a", "s06b", "s06c"], 16, f"{CARDS}/cap-s06.png")),
    ("s07", lambda: frames_scene("s07", ["s07"], 14, f"{CARDS}/cap-s07.png")),
    ("s08", lambda: frames_scene("s08", ["s08"], 14, f"{CARDS}/cap-s08.png")),
    ("s09", lambda: frames_scene("s09", ["s09"], 14, f"{CARDS}/cap-s09.png")),
    ("s10", lambda: still_scene("s10", f"{CARDS}/s10.png", 14, f"{CARDS}/cap-s10.png", zoom=False)),
    ("s11", lambda: still_scene("s11", f"{CARDS}/s11.png", 12)),
    ("s12", lambda: still_scene("s12", f"{CARDS}/s12.png", 6)),
]
for name, fn in SC:
    if not only or name in only: fn()

if not only or "final" in only:
    with open(f"{OUT}/list.txt", "w") as fh:
        for name, _ in SC: fh.write(f"file '{OUT}/{name}.mp4'\n")
    final = f"{V}/OFAD-GOLEM-presentation-{lang.upper()}.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", f"{OUT}/list.txt", "-c", "copy", f"{OUT}/video.mp4"], check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", f"{OUT}/video.mp4", "-i", f"{V}/son-final.wav", "-map", "0:v", "-map", "1:a",
                    "-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest", "-movflags", "+faststart", final + ".tmp.mp4"], check=True)
    os.replace(final + ".tmp.mp4", final)
    print("FINAL", final)
