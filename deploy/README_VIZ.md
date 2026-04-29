# 3D Live Rendering — Quick Start

## Architecture

```
WSL (AlmaLinux-10, Linux ELF — no Device Guard)
    └── vsepr viz Ar -T 300
          ├── :9999  NDJSON → Atomic View   (15 fps)
          └── :10001 NDJSON → Analysis View  (2 fps)

WSL2 auto-forwards localhost ports to Windows host

Windows
    ├── python tools/viz_atomic.py   → Port 9999  3D atomic scene
    └── python tools/viz_analysis.py → Port 10001 graphs + properties

nginx (optional)
    Stream proxy in WSL for port management / logging
    Config: deploy/nginx-viz.conf
```

---

## One-time WSL build

```powershell
# From PowerShell (runs once, binary stays at build-linux/vsepr)
wsl -d AlmaLinux-10 -- bash /mnt/c/R/VSPER-SIM/deploy/build_wsl.sh
```

---

## Launch everything (one command)

```powershell
# From C:\R\VSPER-SIM in PowerShell
.\deploy\launch_viz.ps1

# With options
.\deploy\launch_viz.ps1 -Formula N2 -Args "-T 400 -N 125 --verbose"
.\deploy\launch_viz.ps1 -Formula CO2 -Args "--T-start 200 --T-end 800 -N 64"
```

This opens three windows:
1. **WSL terminal** — vsepr viz server (AlmaLinux, no Device Guard)
2. **Atomic Viewer** — `tools/viz_atomic.py` on port 9999
3. **Analysis Viewer** — `tools/viz_analysis.py` on port 10001

---

## Manual launch (three terminals)

**Terminal 1 — WSL server:**
```bash
wsl -d AlmaLinux-10 -- bash /mnt/c/R/VSPER-SIM/deploy/start_viz_server.sh Ar -T 300 -N 64 --verbose
```

**Terminal 2 — Atomic Viewer (Windows):**
```powershell
python tools\viz_atomic.py
```

**Terminal 3 — Analysis Viewer (Windows):**
```powershell
python tools\viz_analysis.py
```

---

## Continuous random sims (scaled compute)

```powershell
# Spawn 4 parallel workers with randomised species/T/N each
python scripts\run_sims.py --workers 4
```

Each worker runs `vsepr viz` inside WSL with a random formula and temperature.
Failed or completed workers auto-restart.

---

## nginx TCP proxy (optional)

nginx adds logging and clean port management if you want it.

```bash
# Inside WSL
sudo cp /mnt/c/R/VSPER-SIM/deploy/nginx-viz.conf /etc/nginx/conf.d/viz.conf
sudo nginx -t && sudo systemctl start nginx
```

Then launch with the `--nginx` flag:
```powershell
.\deploy\launch_viz.ps1 -Formula Ar -Args "-T 300 --nginx"
```

---

## Viewer controls

### Atomic Viewer (port 9999)
| Key | Action |
|-----|--------|
| Mouse drag | Rotate |
| Scroll | Zoom |
| R | Reset view |
| B | Toggle bonds |
| L | Toggle lattice |
| C | Cycle colour mode (element / energy / velocity / defect / χ) |
| Space | Pause/resume |
| Q / Esc | Quit |

### Analysis Viewer (port 10001)
Auto-updating 2×3 panel grid:
- E/T/ρ timeline · f(v) speed distribution · 3D auto-rotating crystal
- Macro-property bars · EOS candidate comparison · KE histogram + crystal metrics

---

## Ports summary

| Port  | View             | Rate   | Content |
|-------|-----------------|--------|---------|
| 9999  | Atomic View      | 15 fps | Atom positions, bonds, lattice, HUD |
| 10001 | Analysis View    |  2 fps | History graphs, properties, candidates |
