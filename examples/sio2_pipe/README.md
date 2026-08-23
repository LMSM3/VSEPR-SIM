# SiO2 Pipe Case 001 — Example Project

This directory contains a complete example XSIM study for a silicon dioxide
pipe section exposed to a high-temperature particle stream at 600 K.

## Files

| File | Purpose |
|------|---------|
| `main.xsim` | Simulation script defining geometry, run parameters, and analysis settings |
| `export.x` | Export package declaration — drives `xsim xport` |
| `runs/sio2_pipe_case_001/` | Completed simulation output (placeholder data) |

## Run and export

```sh
# Run the simulation
xsim run examples/sio2_pipe/main.xsim

# Export the result bundle
xsim xport examples/sio2_pipe/export.x

# Verify the bundle
xsim verify out/sio2_pipe_analysis/final_bundle

# Inspect generated files
tree out/sio2_pipe_analysis/final_bundle
```

## Expected output bundle

```
out/sio2_pipe_analysis/final_bundle/
├── trajectory.xyz
├── analysis.json
├── metrics.tsv
├── events.json
├── symbolic_trace.json
├── report.md
├── verify_report.md
├── verify.tsv
└── manifest.json
```

## C++ usage

```cpp
#include <xsim/xport/xsim_xport_config.hpp>
#include <xsim/xport/xsim_xport_job.hpp>
#include <xsim/xport/xsim_xport.hpp>

int main() {
	using namespace xsim::xport;

	ExportConfig config;
	config.write_xyz          = true;
	config.write_analysis_json = true;
	config.write_metrics_tsv  = true;
	config.write_report_md    = true;
	config.write_manifest_json = true;
	config.output_dir         = "out/final_bundle";

	ExportJob job;
	job.run_dir      = "runs/sio2_pipe_case_001";
	job.output_dir   = config.output_dir;
	job.config       = config;
	job.run_id       = "sio2_pipe_case_001";
	job.package_name = "sio2_pipe_analysis_export";

	ExportResult result = run_export(job);

	if (!result.ok()) {
		for (const auto& error : result.errors)
			std::cerr << "Export error: " << error << '\n';
		return 1;
	}

	for (const auto& file : result.written_files)
		std::cout << "Wrote: " << file << '\n';
}
```
