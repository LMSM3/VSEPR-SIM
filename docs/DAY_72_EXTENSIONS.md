# VSIM Day-72 Language Extensions
<!-- WO-72U / WO-72L / WO-72S -->

## Overview

Day 72 introduces three new control-flow and decision-support sections to the VSIM
scripting language:

| Section | Tag | Purpose |
|---|---|---|
| `[until]` | WO-72U | Stop-loop: run until a condition becomes true |
| `[loop]` | WO-72L | Smart loop: fixed iterations with variable stepping |
| `[select]` | WO-72S | Weighted decision table (Uitt2 method) |

These three sections form the **functional-discovery layer** of VSIM — the base for
semi-off-rails randomization and real material science screening.

---

## WO-72U — `[until]` stop-loop

### Concept

Inverted `[while]`. The body runs _until_ the stop condition is satisfied rather than
_while_ it is satisfied. This maps directly onto the physical idiom:

> "keep annealing until the order parameter reaches 0.98"
> "keep running until molecule[0].order_param >= 0.98"

### New condition forms

```
molecule[i].<property> >= <value>
property[<name>]       >  <value>
```

Both are resolved by scanning the `KernelEventLog` in reverse for the most-recent
event whose `source_formula` or `equation_symbolic` contains the key string, then
comparing `result_value` against the threshold.

All six operators are supported: `>`, `<`, `>=`, `<=`, `==`.

### Schema additions

```cpp
struct UntilGuard {
    std::string name;
    std::string condition;
    int         body_steps    = 200;
    int         max_iters     = 50;
    std::vector<std::string> measure;
    int         iter_delay_ms = 0;
    bool        export_each   = false;
};
struct UntilSection { std::vector<UntilGuard> guards; };
// VsimDocument: UntilSection until_cfg;
```

### Runtime

`VsimRuntime::run_until()` — evaluates condition _before_ each body run, stops on
first `true`. Reports per-iteration event count. Optional export flush per body.

---

## WO-72L — `[loop]` smart loop

### Concept

Fixed-count loop where one or more named variables advance each iteration. Supports
four stepping modes:

| Mode | Formula |
|---|---|
| `linear` | `value = start + iter × delta` |
| `geometric` | `value = start × factor^iter` |
| `random` | `value = base ± noise`  (seeded RNG — reproducible) |
| `combo` | same as `random`; communicates "trend + jitter" intent |

`combo` / `random` mode is the base for **semi-off-rails randomization**: the variable
follows a linear trend but each point is perturbed by a seeded noise field, producing
a different exploration path per `rand_seed` while remaining fully reproducible.

### Compact `var` syntax

```
var = "temperature  start=300  stop=1500  delta=63  mode=linear"
var = "defect_rate  start=0.0  stop=0.05  delta=0.0025  mode=random  noise=0.001"
```

Multiple `var` lines are allowed; each advances independently.

### Schema additions

```cpp
struct LoopVarStep {
    std::string var_name;
    double      start       = 0.0;
    double      stop        = 0.0;
    double      step_delta  = 1.0;
    double      step_factor = 1.0;
    double      noise       = 0.0;
    std::string step_mode   = "linear";
};
struct SmartLoopSection {
    std::string name;
    int         iterations     = 10;
    int         body_steps     = 200;
    std::string stop_condition;
    bool        record_each    = false;
    bool        export_each    = false;
    uint64_t    rand_seed      = 0;
    std::string perturb_mode   = "combo";
    std::vector<LoopVarStep> var_steps;
};
// VsimDocument: SmartLoopSection loop_cfg;
```

### Runtime

`VsimRuntime::run_smart_loop()` — advances all `var_steps` per iteration, then runs
`body_steps`, optionally flushes exports, and optionally checks `stop_condition`.

---

## WO-72S — `[select]` weighted decision (Uitt2 method)

### Concept

A first-class weighted-criteria table that mirrors the material-selection matrix used
in nuclear / structural material screening. Each row is a `[[select.criterion]]` block
with a `weight` and a `scores` list aligned to `candidates`.

Weighted score: `Σ (weight[c] × score[c][i])` for each candidate `i`.

### Primary use case: nuclear cladding material screening

The canonical example from Day 72 mirrors the table:

