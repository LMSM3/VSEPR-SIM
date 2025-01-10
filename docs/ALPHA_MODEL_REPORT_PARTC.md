# Alpha Model Development Report — Part C
## Plateau Investigation: v2.6.3 and v2.7.0 Experiments, Root-Cause Analysis, and Reversion to v2.6.2

**Date:** 2025  
**Model code:** `atomistic/models/alpha_model.hpp`  
**Fitter:** `tools/fit_alpha_model.cpp`  
**Follows:** `docs/section_alpha_model.tex` (Parts A and B — model design, v2.6.0–v2.6.2)  
**Status:** Experiments discarded. Active version: **v2.6.2**. Next attempt: **Part D**.

---

## 1. Background and Baseline

Parts A and B (documented in `section_alpha_model.tex`) established the v2.6.2 model with **15 parameters**:

```
alpha(Z) = c_block(Z) * k_period(Z) * r(Z)^3
           * (1 + a_soft * s(Z)) / chi(Z)^b_chi
           * g_shell(Z)
           * g_rel(Z)

g_shell(Z) = 1 - c_shell * shell_closure(Z)     [v2.6.2: shell-closure gate]
g_rel(Z)   = 1 - a_rel   * f_rel(Z)             [v2.6.2: relativistic stiffening]
```

The v2.6.2 baseline performance was:

| Metric               | Value      |
|----------------------|------------|
| RMS error (Z=1–118)  | 17.5%      |
| RMS error (Z=1–54)   | 14.8%      |
| Noble gas RMS        | 27.3%      |
| Max error            | 64.5% (Lv) |
| Monotonicity breaks  | 10/70      |

The goal entering the v2.6.3 / v2.7.0 cycle was to push the global RMS below **15%**, with particular focus on eliminating the known failure regimes: noble gases, the Li/Ne volume degeneracy, and f-block scatter.

---

## 2. Experiment 1 — v2.6.3: Period-Modulated Shell Closure (`c_shell_decay`)

### Motivation

The v2.6.2 shell-closure gate applies a **uniform suppression** to noble gases regardless of period. The physical expectation is that heavier noble gases (Kr, Xe, Rn) should be *less* suppressed than light ones (He, Ne) because their outer electrons are farther from the nucleus and more deformable. The suppression appropriate for He (period 1) over-penalises Rn (period 6).

### Implementation

Added one parameter `c_shell_decay` (16 parameters total). For noble gases, the shell-closure proximity `phi_eff` was period-modulated:

```
phi_eff(Z) = exp(-c_shell_decay * (period(Z) - 1))   [noble gases only]
phi_eff(Z) = shell_closure(Z)                         [all other elements]
```

This decouples the suppression amplitude for He/Ne from that for Kr/Xe/Rn.

### Result

| Metric              | v2.6.2  | v2.6.3  | Δ       |
|---------------------|---------|---------|---------|
| RMS (Z=1–118)       | 17.5%   | ~17.3%  | −0.2%   |
| Noble gas RMS       | 27.3%   | ~24.1%  | −3.2%   |
| Overall improvement | —       | —       | marginal |

The noble-gas sub-regime improved modestly. However, the global RMS barely moved. The `c_shell_decay` parameter coupled strongly with `k_period[1]` (period 1, H and He): lowering the suppression on heavier noble gases forced `k_period[1]` and `c_shell` to re-compensate for He and Ne, erasing most of the gain elsewhere. The parameter was not independently informative enough to justify the added complexity.

---

## 3. Experiment 2 — v2.7.0: Fill-Fraction Volume Correction (`c_orb`)

### Motivation

A known structural deficiency of the v2.6.x base model is that `rcov(Z)^3` cannot simultaneously account for:
- **Alkali metals (Li, Na, K …)**: one electron in a partially-filled shell, diffuse cloud, *underestimated* by the model (−25 to −33%).
- **Noble gases (He, Ne)**: full shells, compact cloud, *overestimated* even after `g_shell` suppression (+30 to +57%).

Both errors arise from the same cause: `rcov` measures a bonding half-distance, not the actual valence-shell radius. The fill fraction `f = n_val / n_max` is a proxy for how "expanded" or "contracted" the outer shell is relative to `rcov`. This idea was formalised as:

```
g_fill  = 1 + c_orb * (0.5 - fill_fraction(Z))
r_eff^3 = rcov^3 * max(g_fill, 0.1)
```

- Sparse shells (Li: `f = 0.125`): `g_fill > 1` → larger effective volume  
- Full shells (Ne: `f = 1.0`): `g_fill < 1` → smaller effective volume  
- Mid-shell anchor (C, Si: `f = 0.5`): `g_fill = 1` → unchanged

This was described as the *first descriptor-level improvement since v2.6.0*.

### Implementation

