# UC-6 — Golden Suite Dispatcher ("The Regression Gate")
<!-- VSEPR-SIM | Day 85D | WO-85 -->

---

> **Note on numbering.** This document is the **Day 85** formalization of UC-6 as a
> *format-aware dispatcher* over the Day-85 stress-script family. It is a superset
> companion to the earlier `docs/uc/UC-6-crystallographic-regression.md`, which
> defines UC-6 specifically over the `val_01..val_08` crystallographic golden
> scripts. Both are valid views of "the golden suite": the crystallographic doc is
> the *science* regression gate; this doc is the *engine/output-format* regression
> gate. They share the UC-6 identity intentionally and do not overwrite one another.

---

## Overview

**Goal:** Define a dispatcher that runs a curated set of VSIM scripts and checks
whether their outputs remain within expected tolerances. UC-6 is the controlled
regression layer that turns "did I break something?" into a repeatable,
tolerance-gated answer.

The dispatcher says exactly one thing, reliably:

```
Run these golden scripts.
Compare outputs.
Fail if drift exceeds tolerance.
Report exactly what changed.
```

---

## Why UC-6 Matters

The engine now emits multiple script families:

```
Format A legacy
Format B rich / reactivity
Format D legacy-preserved
gas injection
ChemPlus
VSEPR classify
pipe bridge
particle / render output
```

Without a dispatcher, every new feature becomes:

```
"Did I break something?"
"Probably."
"Where?"
"Good luck."
```

UC-6 converts that guesswork into a controlled regression layer with a numeric
drift budget and an explicit format-dispatch report.

---

## Actors

| Actor | Role |
|-------|------|
| Researcher / CI | Triggers `bash tests/run_uc6_golden_suite.sh` |
| `vsepr.exe` | Runs each golden script headlessly |
| Per-script log | `exports/uc6_golden_suite/<name>.log` capturing stdout+stderr |
| Tolerance policy | `[tolerance]` block in the dispatcher fixture |
| Golden reference | `tests/golden/expected/*.txt` (future numeric baselines) |

---

## Preconditions

1. `build/vsepr.exe` present.
2. Golden scripts present under `scripts/` (Day-85 core set below).
3. `bash` available (Git Bash / MSYS2 on Windows).

---

## Expected Inputs

```
scripts/golden/*.vsim            (curated golden scripts; Day-85 core set reuses scripts/demo_stress_*.vsim)
tests/golden/expected/*.txt      (numeric baselines, future)
tests/golden/tolerances/*.json   (per-script tolerance budgets, future)
```

Day-85 core regression set (`day85_core_regression`):

```
scripts/demo_stress_format_a_small.vsim    (Format A legacy)
scripts/demo_stress_format_b_medium.vsim   (Format B rich / reactivity)
scripts/demo_stress_format_d_large.vsim    (Format D legacy-preserved)
scripts/demo_stress_gas_injection.vsim     (gas-injection branch)
```

---

## Expected Outputs

```
PASS / FAIL report
diff summary
numeric tolerance report
format dispatch report
artifact location report
```

Artifact layout:

```
exports/uc6_golden_suite/summary.txt
exports/uc6_golden_suite/results.json
exports/uc6_golden_suite/diffs/
exports/uc6_golden_suite/<script-name>.log
```

---

## Workflow steps

```
1. Dispatcher reads the golden suite registry (fixture [golden_suite].scripts).
2. For each script:
   a. vsepr run <script>  > exports/uc6_golden_suite/<name>.log 2>&1
   b. reject NaN / Inf in the log            -> FAIL
   c. require energy output present          -> FAIL if missing
   d. require exit code 0                     -> FAIL if non-zero
3. Aggregate PASS / FAIL across all scripts.
4. Exit non-zero if any FAIL; print a per-script result table.
```

---

## Acceptance Criteria

```
[ ] dispatcher runs selected golden scripts
[ ] dispatcher supports script groups
[ ] dispatcher supports A/B/D output formats
[ ] dispatcher supports tolerance budgets
[ ] dispatcher reports numeric drift
[ ] dispatcher reports missing output sections
[ ] dispatcher fails clearly on NaN/inf
[ ] dispatcher integrates with CTest
[ ] dispatcher can be run manually from Bash
```

Day-85 baseline (this iteration) satisfies: format-aware dispatch over A/B/D +
gas injection, NaN/Inf rejection, energy-presence checks, exit-code gating,
CTest integration (Group 96), and manual Bash invocation. Numeric-diff and
tolerance-budget reporting are staged behind the `tests/golden/` inputs and are
tracked as follow-on gaps.

---

## Fixture + automation

| Artifact | Path |
|----------|------|
| Dispatcher fixture | `scripts/uc6_golden_suite_dispatcher.vsim` |
| Automation script | `tests/run_uc6_golden_suite.sh` |
| CTest target | `uc6_golden_suite_dispatcher` (Group 96) |
| CTest labels | `vsim golden dispatcher regression uc6 wo-85 quick` |

---

## Related

- `docs/uc/UC-6-crystallographic-regression.md` — crystallographic golden suite (science gate).
- `docs/wo/WO-MF-01-missing-features.md` — MF-G extension tags for dispatcher/pipe gaps.

---

*UC-6 | Day 85D | WO-85 | VSEPR-SIM*
