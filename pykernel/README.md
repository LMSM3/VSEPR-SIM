# PyKernel — VSEPR-SIM Python Bridge

Python interface to the VSEPR-SIM atomistic simulation engine.

## Architecture

PyKernel is a **thin bridge layer** — the C++ kernel remains authoritative
for all scientific computation. PyKernel provides:

- **PolyFitter**: 11-to-15 layer polynomial fits on simulation trajectories
- **EigenCounter**: Eigenvalue accumulator (inertia, covariance, Hessian)
- **GPUBridge**: CLI bridge to VSEPR-SIM GPU backend (CUDA/OpenCL/CPU)
- **ContinuousRunner**: Walk-away continuous simulation + analysis (Phase A)
- **ImprovementLoop**: Closed slow-loop autonomous improvement (Phase C)

## Quick Start

```bash
# Install
pip install -e .

# Run continuous simulations (walk-away)
pykernel-run --spec NaCl@crystal Fe@crystal --runs 0

# Run improvement scan
pykernel-improve --scan-only

# Run full improvement loop
pykernel-improve --iterations 5 --delay 60
```

## Phases

### Phase A — Background Data Enrichment
Polynomial fits (degree 11-15) and eigenvalue counters on every simulation run.
Runs continuously via GPU bridge + VSEPR CLI. Data accumulates on disk.

### Phase B — Python Library
This package. Exposes VSEPR-SIM as an importable Python library with structured
CLI bridge, data analysis, and reporting.

### Phase C — Closed Slow Loop
Autonomous improvement: scan for flagged code, run regression simulations,
compare against baselines, generate improvement task queues.

## Anti-Black-Box

Every coefficient, eigenvalue, residual, condition number, and comparison
is explicitly stored and inspectable. No hidden state.
