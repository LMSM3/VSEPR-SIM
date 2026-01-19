# VSEPR-Sim Python Orchestrator

**One command = one outcome. No notebooks required.**

## Philosophy

The orchestrator does exactly three jobs:

1. **Run external tools** (bash, cmake, g++, chromium, etc.)
2. **Call C++ binaries** (CLI entrypoints, treated as black boxes)
3. **Manage artifacts** (outputs, logs, exports, packaging)

### What Python Owns
- Orchestration
- Inputs/outputs  
- Packaging
- Reporting
- Automation

### What C++ Owns (The Authoritative Compute)
- Simulation
- Geometry optimization
- Heavy math
- Performance-critical logic

### What Bash Owns
- Tool glue (when needed)
- System command composition

---

## Architecture

```
tools/
├── flower.py       # Single CLI entrypoint
├── runner.py       # Subprocess wrapper, environment control
├── targets.py      # Build/test/run tasks (the verbs)
├── artifacts.py    # Copy/export/zip management
└── config.py       # Paths and defaults (single source of truth)
```

### The Stack

```
Python (task brain) 
  ↓
Bash (tool glue)
  ↓
C++ (compute) + OpenGL (view) + HTML (export)
```

---

## Quick Start

### CLI Usage

```bash
# Build everything
python -m tools.flower build

# Run tests
python -m tools.flower test

# Build a molecule
python -m tools.flower run H2O --xyz

# Generate visualization
python -m tools.flower viz H2O

# Export in multiple formats
python -m tools.flower export H2O --formats xyz html

# Create analysis package
python -m tools.flower report H2O

# Clean up
python -m tools.flower clean
```

### API Usage (for Jupyter or scripts)

```python
from tools import build, run, viz, export, report

# Build binaries
build()

# Create molecule
xyz_file = run("H2O", optimize=True)

# Generate viewer
html_file = viz("H2O", open_browser=True)

# Export everything
export("H2O", formats=["xyz", "html", "json"])

# Package results
report("H2O", format="html")
```

---

## The Verbs

Every command is a verb. Each verb does one thing well.

### `build` - Build C++ binaries

```bash
python -m tools.flower build
python -m tools.flower build --clean  # Clean first
python -m tools.flower build -v       # Verbose
```

**What it does:**
1. Runs CMake configuration
2. Compiles C++ with 4 parallel jobs
3. Creates binaries in `build/bin/`

**Logs to:** `logs/YYYY-MM-DD/build.log`

---

### `test` - Run tests

```bash
python -m tools.flower test              # All tests
python -m tools.flower test pbc*         # Pattern match
python -m tools.flower test chemistry*   # Specific suite
```

**What it does:**
1. Runs CTest on built binaries
2. Shows pass/fail summary
3. Outputs details on failure

**Logs to:** `logs/YYYY-MM-DD/test.log`

---

### `run` - Build molecule (calls C++ CLI)

```bash
python -m tools.flower run H2O --xyz
python -m tools.flower run CH4 --no-optimize
python -m tools.flower run "NH3" --xyz -v
```

**What it does:**
1. Calls `./build/bin/vsepr build <formula>`
2. Runs geometry optimization (unless `--no-optimize`)
3. Exports XYZ file to `out/<formula>.xyz`

**Logs to:** `logs/YYYY-MM-DD/mol_<formula>.log`

**Returns:** Path to XYZ file (or None on failure)

---

### `viz` - Generate HTML visualization

```bash
python -m tools.flower viz H2O
python -m tools.flower viz H2O --no-open    # Don't auto-open
python -m tools.flower viz water.xyz        # From file
```

**What it does:**
1. Finds XYZ file (from `run` or specified path)
2. Calls `scripts/viewer_generator.py`
3. Creates interactive Three.js HTML
4. Auto-opens in browser (unless `--no-open`)

**Logs to:** `logs/YYYY-MM-DD/viz_<molecule>.log`

**Output:** `out/<molecule>_viewer.html`

---

### `export` - Export in multiple formats

```bash
python -m tools.flower export H2O
python -m tools.flower export H2O --formats xyz html json
```

**What it does:**
1. Collects all outputs for molecule
2. Copies to standardized locations
3. Generates missing formats

