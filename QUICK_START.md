# VSEPR-Sim Quick Start

## The Orchestrator is Live! 🌸

Single command interface for all VSEPR operations.

## Basic Usage

All commands follow this pattern:
```bash
wsl -- bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && python3 -m tools.flower <verb> [args]"
```

Or create an alias:
```bash
# Add to ~/.bashrc or PowerShell profile
alias flower='wsl -- bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && python3 -m tools.flower"'
```

Then use simply:
```bash
flower build
flower viz test_molecule.xyz
flower clean
```

## Available Commands

### Build C++ Binaries
```bash
python3 -m tools.flower build
```
Runs CMake configure + make. Logs to `logs/YYYY-MM-DD/build.log`

### Run Tests
```bash
python3 -m tools.flower test
```
Executes CTest suite. Logs to `logs/YYYY-MM-DD/test.log`

### Build Molecules
```bash
# Using C++ CLI (when vsepr binary works)
python3 -m tools.flower run H2O --xyz

# Using WSL bash wrapper (current workaround)
python3 -m tools.flower run H2O --wsl
```
Output goes to `out/H2O.xyz`

### Generate Visualizations
```bash
python3 -m tools.flower viz test_molecule.xyz
```
Creates `test_molecule_viewer.html` with Three.js interactive viewer

### Export Data
```bash
python3 -m tools.flower export H2O
```
Exports multiple formats (XYZ, JSON, etc.) to `out/`

### Generate Reports
```bash
python3 -m tools.flower report H2O --format html
```
Packages all results into ZIP archive

### Clean Everything
```bash
python3 -m tools.flower clean
```
Removes `build/`, `out/`, `logs/` directories

## Verified Working Examples

### 1. Visualization Test (✓ WORKS)
```bash
wsl -- bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && python3 -m tools.flower viz test_molecule.xyz"
```

Output:
```
→ Generating HTML viewer...
✓ viz_test_molecule.xyz (0.1s)
✓ Viewer: test_molecule_viewer.html
```

Log created: `logs/2026-01-18/viz_test_molecule.xyz.log`

### 2. Help System (✓ WORKS)
```bash
wsl -- bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && python3 -m tools.flower --help"
```

Shows all available verbs and examples.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Python Orchestrator (tools/flower.py)                       │
│ - CLI parsing (argparse)                                    │
│ - Verb dispatch                                             │
│ - Error handling                                            │
└─────────────────────────────────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ Runner       │  │ Targets      │  │ Artifacts    │
│ - Subprocess │  │ - build()    │  │ - copy()     │
│ - Logging    │  │ - test()     │  │ - export()   │
│ - Timeouts   │  │ - run()      │  │ - package()  │
│              │  │ - viz()      │  │ - clean()    │
└──────────────┘  └──────────────┘  └──────────────┘
        │                 │                 │
        └─────────────────┼─────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ C++ Binaries │  │ Bash Scripts │  │ Python Tools │
│ - vsepr      │  │ - build.sh   │  │ - viewer_gen │
│ - vsepr-view │  │ - run.sh     │  │              │
└──────────────┘  └──────────────┘  └──────────────┘
```

## File Organization

```
vsepr-sim/
├── tools/               # Orchestrator system
│   ├── flower.py        # Main CLI entrypoint
│   ├── runner.py        # Subprocess wrapper
│   ├── targets.py       # Verb implementations
│   ├── artifacts.py     # File management
│   └── config.py        # Paths and defaults
│
├── logs/                # All execution logs
│   └── YYYY-MM-DD/      # Date-organized logs
│       ├── build.log
│       ├── test.log
│       └── viz_*.log
│
├── out/                 # All outputs
│   ├── *.xyz            # Molecule files
│   ├── *.html           # Viewers
│   └── reports/         # Packaged results
│
└── build/               # CMake build directory
    └── bin/             # Compiled executables
```

## Design Principles

1. **One Command = One Outcome**
   - Each verb does exactly one thing
   - Clear success/failure indication (✓/✗)
   - Nonzero exit code on failure

2. **Python Orchestrates, C++ Computes**
   - Python calls external tools
   - C++ owns physics truth
   - No Python-C++ bindings needed

3. **Everything is Logged**
   - All commands → `logs/YYYY-MM-DD/<task>.log`
   - Includes command, stdout, stderr, exit code, duration
   - Timestamped for audit trail

4. **Clean Separation**
   - `tools/` = orchestration
   - `src/` = C++ compute
   - `scripts/` = utility tools
   - `out/` = results

## Next Steps

1. **Fix Main CLI**: Resolve Hybridization enum redefinition to enable `vsepr` binary
2. **Test Molecule Building**: `flower run H2O --xyz` once CLI works
3. **Test Full Pipeline**: build → run → viz → export → report
4. **Create Truth Format**: Implement `truth_state.json` export
5. **Add QA Gate**: C++ validation layer between compute and viewers

## API Mode (Jupyter)

Can also import as Python module:

```python
from tools import build, run, viz, export, report

# Build project
result = build()
print(f"Build {'passed' if result.success else 'failed'}")

# Run molecule
molecule = run("H2O", xyz=True)

# Generate viewer
viz("out/H2O.xyz")

# Package results
report("H2O", format="html")
```

All logging and artifact management happens automatically.

## Troubleshooting

**Q: Command not found**
- Make sure you're in WSL bash context
- Check Python 3 is installed: `python3 --version`

**Q: No module named 'tools'**
- Ensure you're in vsepr-sim directory
- Check tools/__init__.py exists

**Q: C++ binary not found**
- Run `flower build` first
- Check build/bin/ directory exists

**Q: Where are my logs?**
- All logs: `logs/YYYY-MM-DD/`
- Current date: `logs/$(date +%Y-%m-%d)/`

**Q: How do I clean everything?**
- `flower clean` removes build/, out/, logs/
- Use with caution!

## Status

✅ Orchestrator CLI working  
✅ Visualization generation working  
✅ Logging system working  
✅ File management working  
⚠️ Main CLI blocked by enum redefinition  
⚠️ 6 tests disabled pending API updates  

Total lines of orchestrator code: ~660 lines across 7 files
