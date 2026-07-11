# VSEPR-Sim Quick Build Guide

**Version:** 5.0.0  
**Last Updated:** 2026  

---

## Prerequisites

| Requirement | Minimum |
|-------------|---------|
| C++ Compiler | GCC 9+, Clang 12+, or MSVC 2019+ (C++20) |
| CMake | 3.15+ |
| Python | 3.8+ (for `vsepr_xyz_popup.pyw` fallback viewer) |
| Inno Setup | 6+ (Windows installer only) |

---

## Build (WSL / Linux — Recommended)

```bash
# 1. Clone and enter the repo
git clone https://github.com/LMSM3/VSEPR-SIM
cd VSEPR-SIM

# 2. Configure (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 3. Build the kernel + CLI tools
cmake --build build --parallel

# 4. Verify
./build/vsepr.exe --version
```

**Result:** Executables land in `build/`.

---

## Installed Directory Layout

After running the Windows installer (`installer/output/vsepr-sim-5.0.0-setup.exe`),
the install root (`%ProgramFiles%\VSEPR-SIM` or user-chosen location) contains:

```
VSEPR-SIM\
├── bin\
│   ├── vsepr.exe              ← main kernel entry point
│   ├── vsepr-cli.exe          ← command-line interface
│   ├── vsepr_batch.exe        ← batch / scripted runs
│   ├── vsepr_live.exe         ← live-session runner
│   ├── vsepr-entry.exe        ← lightweight entry-point launcher
│   ├── vsepr-ufx.exe          ← UFX/UFF forcefield driver
│   ├── atomistic-sim.exe      ← atomistic simulation engine
│   ├── atomistic-relax.exe    ← energy relaxation
│   ├── atomistic-align.exe    ← structure alignment
│   ├── atomistic-discover.exe ← reaction discovery
│   ├── open_vsim_file.cmd     ← universal file-type opener (shell target)
│   └── vsepr_xyz_popup.pyw    ← Python coordinate popup (fallback viewer)
├── include\vsim\              ← VSIM parser + SDK headers
├── data\                      ← built-in reference data
├── scripts\                   ← utility scripts
├── docs\                      ← documentation
├── resources\
│   └── vsepr.ico
├── installer\
│   └── register-file-associations.ps1
├── README.md
├── VSIM_REFERENCE.md
└── LICENSE
```

---

## Available Binaries

### Kernel / Simulation

| Binary | Purpose |
|--------|---------|
| `vsepr.exe` | Primary kernel entry point — run, inspect, validate |
| `vsepr-cli.exe` | Interactive CLI interface |
| `vsepr_batch.exe` | Scripted / batch simulation runner |
| `vsepr_live.exe` | Live/streaming session mode |
| `vsepr-entry.exe` | Lightweight entry-point launcher |
| `vsepr-ufx.exe` | UFF/UFX forcefield driver |
| `atomistic-sim.exe` | Atomistic simulation engine |
| `atomistic-relax.exe` | Energy minimization / relaxation |
| `atomistic-align.exe` | Structure alignment |
| `atomistic-discover.exe` | Reaction pathway discovery |

### File Integration (Shell / Viewer)

| File | Purpose |
|------|---------|
| `open_vsim_file.cmd` | Universal opener — priority: `vsepr.exe open` → `vsepr-cli.exe open` → Python popup |
| `vsepr_xyz_popup.pyw` | Python coordinate popup (fallback when no GPU binary available) |

---

## File Associations (Windows)

The installer registers VSIM/XYZ file types via:

```
installer\register-file-associations.ps1
```

This runs post-install under HKCU (no admin required).

| Extension | Behavior |
|-----------|----------|
| `.vsim` | Double-click → `vsepr.exe run` |
| `.xyzFull` / `.xyzfull` | Double-click → priority opener (replay viewer) |
| `.vsxyz` | Double-click → priority opener (coordinate viewer) |
| `.xyza` / `.xyzA` | Double-click → priority opener |
| `.xyzc` | Double-click → priority opener (checkpoint) |
| `.xyzf` / `.xyzF` | Double-click → priority opener (trajectory) |
| `.xyz` | Right-click context menu only — **default handler unchanged** |

To register manually (or re-register after repair):

```powershell
# From the install root:
powershell -ExecutionPolicy Bypass -File installer\register-file-associations.ps1 -BinaryPath bin\vsepr.exe

# To unregister:
powershell -ExecutionPolicy Bypass -File installer\register-file-associations.ps1 -Unregister
```

---

## Building the Windows Installer

```powershell
# 1. Build the project in Release mode first
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 2. Compile the installer (requires Inno Setup 6+)
iscc installer\setup.iss

# Output: installer\output\vsepr-sim-5.0.0-setup.exe
```

---

## Quick Commands

```powershell
# Run the kernel
.\build\vsepr.exe --version

# Inspect a VSIM file
.\build\vsepr.exe inspect path\to\structure.vsim

# Run a VSIM script
.\build\vsepr.exe run path\to\script.vsim

# Open an XYZ coordinate file
.\build\vsepr.exe open path\to\structure.xyz

# Batch simulation
.\build\vsepr_batch.exe --input runs.vsim --output results\

# Clean rebuild
Remove-Item -Recurse -Force build; cmake -B build -S .; cmake --build build --parallel
```

---

## Troubleshooting

### Binary not found after install

Check that `%ProgramFiles%\VSEPR-SIM\bin` (or your custom install path) is in PATH,
or use the full path explicitly.

### File associations not working

Re-run the association script from the install root:
```powershell
powershell -ExecutionPolicy Bypass -File installer\register-file-associations.ps1 -BinaryPath bin\vsepr.exe
```
Then log out / log in, or run `ie4uinit.exe -show` to refresh the shell.

### CMake can't find compiler

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake

# MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

### Python popup doesn't open

Ensure `pythonw.exe` is on PATH (standard Python 3 install includes it).  
`vsepr_xyz_popup.pyw` is a fallback only — if `vsepr.exe` is installed, it takes priority.

---

## Platform Notes

| Platform | Status | Notes |
|----------|--------|-------|
| Windows 10/11 x64 | ✅ Primary | Full installer + file associations |
| WSL (Ubuntu) | ✅ Supported | Build + CLI; no installer integration |
| Linux native | ✅ Supported | Build + CLI; no installer |
| macOS | ⚠️ Partial | Build works; no installer; OpenGL deprecated |

---

## Legacy / Experimental Directories

The following directories exist for historical reference and are **not part of the active build**:

| Directory | Status |
|-----------|--------|
| `legacy/` | Pre-v4 C API — superseded by C++ kernel |
| `archive/` | Build system snapshots — frozen reference |
| `bridge_beta/` | Next-gen bridge scaffold — experimental, not wired |
| `v4/` | v4 scoring/formation era — absorbed into v5 |
| `v5/` | Early v5 prototype demos — production kernel is in `src/` |

Each directory has a `README.md` explaining its status.  
See `CONTRIBUTING-DEPRECATION.md` for the full deprecation policy.
