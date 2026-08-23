[package]
name    = "sio2_pipe_analysis_export"
kind    = "xsim_export_bundle"
version = "stable+0.0.1"

[run]
run_dir = "runs/sio2_pipe_case_001"

[export]
write_xyz                 = true
write_analysis_json       = true
write_metrics_tsv         = true
write_report_md           = true
write_events_json         = true
write_symbolic_trace_json = true
write_manifest_json       = true
write_verify_report       = true
write_verify_tsv          = true
output_dir                = "out/sio2_pipe_analysis/final_bundle"
