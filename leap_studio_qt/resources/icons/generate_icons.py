#!/usr/bin/env python3
"""Generate LEAP Conformance Studio Windows icon assets."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
SIZES = (16, 24, 32, 48, 64, 128, 256)


def _font(size: int, bold: bool = True) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/segoeuib.ttf") if bold else Path("C:/Windows/Fonts/segoeui.ttf"),
        Path("C:/Windows/Fonts/arialbd.ttf") if bold else Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def _rounded_rect(
    draw: ImageDraw.ImageDraw,
    box: tuple[int, int, int, int],
    radius: int,
    fill: tuple[int, int, int, int],
    outline: tuple[int, int, int, int] | None = None,
    width: int = 1,
) -> None:
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def render_icon(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    scale = size / 256.0

    def s(value: float) -> int:
        return max(1, int(round(value * scale)))

    bg = (43, 43, 43, 255)
    frame = (61, 90, 128, 255)
    frame_border = (74, 122, 176, 255)
    inner = (36, 52, 71, 255)
    text_color = (232, 232, 232, 255)
    pass_green = (61, 186, 108, 255)
    safe_yellow = (230, 194, 0, 255)

    _rounded_rect(draw, (0, 0, size - 1, size - 1), s(52), bg)

    if size >= 32:
        _rounded_rect(
            draw,
            (s(44), s(68), size - s(44), size - s(52)),
            s(16),
            frame,
            outline=frame_border,
            width=max(1, s(6)),
        )
        _rounded_rect(
            draw,
            (s(58), s(82), size - s(58), size - s(86)),
            s(10),
            inner,
        )

        font_px = max(8, s(40))
        font = _font(font_px, bold=True)
        label = "LEAP" if size >= 48 else "L"
        bbox = draw.textbbox((0, 0), label, font=font)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]
        text_x = (size - text_w) // 2
        text_y = (size - text_h) // 2 - s(4)
        draw.text((text_x, text_y), label, fill=text_color, font=font)

        bar_w = s(112)
        bar_h = max(2, s(8))
        bar_x = (size - bar_w) // 2
        bar_y = size - s(80)
        _rounded_rect(draw, (bar_x, bar_y, bar_x + bar_w, bar_y + bar_h), s(4), safe_yellow)

        badge_r = s(34)
        badge_cx = size - s(60)
        badge_cy = s(60)
        draw.ellipse(
            (
                badge_cx - badge_r,
                badge_cy - badge_r,
                badge_cx + badge_r,
                badge_cy + badge_r,
            ),
            fill=pass_green,
        )
        check = max(3, s(7))
        draw.line(
            (
                badge_cx - s(14),
                badge_cy,
                badge_cx - s(4),
                badge_cy + s(10),
                badge_cx + s(14),
                badge_cy - s(12),
            ),
            fill=(255, 255, 255, 255),
            width=check,
            joint="curve",
        )
    elif size >= 24:
        _rounded_rect(draw, (s(40), s(72), size - s(40), size - s(72)), s(12), frame)
        font = _font(max(9, s(28)), bold=True)
        draw.text((s(8), s(4)), "L", fill=text_color, font=font)
        draw.ellipse((size - s(52), s(28), size - s(20), s(60)), fill=pass_green)
    else:
        _rounded_rect(draw, (1, 1, size - 2, size - 2), 3, frame, outline=frame_border, width=1)
        font = _font(9, bold=True)
        draw.text((3, 0), "L", fill=text_color, font=font)
        draw.rectangle((size - 6, 1, size - 1, 6), fill=pass_green)

    return img.convert("RGBA")


def write_pngs() -> list[Path]:
    paths: list[Path] = []
    for size in SIZES:
        path = ROOT / f"leap_studio_{size}.png"
        render_icon(size).save(path, format="PNG")
        paths.append(path)
    return paths


def write_ico() -> Path:
    """Write a multi-size ICO using Pillow (largest frame first for all DPIs)."""
    ico_path = ROOT / "leap_studio.ico"
    images = [render_icon(size) for size in reversed(SIZES)]
    images[0].save(ico_path, format="ICO", append_images=images[1:])
    return ico_path


def main() -> None:
    png_paths = write_pngs()
    ico_path = write_ico()
    rc_path = ROOT.parent / "leap_studio.rc"
    if rc_path.exists():
        rc_path.touch()
    print(f"Wrote {len(png_paths)} PNG sizes and {ico_path.name}")


if __name__ == "__main__":
    main()
