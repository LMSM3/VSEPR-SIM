#!/usr/bin/env python3
"""Create deterministic GR-1 Excel and PNG artifacts without third-party packages."""
import argparse
import csv
import json
import math
import struct
import zlib
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

W, H = 1200, 720
PALETTE = [(35, 92, 160), (219, 83, 69), (46, 125, 50), (245, 166, 35), (120, 80, 160)]


def load(run_dir):
    run_dir = Path(run_dir)
    with (run_dir / "manifest.json").open(encoding="utf-8") as f:
        manifest = json.load(f)
    with (run_dir / "diagnostics.csv").open(newline="", encoding="utf-8") as f:
        diagnostics = list(csv.DictReader(f))
    with (run_dir / "trajectory.jsonl").open(encoding="utf-8") as f:
        frames = [json.loads(line) for line in f if line.strip()]
    return manifest, diagnostics, frames


def column_name(index):
    result = ""
    while index:
        index, remainder = divmod(index - 1, 26)
        result = chr(65 + remainder) + result
    return result


def cell(ref, value, style=""):
    style_attr = f' s="{style}"' if style else ""
    if isinstance(value, bool):
        return f'<c r="{ref}"{style_attr} t="b"><v>{int(value)}</v></c>'
    if isinstance(value, (int, float)) and math.isfinite(value):
        return f'<c r="{ref}"{style_attr}><v>{value}</v></c>'
    return f'<c r="{ref}"{style_attr} t="inlineStr"><is><t>{escape(str(value))}</t></is></c>'


def worksheet(rows):
    xml = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>']
    for r, row in enumerate(rows, 1):
        xml.append(f'<row r="{r}">')
        for c, value in enumerate(row, 1):
            xml.append(cell(f'{column_name(c)}{r}', value, "1" if r == 1 else ""))
        xml.append('</row>')
    xml.append('</sheetData><autoFilter ref="A1:{}{}"/><sheetViews><sheetView workbookViewId="0"/></sheetViews></worksheet>'.format(column_name(max(map(len, rows))), len(rows)))
    return "".join(xml)


