[xsuite]
name        = release_gate_presolve
version     = 5.2
mode        = release_gate
created_by  = vsepr

[run]
entry       = scripts/release_gate_entry.vsim
mode        = presolve
num_steps   = 1
dt          = 1.0e-15

[files]
snapshot    = archive/release_gate_snapshot.xyz

[release_gate]
enabled                = true
baseline_version       = v5.1.2
candidate_version      = v5.1.3
require_hash_success   = 0.999
require_curvefit_score = 0.85
require_eigen_quality  = 0.80
max_failure_rate       = 0.01
write_gate_report      = reports/release_gate.md
write_comparison       = archive/release_comparison.ndjson

[actions]
run      = true
validate = true
export   = true

[outputs]
report  = reports/release_gate_report.md
log     = logs/release_gate.log
