[xsuite]
name        = eigenmine_10m
version     = 5.2
mode        = presolve_suite
created_by  = vsepr

[run]
entry       = scripts/eigenmine_entry.vsim
mode        = presolve
num_steps   = 2000
dt          = 1.0e-15

[files]
snapshot    = archive/eigenmine_snapshot.xyz

[live]
enabled             = true
persistent_instance = true
state_flush_interval  = 25
health_flush_interval = 10

[checkpoint]
enabled    = true
format     = xyzc
directory  = checkpoints/
interval   = 500
keep_last  = 5
write_hash = true

[eigenmine]
enabled               = true
target_solves         = 10000000
batch_count           = 1000
seeds_per_batch       = 100
systems_per_seed      = 100
matrix_source         = state_force_event
mode_rank_limit       = 256
min_recurrence        = 0.70
max_reconstruction_error = 0.05
write_modes           = archive/eigenmine_modes.ndjson
write_basis           = archive/eigenmine_basis.bin
write_summary         = reports/eigenmine_report.md

[actions]
run     = true
validate = true
replay  = false
export  = true

[outputs]
report  = reports/eigenmine_10m_report.md
log     = logs/eigenmine_10m.log
