# V6 Public API Audit

## Scope
This audit covers the public interfaces of the frozen v5.16.0 baseline that are candidates for migration into V6 DFAE.

## Public API surface

### 1. `vsepr::core` (include/core/)
- `ElementDatabase`  — element lookup by Z/symbol; V6 RETAIN, promote to precomputed registry.
- `PeriodicTable`    — JSON-backed element data loader; V6 RETAIN as source data reader.
- `ElementIndexTracker` — stable index mapping; V6 ADAPT to live-state object identity.
- `Status`           — result/error carrier; V6 RETAIN pattern, retype as RuntimeEvent.

### 2. `vsepr::formula` (include/vsepr/formula_parser.hpp)
- `parse(formula)`   — maps chemical formula to element composition; V6 RETAIN.

### 3. `vsepr::molecular` (include/molecular/molecule_database.hpp)
- `MoleculeDatabase` — curated molecular database; V6 RETAIN as inspection source.

### 4. `vsepr::cli` (include/cli/)
- `cmd_inspect`, `cmd_run_vsim`, `cmd_validate`, etc.  — V6 RETAIN CLI surface; retype to live-state commands.

### 5. `vsim::` (include/vsim/)
- `VsimDocument`     — parsed declarative script; V6 RETAIN as input contract.
- `VsimRuntime`      — script execution; V6 ADAPT to emit state revisions rather than own state.
- `VsimParser`       — parser; V6 RETAIN.

### 6. `atomistic::State` / `atomistic::classify` (atomistic/)
- Core physics state and VSEPR classification; V6 ADAPT to live-state observers.

### 7. `vsepr::pipeline` (include/pipeline/)
- `run_pipeline`, `stage_fingerprint`, `stage_cluster`, `stage_analysis`; V6 ADAPT to consume committed FormationEvents.

## Contracts that will change
- Global simulation object ownership → runtime-committed live state.
- Frame-copy/update visualization → delta-driven viewer updates.
- Standalone viewer command loops → runtime command submission.

## Contracts that remain stable
- Element symbol / Z lookup.
- Formula parsing and composition.
- XYZ/xyzFull scientific replay formats.
- Declarative .vsim input grammar.
