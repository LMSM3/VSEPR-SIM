#pragma once
/**
 * cmd_batch.hpp  -  vsepr batch <args>  subcommand
 * =================================================
 *
 * Runs one or more .vsim scripts through the full cmd_run_vsim() pipeline,
 * reports per-job timing and status, and optionally writes a JSON summary.
 *
 * Input forms (combinable):
 *   positional paths / globs   vsepr batch a.vsim b.vsim scripts/*.vsim
 *   --manifest <file>          text file, one .vsim path per line
 *   --dir      <directory>     recurse for *.vsim files
 *
 * Options:
 *   --jobs     N               parallel worker threads  (default: 1)
 *   --stop-on-fail             abort remaining jobs after first failure
 *   --dry-run                  print job list without executing
 *   --report   <path>          write JSON summary to <path>
 *   --verbose  / -v            echo each job's stdout/stderr
 *   --quiet    / -q            suppress per-job progress lines
 *   --label    <text>          tag printed in the summary header
 *
 * Sweep mode (Gaussian / multi-distribution parameter exploration):
 *   --base  <path.vsim>           Clone base script, vary parameters per run
 *   --sweep-config  <file.bsweep> JSON sweep config (axes, distributions, design)
 *   --axis  "name:target:dist:mean:sigma[:min:max[:N]]"
 *   --sweep-design  factorial|joint|latin_hypercube
 *   --replicates N   --sweep-out <dir>  --sweep-seed N  --max-jobs N
 *   --keep-generated
 *   See cmd_batch_sweep.hpp for full documentation.
 *
 * Exit codes:
 *   0   all jobs passed
 *   1   one or more jobs failed
 *   2   bad arguments / no jobs found
 *
 * WO-BATCH-INLET  |  v5.0.0
 */

#include <string>
#include <vector>

namespace vsepr::cli {

/**
 * Entry point for both the `vsepr batch` subcommand and the standalone
 * vsepr-batch executable.
 *
 * @param args  Everything after the "batch" token (or after argv[0] for
 *              the standalone binary).
 * @return      Exit code.
 */
int cmd_batch(const std::vector<std::string>& args);

} // namespace vsepr::cli
