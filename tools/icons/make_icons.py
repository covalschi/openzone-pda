# openzone-pda/tools/icons/make_icons.py
"""Draw the PDA's icon atlas and write its imageset.

Every sprite is white on transparent, drawn 4x oversampled and downscaled,
so the widget's `color` tints it and the edges stay smooth at any scale.
Cells are 64 px on a 512x512 sheet (8x8 = 64 slots); a sprite is 56 px with
a 4 px gutter, so neighbours never bleed when the engine scales a cell.

    <venv python> tools/icons/make_icons.py     # writes the PNG and the .imageset

Then convert the PNG with the MCP tool asset_convert -> oz_pda_icons_ca.paa
(the _ca suffix keeps the alpha; _co would drop it). The set is registered in
config.cpp (class imageSets); a layout refers to a sprite as
`image0 "set:oz_pda_icons image:tab_chat"`.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "OpenZone_PDA" / "gui" / "imagesets"
SET_NAME = "oz_pda_icons"
TEXTURE = "OpenZone_PDA/gui/imagesets/oz_pda_icons_ca.paa"
SHEET = 512
CELL = 64
GUTTER = 4
SPRITE = CELL - 2 * GUTTER   # 56
S = 4                        # oversampling factor
W = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)

SPRITES: list[tuple[str, object]] = []


def sprite(name: str):
    def register(fn):
        SPRITES.append((name, fn))
        return fn
    return register


def u(v: float) -> float:
    return v * S


def bbox(x0, y0, x1, y1):
    return (u(x0), u(y0), u(x1), u(y1))


def ring(d, cx, cy, r, w=4, start=0, end=360):
    d.arc(bbox(cx - r, cy - r, cx + r, cy + r), start=start, end=end, fill=W, width=int(u(w)))


def disc(d, cx, cy, r, fill=W):
    d.ellipse(bbox(cx - r, cy - r, cx + r, cy + r), fill=fill)


def rect(d, x0, y0, x1, y1, w=0, fill=W, radius=0):
    if w:
        d.rounded_rectangle(bbox(x0, y0, x1, y1), radius=u(radius), outline=W, width=int(u(w)))
    else:
        d.rounded_rectangle(bbox(x0, y0, x1, y1), radius=u(radius), fill=fill)


def poly(d, pts, w=0):
    scaled = [(u(x), u(y)) for x, y in pts]
    if w:
        d.line(scaled + [scaled[0]], fill=W, width=int(u(w)), joint="curve")
    else:
        d.polygon(scaled, fill=W)


def line(d, pts, w=4):
    d.line([(u(x), u(y)) for x, y in pts], fill=W, width=int(u(w)), joint="curve")


def star(cx, cy, r_out, r_in, n=5):
    pts = []
    for i in range(2 * n):
        r = r_out if i % 2 == 0 else r_in
        a = -math.pi / 2 + i * math.pi / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


# ----- tabs --------------------------------------------------------------
@sprite("tab_device")
def _(d):
    rect(d, 16, 6, 40, 50, w=4, radius=6)
    rect(d, 21, 12, 35, 36, fill=W, radius=2)

@sprite("tab_map")
def _(d):
    ring(d, 28, 22, 12, w=4)
    poly(d, [(18, 30), (38, 30), (28, 50)])
    disc(d, 28, 22, 4)

@sprite("tab_contacts")
def _(d):
    disc(d, 21, 19, 8)
    rect(d, 7, 31, 35, 48, fill=W, radius=8)
    disc(d, 39, 18, 6)
    rect(d, 31, 32, 51, 46, fill=W, radius=6)

@sprite("tab_notes")
def _(d):
    rect(d, 14, 6, 42, 50, w=4, radius=3)
    for y in (18, 26, 34):
        line(d, [(21, y), (35, y)], w=3)

@sprite("tab_chat")
def _(d):
    rect(d, 6, 10, 50, 38, w=4, radius=8)
    poly(d, [(14, 36), (24, 36), (12, 48)])

@sprite("tab_news")
def _(d):
    rect(d, 8, 8, 48, 48, w=4, radius=3)
    rect(d, 14, 14, 42, 22, fill=W)
    line(d, [(14, 30), (42, 30)], w=3)
    line(d, [(14, 38), (34, 38)], w=3)

@sprite("tab_journal")
def _(d):
    line(d, [(14, 6), (14, 50)], w=4)
    poly(d, [(14, 8), (46, 16), (14, 26)])

@sprite("tab_faction")
def _(d):
    poly(d, [(28, 6), (48, 14), (46, 32), (28, 50), (10, 32), (8, 14)], w=4)

@sprite("tab_radio")
def _(d):
    disc(d, 28, 42, 4)
    for r in (12, 20, 28):
        ring(d, 28, 42, r, w=4, start=210, end=330)

@sprite("tab_page")
def _(d):
    rect(d, 10, 10, 46, 46, w=4, radius=3)
    disc(d, 28, 28, 5)

# ----- status ------------------------------------------------------------
@sprite("st_signal")
def _(d):
    for i, h in enumerate((12, 22, 32, 44)):
        x = 8 + i * 12
        rect(d, x, 50 - h, x + 8, 50, fill=W)

@sprite("st_battery")
def _(d):
    rect(d, 6, 18, 44, 38, w=3, radius=2)
    rect(d, 44, 24, 50, 32, fill=W)
    rect(d, 10, 22, 40, 34, fill=W)

@sprite("st_battery_low")
def _(d):
    rect(d, 6, 18, 44, 38, w=3, radius=2)
    rect(d, 44, 24, 50, 32, fill=W)
    rect(d, 10, 22, 18, 34, fill=W)

@sprite("st_lock")
def _(d):
    ring(d, 28, 22, 10, w=4, start=180, end=360)
    rect(d, 12, 24, 44, 48, fill=W, radius=3)

@sprite("st_hidden")
def _(d):
    d.ellipse(bbox(6, 18, 50, 38), outline=W, width=int(u(4)))
    disc(d, 28, 28, 6)
    line(d, [(10, 46), (46, 10)], w=4)

@sprite("st_power")
def _(d):
    ring(d, 28, 30, 16, w=4, start=300, end=240)
    line(d, [(28, 8), (28, 28)], w=4)

@sprite("badge")
def _(d):
    disc(d, 28, 28, 24)

# ----- map markers -------------------------------------------------------
@sprite("mk_marker")
def _(d):
    disc(d, 28, 20, 10)
    poly(d, [(18, 26), (38, 26), (28, 50)])

@sprite("mk_stash")
def _(d):
    rect(d, 8, 26, 48, 46, fill=W, radius=3)
    rect(d, 8, 12, 48, 23, fill=W, radius=5)

@sprite("mk_danger")
def _(d):
    poly(d, [(28, 6), (50, 48), (6, 48)], w=4)
    rect(d, 26, 18, 30, 36, fill=W)
    disc(d, 28, 42, 3)

@sprite("mk_route")
def _(d):
    for cx, cy in ((10, 46), (28, 28), (46, 10)):
        disc(d, cx, cy, 4)
    line(d, [(14, 42), (24, 32)], w=3)
    line(d, [(32, 24), (42, 14)], w=3)

# ----- faction emblems ---------------------------------------------------
@sprite("fx_duty")
def _(d):
    poly(d, [(28, 4), (50, 12), (48, 32), (28, 52), (8, 32), (6, 12)], w=4)
    rect(d, 26, 14, 30, 42, fill=W)
    rect(d, 16, 24, 40, 28, fill=W)

@sprite("fx_freedom")
def _(d):
    ring(d, 28, 28, 20, w=4)
    line(d, [(28, 28), (28, 8)], w=4)
    line(d, [(28, 28), (11, 40)], w=4)
    line(d, [(28, 28), (45, 40)], w=4)

@sprite("fx_loners")
def _(d):
    poly(d, star(28, 30, 22, 9))

@sprite("fx_bandits")
def _(d):
    disc(d, 28, 22, 16)
    rect(d, 18, 34, 38, 46, fill=W, radius=3)
    disc(d, 21, 20, 5, fill=CLEAR)
    disc(d, 35, 20, 5, fill=CLEAR)

@sprite("fx_mercs")
def _(d):
    poly(d, [(28, 4), (49, 16), (49, 40), (28, 52), (7, 40), (7, 16)], w=4)
    disc(d, 28, 28, 6)

@sprite("fx_monolith")
def _(d):
    poly(d, [(28, 4), (46, 26), (28, 52), (10, 26)])

@sprite("fx_ecologists")
def _(d):
    rect(d, 24, 6, 32, 22, fill=W)
    disc(d, 28, 36, 14)

@sprite("fx_clearsky")
def _(d):
    disc(d, 18, 32, 10)
    disc(d, 30, 24, 13)
    disc(d, 40, 32, 10)
    rect(d, 10, 32, 50, 42, fill=W, radius=4)

@sprite("fx_military")
def _(d):
    line(d, [(8, 18), (28, 38), (48, 18)], w=8)
    line(d, [(8, 32), (28, 52), (48, 32)], w=5)

# ----- ui ----------------------------------------------------------------
@sprite("ui_close")
def _(d):
    line(d, [(12, 12), (44, 44)], w=5)
    line(d, [(44, 12), (12, 44)], w=5)

@sprite("ui_check")
def _(d):
    line(d, [(10, 30), (24, 44), (48, 14)], w=5)

@sprite("ui_mine")
def _(d):
    line(d, [(8, 28), (34, 28)], w=5)
    poly(d, [(30, 14), (48, 28), (30, 42)])


def draw_sprite(fn) -> Image.Image:
    img = Image.new("RGBA", (SPRITE * S, SPRITE * S), CLEAR)
    fn(ImageDraw.Draw(img))
    return img.resize((SPRITE, SPRITE), Image.LANCZOS)


def write_imageset(names: list[str], path: Path) -> None:
    per_row = SHEET // CELL
    lines = ["ImageSetClass {", f' Name "{SET_NAME}"', f" RefSize {SHEET} {SHEET}", " Textures {",
             "  ImageSetTextureClass {", "   mpix 0", f'   path "{TEXTURE}"', "  }", " }", " Images {"]
    for i, name in enumerate(names):
        px = (i % per_row) * CELL + GUTTER
        py = (i // per_row) * CELL + GUTTER
        lines += [f"  ImageSetDefClass {name} {{", f'   Name "{name}"', f"   Pos {px} {py}",
                  f"   Size {SPRITE} {SPRITE}", "   Flags 0", "  }"]
    lines += [" }", " Groups {", " }", "}"]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def main() -> int:
    if len(SPRITES) > (SHEET // CELL) ** 2:
        raise SystemExit(f"{len(SPRITES)} sprites do not fit {SHEET}x{SHEET}")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (SHEET, SHEET), CLEAR)
    per_row = SHEET // CELL
    for i, (name, fn) in enumerate(SPRITES):
        px = (i % per_row) * CELL + GUTTER
        py = (i // per_row) * CELL + GUTTER
        sheet.paste(draw_sprite(fn), (px, py))
    png = OUT_DIR / f"{SET_NAME}_ca.png"
    sheet.save(png)
    write_imageset([name for name, _fn in SPRITES], OUT_DIR / f"{SET_NAME}.imageset")
    print(f"wrote {png} ({len(SPRITES)} sprites) and {SET_NAME}.imageset")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