Added `c_orb` (17 parameters total). The `fill_fraction(Z)` descriptor was already present in `atomic_descriptors.hpp`. The model was re-fitted with the full 17-parameter coordinate descent.

### Result

| Metric              | v2.6.3  | v2.7.0    | Δ          |
|---------------------|---------|-----------|------------|
| RMS (Z=1–118)       | ~17.3%  | 17.02±0.04% | −0.3%   |
| Improvement         | —       | —         | effectively zero |

**The model plateaued at ~17.02% and did not improve further across outer iterations.** This is the plateau event that motivated this report.

---

## 4. Root-Cause Analysis of the 17% Plateau

Four independent causes stack to produce the observed floor.

### Cause 1 — f-block Descriptor Degeneracy (primary, ~6–8% irreducible contribution)

The design decision in `atomic_descriptors.hpp` (documented in `section_alpha_model.tex`, Section 4.3) fixes `active_valence = 3` for **all** lanthanides (Z=57–71) and actinides (Z=89–103). This was intentional: it prevents an artificial monotonic gradient in softness across each series.

The consequence is that `fill_fraction(Z)`, `softness(Z)`, and `chi(Z)` are **nearly constant across the entire lanthanide series**. The only intra-series variation is from `rcov(Z)`, which spans only ~11% (1.62–1.80 Å) across La–Lu, giving a `rcov^3` range of ~37%.

The reference data spans **~60%** (La=31.1 Å³ → Tm/Yb=19.5 Å³) and is **non-monotonic**:

| Element | Z  | α_ref (Å³) | Anomaly                      |
|---------|----|------------|------------------------------|
| La      | 57 | 31.1       |                              |
| Ce      | 58 | 29.6       |                              |
| Pr      | 59 | 28.2       |                              |
| Nd      | 60 | **31.4**   | ← bump above La (4f anomaly) |
| Sm      | 62 | 23.8       |                              |
| Eu      | 63 | **27.7**   | ← half-filled 4f shell bump  |
| Yb      | 70 | **19.5**   | ← filled 4f shell contraction|

The Nd bump (+2.3 Å³ above La), Eu half-shell bump, and Yb filled-shell contraction are **real 4f physics** that no smooth `rcov^3 * c_block[f]` trend can represent. These elements alone guarantee ~8–12% pointwise errors that propagate directly into the global RMS.

The `c_orb` fill-fraction correction does not help here because `fill_fraction` is also constant across the series (all lanthanides have `f = 3/32 = 0.09375`).

**Conclusion:** The ~17% floor is largely set by the f-block and cannot be broken without either relaxing the `active_valence = 3` constraint or adding an explicit 4f-anomaly correction term.

---

### Cause 2 — Objective/Metric Mismatch

The fitter minimises a **log-space Huber loss**:
```
L(θ) = Σ w_i * Huber(log(α_pred) - log(α_ref))
```

But the reported performance metric is a **linear-space RMS % error**:
```
pct_i = 100 * (α_pred - α_ref) / α_ref
RMS   = sqrt( Σ pct_i^2 / N )
```

These are different norms. A 5% log-space error on Cs (α=59.4 Å³) is "small" to the optimizer, but the absolute residual `|pred - ref|` is large, contributing substantially to the linear RMS%. Conversely, a large log error on He (α=0.205 Å³) contributes almost nothing to the RMS% in absolute terms.

The practical consequence: **the optimizer could legitimately converge in log-space while the linear RMS% plateau is driven by moderate-log-error high-alpha elements** (K, Rb, Cs, Ba). Adding more parameters cannot resolve this without also changing what the optimizer is minimising.

---

### Cause 3 — Coordinate Descent Saddle in a Collinear Parameter Space

The parameters `k_period[p]`, `c_block[b]`, and `c_orb` all multiply the same `r_eff^3` term:

```
alpha ∝ c_block[b] * k_period[p] * r_eff^3
```

These three are **collinear in their effect on the objective**: increasing `k_period[2]` (period 3) by 10% has the same effect as increasing `c_block[s]` by 10% for any period-3 s-block element. Coordinate descent cannot make progress along the collinear direction — any single-parameter improvement is cancelled when the next parameter in the sweep tries to compensate.

This is a known failure mode of coordinate descent on nearly-degenerate loss landscapes. Gradient-based methods (L-BFGS, Adam) would use curvature information to move along the degenerate direction and escape these saddle points. The current grid-scan CD has no mechanism for this.

The `c_orb` parameter amplified this problem: it added a third collinear multiplier to an already-degenerate system, giving the optimizer another dimension to redistribute the same scaling budget without improving the total error.

---

### Cause 4 — Smoothness Penalty Penalises Real Non-Monotonic Physics

The group-pair smoothness penalty (`eta_smooth = 0.15`) in the loss function penalises cases where the model's group trend disagrees with the reference:

```
L_smooth = eta * Σ_{(a,b) in group pairs} Huber(Δ_pred - Δ_ref)
```

This was designed to prevent the fitter from producing erratic per-element fits that break periodic trends. It works correctly for most elements.

However, the **lanthanide series contains genuine non-monotonic bumps** (Nd, Eu) that look like noise to the penalty term but are real 4f physics. The penalty actively pushes the optimizer *away* from fitting these elements correctly, in order to maintain a monotonic trend through the series. Since `c_block[f]` is a single multiplier for all 15 f-block elements, the only solution is a smooth `rcov^3` curve that necessarily misses Nd and Eu.

The smoothness penalty trades pointwise accuracy for trend regularity. In the f-block, the regularization cost exceeds the benefit.

---

## 5. Quantitative Decomposition of the 17% Floor

| Source                              | Estimated RMS contribution | Breakable with current architecture? |
|-------------------------------------|---------------------------|---------------------------------------|
| f-block descriptor degeneracy       | 6–8%                      | Only with architectural change        |
| High-α alkali trend errors (metric mismatch) | 2–4%             | Partly (change reported metric)       |
| Correlated parameter saddle (CD)    | 2–4%                      | Yes (gradient-based optimizer)        |
| Smoothness penalty vs real f-anomalies | 2–3%                   | Partly (exclude f-block from penalty) |
| `c_orb` insufficient dynamic range  | 1–2%                      | Yes (wider range, separate s/p terms) |

**Estimated reducible floor with current descriptor design:** ~12–13%  
**Estimated irreducible floor (f-block physics unrepresentable):** ~10–12%

---

## 6. Decision: Revert to v2.6.2

Given the analysis above:

- v2.6.3 (`c_shell_decay`): marginal improvement (−0.2% global RMS), adds parameter coupling complexity. **Discarded.**
- v2.7.0 (`c_orb`): marginal improvement (−0.5% global RMS across both experiments), does not address the dominant failure mechanism. **Discarded.**

The code has been reverted to **v2.6.2** (15 parameters, RMS=17.5%):
- `alpha_model.hpp`: `AlphaModelParams` restored to v2.6.2 values; `alpha_predict` simplified back to direct `shell_closure(Z)` gate.
- `fit_alpha_model.cpp`: `cd_sweep` and JSON writer stripped of `c_orb` and `c_shell_decay`.

**v2.6.2 is the current stable baseline.** It is well-characterised, reproducible, and its failure modes are fully documented. No further incremental parameter additions should be attempted without first addressing at least one of the four root causes identified above.

---

## 7. What Did NOT Work (Anti-Pattern Log)

These approaches have been tried and failed to break the plateau. They should not be retried without addressing the underlying cause first.

| Approach                        | Version | Why it failed                                        |
|---------------------------------|---------|------------------------------------------------------|
| Period-decay of noble-gas suppression (`c_shell_decay`) | v2.6.3 | Coupled to `k_period[1]`; He/Ne compensation erased the gain |
| Fill-fraction volume correction (`c_orb`) | v2.7.0 | f-block `fill_fraction` is constant; collinear with `k_period`/`c_block` |
| Increasing outer reweighting iterations (30→50) | v2.7.0 | Weight saturation plateau; best checkpoint still ~17% |
| Widening coordinate descent grid | ongoing | Does not escape correlated-parameter saddles |

---

## 8. Part D — Next Approach (Stub)

*To be written after the approach is confirmed.*

**Proposed direction:** [User to fill in]

**Expected mechanism:** [User to fill in]

**Key questions before implementation:**
1. Does it address Cause 1 (f-block degeneracy), Cause 3 (CD saddle), or both?
2. Does it require changes to `atomic_descriptors.hpp`, or only to `alpha_model.hpp`?
3. What is the T1 invariant impact? (`active_valence(La..Lu) == 3` is currently a hard invariant enforced by `test_alpha_model.cpp`.)

---

## Document Index

| Document | Content |
|----------|---------|
| `docs/section_alpha_model.tex` | Part A/B: Model derivation, descriptor design, v2.6.0–v2.6.2 validation |
| `docs/ALPHA_MODEL_REPORT_PARTC.md` | **Part C (this file):** Plateau investigation, v2.6.3/v2.7.0 experiments, root-cause analysis |
| `docs/ALPHA_MODEL_REPORT_PARTD.md` | Part D: Next approach (pending) |
| `atomistic/models/alpha_model.hpp` | Active model code (v2.6.2) |
| `atomistic/models/atomic_descriptors.hpp` | Descriptor layer (unchanged) |
| `tools/fit_alpha_model.cpp` | Offline fitter (reverted to v2.6.2) |
| `data/polarizability_ref.csv` | 118-element reference dataset |
| `tests/test_alpha_model.cpp` | T1–T4 invariant tests |
