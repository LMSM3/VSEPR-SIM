# WO-73: Empirical Scale Registry and Simulation Validation Pipeline

**Status:** ✅ COMPLETE (73D `length_scale_fitter` + 73E `vsim_output_filter` delivered under Day-74 gate; see STAGE.md Day 74)
**Day:** 73 (spec) / Day 74 (delivery)
**Feeds into:** WO-74 (Self-Refining Continual Multiscale Runs)
**Owner:** SM

---

## Final Target

$$\text{Documentation backmatter} = \text{solved length-energy tables} + \text{simulation-vs-data gap audit} + \text{empirical support registry}$$

Everything before that exists to produce those tables cleanly.

---

## Backward Dependency Chain

### Final Output: Documentation Backmatter

The backmatter requires, for each interaction object:

$$\Lambda_X^\star,\quad E_X^\star,\quad Q_X,\quad \mathrm{gap}_X$$

For objects including:

$$\pi_7,\ HB_N,\ vdW_N,\ \mathcal{H}_\sigma^+,\ ML_N,\ solv_N$$

**Final output table schema:**

```
object_id
family
state
casting
Lambda_empirical
Lambda_sim
Lambda_solved
E_empirical
E_sim
E_solved
confidence_score
gap_label
```

---

## Work Packages

### 73A — Cleanup

**Goal:** Make the codebase and notation stable enough to build the pipeline without it collapsing on contact with reality.

**Lock these names — pick one, stop inventing synonyms:**

| Canonical name | Banned aliases |
|----------------|----------------|
| `Lambda_nm` | `scaleLength`, `lengthscale`, `lscale`, `lambda`, `lambda_nm`, `ideal_len` |
| `E_value` | — |
| `UFF` | — |
| `CFF` | — |
| `eta_scale` | — |
| `primary_scale` | — |
| `z_scale_1` ... `z_scale_6` | — |
| `state` | — |
| `family` | — |
| `casting` | — |
| `source_type` | — |
| `run_id` | — |

**Also clean:**
- Interaction registry names
- Output column names
- Scale symbols
- State labels
- Old dead output formats
- Duplicated simulation logs

**Deliverables:**
- `naming_convention_map.csv`
- `deprecated_output_list.md`

---

### 73B — Draft WO and Papers

**Goal:** Lock the conceptual pipeline before writing any code. Code that chases a moving conceptual target is archaeology, not engineering.

**Recommended WO title:**
```
WO-73: Empirical Scale Registry and Simulation Validation Pipeline
```

**Paper title draft:**
```
Recoverable Length Scales for Multiscale Field-Interaction Simulation:
A Registry-Based Empirical and Computational Framework
```

**Core paper claim:**

Physical interactions can be represented as registry objects with fitted recovered length scales, state-dependent priors, and before-after simulation deltas. Independent empirical and simulation tracks are compared to solve length-energy scale values and identify model gaps.

**Paper structure:**

```
1.  Motivation
2.  Scale registry
3.  Interaction families
4.  Field-field vs. force-force distinction
5.  Recovery length definition
6.  EmpiricalDB track
7.  Simulation track
8.  Comparison and gap audit
9.  Backmatter tables
```

**Deliverables:**
- `WO-73.md`
- `paper_outline.md`
- `backmatter_schema.md`

---

### 73C — Empirical Expansion

**Goal:** Create the imported-data side $\mathcal{R}_{data}$.

Registry families to cover:

```
pi_N
HB_N
vdW_N
disp_N
rep_N
sign_halo
sigma_hole
pi_hole
metal_ligand
solvation
surface
```

**EmpiricalDB record fields:**

```
object_id
family
state
length_prior_nm
length_sigma_nm
energy_prior
energy_sigma
dataset_source
coverage_count
confidence
```

**Example record:**

```
object_id:         pi_7
family:            pi_N
state:             crystal
length_prior_nm:   1.20
length_sigma_nm:   0.30
dataset_source:    CSD/PDB/QM-derived
confidence:        medium
```

This is the external reference track. Not final truth. Just the best available external constraint.

**Deliverables:**
- `empiricalDB_schema.csv`
- `interaction_registry_seed.csv`
- `state_prior_table.csv`

---

### 73D — C++ Length-Scale Fitter

**Goal:** Fit $\Lambda_X$ from recovery curves.

**Core definition:** $\Lambda_X$ is the length $L$ at which the recovery fraction reaches $1 - e^{-1}$:

$$R_X(L) = \frac{A_X(L)}{A_X(\infty)}, \quad \Lambda_X = R_X^{-1}(1 - e^{-1})$$

**Input fields:**

```
object_id
L
recovered_value
total_value
state
family
```

**Output fields:**

```
object_id
Lambda_fit_nm
sigma_fit_nm
fit_error
confidence
```

**Required fitting modes:**

```
linear interpolation
spline interpolation
log-scale fitting
asymmetric below/above ideal penalties
```

**Asymmetry rule (do not make it symmetric):**

$$\ell > \Lambda_X \Rightarrow \text{weaker recovery (above-ideal)}$$
$$\ell < \Lambda_X \Rightarrow \text{non-ideal / under-resolved (below-ideal)}$$

These two failure modes have different physical causes. They should not be treated as equivalent deviations.

**Deliverables:**
- `length_scale_fitter.hpp`
- `length_scale_fitter.cpp`
- `test_length_scale_fitter.cpp`

**Core output fields:** `Lambda_fit_nm`, `sigma_fit_nm`, `fit_quality`

---

### 73E — VSIM Output Filter

**Goal:** Convert raw VSIM output into empiricalDB-compatible format. This is the most important bridge.

$$\mathcal{R}_{sim} \rightarrow \mathrm{EmpiricalDB\ Schema}$$

**Transformation:**

```
raw VSIM output
→ normalized interaction rows
→ empiricalDB-compatible records
```

**Example output row:**

```
source_type:    simulation
object_id:      pi_7
family:         pi_N
state:          gas
casting:        field_field
Lambda_nm:      1.18
E_value:        -12.4
UFF:            -10.1
CFF:            0.71
primary_scale:  S3
gap_label:      pending
run_id:         SIM_RUN_0073
```

This lets simulation and empirical data be compared without duct-taping CSVs together.

**Deliverables:**
- `vsim_output_filter.hpp`
- `vsim_output_filter.cpp`
- `test_vsim_output_filter.cpp`
- `simulation_records_normalized.csv`

---

## Day Schedule

### Morning — 73A through 73B

**Focus:** Cleanup and conceptual locking  
**Goal by lunch:**
- `naming_convention_map.csv`
- `deprecated_output_list.md`
- `WO-73.md`
- `paper_outline.md`
- `backmatter_schema.md`

### Afternoon — 73C through 73D

**Focus:** EmpiricalDB schema and length-scale fitter  
**Goal by late afternoon:**
- `empiricalDB_schema.csv`
- `interaction_registry_seed.csv`
- `state_prior_table.csv`
- `length_scale_fitter.hpp/.cpp`
- `test_length_scale_fitter.cpp`

### Evening — 73E

**Focus:** VSIM output filter  
**Goal by close:**
- `vsim_output_filter.hpp/.cpp`
- `test_vsim_output_filter.cpp`
- `simulation_records_normalized.csv`

---

## Later: Continual Generation Phase

Once 73A–73E exist, variable-varied continual generation begins (see WO-74):

$$\boxed{\text{variable-varied continual generation}}$$

Run many simulations varying:

```
state               sign-halo direction
temperature         solvent screening
density             surface roughness
charge              force-field toggles
orientation
pi_N count
vdW packing
```

Each run produces a `SIM_RUN_####`. Comparison produces a `COMPARE_RUN_####`. Final solved values go to `solved_length_energy_backmatter.csv`.

---

## Day 73 Thesis

Day 73 converts the multiscale interaction theory from notation into a validation pipeline by aligning proprietary simulation output with an empirical database schema, fitting recovered length scales, and preparing the backmatter tables needed to compare simulation accuracy against external data.

---

## Summary of All Deliverables

| Work Package | Deliverables |
|-------------|-------------|
| 73A | `naming_convention_map.csv`, `deprecated_output_list.md` |
| 73B | `WO-73.md`, `paper_outline.md`, `backmatter_schema.md` |
| 73C | `empiricalDB_schema.csv`, `interaction_registry_seed.csv`, `state_prior_table.csv` |
| 73D | `length_scale_fitter.hpp`, `length_scale_fitter.cpp`, `test_length_scale_fitter.cpp` |
| 73E | `vsim_output_filter.hpp`, `vsim_output_filter.cpp`, `test_vsim_output_filter.cpp`, `simulation_records_normalized.csv` |