def write_xlsx(path, manifest, diagnostics, frames):
    headings = list(diagnostics[0])
    diag_rows = [headings] + [[row[key] == "1" if key == "liquid" else float(row[key]) for key in headings] for row in diagnostics]
    summary = [["GR-1 post-render report", manifest["run_id"]], ["Scenario", manifest["scenario_name"]], ["Technique", manifest["resolved_configuration"].get("technique", "unknown")], ["Result", manifest["final_result"]], ["Trajectory hash", manifest["trajectory_hash"]], ["Final-state hash", manifest["final_state_hash"]], ["Samples", len(diagnostics)], ["Frames", len(frames)]]
    frame_rows = [["sample", "step", "time", "particle_count"]] + [[index, frame["step"], frame["time"], len(frame["particles"])] for index, frame in enumerate(frames)]
    particles = [["particle", "x", "y", "z", "vx", "vy", "vz", "cluster_id"]]
    for index, particle in enumerate(frames[-1]["particles"]):
        particles.append([index, *particle["position"], *particle["velocity"], particle["cluster_id"]])
    sheets = [("Summary", summary), ("Diagnostics", diag_rows), ("Frames", frame_rows), ("Final particles", particles)]
    content_types = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>']
    workbook_sheets, relationships = [], []
    for index, (name, _) in enumerate(sheets, 1):
        content_types.append(f'<Override PartName="/xl/worksheets/sheet{index}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>')
        workbook_sheets.append(f'<sheet name="{escape(name)}" sheetId="{index}" r:id="rId{index}"/>')
        relationships.append(f'<Relationship Id="rId{index}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{index}.xml"/>')
    content_types.append('</Types>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as xlsx:
        xlsx.writestr('[Content_Types].xml', ''.join(content_types))
        xlsx.writestr('_rels/.rels', '<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>')
        xlsx.writestr('xl/workbook.xml', '<?xml version="1.0" encoding="UTF-8"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>' + ''.join(workbook_sheets) + '</sheets></workbook>')
        xlsx.writestr('xl/_rels/workbook.xml.rels', '<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' + ''.join(relationships) + '<Relationship Id="rId%d" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>' % (len(sheets) + 1))
        xlsx.writestr('xl/styles.xml', '<?xml version="1.0" encoding="UTF-8"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="11"/><name val="Calibri"/></font></fonts><fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill></fills><borders count="1"><border/></borders><cellStyleXfs count="1"><xf/></cellStyleXfs><cellXfs count="2"><xf xfId="0"/><xf xfId="0" fontId="1" applyFont="1"/></cellXfs></styleSheet>')
        for index, (_, rows) in enumerate(sheets, 1):
            xlsx.writestr(f'xl/worksheets/sheet{index}.xml', worksheet(rows))


def canvas(): return bytearray([255, 255, 255] * W * H)
def pixel(img, x, y, color):
    if 0 <= x < W and 0 <= y < H:
        pos = (y * W + x) * 3
        img[pos:pos + 3] = bytes(color)

def line(img, x0, y0, x1, y1, color):
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    for step in range(steps + 1): pixel(img, round(x0 + (x1 - x0) * step / steps), round(y0 + (y1 - y0) * step / steps), color)
def disc(img, x, y, radius, color):
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if dx * dx + dy * dy <= radius * radius: pixel(img, x + dx, y + dy, color)
def png(path, img):
    raw = b''.join(b'\0' + bytes(img[row * W * 3:(row + 1) * W * 3]) for row in range(H))
    chunk = lambda kind, data: struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
def chart(path, diagnostics, fields):
    img = canvas(); left, top, right, bottom = 80, 50, W - 50, H - 70
    line(img, left, bottom, right, bottom, (30, 30, 30)); line(img, left, top, left, bottom, (30, 30, 30))
    for field, color in zip(fields, PALETTE):
        values, last_finite = [], 0.0
        for row in diagnostics:
            value = float(row[field])
            if math.isfinite(value): last_finite = value
            values.append(last_finite)
        lo, hi = min(values), max(values); span = max(hi - lo, 1e-12)
        for index in range(1, len(values)): line(img, left + (right-left)*(index-1)//(len(values)-1), bottom - round((values[index-1]-lo)/span*(bottom-top)), left + (right-left)*index//(len(values)-1), bottom - round((values[index]-lo)/span*(bottom-top)), color)
    png(path, img)
def frame_png(path, frame):
    img = canvas(); points = [p["position"] for p in frame["particles"]]; lo = min(min(p[0], p[1]) for p in points); hi = max(max(p[0], p[1]) for p in points); span = max(hi-lo, 1e-12)
    for particle in frame["particles"]:
        x, y, _ = particle["position"]; color = PALETTE[particle["cluster_id"] % len(PALETTE)]
        disc(img, 70 + round((x-lo)/span*(W-140)), 50 + round((y-lo)/span*(H-100)), 6, color)
    png(path, img)

def main():
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("run_directory", type=Path); parser.add_argument("--output", type=Path); args = parser.parse_args()
    manifest, diagnostics, frames = load(args.run_directory); output = args.output or args.run_directory / "post_render"; output.mkdir(parents=True, exist_ok=True)
    write_xlsx(output / "gr1_report.xlsx", manifest, diagnostics, frames)
    chart(output / "energy_temperature.png", diagnostics, ["total", "temperature"])
    chart(output / "phase_cluster.png", diagnostics, ["local_density", "dominant_cluster_fraction", "dispersion"])
    chart(output / "pressure_stability.png", diagnostics, ["pressure", "pressure_spread", "temperature_spread"])
    frame_png(output / "final_frame_xy.png", frames[-1])
    print(f"Wrote {output / 'gr1_report.xlsx'} and 4 PNG post-renders")

if __name__ == "__main__": main()
