#!/usr/bin/env python3
"""vsepr_png_to_pdf.py

Quickly embed a PNG into a single-page A4 PDF.
This is a temporary fallback while the integrated OpenGL/VTK/Qt3D live
viewer is unavailable. The resulting PDF is opened in Chrome or the
system's default reader.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def make_pdf_from_png(png_path: Path, pdf_path: Path, title: str | None = None) -> None:
    """Create a single-page PDF containing ``png_path``."""
    if title is None:
        title = png_path.stem

    try:
        _make_pdf_with_library_backend(png_path, pdf_path, title)
        return
    except Exception as exc:  # noqa: BLE001
        # Last resort: zero-dependency minimal PDF embedding the PNG bytes.
        _make_pdf_zero_dependency(png_path, pdf_path, title)


def _image_size(png_path: Path) -> tuple[int, int]:
    """Read PNG IHDR width/height without third-party dependencies."""
    data = png_path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{png_path} is not a PNG")
    idx = 8
    while idx < len(data):
        length = int.from_bytes(data[idx : idx + 4], "big")
        chunk = data[idx + 4 : idx + 8]
        if chunk == b"IHDR":
            width = int.from_bytes(data[idx + 8 : idx + 12], "big")
            height = int.from_bytes(data[idx + 12 : idx + 16], "big")
            return width, height
        if chunk == b"IEND":
            break
        idx += 8 + length + 4
    raise ValueError(f"Could not read IHDR from {png_path}")


def _fit_a4(img_w_px: int, img_h_px: int) -> tuple[float, float, float, float]:
    """Return A4 page size and centered image size in mm, preserving aspect."""
    page_w_mm, page_h_mm = 210.0, 297.0
    margin_mm = 10.0
    max_w = page_w_mm - 2 * margin_mm
    max_h = page_h_mm - 2 * margin_mm
    aspect = img_h_px / img_w_px
    img_w_mm = max_w
    img_h_mm = img_w_mm * aspect
    if img_h_mm > max_h:
        img_h_mm = max_h
        img_w_mm = img_h_mm / aspect
    return page_w_mm, page_h_mm, img_w_mm, img_h_mm


def _pdf_catalog(pdf_len: int, xref_offset: int) -> bytes:
    return (
        b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
        b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
        b"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /XObject << /Im0 4 0 R >> >> /Contents 5 0 R >>\nendobj\n"
    )


def _make_pdf_zero_dependency(
    png_path: Path, pdf_path: Path, title: str
) -> None:
    """Minimal spec-1.4 PDF that embeds a PNG as an XObject without re-encoding."""
    img_w_px, img_h_px = _image_size(png_path)
    _, _, img_w_mm, img_h_mm = _fit_a4(img_w_px, img_h_px)

    # PDF units: 1 pt = 1/72 inch.
    page_w_pt, page_h_pt = 595.0, 842.0
    img_w_pt = img_w_mm * 72.0 / 25.4
    img_h_pt = img_h_mm * 72.0 / 25.4
    x = (page_w_pt - img_w_pt) / 2.0
    y = (page_h_pt - img_h_pt) / 2.0

    png_bytes = png_path.read_bytes()
    try:
        import zlib  # available in every CPython build
        compressed = zlib.compress(png_bytes)
    except Exception:  # noqa: BLE001
        compressed = png_bytes

    # Object 4: image XObject.
    image_obj = (
        b"4 0 obj\n<< /Type /XObject /Subtype /Image /Width "
        + str(img_w_px).encode()
        + b" /Height "
        + str(img_h_px).encode()
        + b" /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /Length "
        + str(len(compressed)).encode()
        + b" >>\nstream\n"
    )

    # Object 5: content stream painting the image.
    content = (
        f"q {img_w_pt:.3f} 0 0 {img_h_pt:.3f} {x:.3f} {y:.3f} cm /Im0 Do Q".encode()
    )
    try:
        content_z = zlib.compress(content)
    except Exception:  # noqa: BLE001
        content_z = content
    content_obj = (
        b"5 0 obj\n<< /Length "
        + str(len(content_z)).encode()
        + b" /Filter /FlateDecode >>\nstream\n"
    )

    header = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
    body = _pdf_catalog(0, 0)
    offsets: list[int] = []
    def put(obj: bytes) -> None:
        offsets.append(len(body) + len(header))
        body.extend(obj)

    body = bytearray()
    offsets.clear()
    # Re-build with proper offsets.
    pieces: list[bytes] = []

    catalog = _pdf_catalog(0, 0)
    pieces.append(catalog)
    pieces.append(image_obj + compressed + b"\nendobj\n")
    pieces.append(content_obj + content_z + b"\nendobj\n")

    xref_locations: list[int] = []
    full_body = bytearray()
    for piece in pieces:
        xref_locations.append(len(full_body))
        full_body.extend(piece)

    xref_offset = len(full_body)
    # 1-based objects; object 0 is free.
    xref = b"xref\n0 6\n0000000000 65535 f \n"
    for loc in xref_locations:
        xref += f"{loc:010d} 00000 n \n".encode()

    trailer = (
        b"trailer\n<< /Size 6 /Root 1 0 R /Info << /Title ("
        + title.encode("latin-1", "ignore")
        + b") >> >>\nstartxref\n"
        + str(xref_offset).encode()
        + b"\n%%EOF\n"
    )

    pdf_path.write_bytes(header + bytes(full_body) + xref + trailer)


def _make_pdf_with_library_backend(
    png_path: Path, pdf_path: Path, title: str
) -> None:
    """Prefer fpdf2, then reportlab; raise to trigger zero-dependency fallback."""
    try:
        from fpdf import FPDF  # type: ignore

        img_w_px, img_h_px = _image_size(png_path)
        page_w_mm, page_h_mm, img_w_mm, img_h_mm = _fit_a4(img_w_px, img_h_px)

        pdf = FPDF(unit="mm", format="A4")
        pdf.add_page()
        pdf.set_title(title)
        x = (page_w_mm - img_w_mm) / 2.0
        y = (page_h_mm - img_h_mm) / 2.0
        pdf.image(str(png_path), x=x, y=y, w=img_w_mm, h=img_h_mm)
        pdf.output(str(pdf_path), "F")
        return
    except Exception:  # noqa: BLE001
        pass

    try:
        from reportlab.lib.pagesizes import A4  # type: ignore
        from reportlab.lib.units import mm  # type: ignore
        from reportlab.pdfgen import canvas  # type: ignore

        page_w_mm, page_h_mm = A4[0] / mm, A4[1] / mm
        img_w_px, img_h_px = _image_size(png_path)
        _, _, img_w_mm, img_h_mm = _fit_a4(img_w_px, img_h_px)

        c = canvas.Canvas(str(pdf_path), pagesize=A4)
        c.setTitle(title)
        x = (page_w_mm - img_w_mm) / 2.0 * mm
        y = (page_h_mm - img_h_mm) / 2.0 * mm
        c.drawImage(str(png_path), x, y, width=img_w_mm * mm, height=img_h_mm * mm)
        c.save()
        return
    except Exception:  # noqa: BLE001
        pass

    raise RuntimeError("No PDF library backend")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert a PNG image into a single-page PDF."
    )
    parser.add_argument("png", type=Path, help="input PNG file")
    parser.add_argument(
        "--output", "-o", type=Path, default=None, help="output PDF path"
    )
    parser.add_argument("--title", "-t", default=None, help="PDF document title")
    args = parser.parse_args(argv)

    png_path = Path(args.png)
    if not png_path.is_file():
        print(f"error: PNG not found: {png_path}", file=sys.stderr)
        return 1

    pdf_path = Path(args.output) if args.output else png_path.with_suffix(".pdf")
    make_pdf_from_png(png_path, pdf_path, args.title)
    print(str(pdf_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
