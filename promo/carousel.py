"""A five-slide Instagram carousel for Tertiary (4:5, 1080x1350), from the plugin's own UI as drawn in Reel 1.

  python carousel.py <ui frame png> <out dir>
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

UI = Image.open(sys.argv[1]).convert("RGB")  # 1500 x 1030, the editor at 2x
OUT = Path(sys.argv[2])
W, H = 1080, 1350
F = r"C:\Windows\Fonts"
font = lambda name, size: ImageFont.truetype(f"{F}\\{name}", size)
KICKER, TITLE, BODY, FOOT = font("seguisb.ttf", 28), font("seguisb.ttf", 64), font("segoeui.ttf", 42), font("segoeui.ttf", 30)
TEXT, MUTED, FAINT = (236, 236, 240), (160, 160, 172), (96, 96, 110)


def background():
    bg = Image.new("RGB", (W, H), (11, 10, 14))
    wash = Image.new("RGB", (W, H), (34, 18, 40))
    mask = Image.linear_gradient("L").resize((W, H)).point(lambda v: int(max(0, 255 - v) * 0.45))
    return Image.composite(wash, bg, mask)


def center(d, y, s, f, fill, spacing=10):
    for line in s.split("\n"):
        d.text(((W - d.textlength(line, font=f)) / 2, y), line, font=f, fill=fill)
        y += f.size + spacing
    return y


def place(img, piece, width, y):
    h = int(piece.height * width / piece.width)
    p = piece.resize((width, h), Image.LANCZOS)
    x = (W - width) // 2
    sh = Image.new("RGBA", (width + 80, h + 80), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle((40, 50, width + 40, h + 50), 22, fill=(0, 0, 0, 170))
    sh = sh.filter(ImageFilter.GaussianBlur(22))
    img.paste(sh, (x - 40, y - 40), sh)
    m = Image.new("L", (width, h), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, width, h), 16, fill=255)
    img.paste(p, (x, y), m)
    return y + h


def slide(kicker, title, piece, width, body, n):
    img = background()
    d = ImageDraw.Draw(img)
    y = center(d, 110, " ".join(kicker), KICKER, MUTED)
    y = center(d, y + 18, title, TITLE, TEXT, 8)
    if piece is not None:
        y = place(img, piece, width, y + 50)
    center(d, y + 56, body, BODY, TEXT, 12)
    center(d, H - 90, "Joe Caulfield  ·  Wonderland Audio", FOOT, FAINT)
    d.text((W - 90, H - 90), f"{n}/5", font=FOOT, fill=FAINT)
    img.save(OUT / f"Carousel {n} of 5.png")


OUT.mkdir(parents=True, exist_ok=True)
scope = UI.crop((14, 104, 742, 506))
freq = UI.crop((758, 104, 1488, 506))
controls = UI.crop((76, 516, 1424, 1024))
slide("TERTIARY", "A tremolo\nin three bands", UI, 1000, "Split the sound into lows, mids and highs,\nand give each its own tremolo.", 1)
slide("THREE LFOS", "Shape, depth,\nrhythm, phase", scope, 960, "Each band moves on its own.\nSeven shapes, synced to the song or free.", 2)
slide("TWO CROSSOVERS", "Choose where\nthe bands split", freq, 960, "Put the split points where the song needs them,\nand watch each band on the spectrum.", 3)
slide("THE IDEA", "Hold the low end.\nMove the top.", controls, 1000, "Or the other way round. Bypass, solo\nor mute any band while you set it up.", 4)
slide("TERTIARY", "Made by me,\nbuilt in JUCE", None, 0,
      "VST3 and standalone for Mac and Windows,\nAU on Mac. Tested in Cubase and Reaper.\n\nA demo is at wonderlandaudio.com", 5)
print("ok")