**Supported formats:**
- `xyz` - Geometry file
- `html` - Interactive viewer
- `json` - Metadata (planned)
- `png` - Static image (planned)

**Output:** Files in `out/`

---

### `report` - Create analysis package

```bash
python -m tools.flower report H2O
python -m tools.flower report H2O --format pdf
```

**What it does:**
1. Gathers all outputs for molecule
2. Packages into zip archive
3. Creates summary (format-dependent)

**Output:** `out/<molecule>_results.zip`

---

### `clean` - Remove artifacts

```bash
python -m tools.flower clean              # Everything
python -m tools.flower clean build        # Just build/
python -m tools.flower clean out          # Just outputs
python -m tools.flower clean logs         # Just logs
```

**What it removes:**
- `all`: build/ + out/ + logs/
- `build`: CMake cache and compiled binaries
- `out`: Generated outputs
- `logs`: All log files

---

## File Organization

```
vsepr-sim/
├── build/              # CMake build artifacts
│   └── bin/           # C++ executables (authoritative)
│       ├── vsepr      # Main CLI
│       ├── vsepr-view # OpenGL viewer
│       └── vsepr_batch
│
├── out/               # Python orchestrator outputs
│   ├── H2O.xyz
│   ├── H2O_viewer.html
│   └── H2O_results.zip
│
├── logs/              # All logs organized by date
│   └── 2026-01-18/
│       ├── build.log
│       ├── mol_H2O.log
│       └── viz_H2O.log
│
└── tools/             # Python orchestrator
    ├── flower.py      # CLI entrypoint
    ├── runner.py      # Command execution
    ├── targets.py     # Task implementations
    ├── artifacts.py   # File management
    └── config.py      # Configuration
```

---

## Logging

Every command logs to `logs/YYYY-MM-DD/<task>.log`

**Log format:**
```
=== build started at 2026-01-18 14:32:01 ===

[14:32:01] CMD: cmake -B build -S .
[14:32:01] CWD: /path/to/vsepr-sim
[14:32:03] STDOUT:
-- Configuring done
-- Generating done
[14:32:03] EXIT: 0 (2.31s)

[14:32:03] CMD: cmake --build build -j4
...
```

**Console output:**
```
→ Configuring CMake...
✓ build (2.3s)
→ Building C++ binaries...
✓ build (45.7s)
```

---

## Status Symbols

Because apparently humans like visual feedback:

- `✓` - Success
- `✗` - Failure  
- `→` - Action starting
- `•` - Info/detail

---

## Integration Examples

### Jupyter Notebook

```python
# Cell 1: Setup
from tools import build, run, viz

# Cell 2: Build project
build(verbose=True)

# Cell 3: Create molecules
molecules = ["H2O", "CH4", "NH3"]
for mol in molecules:
    xyz = run(mol, optimize=True)
    viz(mol, open_browser=False)

# Cell 4: Display in notebook
from IPython.display import IFrame
IFrame("out/H2O_viewer.html", width=800, height=600)
```

### Bash Script

```bash
#!/bin/bash
# Batch process molecules

python -m tools.flower build || exit 1

for mol in H2O CH4 NH3 CO2; do
    python -m tools.flower run "$mol" --xyz || continue
    python -m tools.flower viz "$mol" --no-open
    python -m tools.flower report "$mol"
done

echo "✓ All molecules processed"
```

### CI/CD Pipeline

```yaml
# .github/workflows/test.yml
- name: Build
  run: python -m tools.flower build

- name: Test
  run: python -m tools.flower test

- name: Build samples
  run: |
    python -m tools.flower run H2O --xyz
    python -m tools.flower run CH4 --xyz

- name: Archive outputs
  uses: actions/upload-artifact@v3
  with:
    name: molecules
    path: out/
```

---

## Error Handling

Every command returns nonzero on failure:

```bash
python -m tools.flower run InvalidFormula
# Output:
# → Building InvalidFormula...
# ✗ mol_InvalidFormula failed (exit 1)
# Exit code: 1

echo $?  # Returns 1
```

**Exit codes:**
- `0` - Success
- `1` - Task failed
- `130` - Interrupted (Ctrl+C)

---

## Extending the Orchestrator

### Adding a New Verb

1. **Define function in `targets.py`:**

