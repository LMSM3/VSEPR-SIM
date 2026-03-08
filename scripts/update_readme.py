#!/usr/bin/env python3
"""Patch README.md Quick Start section to reference docs/USAGE.md."""
import os, textwrap

ROOT = "/mnt/c/Users/Liam/Desktop/vsepr-sim"
path = ROOT + "/README.md"
src = open(path).read()

old = """\
## Quick Start

### Full Verification (Recommended first step)
```bash
# Quick local run (Suites A-N, Q, I-M \u2014 no network)
wsl bash scripts/run_all_verification.sh

# Full run including PubChem/NIST cross-check (Suite P)
wsl bash scripts/run_all_verification.sh --full

# Verbose output
wsl bash scripts/run_all_verification.sh --verbose
```

### Build & Run
```bash
# WSL / Linux
cmake -B wsl-build -DBUILD_VIS=OFF -DBUILD_TESTS=ON
cmake --build wsl-build -j$(nproc)

# Run deep verification
./wsl-build/deep_verification --verbose

# Fetch empirical references (PubChem + NIST)
python3 scripts/fetch_empirical.py

# Interactive molecular builder
./wsl-build/atomistic-build
>> build H2O
>> info
>> save H2O.xyz
>> exit

# Energy minimisation
./wsl-build/atomistic-relax H2O.xyz

# Molecular dynamics
./wsl-build/atomistic-sim simulate H2O.xyz --temp 300 --steps 10000
```

### Windows (MSVC)
```cmd
build_automated.bat                   # Build + run all tests
build_automated.bat --clean           # Clean build + tests
```"""

assert old in src, "Pattern not found in README.md"

new = """\
## Quick Start

> **Full usage reference:** [`docs/USAGE.md`](docs/USAGE.md)

### Full Verification (recommended first step)
```bash
# Quick local run (Suites A-N, Q, I-M \u2014 no network, ~10 s)
wsl bash scripts/run_all_verification.sh

# Full run \u2014 also fetches PubChem/NIST and runs Suite P
wsl bash scripts/run_all_verification.sh --full

# Verbose: print each individual PASS/FAIL line
wsl bash scripts/run_all_verification.sh --full --verbose
```

### Build
```bash
# Engine + verification executables
wsl bash -c "cmake -B wsl-build -DBUILD_VIS=OFF -DBUILD_TESTS=ON ; cmake --build wsl-build -j4"

# Desktop application (requires Qt6)
wsl bash -c "cmake -B wsl-build-desktop -DBUILD_DESKTOP=ON -DBUILD_VIS=OFF ; cmake --build wsl-build-desktop -j4"
```

### Deep Verification
```bash
# Standard run (seed printed at end for reproduction)
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification"

# Reproduce the recorded Milestone A run exactly
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification --seed 1772952709"

# Stress-test: 2 000 samples per randomised suite
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification --rand-iters 2000 --verbose"
```

### Interactive molecular builder
```bash
./wsl-build/atomistic-build
```
```
>> build H2O
>> validate
>> save H2O.xyz
>> exit
```

### FIRE energy minimisation
```bash
./wsl-build/atomistic-relax molecule.xyz --force-tol 1e-5 --max-iter 5000 --output relaxed.xyza
```

### Molecular dynamics
```bash
./wsl-build/atomistic-sim md-nvt --temp 300 --steps 10000 molecule.xyz
./wsl-build/atomistic-sim energy molecule.xyz
```

### Desktop application
```bash
wsl bash -c "./wsl-build-desktop/vsepr-desktop"
```

Console quick-reference (type at the `>` prompt \u2014 full list at [`docs/USAGE.md \u00a7 12`](docs/USAGE.md#12-desktop--console-command-reference)):

| Command | Action |
|---------|--------|
| `relax` | FIRE minimisation (F5) |
| `md` | NVT Langevin MD 300 K (F6) |
| `sp` | Single-point energy (F4) |
| `crystal` | Crystal Preset dialog (Ctrl+K) |
| `bs` / `cpk` / `sticks` / `wire` | Render mode |
| `labels` / `axes` / `box` | Toggle overlays |
| `play` / `stop` / `next` / `prev` | Trajectory playback |
| `fit` / `reset` | Camera |

### Fetch empirical references
```bash
wsl python3 scripts/fetch_empirical.py           # live PubChem + NIST
wsl python3 scripts/fetch_empirical.py --offline   # bundled defaults only
```"""

src = src.replace(old, new, 1)
open(path, 'w', newline='\n').write(src)
print(f"README updated ({len(src.splitlines())} lines)")
