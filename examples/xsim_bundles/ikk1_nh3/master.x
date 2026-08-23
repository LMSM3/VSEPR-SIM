# ikk1_nh3/master.x
# ==================
# Minimal XSIM bundle command script.
# Declares package dependencies, run parameters, and export targets.
#
# Run via:
#   xsim run ikk1_nh3/master.x

use package export
use package frames
use package identity

run dynamic
steps 100000
dt_fs 0.25

export xyz stride 10
export report_md true
