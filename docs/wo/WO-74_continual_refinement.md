# WO-74: Self-Refining Continual Multiscale Runs

**Status:** ✅ COMPLETE — WO-74A gap classifier, WO-74B output filter, WO-74C fitter validation all COMPLETE in STAGE.md v5.13.4; schemas/rules/demo deliverables produced
**Day:** 74
**Depends on:** WO-73 (empiricalDB / simulationDB pipeline)
**Owner:** SM

---

## 0. Day-Wide Objective

By end of day, produce a working scaffold for the self-refining continual run loop:

$$\text{simulate} \rightarrow \text{normalize} \rightarrow \text{compare} \rightarrow \text{gap-label} \rightarrow \text{update priors} \rightarrow \text{generate next runs}$$

**Scope constraint:** Do not rewrite the physics kernel. Day 74 is a control-layer and validation-layer day.

---

## 1. Required Inputs from Day 73

| File | Status |
|------|--------|
| `empiricalDB_schema.csv` | expected |
| `interaction_registry_seed.csv` | expected |
| `state_prior_table.csv` | expected |
| `length_scale_fitter.cpp` | expected |
| `vsim_output_filter.cpp` | expected |
| `simulation_records_normalized.csv` | expected |

> If incomplete, WO-74 proceeds with placeholder CSVs.

---

## 2. Main Outputs

### Required

| File | Work Package |
|------|-------------|
| `WO-74_continual_refinement.md` | — |
| `continual_run_schema.csv` | 74A |
| `state_prior_table.csv` | 74B |
| `gap_label_registry.csv` | 74C |
| `prior_update_rules.md` | 74D |
| `run_generation_rules.json` | 74E |
| `day74_demo_case_pi7_pocket.csv` | 74F |
| `comparison_summary_schema.csv` | 74G |
| `backmatter_target_table.md` | 74H |

### Optional C++ Scaffolds

| File | Work Package |
|------|-------------|
| `continual_run_controller.hpp/.cpp` | 74A |
| `gap_classifier.hpp/.cpp` | 74C |
| `prior_update.hpp/.cpp` | 74D |

---

## 3. Work Packages

### 74A — Continual Run Schema

Define the master run record. Each record is:

$$\mathcal{R}_n = (X, \mathrm{state}, \Lambda_X, E_X, Q_X, \mathrm{gap}_X)$$

**Schema fields:**

```
continual_run_id
parent_run_id
generation_index
source_type
object_id
family
substance_primary_state
primary_scale
casting_primary
Lambda_prior_nm
Lambda_sim_nm
Lambda_data_nm
Lambda_solved_nm
E_sim
E_data
gap_label
confidence_score
next_action
```

**Deliverable:** `continual_run_schema.csv`

---

### 74B — State-of-Matter Control Layer

Legal `substance_primary_state` values:

```
gas
plasma
liquid
solution
crystal
amorphous_solid
surface_adsorbed
interface_bound
biological_pocket
molten_salt
supercritical_fluid
macro_composite
```

Each state gets a prior mean $\bar{\Lambda}_{X,\mathrm{state}}$ and standard deviation $\sigma_{X,\mathrm{state}}$.

**State prior rule:**

$$\Lambda_X^{(0)} = \bar{\Lambda}_{X,\mathrm{state}} + \Delta\Lambda_{\mathrm{family}} + \Delta\Lambda_{\mathrm{topology}} + \Delta\Lambda_{\mathrm{env}}$$

**Deliverable:** `state_prior_table.csv`

---

### 74C — Gap-Label Registry

**Standardized gap labels:**

```
good_match
scale_shift
energy_shift
overextended_field
underresolved_scale
missing_vdw_many_body
missing_sign_halo
bad_state_prior
bad_screening
force_mismatch
orientation_failure
topology_sensitive
resolution_failure
insufficient_data
```

**Gap test:**

$$\epsilon_{\Lambda,X} = \Lambda_X^{sim} - \Lambda_X^{data}$$
$$\epsilon_{E,X} = E_X^{sim} - E_X^{data}$$

**Classification examples:**

| Condition | Label |
|-----------|-------|
| $\Lambda_X^{sim} \gg \Lambda_X^{data}$ | `overextended_field` |
| $\Lambda_X^{sim} \ll \Lambda_X^{data}$ | `underresolved_scale` |
| $\|\epsilon_\Lambda\| \leq \text{tol}$ | `good_match` |

**Deliverable:** `gap_label_registry.csv`

---

### 74D — Prior Update Rule

**Uncertainty-weighted lambda update:**

$$\Lambda_X^\star = \frac{\Lambda_X^{data}/\sigma_{data}^2 + \Lambda_X^{sim}/\sigma_{sim}^2}{1/\sigma_{data}^2 + 1/\sigma_{sim}^2}$$

**Energy update:**

$$E_X^\star = \frac{E_X^{data}/\sigma_{E,data}^2 + E_X^{sim}/\sigma_{E,sim}^2}{1/\sigma_{E,data}^2 + 1/\sigma_{E,sim}^2}$$

**Confidence score:**

$$Q_X = r_X \exp(-MAE_X/\tau_X)\, C_{\mathrm{coverage}}$$

**Doctrine:**

$$\boxed{\text{update calibration layer, not raw kernel}}$$

The physics kernel is not rewritten because one CSV had a bad afternoon.

**Deliverable:** `prior_update_rules.md`

---

