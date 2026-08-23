[package]
name    = "xsim_export_package"
kind    = "xsim_export_bundle"
version = "stable+0.0.1"

[run]
run_dir = ""

[export]
write_xyz                 = true
write_analysis_json       = true
write_metrics_tsv         = true
write_report_md           = true
write_events_json         = false
write_symbolic_trace_json = false
write_manifest_json       = true
write_verify_report       = false
write_verify_tsv          = false
output_dir                = "out/export"
