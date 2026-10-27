"""Render the v2 Reels from the Duet project "Tertiary Promo" (stems through the mini PC's engine, then reels_v2.py).

    python make_promo_v2.py                 every Reel
    python make_promo_v2.py "02 " "03 "     only these (Reels 02 and 03 need the KEYS track's instrument loaded)

Reels 02 and 03 play the arpeggio and stabs on the KEYS track (the sampled-guitar GTR track is muted, Joe 2026-10-27).
"""
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DUET = Path(r"C:\Users\Joe\Documents\GitHub\duet")
OUT = Path(r"C:\Users\Joe\Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-27 v2")
STEMS = HERE / "work" / "stems"
PIECES = [("P1", 1, 11), ("P2", 11, 21), ("P3", 21, 31), ("P4", 31, 41)]

env = dict(os.environ, DUET_PROJECT="Tertiary Promo")
for name, a, b in PIECES:
    d = STEMS / name
    d.mkdir(parents=True, exist_ok=True)
    for f in d.glob("*.wav"):
        f.unlink()
    subprocess.run(["node", "duet.mjs", "render", str(d / "mix.wav"), "--from", str(a), "--to", str(b), "--stems"],
                   cwd=DUET, env=env, check=True)
subprocess.run([sys.executable, str(HERE / "reels_v2.py"), str(STEMS), str(OUT)] + sys.argv[1:], check=True)
