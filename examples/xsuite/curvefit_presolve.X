[xsuite]
name        = curvefit_presolve
version     = 5.2
mode        = presolve_suite
created_by  = vsepr

[run]
entry       = scripts/curvefit_entry.vsim
mode        = presolve
num_steps   = 1
dt          = 1.0e-15

[files]
snapshot    = archive/curvefit_snapshot.xyz

[curvefit]
enabled             = true
source              = archive/eigenmine_modes.ndjson
target              = archive/presolve_batch.ndjson
model               = poly_log_eigen_hybrid
max_order           = 3
regularization      = 1.0e-4
train_fraction      = 0.80
validation_fraction = 0.20
write_model         = archive/curvefit_model.json
write_coefficients  = reports/curvefit_coefficients.tsv
write_report        = reports/curvefit_validation.md

[actions]
run      = true
validate = true
export   = true

[outputs]
report  = reports/curvefit_presolve_report.md
log     = logs/curvefit_presolve.log
