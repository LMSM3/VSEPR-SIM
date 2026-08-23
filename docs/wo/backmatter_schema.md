# Backmatter Schema — WO-73B

**Status:** Locked  
**Purpose:** Define the final output row schema for the simulation validation backmatter tables.

---

## Schema

`
object_id           — canonical interaction object identifier (e.g. pi_7, HB_N, vdW_N)
family              — interaction family (pi_N, HB_N, vdW_N, sign_halo, ...)
state               — substance_primary_state (gas, crystal, biological_pocket, ...)
casting             — field_field | force_force | mixed
Lambda_empirical    — empirical reference length scale (nm)
Lambda_sim          — simulation-derived length scale (nm)
Lambda_solved       — uncertainty-weighted fused length scale (nm)
E_empirical         — empirical reference energy
E_sim               — simulation energy
E_solved            — uncertainty-weighted fused energy
confidence_score    — Q_X in [0,1]
gap_label           — standardised gap classification string
validation_status   — pass | gap | insufficient_data | needs_rerun
`

---

## Example Row

`
object_id:        pi_7
family:           pi_N
state:            biological_pocket
casting:          field_field
Lambda_empirical: 1.20
Lambda_sim:       1.18
Lambda_solved:    1.19
E_empirical:      -13.1
E_sim:            -12.4
E_solved:         -12.8
confidence_score: 0.83
gap_label:        good_match
validation_status: pass
`

---

## Notes

- `Lambda_solved` and `E_solved` are computed via `uncertainty_weighted_update()` (WO-74D).  
- `confidence_score` uses `Q_X = r_X * exp(-MAE_X / tau_X) * C_coverage`.  
- `validation_status` is set by the continual run controller after gap classification.  
- All column names are canonical per `naming_convention_map.csv`.