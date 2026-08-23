# =============================================================================
# sphere_shaded.x
#
# Rotation demo — shaded, sampled sphere.
#
# This is the .x package side.  The companion .vsim simulation that generates
# the sampled trajectory is sphere_shaded.vsim.
#
# Workflow:
#   1. Run the simulation:
#        xsim run examples/rotation/sphere_shaded.vsim
#   2. Export the shaded bundle:
#        xsim xport examples/rotation/sphere_shaded.x
#   3. Inspect:
#        tree out/sphere_shaded/
#
# The .x package reads from the completed run at runs/sphere_shaded/ and
# produces a fully verified, reportable output bundle.
#
# stable+0.0.1
# =============================================================================

[package]
name    = "sphere_shaded_export"
kind    = "xsim_export_bundle"
version = "stable+0.0.1"

# ----------------------------------------------------------------------------
# Source run
# ----------------------------------------------------------------------------
[run]
run_dir = "runs/sphere_shaded"

# ----------------------------------------------------------------------------
# Export flags
# ----------------------------------------------------------------------------
[export]
write_xyz                 = true    # trajectory.xyz  — full sampled trajectory
write_analysis_json       = true    # analysis.json   — RDF, density, energy
write_metrics_tsv         = true    # metrics.tsv     — per-step energy/temp
write_report_md           = true    # report.md       — human-readable summary
write_events_json         = false   # no events in this scene
write_symbolic_trace_json = false   # no symbolic trace
write_verify_report       = true    # verify_report.md
write_verify_tsv          = true    # verify.tsv
write_manifest_json       = true    # manifest.json   — artifact inventory
output_dir                = "out/sphere_shaded"
