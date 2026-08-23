#!/usr/bin/env python3
"""Generate project-owned 64px classic UI PNG assets using only Python stdlib."""
from pathlib import Path
import struct
import zlib

SIZE = 64
OUT = Path(__file__).resolve().parents[1] / "assets" / "classic"


def png(path, pixels):
    raw = b"".join(b"\0" + bytes(pixels[y * SIZE * 4:(y + 1) * SIZE * 4]) for y in range(SIZE))
    chunk = lambda tag, data: struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def icon(name, color, draw):
    pixels = bytearray([0, 0, 0, 0] * SIZE * SIZE)
    def point(x, y, c=color):
        if 0 <= x < SIZE and 0 <= y < SIZE:
            i = (y * SIZE + x) * 4; pixels[i:i + 4] = bytes((*c, 255))
    def line(x0, y0, x1, y1, c=color, width=5):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for n in range(steps + 1):
            x, y = round(x0 + (x1 - x0) * n / steps), round(y0 + (y1 - y0) * n / steps)
            for dx in range(-width // 2, width // 2 + 1):
                for dy in range(-width // 2, width // 2 + 1): point(x + dx, y + dy, c)
    def rect(x0, y0, x1, y1, c=color, fill=True):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if fill or x in (x0, x1) or y in (y0, y1): point(x, y, c)
    def circle(cx, cy, radius, c=color, fill=True):
        for y in range(cy - radius, cy + radius + 1):
            for x in range(cx - radius, cx + radius + 1):
                d = (x-cx)*(x-cx)+(y-cy)*(y-cy)
                if d <= radius*radius and (fill or d >= (radius-3)*(radius-3)): point(x, y, c)
    draw(point, line, rect, circle)
    png(OUT / f"{name}.png", pixels)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    icon("checkmark", (37, 142, 66), lambda p,l,r,c: (l(14,34,26,46), l(26,46,51,17)))
    icon("warning", (230, 154, 32), lambda p,l,r,c: (l(32,11,55,52), l(55,52,9,52), l(9,52,32,11), c(32,42,3)))
    icon("critical", (204, 53, 48), lambda p,l,r,c: (c(32,32,24,fill=False), l(19,19,45,45), l(45,19,19,45)))
    icon("information", (43, 105, 181), lambda p,l,r,c: (c(32,32,24,fill=False), c(32,19,3), l(32,28,32,47)))
    icon("question", (111, 79, 157), lambda p,l,r,c: (c(32,32,24,fill=False), l(21,24,26,18), l(26,18,39,18), l(39,18,43,25), l(43,25,32,35), l(32,35,32,40), c(32,48,3)))
    icon("copy", (76, 114, 176), lambda p,l,r,c: (r(18,12,45,43,fill=False), r(10,22,37,53,fill=False)))
    icon("crosshairs", (38, 132, 124), lambda p,l,r,c: (c(32,32,16,fill=False), l(32,7,32,24), l(32,40,32,57), l(7,32,24,32), l(40,32,57,32), c(32,32,3)))
    icon("checkers", (92, 92, 92), lambda p,l,r,c: [r(x,y,x+11,y+11,(70,70,70) if (x//12+y//12)%2 else (210,210,210)) for x in range(8,56,12) for y in range(8,56,12)])
    icon("window_border", (58, 91, 126), lambda p,l,r,c: (r(7,7,56,56,fill=False), r(7,7,56,17), c(15,12,2,(220,80,80)), c(23,12,2,(230,170,45)), c(31,12,2,(55,160,80))))

if __name__ == "__main__": main()