| Criterion | Wt. | Zircaloy-4 | ZIRLO/M5FeCrAl | SiC/SiC | SS 316 |
|---|---|---|---|---|---|
| Strength and stiffness | 25% | 3 | 4 | 5 | 3 |
| Creep and thermal stability | 20% | 3 | 3 | 5 | 3 |
| Toughness and fracture resistance | 15% | 4 | 4 | 2 | 5 |
| Corrosion and oxidation resistance | 15% | 3 | 4 | 5 | 3 |
| Density | 10% | 3 | 2 | 5 | 2 |
| Cost and manufacturability | 15% | 5 | 5 | 2 | 5 |
| **Weighted score** | | **3.45** | **3.70** | **4.25** | **3.45** |

### Schema additions

```cpp
struct SelectCriterion {
    std::string         name;
    double              weight = 0.0;
    std::vector<double> scores;
};
struct SelectSection {
    std::string              name;
    std::vector<std::string> candidates;
    std::vector<SelectCriterion> criteria;
    bool    print_table   = true;
    bool    print_winner  = true;
    bool    output_json   = false;
    int     score_scale_max = 5;
};
// VsimDocument: SelectSection select_cfg;
```

### Runtime

`VsimRuntime::run_select()` — computes weighted scores, prints a formatted table, and
announces the winner. Does not drive simulation steps; it is a scoring / reporting
pass that can sit alongside any simulation block.

---

## Full combined example — zircaloy anneal with candidate selection

```vsim
[project]
name      = "nuclear_clad_day72"
seed_base = 7200

[select]
name        = "cladding_choice"
candidates  = ["Zircaloy-4", "SiC/SiC", "SS316"]
print_table  = true
print_winner = true

[[select.criterion]]
name   = "Strength and stiffness"
weight = 0.25
scores = [3, 5, 3]

[[select.criterion]]
name   = "Creep and thermal stability"
weight = 0.20
scores = [3, 5, 3]

[[select.criterion]]
name   = "Toughness and fracture resistance"
weight = 0.15
scores = [4, 2, 5]

[[select.criterion]]
name   = "Corrosion and oxidation resistance"
weight = 0.15
scores = [3, 5, 3]

[[select.criterion]]
name   = "Density"
weight = 0.10
scores = [3, 5, 2]

[[select.criterion]]
name   = "Cost and manufacturability"
weight = 0.15
scores = [5, 2, 5]

[material]
formula   = "Zr"
structure = "hcp"
cell      = "4x4x4"

[run]
mode      = "md"
max_steps = 2000

[loop]
name         = "thermal_ramp"
iterations   = 15
body_steps   = 300
record_each  = true
export_each  = true
rand_seed    = 7201
perturb_mode = "combo"

var = "temperature  start=300  stop=1200  delta=60  mode=linear"
var = "defect_seed  start=0    stop=0     delta=0   mode=random  noise=50"

[until]
name        = "target_order"
condition   = "molecule[0].order_param >= 0.98"
max_iters   = 30
body_steps  = 500
measure     = [order_param, energy_var]
export_each = true

[export]
write_xyz          = true
write_report_md    = true
write_metrics_tsv  = true
write_manifest_json = true
output_dir         = "out/nuclear_clad_day72"
```

---

## Implementation files changed (Day 72)

| File | Change |
|---|---|
| `include/vsim/vsim_document.hpp` | Added `UntilGuard`, `UntilSection`, `LoopVarStep`, `SmartLoopSection`, `SelectCriterion`, `SelectSection`; wired into `VsimDocument` |
| `include/vsim/vsim_parser.hpp` | Declared `apply_until_key`, `apply_loop_key`, `apply_select_key` |
| `src/vsim/vsim_parser.cpp` | Dispatch + field handlers for `[until]`, `[loop]`, `[select]`, `[[select.criterion]]` |
| `include/vsim/vsim_runtime.hpp` | Added `run_until()`, `run_smart_loop()`, `run_select()`, extended `eval_condition()` and `parse_comparison()` with `>=`, `<=`, `==`, and named-event probe lookup via `extract_named_event_value()` |
| `docs/VSIM_LANGUAGE.md` | New `[until]`, `[loop]`, `[select]` sections; extended `[while]` condition table; updated missing/deferred feature table |
| `docs/DAY_72_EXTENSIONS.md` | This file |

---

*Day 72 — VSIM functional-discovery layer — WO-72U / WO-72L / WO-72S*