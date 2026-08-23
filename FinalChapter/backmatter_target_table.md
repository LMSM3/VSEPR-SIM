# Backmatter Target Table

**Purpose:** Final documentation output. Every interaction object that survives the WO-74 refinement loop and achieves `validation_status: validated` feeds into this table. This is what the suffering was for.

---

## Schema

| Field | Type | Description |
|-------|------|-------------|
| `object_id` | string | Interaction object identifier |
| `family` | string | Interaction family |
| `state` | string | Physical state at time of validation |
| `casting` | string | Primary casting mode |
| `Lambda_data_nm` | float | Empirical length scale (nm) |
| `Lambda_sim_nm` | float | Simulated length scale (nm) |
| `Lambda_solved_nm` | float | Uncertainty-weighted solved value (nm) |
| `E_data` | float | Empirical interaction energy |
| `E_sim` | float | Simulated interaction energy |
| `E_solved` | float | Uncertainty-weighted solved energy |
| `confidence_score` | float | Final Q_X score [0, 1] |
| `gap_label` | string | Final gap classification |
| `validation_status` | string | `validated` / `partially_validated` / `simulation_only` |

---

## Current Records

| object_id | family | state | casting | Lambda_data_nm | Lambda_sim_nm | Lambda_solved_nm | E_data | E_sim | E_solved | confidence_score | gap_label | validation_status |
|-----------|--------|-------|---------|---------------|---------------|-----------------|--------|-------|----------|-----------------|-----------|-------------------|
| pi_7 | pi_N | biological_pocket | field_field | pending | 1.18 | pending | pending | pending | pending | 0.62 | insufficient_data | simulation_only |

> Table grows as continual runs complete and empirical data is imported.

---

## Promotion Criteria

A record is promoted from `comparison_summary` to this table when:

1. `gap_label` is `good_match` or `scale_shift` with resolved update
2. `confidence_score >= 0.50`
3. At least one empirical data source has been compared (`Lambda_data_nm` is not null)
4. `validation_status` has been manually reviewed and approved

Records with `confidence_score < 0.50` may appear as `partially_validated` with a note.

---

## Mathematical Basis

All solved values use the uncertainty-weighted update:

$$\Lambda_X^\star = \frac{\Lambda_X^{data}/\sigma_{data}^2 + \Lambda_X^{sim}/\sigma_{sim}^2}{1/\sigma_{data}^2 + 1/\sigma_{sim}^2}$$

Confidence:

$$Q_X = r_X \exp(-MAE_X/\tau_X)\, C_{\mathrm{coverage}}$$

---

## Notes

- `Lambda_solved_nm` is the value that appears in the final paper tables.
- `Lambda_sim_nm` is presented alongside it as the raw simulation prediction.
- `Lambda_data_nm` is the empirical reference.
- All three columns are required for a fully validated row. Rows missing `Lambda_data_nm` are flagged `simulation_only` and do not constitute validation.
