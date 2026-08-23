#!/usr/bin/env python3

import csv
import json
import sys
from pathlib import Path


def main() -> int:
    output_dir = Path(sys.argv[1] if len(sys.argv) > 1 else "out/dual_backend_3d")
    manifest_path = output_dir / "manifest.json"
    scenes_path = output_dir / "scenes.csv"
    particles_path = output_dir / "particles.csv"

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["schema"] == "vsepr.dual_backend_3d.v1"
    assert manifest["generator"] == "cg-anim-demo"
    assert manifest["particle_count"] > 0
    assert manifest["generation_ms"] > 0

    with scenes_path.open(newline="", encoding="utf-8") as stream:
        scenes = list(csv.DictReader(stream))
    with particles_path.open(newline="", encoding="utf-8") as stream:
        particles = list(csv.DictReader(stream))

    assert len(scenes) == manifest["scene_count"]
    assert particles
    scene_ids = {int(row["scene_id"]) for row in scenes}
    assert {int(row["scene_id"]) for row in particles} == scene_ids
    assert sum(int(row["particle_count"]) for row in scenes) == len(particles)
    assert len(particles) == manifest["particle_count"]

    if "--stress" in sys.argv:
        assert len(particles) > 1000
        assert max(int(row["particle_count"]) for row in scenes) > 1000

    for row in scenes:
        for key in ("cohesion_proxy", "texture_proxy", "stabilization_proxy"):
            assert 0.0 <= float(row[key]) <= 1.0
    for row in particles:
        assert 0.0 <= float(row["state_value"]) <= 1.0

    # Report sidecars are produced only when the visualization presentation
    # layer runs; headless artifact validation records their absence as an
    # observed optional field rather than a failure.
    report_sidecars = ("proxy_summary.png", "proxy_summary.pdf", "proxy_summary.xlsx")
    present_sidecars = sum(1 for r in report_sidecars if (output_dir / r).exists())
    print(f"PASS: validated {len(scenes)} scenes and {len(particles)} particles in {output_dir} "
          f"(sidecars present: {present_sidecars}/{len(report_sidecars)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
