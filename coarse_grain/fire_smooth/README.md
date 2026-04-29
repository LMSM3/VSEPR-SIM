# fire_smooth/ — Experimental FIRE Smooth-Sampling Task

**Status:** Experimental (active development)

## Purpose

`fire_smooth` is an experimental task layer that extends the standard FIRE
minimiser with **smooth random sampling** — drawing starting configurations
from a seeded distribution rather than a single fixed point — then propagating
each sample through the full 6+9 FIRE relaxation.

This is the integration point between:
- Random sampling (seed-reproducible, traceable)
- FIRE atomistic relaxation (deterministic per seed)
- QM descriptors (charge, orbital proxy, polarisability — via `qm/`)
- Level 3 Bead preparation (macro-DM handoff — via `level3/`)

## Architecture

```
Random seed → SampleDescriptor
                  ↓
          FIRE relaxation (6+9 stepper)
                  ↓
          SmoothSampleRecord (energy, forces, η, ρ, QM descriptors)
                  ↓
          Level3BeadRecord (macro-DM precursor payload)
                  ↓
          MacroPrecursorState (rigidity*, ductility*, transport*...)
```

## Files

| File | Purpose |
|------|---------|
| `smooth_sample.hpp` | `SampleDescriptor`, `SmoothSampleRecord`, sampler |
| `fire_smooth_runner.hpp` | Core runner: draw N samples, relax, collect records |

## Relationship to Other Layers

- Reads from: `coarse_grain/models/seed_bead_stepper.hpp` (FIRE 6+9)
- Reads from: `coarse_grain/qm/qm_descriptors.hpp` (QM prep layer)
- Writes to:  `coarse_grain/level3/level3_bead_record.hpp` (L3 handoff)
- Feeds:      `coarse_grain/analysis/macro_precursor.hpp` (macro-DM)
