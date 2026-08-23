#pragma once
/**
 * cmd_batch_sweep.hpp  -  Gaussian (and multi-distribution) parameter sweep
 *                         engine for the vsepr batch inlet
 * =========================================================================
 *
 * The sweep engine generates a set of resolved .vsim variants from a single
 * base script by sampling each declared axis from a probability distribution
 * (default: normal / Gaussian bell curve), then runs all variants through the
 * standard cmd_run_vsim() pipeline.
 *
 * -------------------------------------------------------------------------
 * AXIS DEFINITION  (one axis = one numeric parameter in the base .vsim)
 * -------------------------------------------------------------------------
 *
 *   target      dot-path into the .vsim document, e.g.
 *                  run.temperature_K          run.pressure_GPa
 *                  run.max_steps              run.dt_fs
 *                  simulation.box_size_ang    simulation.fire_dt_fs
 *                  simulation.fire_max_steps  simulation.ewald_alpha
 *                  simulation.ewald_rcut      simulation.ewald_kmax
 *                  simulation.step_delay_ms
 *                  environment.temperature    environment.pressure
 *                  environment.field_x        environment.field_y
 *                  environment.field_z        environment.humidity
 *                  molecule[0].temperature_K  molecule[0].count
 *                  molecule[0].velocity_drift molecule[0].n_layers
 *                  seed.foundation
 *                  cell.lx  cell.ly  cell.lz
 *                  excite.<name>.intensity    excite.<name>.fluence
 *                  excite.<name>.pulse_width_fs
 *
 *   distribution  "normal"           Gaussian bell curve  (default)
 *                 "uniform"          Flat over [min, max]
 *                 "log_normal"       Log-normal (mean/sigma are of ln(x))
 *                 "truncated_normal" Normal hard-clipped to [min, max]
 *
 *   mean         Centre of the bell (or uniform midpoint as a hint)
 *   sigma        Standard deviation  (ignored for uniform)
 *   min / max    Hard domain bounds applied after sampling
 *   n            Number of sample points drawn from the distribution
 *
 * -------------------------------------------------------------------------
 * SWEEP DESIGN
 * -------------------------------------------------------------------------
 *
 *   "factorial"        Cartesian product of all axis sample sets.
 *                      Total runs = prod(n_i) * replicates.
 *                      Guard: aborts if total > max_jobs (default 2000).
 *
 *   "joint"            All axes sampled together, producing exactly N tuples
 *                      where N = max(n_i).  Axes with fewer samples are
 *                      zero-padded by resampling from their distribution.
 *                      Total runs = N * replicates.
 *
 *   "latin_hypercube"  Stratified sampling: divides [0,1] into N equal
 *                      strata, picks one random point per stratum, maps
 *                      through the inverse CDF of each axis distribution.
 *                      Total runs = N * replicates.
 *
 * -------------------------------------------------------------------------
 * INPUT FORMS  (combinable on the vsepr batch / vsepr-batch CLI)
 * -------------------------------------------------------------------------
 *
 *   --base  <path.vsim>           Base script to clone + patch per run
 *   --sweep-config  <path.bsweep> JSON config file (see below)
 *   --axis  "name:target:dist:mean:sigma:min:max:N"
 *           Short inline axis spec; repeat for multiple axes.
 *           Fields after sigma are optional (min/max default to ±∞; N=9).
 *   --sweep-design  factorial|joint|latin_hypercube  (default: factorial)
 *   --sweep-out  <dir>            Output directory for generated .vsim files
 *                                 (default: batch_out/sweep_<timestamp>)
 *   --replicates  N               Replicate runs per parameter combination
 *   --max-jobs  N                 Safety cap on total generated jobs  (2000)
 *   --sweep-seed  N               RNG seed for stochastic sampling  (0=time)
 *   --keep-generated              Do not delete generated .vsim files after run
 *
 * -------------------------------------------------------------------------
 * .bsweep CONFIG FILE FORMAT  (JSON)
 * -------------------------------------------------------------------------
 *
 *   {
 *     "base":       "scripts/demo.vsim",
 *     "design":     "factorial",
 *     "replicates": 1,
 *     "axes": [
 *       { "name":         "temperature",
 *         "target":       "run.temperature_K",
 *         "distribution": "normal",
 *         "mean":         300.0,
 *         "sigma":        50.0,
 *         "min":          100.0,
 *         "max":          900.0,
 *         "n":            9 },
 *       { "name":         "pressure",
 *         "target":       "run.pressure_GPa",
 *         "distribution": "uniform",
 *         "min":          0.0,
 *         "max":          2.0,
 *         "n":            5 }
 *     ]
 *   }
 *
 * -------------------------------------------------------------------------
 * EXIT CODES  (same as cmd_batch)
 * -------------------------------------------------------------------------
 *   0   all generated jobs passed
 *   1   one or more jobs failed
 *   2   bad arguments / config parse error / no jobs generated
 *
 * WO-BATCH-SWEEP  |  v5.0.0
 */

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace vsepr::cli {

// ---------------------------------------------------------------------------
// Distribution kind
// ---------------------------------------------------------------------------
enum class SweepDist {
	Normal,           // bell curve  (default)
	Uniform,          // flat over [min, max]
	LogNormal,        // log-normal
	TruncatedNormal,  // normal hard-clipped to [min, max]
};

// ---------------------------------------------------------------------------
// One sweep axis
// ---------------------------------------------------------------------------
struct SweepAxis {
	std::string name;                       // display label
	std::string target;                     // dot-path into .vsim
	SweepDist   dist     = SweepDist::Normal;
	double      mean     = 0.0;
	double      sigma    = 1.0;
	double      min_val  = -std::numeric_limits<double>::infinity();
	double      max_val  =  std::numeric_limits<double>::infinity();
	int         n        = 9;               // sample count
	bool        integer_valued = false;     // round samples to nearest int
};

// ---------------------------------------------------------------------------
// Sweep design enum
// ---------------------------------------------------------------------------
enum class SweepDesign {
	Factorial,       // Cartesian product
	Joint,           // all axes sampled together (N = max axis n)
	LatinHypercube,  // stratified joint sampling
};

// ---------------------------------------------------------------------------
// Full sweep configuration
// ---------------------------------------------------------------------------
struct SweepConfig {
	std::string              base_vsim;          // path to base .vsim
	std::vector<SweepAxis>   axes;
	SweepDesign              design     = SweepDesign::Factorial;
	int                      replicates = 1;
	int                      max_jobs   = 2000;
	std::string              out_dir;            // empty = auto
	uint64_t                 sweep_seed = 0;     // 0 = time-seeded
	bool                     keep       = false; // keep generated files
	bool                     dry_run    = false;
	bool                     verbose    = false;
	bool                     quiet      = false;
	int                      jobs       = 1;     // parallel workers
	bool                     stop_fail  = false;
	std::string              report_path;
	std::string              label;
};

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

/**
 * Parse a .bsweep JSON file into a SweepConfig.
 * Returns false and writes a diagnostic on error.
 */
bool load_sweep_config(const std::string& path, SweepConfig& out);

/**
 * Parse a single inline --axis spec string:
 *   "name:target:dist:mean:sigma[:min:max[:N]]"
 * Returns false on parse error.
 */
bool parse_axis_spec(const std::string& spec, SweepAxis& out);

/**
 * Full sweep entry point.  Called from cmd_batch() when --base or
 * --sweep-config is present, or directly from a dedicated sweep binary.
 *
 * @param args  Everything after the "batch" token (argv[1..] for standalone).
 * @return      Exit code  (0/1/2).
 */
int cmd_batch_sweep(const std::vector<std::string>& args);

} // namespace vsepr::cli
