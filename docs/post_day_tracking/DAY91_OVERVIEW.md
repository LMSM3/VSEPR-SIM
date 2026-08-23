# Day 91 Overview - Geometry Diagnostics and Revision Safety

[W] Day 91: dynamic-object geometry closure | Authority: `docs/day91/DAY91_OVERVIEW.tex`
[D] ACTIVE - B-U implemented or contract-frozen; A/V administrative close remains
[I] PARTIAL - code and verification gates pass; item-level TeX authority remains open
[V] PASS - expanded Group 91 plus both Group 89 regressions in normal and clean builds
[P] LOCAL - source/viewer/tests and ledgers updated; unrelated worktree changes remain untouched
[N] Review T/U before opening adaptive sampling; imported STL and Pd-H remain out of scope

## Day Objective

Make geometry failure, object replacement, stale access, and unsupported
downstream state visible and reproducible. Preserve the single supported live
viewer as an immutable-snapshot consumer and freeze the geometry-query contract
needed by adaptive sampling.

## A-Z Map

| Letter | Focus | Opening state |
|---|---|---|
| A | Baseline and authority freeze | Partial |
| B | Human/machine diagnostic record | Complete |
| C | Invalid primitive rejection | Complete |
| D | Transform convention and rejection | Complete |
| E | Ordered and nested CSG behavior | Complete |
| F | Empty CSG and invalid bounds | Complete |
| G | Undefined/ambiguous normal behavior | Complete |
| H | Object ID and geometry revision identity | Complete |
| I | Stale ID, stale snapshot, and retained-snapshot lifetime | Complete |
| J | Downstream invalidation and slot guards | Complete |
| K | Live-viewer acknowledgement and accepted-snapshot path | Complete |
| L | Deterministic acknowledgement ledger | Complete |
| M | Group 91 geometry regression expansion | Complete |
| N | Group 91 lifecycle and mixed-object regressions | Complete |
| O | Group 89 viewer-feed replacement/destruction regressions | Complete |
| P | Retired-viewer exclusion gate | Complete |
| Q | Clean/repeated build and memory-safety record | Complete with sanitizer-runtime limitation recorded |
| R | Geometry-state and object-revision diagrams | Complete |
| S | SDF, transform, CSG, snapshot, and invalidation documentation | Complete |
| T | Future imported-surface adapter contract | Complete (contract only) |
| U | Future adaptive-sampling query contract | Complete (contract only) |
| V | Day close and carry-forward ledger | Partial - waits on A |
| W |  | Reserved - intentionally blank |
| X |  | Reserved - intentionally blank |
| Y |  | Reserved - intentionally blank |
| Z |  | Reserved - intentionally blank |

## Current Evidence

- `vsepr_multiscale` compiles with primitive sphere/box SDFs, ordered nested
  CSG, validated transforms, typed diagnostics, stable monotonic object IDs,
  atomic design revisions, and immutable snapshots.
- `DynamicObjectPhase1Group91`, `LiveSimulationVisualsGroup89`, and
  `LiveSimulationThreadGroup89` pass in `build_vis` and the clean
  `build_day91_clean` directory.
- The immediate repeated build reports no work; the fixed-rate worker gate
  passes ten consecutive runs after active-time rate reporting was corrected.
- GCC and Clang sanitizer runtimes are not installed. Clang static analysis
  passes on the multiscale implementation and its regression source.
- `vsepr-view` displays deduplicated acknowledgements and accepted snapshot
  ID/revision/stage/bounds. Missing artifacts emit `VIEW-E010` and do not load
  fallback geometry.
- No STL reader or triangle importer was found for promotion. The existing
  XBIT/STL declarations remain migration evidence only.
- No material, mesh, continuum, MolecularDB, or Pd-H result is claimed.

## MolecularDB Background

Day 91 may emit future evidence references, but it does not own molecular
identity. One canonical molecule retains one lazily created MolecularDB
directory. A molecular study may append references to an object ID, geometry
revision, artifact hash, and acknowledgement; geometry objects never cause an
installation-time directory forest or duplicate formula-level ownership.

## Exit Gate

- Invalid geometry and stale access fail visibly with structured diagnostics.
- Object identity and geometry revision are independently inspectable.
- Failed replacement is atomic and retained snapshots stay usable.
- The viewer displays acknowledgements without producing diagnostics in its hot
  render loop or substituting valid-looking content.
- Expanded Group 91 and Group 89 tests, clean/repeated build evidence, and the
  supported memory-safety result are recorded.
- Imported-surface and adaptive-sampling contracts are specified without
  claiming those stages are implemented.
- W-Z remain blank unless the close ledger documents a justified exception.

Detailed acceptance, contracts, diagrams, and evidence live in
`docs/wo/WO-91-DYNAMIC-OBJECT-GEOMETRY-BRIDGE.md`.
