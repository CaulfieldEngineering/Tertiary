"""Re-render Tertiary's Instagram promo (2026-10-26) from the Duet project "Tertiary Promo".

Joe, 2026-10-26: "let's get in the habit of generating unique midi tracks for content. I can load up on some of those
Kontakt sample libraries tomorrow." So: change the instruments in Duet (the tracks PAD, BASS, GTR, DRUMS), then run

    python make_promo.py

It renders each piece's stems through the Duet engine on the mini PC (node duet.mjs render --stems), runs them
through Tertiary (build-review's ReviewRender, built with -DWLA_REVIEW_RENDER=ON), and rebuilds every Reel and the
carousel into Dropbox\\Caulfield Engineering\\Tertiary\\Promo 2026-10-26. The MIDI itself is compose.py (seeded, so
it writes the same notes every time); run it only to change the music.
"""
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DUET = Path(r"C:\Users\Joe\Documents\GitHub\duet")
OUT = Path(r"C:\Users\Joe\Dropbox\Caulfield Engineering\Tertiary\Promo 2026-10-26")
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
subprocess.run([sys.executable, str(HERE / "reels.py"), str(STEMS), str(OUT)] + sys.argv[1:], check=True)
frame = HERE / "work" / "01 Three bands, three rhythms" / "frames" / "00150.png"
subprocess.run([sys.executable, str(HERE / "carousel.py"), str(frame), str(OUT / "Carousel")], check=True)