```python
def my_verb(arg1: str, arg2: bool = False, verbose: bool = False) -> bool:
    """Do something useful"""
    runner = Runner("my_verb", verbose=verbose)
    result = runner.bash("echo 'Hello'")
    return result.success
```

2. **Export in `__init__.py`:**

```python
from .targets import my_verb

__all__ = ['build', 'test', ..., 'my_verb']
```

3. **Add CLI parser in `flower.py`:**

```python
p_myverb = subparsers.add_parser('my-verb', help='My new command')
p_myverb.add_argument('arg1', help='First argument')
p_myverb.add_argument('--arg2', action='store_true')
```

4. **Add dispatch:**

```python
elif args.verb == 'my-verb':
    success = targets.my_verb(
        arg1=args.arg1,
        arg2=args.arg2,
        verbose=args.verbose
    )
```

---

## Design Principles

### 1. Python Never Owns Physics

C++ is authoritative. Python just orchestrates.

**Good:**
```python
# Python calls C++ binary
result = runner.run(["./build/bin/vsepr", "build", "H2O"])
```

**Bad:**
```python
# Python reimplements optimization
# NO. Just no.
```

### 2. Black Box Commands

Treat C++ binaries as black boxes:
- Pass arguments
- Capture stdout/stderr
- Parse output if needed
- Don't peek inside

### 3. One Command = One Outcome

Each verb should:
- Do exactly one thing
- Write to `out/`
- Log to `logs/`
- Return 0 or nonzero

### 4. Separation of Concerns

| Layer | Responsibility |
|-------|----------------|
| Python | Orchestration, I/O, packaging |
| Bash | System tool glue |
| C++ | Compute, optimization, physics |
| OpenGL | Native visualization |
| HTML | Web export |

### 5. No Hidden Dependencies

- CMake builds C++
- Python calls binaries
- Viewer uses Three.js CDN
- No pip/npm black holes

---

## Comparison: Before vs. After

### Before (Chaos)

```bash
# Build... somehow
mkdir build && cd build && cmake .. && make
cd ..

# Run... maybe?
./build/bin/molecule_builder H2O > out.txt

# Visualize... where's the script?
find . -name "*viewer*"
python scripts/viewer_generator.py out.xyz --open

# Export... manually
cp out.xyz results/
cp out_viewer.html results/

# Logs... everywhere
grep ERROR *.log build/*.log scripts/*.log
```

### After (Sanity)

```bash
python -m tools.flower build
python -m tools.flower run H2O --xyz
python -m tools.flower viz H2O
python -m tools.flower report H2O

# Logs in one place
cat logs/2026-01-18/*.log
```

---

## Future Enhancements

### Planned Features

- **PDF reports** (via LaTeX or headless Chrome)
- **PNG exports** (via headless OpenGL or Three.js screenshot)
- **JSON metadata** (truth_state.json from C++)
- **Batch processing** (parallel molecule building)
- **Watch mode** (auto-rebuild on file changes)

### Optional: Local HTML Generator

Currently: Python → `viewer_generator.py` → Three.js HTML

Future: C++ can embed HTML generation:

```cpp
// In C++
class HTMLGenerator {
    std::string generate(const Molecule& mol);
    // No Python dependency
};
```

Benefit: One less external dependency, faster generation.

---

## Troubleshooting

### "C++ binary not found"

```bash
python -m tools.flower build
# Then try again
```

### "Python module not found"

```bash
cd /path/to/vsepr-sim
python -m tools.flower <verb>
# Run from project root
```

### "Command timeout"

Increase timeout in `runner.py`:

```python
result = subprocess.run(..., timeout=600)  # 10 minutes
```

### "Logs filling up disk"

```bash
python -m tools.flower clean logs
```

---

## Summary

You now have a **single entrypoint** that turns chaos into:

```
py run build
py run test  
py run mol H2O --xyz
py run viz H2O
py run report H2O --pdf
```

**No notebooks required.** Jupyter can use the same API if desired.

The orchestrator is **shaped correctly**:
- Verbs, not framework weirdness
- Logs in one place
- C++ stays authoritative
- Python stays in its lane

**One command = one outcome = emotional stability.**

---

**Status:** Ready to deploy  
**Next:** Test with real molecules, iterate on UX  
**Philosophy:** Don't reinvent build systems. Just glue things that work.
