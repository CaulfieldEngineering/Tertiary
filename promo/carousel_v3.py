"""v3 of the five-slide carousel (slide 5 with Tertiary 1.1's formats); from v2 (Joe, 2026-10-27): no name footer, the Wonderland Audio logo on every slide, a plain
background and plain wording (keep the AI tells low). From the plugin's own UI drawn at 3x in v2 Reel 1.

  python carousel.py <ui frame png> <out dir>
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

UI = Image.open(sys.argv[1]).convert("RGB")  # 2250 x 1545, the editor at 3x
REPO = Path(__file__).resolve().parent.parent
LOGO = Image.open(REPO / "plugin" / "images" / "CompanyTitle.png").convert("RGBA")
LOGO = LOGO.crop(LOGO.getbbox())
OUT = Path(sys.argv[2])
W, H = 1080, 1350
F = r"C:\Windows\Fonts"
font = lambda name, size: ImageFont.truetype(f"{F}\\{name}", size)
KICKER, TITLE, BODY, FOOT = font("seguisb.ttf", 28), font("seguisb.ttf", 64), font("segoeui.ttf", 42), font("segoeui.ttf", 30)
TEXT, MUTED, FAINT = (236, 236, 240), (160, 160, 172), (96, 96, 110)


def background():
    return Image.new("RGB", (W, H), (14, 14, 16))


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
    mark = LOGO.resize((300, int(LOGO.height * 300 / LOGO.width)), Image.LANCZOS)
    img.paste(mark, (60, H - 96), mark)
    d.text((W - 90, H - 90), f"{n}/5", font=FOOT, fill=FAINT)
    img.save(OUT / f"Carousel {n} of 5.png")


OUT.mkdir(parents=True, exist_ok=True)
scope = UI.crop((21, 156, 1113, 759))
freq = UI.crop((1137, 156, 2232, 759))
controls = UI.crop((114, 774, 2136, 1536))
slide("TERTIARY", "Multiband\nTremolo", UI, 1000, "Three bands, lows, mids and highs, each with\nits own LFO: shape, depth, rhythm and phase.", 1)
slide("THE LFOS", "One per band", scope, 960, "Seven shapes. Synced from 1/2 to 1/16 notes\nand triplets, or free-running in Hz.", 2)
slide("THE CROSSOVERS", "Two split points,\n20 Hz to 20 kHz", freq, 960, "Set where each band starts and ends.\nThe spectrum shows what lands in each.", 3)
slide("PER BAND", "Bypass, solo, mute,\ninvert, gain", controls, 1000, "Leave the low end alone and move the top,\nor the other way round.", 4)
slide("TERTIARY", "Mac and Windows", None, 0,
      "Mac (Apple Silicon and Intel, macOS 11 or later):\nVST3, AU and standalone.\nWindows: VST3 and standalone.\nTested in Cubase and Reaper. Built in JUCE.\n\nA demo is at wonderlandaudio.com", 5)
print("ok")