### 74E — Gap-to-Next-Run Generation Rules

Rule map from gap label to next run families:

```json
{
  "missing_vdw_many_body": ["vdW_N", "disp_N", "pack_N", "crystal", "solution"],
  "missing_sign_halo": ["sigma_hole", "pi_hole", "lone_pair_halo", "halogen_bond"],
  "bad_state_prior": ["same_object_different_state", "state_average_retest"],
  "force_mismatch": ["force_force_alignment_sweep", "orientation_variation"],
  "underresolved_scale": ["lower_scale_projection", "smaller_dt", "decompose_composite"],
  "overextended_field": ["cutoff_sweep", "screening_test", "environment_variation"]
}
```

**Deliverable:** `run_generation_rules.json`

---

### 74F — Demo Case: Ligand-Pocket / $\pi_7$

**Object specification:**

```
object_id:            pi_7
family:               pi_N
state:                biological_pocket
secondary_state:      solution
interaction_partner:  pocket_residue_field
families_active:      pi_N, HB_N, vdW_N, sign_halo, ion_dipole, solvation
primary_scale:        S3
secondary_scales:     S2, S4
```

**Expected outputs:**

```
Lambda_pi7_sim
UFF_pi7_pocket
CFF_pi7_steric
eta_scale
eta_orientation
eta_environment
gap_label
next_action
```

**Deliverable:** `day74_demo_case_pi7_pocket.csv`

---

### 74G — Comparison Summary Schema

**Record fields:**

```
compare_run_id
object_id
family
state
Lambda_sim_nm
Lambda_data_nm
dLambda_nm
relative_error_Lambda
E_sim
E_data
dE
confidence_score
gap_label
next_action
solved_Lambda_nm
solved_E
```

**Mathematics:**

$$\Delta\Lambda_X = \Lambda_X^{sim} - \Lambda_X^{data}$$

$$\mathrm{relErr}_\Lambda = \frac{\Lambda_X^{sim} - \Lambda_X^{data}}{\Lambda_X^{data} + \epsilon}$$

**Deliverable:** `comparison_summary_schema.csv`

---

### 74H — Backmatter Target Table

Final backmatter row schema:

```
object_id
family
state
casting
Lambda_data_nm
Lambda_sim_nm
Lambda_solved_nm
E_data
E_sim
E_solved
confidence_score
gap_label
validation_status
```

**Deliverable:** `backmatter_target_table.md`

---

## 4. Day Schedule

### Morning — 74A through 74C

**Focus:** Schemas, state labels, gap labels  
**Goal by lunch:**
- `continual_run_schema.csv`
- `gap_label_registry.csv`
- `state_prior_table.csv`

### Afternoon — 74D through 74F

**Focus:** Prior update rules, run generation, pi_7 demo  
**Goal by late afternoon:**
- `prior_update_rules.md`
- `run_generation_rules.json`
- `day74_demo_case_pi7_pocket.csv`

### Evening — 74G through 74H

**Focus:** Comparison schema, backmatter, WO writeup  
**Goal by close:**
- `WO-74_continual_refinement.md`
- `comparison_summary_schema.csv`
- `backmatter_target_table.md`

---

## 5. Minimal C++ Targets

Do not overbuild. Start with structs only.

### `ContinualRunRecord`

```cpp
struct ContinualRunRecord {
    std::string continual_run_id;
    std::string object_id;
    std::string family;
    std::string state;
    std::string casting_primary;

    double lambda_prior_nm    = 0.0;
    double lambda_sim_nm      = 0.0;
    double lambda_data_nm     = 0.0;
    double lambda_solved_nm   = 0.0;

    double energy_sim         = 0.0;
    double energy_data        = 0.0;

    double confidence_score   = 0.0;

    std::string gap_label;
    std::string next_action;
};
```

### `classify_gap`

```cpp
std::string classify_gap(
    double lambda_sim,
    double lambda_data,
    double tolerance_nm
) {
    const double diff = lambda_sim - lambda_data;

    if (std::abs(diff) <= tolerance_nm) return "good_match";
    if (diff > tolerance_nm)            return "overextended_field";
    return "underresolved_scale";
}
```

### `uncertainty_weighted_update`

```cpp
double uncertainty_weighted_update(
    double sim_value,
    double sim_sigma,
    double data_value,
    double data_sigma
) {
    const double w_sim  = 1.0 / (sim_sigma  * sim_sigma);
    const double w_data = 1.0 / (data_sigma * data_sigma);
    return (sim_value * w_sim + data_value * w_data) / (w_sim + w_data);
}
```

No template metaprogramming demons yet.

---

## 6. Success Criteria

Day 74 is successful when this chain executes end-to-end:

```
pi_7 simulation record
→ normalized continual run record
→ compared against placeholder/imported data record
→ gap label assigned
→ solved Lambda updated
→ next run recommended
→ backmatter row produced
```

The demo does not need to be physically perfect. It needs to prove the pipeline logic.

---

## 7. Final WO Statement

WO-74 establishes the self-refining continual run layer for the multiscale interaction framework. It defines the run schema, state-of-matter controls, gap labels, prior update rules, and gap-driven next-run generation logic. The goal is to convert simulation output and empirical data into a closed refinement loop that produces solved length-energy constants, confidence scores, and backmatter-ready validation tables.

$$\boxed{\text{WO-74 turns Day \#73's validation pipeline into a self-refining simulation loop.}}$$
