# WO-83D - Lone-Pair Inference

**Branch:** `feature/wizard-full-module-expansion`
**Area:** `atomistic/classify/`
**Status:** Implemented
**Developer regression:** `tests/test_classify_lone_pair_inference.cpp`
**VSIM acceptance scripts:** `examples/classify_acceptance/lone_pair_nh3.vsim`, `examples/classify_acceptance/lone_pair_h2o.vsim`

---

## Goal

Add conservative lone-pair inference to the VSEPR classifier using data that is
available in the current `atomistic::State`.

Current source support:

- atomic number via `State::type`
- bond connectivity via `State::B`
- geometry fallback via local bond angles

Current source does not yet provide:

- formal charge
- bond order
- element-specific valence state metadata

---

## Implementation

`VSEPROptions` now includes:

```cpp
bool allow_element_lone_pair_inference = true;
bool allow_geometry_only_fallback = true;
```

`VSEPRSite` now records whether lone-pair assignment came from:

```cpp
bool used_element_lone_pair_inference = false;
bool used_geometry_lone_pair_fallback = false;
```

The inference order is:

1. Use conservative neutral main-group element inference when available.
2. Fall back to geometry-only inference if element inference is disabled or not available.
3. Leave formal-charge and bond-order-aware behavior for later parts.

---

## Covered Cases

| Case | Expected behavior |
|---|---|
| N with three bonded domains | `AX3E`, tetrahedral electron geometry, trigonal pyramidal molecular shape |
| O with two bonded domains | `AX2E2`, tetrahedral electron geometry, bent molecular shape |
| C with two bonded domains | `AX2`, linear molecular shape when bond order is unavailable |
| Geometry fallback disabled? | No; fallback remains available when element inference is disabled |

---

## Validation

Run:

```text
cmake --build build --target test_classify_lone_pair_inference -j 4
ctest --test-dir build -R ClassifyLonePairInferenceTest --output-on-failure
```

The regression test proves:

- element inference can override ambiguous geometry for neutral N/O cases
- geometry-only fallback remains available
- carbon with two bonded domains remains linear without bond-order data

---

## VSIM Acceptance

Runtime acceptance is not complete until real `.vsim` scripts can prove the same
classifier behavior through exported output.

Acceptance scripts live in the VSIM documentation workspace:

```text
examples/classify_acceptance/lone_pair_nh3.vsim
examples/classify_acceptance/lone_pair_h2o.vsim
```

These scripts require `analysis_json`, `metrics_tsv`, and report output to expose
center atom, bonded domains, lone-pair domains, electron domains, AXmEn label,
molecular shape, electron-domain geometry, angle statistics, confidence, and the
lone-pair inference source.

Progression is blocked if the runtime silently drops `vsepr_sites`, omits
`organic_candidate` when requested, or cannot emit enough detail to verify the
expected shapes.

Current runtime check:

```text
.\build\vsepr.exe validate <path-to>\lone_pair_nh3.vsim
.\build\vsepr.exe <path-to>\lone_pair_nh3.vsim validate
```

Both forms fail in formula parsing with the current CLI shape. The source target
builds, but the `.vsim` validate/run/audit bridge is not wired yet.

---

## Carry-Forward

These remain future work:

- formal charge support
- bond-order-aware VSEPR
- species or valence-state metadata
- broader element coverage
