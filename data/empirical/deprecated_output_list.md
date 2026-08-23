# Deprecated Output List — WO-73A

**Status:** Locked  
**Purpose:** Enumerate output column names, file formats, and identifiers that are banned from new code.

---

## Deprecated Column Names

| Deprecated | Canonical Replacement |
|---|---|
| `scaleLength` | `Lambda_nm` |
| `lengthscale` | `Lambda_nm` |
| `lscale` | `Lambda_nm` |
| `lambda` | `Lambda_nm` |
| `lambda_nm` | `Lambda_nm` |
| `ideal_len` | `Lambda_nm` |

---

## Deprecated File Formats / Outputs

| File / Pattern | Status | Reason |
|---|---|---|
| `*_scale_output.txt` | BANNED | Replaced by `simulation_records_normalized.csv` |
| `interaction_log_*.csv` | BANNED | Non-canonical column names; use EmpiricalDB schema |
| `run_output_*.json` | BANNED | Replaced by `continual_run_schema.csv` records |
| `scaleLength_results.csv` | BANNED | Column name violation |
| `lambda_fit_result*.txt` | BANNED | Replaced by fitter output fields |
| Duplicate simulation logs (`sim_log_*.txt`) | BANNED | Single canonical `run_id` per record |

---

## Deprecated Scale Symbols

| Old Symbol | Canonical |
|---|---|
| `L_scale` | `Lambda_nm` |
| `scale_len` | `Lambda_nm` |
| `recovered_lambda` | `Lambda_nm` |
| `fit_lambda` | `Lambda_fit_nm` (fitter output only) |

---

## Enforcement

All new headers, CSV outputs, and test fixtures must use canonical names from `naming_convention_map.csv`.  
Any legacy file matching the above patterns should be considered stale and regenerable.