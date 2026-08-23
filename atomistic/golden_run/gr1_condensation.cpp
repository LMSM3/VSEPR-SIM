#include "gr1_condensation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace atomistic::golden_run {
namespace {

constexpr double pi = 3.14159265358979323846;

struct State {
	std::vector<Vec3> positions;
	std::vector<Vec3> velocities;
	std::vector<Vec3> initial_positions;
	double box_length = 0.0;
	std::uint32_t step = 0;
	std::uint64_t render_frame_index = 0;
};

struct ForceResult {
	std::vector<Vec3> forces;
	double potential = 0.0;
	double virial = 0.0;
};

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double norm(Vec3 a) { return std::sqrt(dot(a, a)); }

double wrap(double value, double box) {
	value = std::fmod(value, box);
	return value < 0.0 ? value + box : value;
}

Vec3 minimum_image(Vec3 delta, double box) {
	delta.x -= box * std::nearbyint(delta.x / box);
	delta.y -= box * std::nearbyint(delta.y / box);
	delta.z -= box * std::nearbyint(delta.z / box);
	return delta;
}

std::string trim(std::string value) {
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return {};
	const auto last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

std::string quote_json(const std::string& value) {
	std::string escaped;
	for (const char c : value) {
		if (c == '"' || c == '\\') escaped.push_back('\\');
		escaped.push_back(c);
	}
	return "\"" + escaped + "\"";
}

std::string fnv1a_hash(const std::string& data) {
	std::uint64_t hash = 14695981039346656037ull;
	for (const unsigned char c : data) {
		hash ^= c;
		hash *= 1099511628211ull;
	}
	std::ostringstream out;
	out << std::hex << std::setfill('0') << std::setw(16) << hash;
	return out.str();
}

std::string serialize_state(const State& state) {
	std::ostringstream out;
	out << std::setprecision(17) << state.step << ' ' << state.box_length << ' ' << state.render_frame_index << '\n';
	for (std::size_t i = 0; i < state.positions.size(); ++i) {
		const auto& x = state.positions[i];
		const auto& v = state.velocities[i];
		out << x.x << ' ' << x.y << ' ' << x.z << ' ' << v.x << ' ' << v.y << ' ' << v.z << '\n';
	}
	return out.str();
}

State deserialize_state(const std::string& text, const std::vector<Vec3>& initial_positions) {
	std::istringstream in(text);
	State state;
	state.initial_positions = initial_positions;
	if (!(in >> state.step >> state.box_length >> state.render_frame_index)) throw std::runtime_error("Invalid GR-1 checkpoint");
	Vec3 position, velocity;
	while (in >> position.x >> position.y >> position.z >> velocity.x >> velocity.y >> velocity.z) {
		state.positions.push_back(position);
		state.velocities.push_back(velocity);
	}
	if (state.positions.empty() || state.positions.size() != initial_positions.size()) throw std::runtime_error("Incomplete GR-1 checkpoint");
	return state;
}

ForceResult evaluate_forces(const State& state, const GR1Config& config) {
	ForceResult result;
	result.forces.assign(state.positions.size(), {});
	const double cutoff2 = config.cutoff * config.cutoff;
	const double inv_cutoff = config.sigma / config.cutoff;
	const double sr2c = inv_cutoff * inv_cutoff;
	const double sr6c = sr2c * sr2c * sr2c;
	const double force_cutoff = 24.0 * config.epsilon * (2.0 * sr6c * sr6c - sr6c) * inv_cutoff;
	const double potential_cutoff = 4.0 * config.epsilon * (sr6c * sr6c - sr6c);

	for (std::size_t i = 0; i < state.positions.size(); ++i) {
		for (std::size_t j = i + 1; j < state.positions.size(); ++j) {
			Vec3 delta = minimum_image(sub(state.positions[i], state.positions[j]), state.box_length);
			const double r2 = dot(delta, delta);
			if (r2 >= cutoff2 || r2 < 1.0e-12) continue;
			const double r = std::sqrt(r2);
			const double inv_r = 1.0 / r;
			const double sr2 = config.sigma * config.sigma * inv_r * inv_r;
			const double sr6 = sr2 * sr2 * sr2;
			const double unshifted_force = 24.0 * config.epsilon * (2.0 * sr6 * sr6 - sr6) * inv_r;
			const double force_magnitude = unshifted_force - force_cutoff;
			const double potential = 4.0 * config.epsilon * (sr6 * sr6 - sr6) - potential_cutoff + (r - config.cutoff) * force_cutoff;
			const Vec3 force = scale(delta, force_magnitude * inv_r);
			result.forces[i] = add(result.forces[i], force);
			result.forces[j] = sub(result.forces[j], force);
			result.potential += potential;
			result.virial += dot(delta, force);
		}
	}
	return result;
}

double kinetic_energy(const State& state) {
	double energy = 0.0;
	for (const auto& velocity : state.velocities) energy += 0.5 * dot(velocity, velocity);
	return energy;
}

double temperature(const State& state) {
	const auto dof = 3.0 * static_cast<double>(state.positions.size() - 1);
	return dof > 0.0 ? 2.0 * kinetic_energy(state) / dof : 0.0;
}

void remove_center_of_mass_velocity(State& state) {
	Vec3 mean{};
	for (const auto& velocity : state.velocities) mean = add(mean, velocity);
	mean = scale(mean, 1.0 / static_cast<double>(state.velocities.size()));
	for (auto& velocity : state.velocities) velocity = sub(velocity, mean);
}

void apply_thermostat(State& state, const GR1Config& config) {
	const double current = temperature(state);
	if (current <= std::numeric_limits<double>::epsilon()) return;
	const double scale_factor = std::sqrt(1.0 + config.thermostat_relaxation * (config.target_temperature / current - 1.0));
	for (auto& velocity : state.velocities) velocity = scale(velocity, scale_factor);
	remove_center_of_mass_velocity(state);
}

void apply_volume_schedule(State& state, const GR1Config& config) {
	const std::uint32_t compression_steps = config.total_steps / 2;
	const double final_box_length = config.box_length * config.final_box_fraction;
	if (state.step >= compression_steps || compression_steps == 0) return;
	const double progress = static_cast<double>(state.step + 1) / compression_steps;
	const double new_box = config.box_length + progress * (final_box_length - config.box_length);
	const double scale_factor = new_box / state.box_length;
	for (auto& position : state.positions) position = scale(position, scale_factor);
	state.box_length = new_box;
}

void integrate_step(State& state, const GR1Config& config, ForceResult& force) {
	for (std::size_t i = 0; i < state.positions.size(); ++i) {
		state.velocities[i] = add(state.velocities[i], scale(force.forces[i], 0.5 * config.timestep));
		state.positions[i] = add(state.positions[i], scale(state.velocities[i], config.timestep));
		state.positions[i].x = wrap(state.positions[i].x, state.box_length);
		state.positions[i].y = wrap(state.positions[i].y, state.box_length);
		state.positions[i].z = wrap(state.positions[i].z, state.box_length);
	}
	apply_volume_schedule(state, config);
	force = evaluate_forces(state, config);
	for (std::size_t i = 0; i < state.positions.size(); ++i) {
		state.velocities[i] = add(state.velocities[i], scale(force.forces[i], 0.5 * config.timestep));
	}
	apply_thermostat(state, config);
	++state.step;
}

std::vector<std::uint32_t> clusters(const State& state, double& dominant_fraction) {
	const auto n = state.positions.size();
	std::vector<std::uint32_t> parent(n);
	std::iota(parent.begin(), parent.end(), 0);
	const auto root = [&parent](std::uint32_t node) {
		std::uint32_t current = node;
		while (parent[current] != current) current = parent[current];
		while (parent[node] != node) { const auto next = parent[node]; parent[node] = current; node = next; }
		return current;
	};
	const double capture2 = 1.45 * 1.45;
	for (std::uint32_t i = 0; i < n; ++i) {
		for (std::uint32_t j = i + 1; j < n; ++j) {
			const auto delta = minimum_image(sub(state.positions[i], state.positions[j]), state.box_length);
			if (dot(delta, delta) <= capture2) {
				const auto a = root(i), b = root(j);
				if (a != b) parent[b] = a;
			}
		}
	}
	std::vector<std::uint32_t> ids(n);
	std::vector<std::uint32_t> counts(n, 0);
	for (std::uint32_t i = 0; i < n; ++i) { ids[i] = root(i); ++counts[ids[i]]; }
	const auto maximum = *std::max_element(counts.begin(), counts.end());
	dominant_fraction = static_cast<double>(maximum) / static_cast<double>(n);
	return ids;
}

double dispersion(const State& state, const std::vector<std::uint32_t>& ids) {
	std::vector<std::uint32_t> counts(state.positions.size(), 0);
	for (const auto id : ids) ++counts[id];
	const auto dominant = static_cast<std::uint32_t>(std::distance(counts.begin(), std::max_element(counts.begin(), counts.end())));
	std::vector<Vec3> unwrapped;
	Vec3 anchor{};
	bool have_anchor = false;
	for (std::size_t i = 0; i < ids.size(); ++i) if (ids[i] == dominant) {
		if (!have_anchor) { anchor = state.positions[i]; have_anchor = true; }
		unwrapped.push_back(add(anchor, minimum_image(sub(state.positions[i], anchor), state.box_length)));
	}
	if (unwrapped.empty()) return state.box_length;
	Vec3 center{};
	for (const auto& point : unwrapped) center = add(center, point);
	center = scale(center, 1.0 / unwrapped.size());
	double squared = 0.0;
	for (const auto& point : unwrapped) squared += dot(sub(point, center), sub(point, center));
	return std::sqrt(squared / unwrapped.size());
}

double local_density(const State& state, double cluster_dispersion, double fraction) {
	const double radius = std::max(cluster_dispersion, 0.5);
	const double volume = 4.0 * pi * radius * radius * radius / 3.0;
	return (state.positions.size() * fraction) / volume;
}

double mean_squared_displacement(const State& state) {
	double value = 0.0;
	for (std::size_t i = 0; i < state.positions.size(); ++i) {
		const auto delta = minimum_image(sub(state.positions[i], state.initial_positions[i]), state.box_length);
		value += dot(delta, delta);
	}
	return value / state.positions.size();
}

double window_spread(const std::vector<TrajectorySample>& samples, double DiagnosticSample::*member) {
	if (samples.size() < 5) return std::numeric_limits<double>::infinity();
	const std::size_t begin = samples.size() - 5;
	double min_value = (samples[begin].diagnostics.*member), max_value = min_value;
	for (std::size_t i = begin + 1; i < samples.size(); ++i) {
		const double value = samples[i].diagnostics.*member;
		min_value = std::min(min_value, value);
		max_value = std::max(max_value, value);
	}
	return std::abs(max_value - min_value) / std::max(std::abs(max_value), 1.0e-12);
}

TrajectorySample sample_state(const State& state, const GR1Config& config, const ForceResult& force,
							   std::vector<TrajectorySample>& prior, double initial_density) {
	TrajectorySample sample;
	double dominant_fraction = 0.0;
	const auto ids = clusters(state, dominant_fraction);
	const double cluster_dispersion = dispersion(state, ids);
	const double density = local_density(state, cluster_dispersion, dominant_fraction);
	auto& d = sample.diagnostics;
	d.step = state.step;
	d.time = state.step * config.timestep;
	d.potential_energy = force.potential;
	d.kinetic_energy = kinetic_energy(state);
	d.total_energy = d.potential_energy + d.kinetic_energy;
	d.temperature = temperature(state);
	d.pressure = (state.positions.size() * d.temperature + force.virial / 3.0) / std::pow(state.box_length, 3.0);
	d.density = density;
	d.dominant_cluster_fraction = dominant_fraction;
	d.dispersion = cluster_dispersion;
	d.mean_squared_displacement = mean_squared_displacement(state);
	d.phase_indicator = dominant_fraction * initial_density / std::max(density, 1.0e-12);
	d.rdf_change = prior.empty() ? std::numeric_limits<double>::infinity() : std::abs(cluster_dispersion - prior.back().diagnostics.dispersion) / std::max(cluster_dispersion, 1.0e-12);
	d.temperature_spread = window_spread(prior, &DiagnosticSample::temperature);
	d.pressure_spread = window_spread(prior, &DiagnosticSample::pressure);
	d.liquid = density / initial_density >= config.phase.minimum_density_ratio
		&& dominant_fraction >= config.phase.minimum_dominant_cluster_fraction
		&& cluster_dispersion <= config.phase.maximum_dispersion
		&& d.mean_squared_displacement <= config.phase.maximum_mean_squared_displacement
		&& d.rdf_change <= config.phase.maximum_rdf_change
		&& d.temperature_spread <= config.phase.maximum_temperature_spread
		&& d.pressure_spread <= config.phase.maximum_pressure_spread;
	sample.particles.reserve(state.positions.size());
	for (std::size_t i = 0; i < state.positions.size(); ++i) sample.particles.push_back({state.positions[i], state.velocities[i], ids[i]});
	return sample;
}

State initial_state(const GR1Config& config) {
	State state;
	state.box_length = config.box_length;
	std::mt19937_64 rng(config.random_seed);
	std::uniform_real_distribution<double> jitter(-0.30, 0.30);
	const auto side = static_cast<std::uint32_t>(std::ceil(std::cbrt(config.particle_count)));
	const double spacing = config.box_length / side;
	for (std::uint32_t i = 0; i < config.particle_count; ++i) {
		const auto x = i % side, y = (i / side) % side, z = i / (side * side);
		state.positions.push_back({wrap((x + 0.5) * spacing + jitter(rng), config.box_length), wrap((y + 0.5) * spacing + jitter(rng), config.box_length), wrap((z + 0.5) * spacing + jitter(rng), config.box_length)});
		state.velocities.push_back({std::sin(static_cast<double>(i + 1) * 1.731 + config.random_seed), std::sin(static_cast<double>(i + 1) * 2.417 + config.random_seed), std::sin(static_cast<double>(i + 1) * 3.119 + config.random_seed)});
	}
	state.initial_positions = state.positions;
	remove_center_of_mass_velocity(state);
	const double current = temperature(state);
	const double scale_factor = std::sqrt(config.initial_temperature / current);
	for (auto& velocity : state.velocities) velocity = scale(velocity, scale_factor);
	return state;
}

bool matches(const State& left, const State& right, const GR1Config& config) {
	if (left.step != right.step || left.positions.size() != right.positions.size()) return false;
	for (std::size_t i = 0; i < left.positions.size(); ++i) {
		if (norm(sub(left.positions[i], right.positions[i])) > config.convergence.restart_position_tolerance
			|| norm(sub(left.velocities[i], right.velocities[i])) > config.convergence.restart_velocity_tolerance) return false;
	}
	return true;
}

std::string state_hash(const State& state) { return fnv1a_hash(serialize_state(state)); }

void write_checkpoint(const State& state, const std::filesystem::path& path) {
	std::ofstream file(path, std::ios::binary);
	file << serialize_state(state);
}

} // namespace

const char* to_string(CompletionState state) {
	switch (state) {
	case CompletionState::NumericallyConverged: return "NUMERICALLY_CONVERGED";
	case CompletionState::PhysicallyStable: return "PHYSICALLY_STABLE";
	case CompletionState::LiquidStateConfirmed: return "LIQUID_STATE_CONFIRMED";
	case CompletionState::HoldPeriodPassed: return "HOLD_PERIOD_PASSED";
	case CompletionState::ConservationPassed: return "CONSERVATION_PASSED";
	case CompletionState::RestartMatched: return "RESTART_MATCHED";
	case CompletionState::RenderStreamValid: return "RENDER_STREAM_VALID";
	case CompletionState::GoldenRunPassed: return "GOLDEN_RUN_PASSED";
	}
	return "UNKNOWN";
}

GR1Config load_gr1_config(const std::filesystem::path& input_path) {
	GR1Config config;
	std::ifstream input(input_path);
	if (!input) throw std::runtime_error("Cannot read GR-1 input deck: " + input_path.string());
	std::string line;
	while (std::getline(input, line)) {
		const auto comment = line.find('#');
		if (comment != std::string::npos) line.erase(comment);
		const auto equal = line.find('=');
		if (equal == std::string::npos) continue;
		const auto key = trim(line.substr(0, equal));
		const auto value = trim(line.substr(equal + 1));
		if (key == "run_id") config.run_id = value;
		else if (key == "scenario_version") config.scenario_version = value;
		else if (key == "particle_count") config.particle_count = static_cast<std::uint32_t>(std::stoul(value));
		else if (key == "species") config.species = value;
		else if (key == "molecular_identity") config.molecular_identity = value;
		else if (key == "technique") config.technique = value;
		else if (key == "random_seed") config.random_seed = std::stoull(value);
		else if (key == "initial_temperature") config.initial_temperature = std::stod(value);
		else if (key == "target_temperature") config.target_temperature = std::stod(value);
		else if (key == "box_length") config.box_length = std::stod(value);
		else if (key == "final_box_fraction") config.final_box_fraction = std::stod(value);
		else if (key == "timestep") config.timestep = std::stod(value);
		else if (key == "total_steps") config.total_steps = static_cast<std::uint32_t>(std::stoul(value));
		else if (key == "sample_interval") config.sample_interval = static_cast<std::uint32_t>(std::stoul(value));
		else if (key == "checkpoint_step") config.checkpoint_step = static_cast<std::uint32_t>(std::stoul(value));
		else if (key == "particle_radius") config.particle_radius = std::stod(value);
		else if (key == "sigma") config.sigma = std::stod(value);
		else if (key == "epsilon") config.epsilon = std::stod(value);
		else if (key == "cutoff") config.cutoff = std::stod(value);
		else if (key == "thermostat_relaxation") config.thermostat_relaxation = std::stod(value);
		else if (key == "integrator") config.integrator = value;
		else if (key == "interaction_model") config.interaction_model = value;
		else if (key == "ensemble") config.ensemble = value;
		else if (key == "relative_residual_tolerance") config.convergence.relative_residual = std::stod(value);
		else if (key == "momentum_tolerance") config.convergence.momentum_tolerance = std::stod(value);
		else if (key == "energy_drift_tolerance") config.convergence.energy_drift_tolerance = std::stod(value);
		else if (key == "restart_position_tolerance") config.convergence.restart_position_tolerance = std::stod(value);
		else if (key == "restart_velocity_tolerance") config.convergence.restart_velocity_tolerance = std::stod(value);
		else if (key == "minimum_density_ratio") config.phase.minimum_density_ratio = std::stod(value);
		else if (key == "minimum_dominant_cluster_fraction") config.phase.minimum_dominant_cluster_fraction = std::stod(value);
		else if (key == "maximum_dispersion") config.phase.maximum_dispersion = std::stod(value);
		else if (key == "maximum_mean_squared_displacement") config.phase.maximum_mean_squared_displacement = std::stod(value);
		else if (key == "maximum_rdf_change") config.phase.maximum_rdf_change = std::stod(value);
		else if (key == "maximum_temperature_spread") config.phase.maximum_temperature_spread = std::stod(value);
		else if (key == "maximum_pressure_spread") config.phase.maximum_pressure_spread = std::stod(value);
		else if (key == "render_fps") config.render_fps = static_cast<std::uint32_t>(std::stoul(value));
		else if (key == "hold_samples") config.phase.hold_samples = static_cast<std::uint32_t>(std::stoul(value));
	}
	if (config.particle_count < 2 || config.sample_interval == 0 || config.total_steps == 0 || config.checkpoint_step >= config.total_steps) throw std::runtime_error("Invalid GR-1 resolved input deck");
	return config;
}

GR1Result run_gr1(const GR1Config& config, bool write_artifacts, const std::filesystem::path& output_directory) {
	GR1Result result;
	result.config = config;
	State state = initial_state(config);
	ForceResult force = evaluate_forces(state, config);
	const double initial_energy = force.potential + kinetic_energy(state);
	State checkpoint;
	bool checkpoint_saved = false;
	const double initial_density = static_cast<double>(config.particle_count) / std::pow(config.box_length, 3.0);

	for (std::uint32_t step = 0; step <= config.total_steps; ++step) {
		if (step % config.sample_interval == 0 || step == config.total_steps) {
			auto sample = sample_state(state, config, force, result.trajectory, initial_density);
			result.trajectory.push_back(std::move(sample));
		}
		if (state.step == config.checkpoint_step) { checkpoint = state; checkpoint_saved = true; }
		if (step == config.total_steps) break;
		integrate_step(state, config, force);
	}

	const std::string checkpoint_bytes = serialize_state(checkpoint);
	State replay = deserialize_state(checkpoint_bytes, state.initial_positions);
	ForceResult replay_force = evaluate_forces(replay, config);
	while (checkpoint_saved && replay.step < config.total_steps) integrate_step(replay, config, replay_force);
	result.restart_matched = checkpoint_saved && matches(state, replay, config);
	result.final_state_hash = state_hash(state);

	std::ostringstream trajectory_bytes;
	for (const auto& sample : result.trajectory) {
		trajectory_bytes << std::setprecision(17) << sample.diagnostics.step << ' ' << sample.diagnostics.total_energy << ' ' << sample.diagnostics.temperature << '\n';
		for (const auto& particle : sample.particles) trajectory_bytes << particle.position.x << ' ' << particle.position.y << ' ' << particle.position.z << ' ' << particle.velocity.x << ' ' << particle.velocity.y << ' ' << particle.velocity.z << '\n';
	}
	result.trajectory_hash = fnv1a_hash(trajectory_bytes.str());

	const auto& first = result.trajectory.front().diagnostics;
	const auto& last = result.trajectory.back().diagnostics;
	result.solver.initial_residual = std::abs(first.temperature - config.target_temperature);
	result.solver.final_residual = std::abs(last.temperature - config.target_temperature);
	result.solver.relative_residual = result.solver.final_residual / std::max(result.solver.initial_residual, 1.0e-12);
	result.solver.step_count = config.total_steps;
	result.solver.tolerance = config.convergence.relative_residual;
	result.solver.stagnated = false;
	result.solver.termination_reason = "MAXIMUM_STEPS_REACHED_AFTER_HOLD_EVALUATION";

	result.conservation.initial_total_energy = initial_energy;
	result.conservation.final_total_energy = last.total_energy;
	result.conservation.relative_energy_drift = std::abs(last.total_energy - initial_energy) / std::max(std::abs(initial_energy), 1.0e-12);
	Vec3 momentum{};
	for (const auto& velocity : state.velocities) momentum = add(momentum, velocity);
	result.conservation.momentum_norm = norm(momentum);
	result.conservation.mass_conserved = true;
	result.conservation.species_balance_conserved = true;
	result.conservation.momentum_conserved = result.conservation.momentum_norm <= config.convergence.momentum_tolerance;
	result.conservation.energy_within_tolerance = result.conservation.relative_energy_drift <= config.convergence.energy_drift_tolerance;

	const bool numeric = result.solver.relative_residual <= config.convergence.relative_residual;
	const bool physical = last.temperature_spread <= config.phase.maximum_temperature_spread && last.pressure_spread <= config.phase.maximum_pressure_spread;
	const bool liquid = last.liquid;
	const bool hold = result.trajectory.size() >= config.phase.hold_samples && std::all_of(result.trajectory.end() - config.phase.hold_samples, result.trajectory.end(), [](const auto& sample) { return sample.diagnostics.liquid; });
	const bool conservation = result.conservation.mass_conserved && result.conservation.species_balance_conserved && result.conservation.momentum_conserved && result.conservation.energy_within_tolerance;
	result.render_stream_valid = config.render_fps == 60 && !result.trajectory.empty() && std::all_of(result.trajectory.begin(), result.trajectory.end(), [](const auto& sample) { return sample.particles.size() > 0; });
	if (numeric) result.completed_states.push_back(CompletionState::NumericallyConverged);
	if (physical) result.completed_states.push_back(CompletionState::PhysicallyStable);
	if (liquid) result.completed_states.push_back(CompletionState::LiquidStateConfirmed);
	if (hold) result.completed_states.push_back(CompletionState::HoldPeriodPassed);
	if (conservation) result.completed_states.push_back(CompletionState::ConservationPassed);
	if (result.restart_matched) result.completed_states.push_back(CompletionState::RestartMatched);
	if (result.render_stream_valid) result.completed_states.push_back(CompletionState::RenderStreamValid);
	result.golden_run_passed = numeric && physical && liquid && hold && conservation && result.restart_matched && result.render_stream_valid;
	if (result.golden_run_passed) result.completed_states.push_back(CompletionState::GoldenRunPassed);

	if (write_artifacts) {
		const auto directory = output_directory.empty() ? std::filesystem::path(config.run_id) : output_directory;
		std::filesystem::create_directories(directory);
		write_checkpoint(checkpoint, directory / "checkpoint.gr1state");
		write_gr1_artifacts(result, directory);
	}
	return result;
}

bool write_gr1_artifacts(const GR1Result& result, const std::filesystem::path& output_directory, const std::string& build_id, const std::string& commit_id) {
	std::filesystem::create_directories(output_directory);
	std::ofstream diagnostics(output_directory / "diagnostics.csv");
	diagnostics << "step,time,potential,kinetic,total,temperature,pressure,local_density,eta,dominant_cluster_fraction,dispersion,msd,rdf_change,temperature_spread,pressure_spread,liquid\n";
	std::ofstream trajectory(output_directory / "trajectory.jsonl");
	std::ofstream render(output_directory / "render_60fps.jsonl");
	if (!diagnostics || !trajectory || !render) return false;
	for (std::size_t frame = 0; frame < result.trajectory.size(); ++frame) {
		const auto& sample = result.trajectory[frame];
		const auto& d = sample.diagnostics;
		diagnostics << std::setprecision(17) << d.step << ',' << d.time << ',' << d.potential_energy << ',' << d.kinetic_energy << ',' << d.total_energy << ',' << d.temperature << ',' << d.pressure << ',' << d.density << ',' << d.phase_indicator << ',' << d.dominant_cluster_fraction << ',' << d.dispersion << ',' << d.mean_squared_displacement << ',' << d.rdf_change << ',' << d.temperature_spread << ',' << d.pressure_spread << ',' << d.liquid << '\n';
		trajectory << "{\"step\":" << d.step << ",\"time\":" << d.time << ",\"particles\":[";
		render << "{\"timestamp\":" << static_cast<double>(frame) / 60.0 << ",\"frame_number\":" << frame << ",\"particle_radius\":" << result.config.particle_radius << ",\"species\":" << quote_json(result.config.species) << ",\"phase_state\":" << quote_json(d.liquid ? "liquid" : "gas-transition") << ",\"diagnostics\":{\"temperature\":" << d.temperature << ",\"pressure\":" << d.pressure << ",\"eta\":" << d.phase_indicator << "},\"particles\":[";
		for (std::size_t i = 0; i < sample.particles.size(); ++i) {
			const auto& p = sample.particles[i];
			if (i != 0) { trajectory << ','; render << ','; }
			trajectory << "{\"position\":[" << p.position.x << ',' << p.position.y << ',' << p.position.z << "],\"velocity\":[" << p.velocity.x << ',' << p.velocity.y << ',' << p.velocity.z << "],\"cluster_id\":" << p.cluster_id << '}';
			render << "{\"world_position\":[" << p.position.x << ',' << p.position.y << ',' << p.position.z << "],\"cluster_id\":" << p.cluster_id << '}';
		}
		trajectory << "]}\n";
		render << "]}\n";
	}
	std::ofstream conservation(output_directory / "conservation_report.json");
	conservation << "{\"mass_conserved\":" << result.conservation.mass_conserved << ",\"species_balance_conserved\":" << result.conservation.species_balance_conserved << ",\"momentum_norm\":" << result.conservation.momentum_norm << ",\"relative_energy_drift\":" << result.conservation.relative_energy_drift << ",\"energy_within_tolerance\":" << result.conservation.energy_within_tolerance << "}\n";
	std::ofstream phase(output_directory / "phase_classification.json");
	const auto& d = result.trajectory.back().diagnostics;
	phase << "{\"liquid\":" << d.liquid << ",\"dominant_cluster_fraction\":" << d.dominant_cluster_fraction << ",\"local_density\":" << d.density << ",\"dispersion\":" << d.dispersion << ",\"mean_squared_displacement\":" << d.mean_squared_displacement << ",\"hold_samples\":" << result.config.phase.hold_samples << "}\n";
	std::ofstream restart(output_directory / "restart_comparison.json");
	restart << "{\"restart_matched\":" << result.restart_matched << ",\"position_tolerance\":" << result.config.convergence.restart_position_tolerance << ",\"velocity_tolerance\":" << result.config.convergence.restart_velocity_tolerance << "}\n";
	std::ofstream manifest(output_directory / "manifest.json");
	manifest << "{\n  \"run_id\": " << quote_json(result.config.run_id) << ",\n  \"golden_run\": \"GR-1\",\n  \"scenario_name\": " << quote_json(gr1_scenario_name) << ",\n  \"input_schema_version\": " << quote_json(gr1_schema_version) << ",\n  \"resolved_configuration\": {\"particle_count\":" << result.config.particle_count << ",\"species\":" << quote_json(result.config.species) << ",\"molecular_identity\":" << quote_json(result.config.molecular_identity) << ",\"technique\":" << quote_json(result.config.technique) << ",\"seed\":" << result.config.random_seed << ",\"initial_temperature\":" << result.config.initial_temperature << ",\"target_temperature\":" << result.config.target_temperature << ",\"box_length\":" << result.config.box_length << ",\"final_box_fraction\":" << result.config.final_box_fraction << ",\"timestep\":" << result.config.timestep << ",\"integrator\":" << quote_json(result.config.integrator) << ",\"interaction_model\":" << quote_json(result.config.interaction_model) << ",\"ensemble\":" << quote_json(result.config.ensemble) << ",\"render_fps\":" << result.config.render_fps << "},\n  \"phase_classification_criteria\": {\"minimum_density_ratio\":" << result.config.phase.minimum_density_ratio << ",\"minimum_dominant_cluster_fraction\":" << result.config.phase.minimum_dominant_cluster_fraction << ",\"maximum_dispersion\":" << result.config.phase.maximum_dispersion << ",\"maximum_mean_squared_displacement\":" << result.config.phase.maximum_mean_squared_displacement << ",\"maximum_rdf_change\":" << result.config.phase.maximum_rdf_change << ",\"maximum_temperature_spread\":" << result.config.phase.maximum_temperature_spread << ",\"maximum_pressure_spread\":" << result.config.phase.maximum_pressure_spread << ",\"hold_samples\":" << result.config.phase.hold_samples << "},\n  \"build_version\": " << quote_json(build_id) << ",\n  \"commit_identifier\": " << quote_json(commit_id) << ",\n  \"compiler_dependency_identity\": \"C++23 standard library\",\n  \"backend\": \"CPU deterministic reduced-unit LJ\",\n  \"hardware_summary\": \"runtime-independent CPU scalar path\",\n  \"checkpoint_lineage\": \"checkpoint.gr1state -> uninterrupted final state\",\n  \"trajectory_hash\": " << quote_json(result.trajectory_hash) << ",\n  \"final_state_hash\": " << quote_json(result.final_state_hash) << ",\n  \"solver\": {\"initial_residual\":" << result.solver.initial_residual << ",\"final_residual\":" << result.solver.final_residual << ",\"relative_residual\":" << result.solver.relative_residual << ",\"termination_reason\":" << quote_json(result.solver.termination_reason) << "},\n  \"completion_states\": [";
	for (std::size_t i = 0; i < result.completed_states.size(); ++i) { if (i) manifest << ','; manifest << quote_json(to_string(result.completed_states[i])); }
	manifest << "],\n  \"final_result\": " << quote_json(result.golden_run_passed ? "GOLDEN_RUN_PASSED" : "GOLDEN_RUN_FAILED") << "\n}\n";
	return true;
}

} // namespace atomistic::golden_run
