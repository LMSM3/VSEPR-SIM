#pragma once
/**
 * cmd_doctor_tests.hpp
 * --------------------
 * Internal module commands surfaced under `vsepr doctor`:
 *
 *   vsepr doctor integratedtest    -  dependency-ordered integration test suite
 *   vsepr doctor benchmark         -  timed throughput / latency benchmarks
 *
 * Both return 0 on pass, non-zero on any failure.
 */

#include <vector>
#include <string>

namespace vsepr::cli {

// Run the integrated test suite.
// Tests are ordered by dependency: primitive math -> state construction ->
// formula parsing -> energy evaluation -> fingerprinting -> clustering -> statistics.
// Returns 0 if all tests pass, 1 if any test fails.
int cmd_integrated_test(const std::vector<std::string>& args);

// Run the benchmark suite.
// Each benchmark is timed with std::chrono::steady_clock and reports
// mean iteration time and throughput.  Never returns non-zero for slow
// results  -  only fails on hard errors (crash / exception).
// Returns 0 on success, 1 on hard error.
int cmd_benchmark(const std::vector<std::string>& args);

} // namespace vsepr::cli
