#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace atomistic::golden_run {

inline constexpr const char* gr1_scenario_id = "GR-1";
inline constexpr const char* gr1_scenario_name = "Gas-to-Liquid Relaxation";
inline constexpr const char* gr1_schema_version = "1.0";

enum class CompletionState {
	NumericallyConverged,
	PhysicallyStable,
	LiquidStateConfirmed,
	HoldPeriodPassed,
	ConservationPassed,
	RestartMatched,
	RenderStreamValid,
	GoldenRunPassed
};

const char* to_string(CompletionState state);

struct Vec3 {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct PhaseThresholds {
	double minimum_density_ratio = 1.10;
	double minimum_dominant_cluster_fraction = 0.80;
	double maximum_dispersion = 3.00;
	double maximum_mean_squared_displacement = 100.0;
	double maximum_rdf_change = 0.20;
	double maximum_temperature_spread = 0.12;
	double maximum_pressure_spread = 12.0;
	std::uint32_t hold_samples = 30;
};

struct ConvergenceThresholds {
	double relative_residual = 1.0e-3;
	double momentum_tolerance = 1.0e-10;
	double energy_drift_tolerance = 10.0;
	double restart_position_tolerance = 1.0e-12;
	double restart_velocity_tolerance = 1.0e-12;
};

struct GR1Config {
	std::string run_id = "GR-1-argon-001";
	std::string scenario_version = "1.0";
	std::uint32_t particle_count = 64;
	std::string species = "Ar";
	std::string molecular_identity = "argon-like Lennard-Jones particle";
	std::string technique = "isothermal compression";
	std::uint64_t random_seed = 20250308;

	double initial_temperature = 1.20;
	double target_temperature = 0.72;
	double box_length = 12.0;
	double final_box_fraction = 0.48;
	double timestep = 0.002;
	std::uint32_t total_steps = 12000;
	std::uint32_t sample_interval = 20;
	std::uint32_t checkpoint_step = 6000;
	double particle_radius = 0.50;
	double sigma = 1.0;
	double epsilon = 1.0;
	double cutoff = 2.5;
	double thermostat_relaxation = 0.02;
	std::string integrator = "velocity-verlet";
	std::string interaction_model = "Lennard-Jones 12-6, shifted-force cutoff";
	std::string ensemble = "NVT deterministic velocity-rescale";
	std::uint32_t render_fps = 60;
	PhaseThresholds phase;
	ConvergenceThresholds convergence;
};

struct ParticleSample {
	Vec3 position;
	Vec3 velocity;
	std::uint32_t cluster_id = 0;
};

struct DiagnosticSample {
	std::uint32_t step = 0;
	double time = 0.0;
	double potential_energy = 0.0;
	double kinetic_energy = 0.0;
	double total_energy = 0.0;
	double temperature = 0.0;
	double pressure = 0.0;
	double density = 0.0;
	double phase_indicator = 0.0;
	double dominant_cluster_fraction = 0.0;
	double dispersion = 0.0;
	double mean_squared_displacement = 0.0;
	double rdf_change = 0.0;
	double temperature_spread = 0.0;
	double pressure_spread = 0.0;
	bool liquid = false;
};

struct TrajectorySample {
	DiagnosticSample diagnostics;
	std::vector<ParticleSample> particles;
};

struct SolverReport {
	double initial_residual = 0.0;
	double final_residual = 0.0;
	double relative_residual = 0.0;
	std::uint32_t step_count = 0;
	double tolerance = 0.0;
	bool stagnated = false;
	std::string termination_reason;
};

struct ConservationReport {
	double initial_total_energy = 0.0;
	double final_total_energy = 0.0;
	double relative_energy_drift = 0.0;
	double momentum_norm = 0.0;
	bool mass_conserved = false;
	bool species_balance_conserved = false;
	bool momentum_conserved = false;
	bool energy_within_tolerance = false;
};

struct GR1Result {
	GR1Config config;
	std::vector<TrajectorySample> trajectory;
	SolverReport solver;
	ConservationReport conservation;
	std::vector<CompletionState> completed_states;
	bool restart_matched = false;
	bool render_stream_valid = false;
	bool golden_run_passed = false;
	std::string trajectory_hash;
	std::string final_state_hash;
};

GR1Config load_gr1_config(const std::filesystem::path& input_path);
GR1Result run_gr1(const GR1Config& config, bool write_artifacts = true,
				  const std::filesystem::path& output_directory = {});
bool write_gr1_artifacts(const GR1Result& result, const std::filesystem::path& output_directory,
						 const std::string& build_id = "unknown", const std::string& commit_id = "unknown");

} // namespace atomistic::golden_run
