#!/usr/bin/env python3
"""Turn the clock artwork into the shape data the web preview draws.

    python3 scripts/make_geometry.py

Reads web/clock.svg and writes web/geometry.json.

The output describes SHAPES ONLY — polygons, gradient axes and the colon
circles. It deliberately says nothing about LEDs: how many sit inside a bar is a
property of the hardware, not the artwork, so the device publishes that at
/api/layout and the preview combines the two at runtime. That is what lets
LEDS_PER_SEGMENT in src/config.h change without regenerating anything here.

Each segment gets a gradient axis: the principal axis of its polygon, spanning
the full extent of the shape. The preview lays the segment's LED colours along
it as gradient stops.
"""

import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SVG = ROOT / "web" / "clock.svg"
OUT = ROOT / "web" / "geometry.json"

DIGIT_SEGMENTS = 28


def principal_axis(points):
    """Direction the bar runs in, from the covariance of its vertices."""
    cx = sum(p[0] for p in points) / len(points)
    cy = sum(p[1] for p in points) / len(points)
    sxx = sum((p[0] - cx) ** 2 for p in points)
    syy = sum((p[1] - cy) ** 2 for p in points)
    sxy = sum((p[0] - cx) * (p[1] - cy) for p in points)
    theta = 0.5 * math.atan2(2 * sxy, sxx - syy)
    return (cx, cy), (math.cos(theta), math.sin(theta))


def main():
    svg = SVG.read_text()

    view = re.search(r'viewBox="([\d.\s-]+)"', svg).group(1).split()
    width, height = float(view[2]), float(view[3])

    polygons = {}
    for m in re.finditer(r'<polygon id="seg(\d+)"[^>]*points="([^"]+)"', svg):
        nums = [float(x) for x in m.group(2).split()]
        polygons[int(m.group(1))] = list(zip(nums[0::2], nums[1::2]))
    if len(polygons) != DIGIT_SEGMENTS:
        raise SystemExit(f"expected {DIGIT_SEGMENTS} polygons, found {len(polygons)}")

    # Colon dots are circles. The path anchor is the bottom of the circle and
    # the first curve operand is the radius.
    radius = None
    dots = {}
    for m in re.finditer(
        r'<path id="(seg28b?)"[^>]*d="M([\d.]+),([\d.]+)c[\d.]+,\d+,([\d.]+)', svg
    ):
        name, x, y, r = m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4))
        radius = r
        dots[name] = (round(x, 1), round(y - r, 1))
    if set(dots) != {"seg28", "seg28b"}:
        raise SystemExit(f"expected both colon dots, found {sorted(dots)}")

    segments = []
    for seg in range(DIGIT_SEGMENTS):
        points = polygons[seg]
        (cx, cy), (ax, ay) = principal_axis(points)
        proj = [(p[0] - cx) * ax + (p[1] - cy) * ay for p in points]
        lo, hi = min(proj), max(proj)
        segments.append(
            {
                "points": [[round(x, 1), round(y, 1)] for x, y in points],
                "axis": [
                    [round(cx + ax * lo, 1), round(cy + ay * lo, 1)],
                    [round(cx + ax * hi, 1), round(cy + ay * hi, 1)],
                ],
            }
        )

    geometry = {
        "width": width,
        "height": height,
        "segments": segments,
        # Dot order matches segment-local LED order: dot 0 is the first LED of
        # the colon segment. seg28 is the upper dot in the artwork.
        "colon": {"r": radius, "dots": [list(dots["seg28"]), list(dots["seg28b"])]},
    }

    OUT.write_text(json.dumps(geometry))
    print(f"{OUT.relative_to(ROOT)}: {len(segments)} segments + colon, "
          f"{OUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()
