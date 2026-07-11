# VSEPR-SIM Development Day #46: Random Element and Index Tracker

## Summary

A tracking subsystem was implemented to decouple persistent particle identity from
mutable container index within VSEPR-SIM. The system supports seeded random element
assignment, stable persistent identifiers, index remapping after reorder operations,
and event-based state logging. This prevents identity corruption during sorting,
deletion, or solver-side data compaction, and establishes a foundation for later
lineage-aware features such as transmutation, decay response, and atomistic object
replacement.

## Motivation

As VSEPR-SIM grows toward deterministic materials formation, dynamic topology changes
and internal array reordering make raw storage index insufficient as a scientific
identifier. A persistent tracker is therefore required to preserve continuity of
identity, maintain correct chemical attribution, and support reproducible debugging
and export.

## Architecture

### Element Descriptor (`element_descriptor.hpp`)
Immutable chemical reference data — pure facts about an element. No simulation state,
no mutation, no index coupling. Includes optional isotope fields (mass number, neutron
number, decay clock, metastable flag, isotope label) for future radioactive/actinide
modeling.

### Particle Record (`element_index_tracker.hpp`)
The tracked simulation entity. Contains:
- `persistent_id` — never changes after creation
- `current_index` — changes as arrays reorder
- Element identity (Z, symbol, mass)
- Creation and update timestamps (simulation steps)
- Active/inactive flag (soft delete)
- Random selection flag
- Parent-child lineage (parent_id, generation)
- State tag ("initialized", "bonded", "excited", "decayed", etc.)
- Optional isotope fields

### Element Index Tracker (`element_index_tracker.hpp/.cpp`)
Manager with bidirectional ID ↔ index maps:
- Registration (new particles and decay/split children)
- Index update (single or bulk remap)
- Soft deletions
- Lookup by ID or by index
- State transition logging
- CSV export
- Event log export
- Internal consistency verification

### Random Element Picker (`random_element_picker.hpp/.cpp`)
Three seeded selection modes:
1. **Uniform** — equal probability for all elements in pool
2. **Weighted** — probability proportional to user-supplied weights
3. **Rule-constrained** — must satisfy a user-defined predicate, with weighted fallback

All modes are deterministic: same seed + same pool + same weights → same sequence.

## Implementation Details

### Error Integration
Uses `vsepr::ErrorContext::code` via the `Status` return type from `core/error.hpp`.
Structured error returns — no exceptions for expected failure paths.

### Identity vs Index

| Property | Persistent ID | Current Index |
|----------|--------------|---------------|
| Stability | Never changes | Changes on reorder |
| Purpose | Scientific identity | Solver array position |
| Use in logs | ✓ | ✗ (meaningless after sort) |
| Use in exports | ✓ | For current snapshot only |

### Event Log Format
```
[Step 000000] created Particle ID=1001 Element=C Z=6 Index=0 Mode=weighted
[Step 000012] state Particle ID=1002 "initialized" -> "bonded"
[Step 000034] reindex Particle ID=1001 OldIndex=0 NewIndex=4
[Step 001000] created Particle ID=2008 Element=U Z=92 Index=7 Parent=1044 Gen=1 State=decay_product
```

## Case Study: 12-Particle Initialization

### Parameters
- Allowed set: H, C, N, O, Si, S, Fe, U, Pu
- Seed: 46017
- Mode: Uniform random

### Workflow
1. Set allowed element pool (9 elements)
2. Seed RNG with 46017
3. Select 12 elements via uniform random
4. Register each in tracker with sequential indices
5. Apply random spatial sort permutation (seed 46017)
6. Verify identity preservation across remap

### Verification Metrics
- All 12 persistent IDs unique
- All indices valid before and after remap
- All symbol/Z pairs consistent post-remap
- Zero index collisions
- Zero duplicate-ID violations
- Zero orphan map entries
- Deterministic: same seed → same selection order → same initialization

## Verification Tests

| Test | Description | Status |
|------|-------------|--------|
| Stable creation | 1000 particles, unique IDs, valid indices | ✓ |
| Reorder stability | Random shuffle, identity preserved | ✓ |
| Deletion/compaction | 10% removed, survivors intact | ✓ |
| Seed reproducibility | Same seed → same sequence | ✓ |
| Parent-child lineage | Pu-239 → U-235 decay | ✓ |
| Constrained selection | Even-Z-only filter | ✓ |
| CSV export | Readable output with all fields | ✓ |
| Event log | All transitions recorded | ✓ |
| Weighted distribution | C dominates at 80% weight | ✓ |
| Case study Day #46 | Full 12-particle scenario | ✓ |

## Deliverables

| File | Purpose |
|------|---------|
| `include/core/element_descriptor.hpp` | Immutable element reference data |
| `include/core/element_index_tracker.hpp` | ParticleRecord + ElementIndexTracker |
| `include/core/random_element_picker.hpp` | Seeded random selection (3 modes) |
| `src/core/element_index_tracker.cpp` | Tracker implementation |
| `src/core/random_element_picker.cpp` | Picker implementation |
| `tests/tracker_tests.cpp` | 10 verification tests |
| `docs/tracker_case_study_day46.md` | This document |

## Extension Points

### Radioactive / Actinide Modeling
The ParticleRecord already includes optional isotope fields:
- `mass_number` (A)
- `neutron_number` (N)
- `decay_clock` (remaining half-life)
- `metastable` flag
- `isotope_label` ("Pu-239")

A decay event is logged as:
```
[Step 450000] state Particle ID=9012 "initialized" -> "decayed"
[Step 450000] created Particle ID=13044 Element=U Z=92 Parent=9012 Gen=1 State=decay_product
```

### Batch Processing Integration
The tracker integrates naturally with `vsepr_batch` via:
1. Batch spec defines element pools and weights per component
2. Picker selects elements deterministically per seed
3. Tracker assigns persistent IDs across the entire batch
4. Remap events preserve identity when solver reorders
5. CSV/JSON export provides batch-wide particle audit trail

### Output Formats
- **CSV** — for Excel review, frequency statistics, identity audits
- **Event log** — for debugging, trajectory audits, export integrity
- **JSON snapshot** — for GUI state restore, replay, interoperability (future)
- **LaTeX table snippet** — for paper/report integration (future)
