/**
 * thermal_loss_explorer.cpp — Molten-salt / channel thermal loss analysis
 * ======================================================================
 *
 * Tracks Tc (center), Th (hot wall / source), T_ambient, and cumulative
 * energy loss across discrete time frames for a 1-D radial annular channel.
 *
 * Features:
 *   - Sweeps multiple dt values to test temporal resolution sensitivity
 *   - Casts a wide initial parameter net (randomised Tc, Th, geometry)
 *   - Logs per-frame: Tc, Th, T_ambient, Q_loss, Q_cumulative, dT/dt
 *   - Computes convergence quality (RMS residual of energy balance)
 *   - Outputs CSV for each trial and a summary JSON ledger
 *
 * Physics (simplified 1-D radial steady-state + transient wrapper):
 *   Governing:  dT/dt = alpha * (d2T/dr2 + (1/r)*dT/dr) - h_loss*(T - T_amb)
 *   alpha = k / (rho * Cp)
 *   Q_loss per frame = h_eff * A * (T_surface - T_ambient) * dt
 *
 * Build (standalone, no project deps):
 *   g++ -std=c++23 -O2 -o thermal_loss_explorer thermal_loss_explorer.cpp
 *
 * Or via CMake (registered in CoreBuild.cmake as an app target).
 *
 * VSEPR-SIM 4.0.4
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <numeric>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <array>
#include <format>
#include <map>

namespace fs = std::filesystem;

// ============================================================================
// Constants
// ============================================================================

constexpr double R_UNIV   = 8.31446;     // J/(mol K)
constexpr double BOLTZ    = 1.380649e-23; // J/K
constexpr double PI       = 3.14159265358979323846;

// ============================================================================
// Material property set (salt or metal channel fill)
// ============================================================================

struct MaterialProps {
	std::string name;
	double k;        // W/(m K)   thermal conductivity
	double rho;      // kg/m^3    density
	double Cp;       // J/(kg K)  specific heat
	double mu;       // Pa s      viscosity  (for Re, not used in conduction-only)

	double alpha() const { return k / (rho * Cp); }
};

// Built-in material table (matches pykernel/plant_alpha2 values)
static const std::array<MaterialProps, 8> MATERIAL_DB {{
	{"FLiBe",           1.0,  1940.0, 2386.0, 0.006  },
	{"FLiNaK",          0.92, 2090.0, 1886.0, 0.0029 },
	{"NaCl_UCl3",       0.55, 3200.0, 1050.0, 0.003  },
	{"Cl_ThUPu",        0.42, 3800.0,  950.0, 0.004  },
	{"MSBR_ref",        1.0,  2290.0, 1357.0, 0.005  },
	{"NaCl_MgCl2_UCl3", 0.50, 2100.0, 1100.0, 0.0035 },
	{"Carbon_Steel",   50.0,  7850.0,  490.0, 0.0    },
	{"Alloy_Steel",    40.0,  7900.0,  500.0, 0.0    },
}};

// ============================================================================
// Channel geometry
// ============================================================================

struct ChannelGeom {
	double r_inner;  // m
	double r_outer;  // m
	double length;   // m  (axial, for surface area)

	double thickness() const { return r_outer - r_inner; }
	double area_inner() const { return 2.0 * PI * r_inner * length; }
	double area_outer() const { return 2.0 * PI * r_outer * length; }
};

// ============================================================================
// Per-frame snapshot
// ============================================================================

struct FrameData {
	int    frame;
	double time_s;
	double Tc;           // center temperature (K)
	double Th;           // hot boundary (K)
	double T_ambient;    // environment (K)
	double T_mid;        // midpoint radial node (K)
	double Q_loss_W;     // instantaneous heat loss (W)
	double Q_cumul_J;    // cumulative energy lost (J)
	double dTc_dt;       // rate of change of Tc (K/s)
	double rms_residual; // FD energy-balance residual
};

// ============================================================================
// Trial configuration
// ============================================================================

struct TrialConfig {
	int         trial_id;
	int         mat_index;
	ChannelGeom geom;
	double      Tc_init;     // initial center T
	double      Th_init;     // hot boundary T (held constant or decaying)
	double      T_ambient;   // environment T (constant)
	double      h_outer;     // outer convective coefficient W/(m^2 K)
	double      dt;          // time step (s)
	int         n_frames;    // how many frames to run
	int         N_radial;    // radial grid nodes
	bool        Th_decays;   // if true, Th loses energy too
};

// ============================================================================
// Trial result
// ============================================================================

struct TrialResult {
	TrialConfig              config;
	std::vector<FrameData>   frames;
	double                   final_Tc;
	double                   final_Th;
	double                   total_Q_loss_J;
	double                   rms_final;
	double                   elapsed_us;
	bool                     converged;  // rms < threshold at end
};

// ============================================================================
// 1-D radial thermal solver (explicit FD, Jacobi-style per frame)
// ============================================================================

TrialResult run_trial(const TrialConfig& cfg) {
	using Clock = std::chrono::high_resolution_clock;
	auto t0 = Clock::now();

	const auto& mat = MATERIAL_DB[cfg.mat_index];
	const double alpha = mat.alpha();
	const double dr = cfg.geom.thickness() / std::max(cfg.N_radial - 1, 1);

	// Radial node temperatures
	std::vector<double> T(cfg.N_radial);
	// Linear initial profile: Tc at inner, Th at outer
	for (int i = 0; i < cfg.N_radial; ++i) {
		double frac = static_cast<double>(i) / std::max(cfg.N_radial - 1, 1);
		T[i] = cfg.Tc_init + frac * (cfg.Th_init - cfg.Tc_init);
	}

	double Tc = cfg.Tc_init;
	double Th = cfg.Th_init;
	double Q_cumul = 0.0;

	// Stability limit for explicit scheme: dt_max = dr^2 / (2 * alpha)
	double dt_stable = (dr * dr) / (2.0 * alpha);
	int sub_steps = std::max(1, static_cast<int>(std::ceil(cfg.dt / dt_stable)));
	double dt_sub = cfg.dt / sub_steps;

	TrialResult result;
	result.config = cfg;
	result.frames.reserve(cfg.n_frames);

	for (int frame = 0; frame < cfg.n_frames; ++frame) {
		double time_s = frame * cfg.dt;

		// --- sub-step the explicit FD ---
		for (int s = 0; s < sub_steps; ++s) {
			std::vector<double> T_new = T;

			// BCs
			T_new[0] = Tc;
			T_new[cfg.N_radial - 1] = Th;

			// Interior: d2T/dr2 + (1/r) dT/dr
			for (int i = 1; i < cfg.N_radial - 1; ++i) {
				double r_i = cfg.geom.r_inner + i * dr;
				double d2T = (T[i + 1] - 2.0 * T[i] + T[i - 1]) / (dr * dr);
				double dT  = (T[i + 1] - T[i - 1]) / (2.0 * dr);
				T_new[i] = T[i] + alpha * dt_sub * (d2T + dT / r_i);
			}

			T = T_new;
		}

		// --- outer wall heat loss to ambient ---
		double T_surface = T[cfg.N_radial - 1];
		double Q_loss = cfg.h_outer * cfg.geom.area_outer()
						* std::max(T_surface - cfg.T_ambient, 0.0);
		Q_cumul += Q_loss * cfg.dt;

		// --- optional Th decay ---
		if (cfg.Th_decays) {
			double dTh = -Q_loss / (mat.rho * mat.Cp * cfg.geom.area_outer() * dr);
			Th += dTh * cfg.dt;
			Th = std::max(Th, cfg.T_ambient);
		}

		// --- Tc tracks inner node ---
		Tc = T[0];

		// --- update boundary nodes ---
		T[cfg.N_radial - 1] = Th;

		// --- RMS energy-balance residual ---
		double sum_sq = 0.0;
		for (int i = 1; i < cfg.N_radial - 1; ++i) {
			double r_i = cfg.geom.r_inner + i * dr;
			double d2T = (T[i + 1] - 2.0 * T[i] + T[i - 1]) / (dr * dr);
			double dT  = (T[i + 1] - T[i - 1]) / (2.0 * dr);
			double residual = d2T + dT / r_i;
			sum_sq += residual * residual;
		}
		double rms = std::sqrt(sum_sq / std::max(cfg.N_radial - 2, 1));

		double T_mid = T[cfg.N_radial / 2];
		double dTc_dt = (frame > 0)
			? (Tc - result.frames.back().Tc) / cfg.dt
			: 0.0;

		result.frames.push_back({
			frame, time_s, Tc, Th, cfg.T_ambient, T_mid,
			Q_loss, Q_cumul, dTc_dt, rms
		});
	}

	auto t1 = Clock::now();
	result.final_Tc = Tc;
	result.final_Th = Th;
	result.total_Q_loss_J = Q_cumul;
	result.rms_final = result.frames.back().rms_residual;
	result.converged = result.rms_final < 1e4; // residual is in T/m^2 units
	result.elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

	return result;
}

// ============================================================================
// CSV writer (one file per trial)
// ============================================================================

void write_trial_csv(const TrialResult& r, const fs::path& dir) {
	const auto& mat = MATERIAL_DB[r.config.mat_index];
	std::string fname = std::format("trial_{:04d}_{}_dt{:.0e}.csv",
									 r.config.trial_id, mat.name, r.config.dt);
	fs::path path = dir / fname;
	std::ofstream out(path);
	if (!out) { std::cerr << "  [ERR] cannot write " << path << "\n"; return; }

	out << "frame,time_s,Tc_K,Th_K,T_ambient_K,T_mid_K,"
		   "Q_loss_W,Q_cumul_J,dTc_dt_Ks,rms_residual\n";
	out << std::fixed;
	for (const auto& f : r.frames) {
		out << f.frame << ","
			<< std::setprecision(6) << f.time_s << ","
			<< std::setprecision(2) << f.Tc << ","
			<< f.Th << "," << f.T_ambient << "," << f.T_mid << ","
			<< std::setprecision(4) << f.Q_loss_W << ","
			<< std::setprecision(4) << f.Q_cumul_J << ","
			<< std::setprecision(6) << f.dTc_dt << ","
			<< std::scientific << std::setprecision(4) << f.rms_residual
			<< "\n";
	}
	out.close();
}

// ============================================================================
// Summary JSON writer
// ============================================================================

void write_summary_json(const std::vector<TrialResult>& results,
						const fs::path& path) {
	std::ofstream out(path);
	out << "[\n";
	for (size_t i = 0; i < results.size(); ++i) {
		const auto& r = results[i];
		const auto& c = r.config;
		const auto& mat = MATERIAL_DB[c.mat_index];
		out << "  {\n"
			<< "    \"trial\": "        << c.trial_id << ",\n"
			<< "    \"material\": \""   << mat.name << "\",\n"
			<< "    \"Tc_init\": "      << c.Tc_init << ",\n"
			<< "    \"Th_init\": "      << c.Th_init << ",\n"
			<< "    \"T_ambient\": "    << c.T_ambient << ",\n"
			<< "    \"dt\": "           << c.dt << ",\n"
			<< "    \"n_frames\": "     << c.n_frames << ",\n"
			<< "    \"N_radial\": "     << c.N_radial << ",\n"
			<< "    \"r_inner\": "      << c.geom.r_inner << ",\n"
			<< "    \"r_outer\": "      << c.geom.r_outer << ",\n"
			<< "    \"h_outer\": "      << c.h_outer << ",\n"
			<< "    \"Th_decays\": "    << (c.Th_decays ? "true" : "false") << ",\n"
			<< "    \"final_Tc\": "     << std::fixed << std::setprecision(3) << r.final_Tc << ",\n"
			<< "    \"final_Th\": "     << r.final_Th << ",\n"
			<< "    \"total_Q_loss_J\": " << std::setprecision(2) << r.total_Q_loss_J << ",\n"
			<< "    \"rms_final\": "    << std::scientific << std::setprecision(4) << r.rms_final << ",\n"
			<< "    \"converged\": "    << (r.converged ? "true" : "false") << ",\n"
			<< "    \"elapsed_us\": "   << std::fixed << std::setprecision(1) << r.elapsed_us << "\n"
			<< "  }" << (i + 1 < results.size() ? "," : "") << "\n";
	}
	out << "]\n";
	out.close();
}

// ============================================================================
// Random net caster
// ============================================================================

std::vector<TrialConfig> cast_random_net(int n_trials, uint32_t seed) {
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> u01(0.0, 1.0);

	// dt values to sweep
	std::vector<double> dt_choices = {0.0001, 0.001, 0.005, 0.01, 0.05, 0.1};

	std::vector<TrialConfig> configs;
	configs.reserve(n_trials);

	for (int i = 0; i < n_trials; ++i) {
		TrialConfig cfg{};
		cfg.trial_id = i;

		// random material
		std::uniform_int_distribution<int> mat_dist(0, static_cast<int>(MATERIAL_DB.size()) - 1);
		cfg.mat_index = mat_dist(rng);

		// wide initial net for temperatures
		cfg.Tc_init   = 500.0  + u01(rng) * 800.0;   // 500 - 1300 K
		cfg.Th_init   = cfg.Tc_init + 50.0 + u01(rng) * 400.0; // Th > Tc
		cfg.T_ambient = 250.0  + u01(rng) * 100.0;   // 250 - 350 K

		// geometry
		cfg.geom.r_inner = 0.005 + u01(rng) * 0.015; // 5-20 mm
		cfg.geom.r_outer = cfg.geom.r_inner + 0.01 + u01(rng) * 0.06; // 10-70 mm gap
		cfg.geom.length  = 0.5 + u01(rng) * 2.0;     // 0.5-2.5 m axial

		// outer convection
		cfg.h_outer = 5.0 + u01(rng) * 50.0;         // 5-55 W/(m^2 K)

		// dt: cycle through choices, then also random
		if (i < static_cast<int>(dt_choices.size())) {
			cfg.dt = dt_choices[i];
		} else {
			std::uniform_int_distribution<int> dt_idx(0, static_cast<int>(dt_choices.size()) - 1);
			cfg.dt = dt_choices[dt_idx(rng)];
		}

		cfg.n_frames  = 200;
		cfg.N_radial  = 16 + static_cast<int>(u01(rng) * 48); // 16-64 nodes

		// half the trials let Th decay
		cfg.Th_decays = (u01(rng) > 0.5);

		configs.push_back(cfg);
	}

	return configs;
}

// ============================================================================
// Print progress table
// ============================================================================

void print_trial_line(const TrialResult& r) {
	const auto& c = r.config;
	const auto& mat = MATERIAL_DB[c.mat_index];
	const char* tag = r.converged ? "OK" : "!!";

	std::cout << std::format("  {:4d}  {:20s}  Tc={:7.1f}  Th={:7.1f}  Ta={:5.1f}  "
							 "dt={:.0e}  N={:3d}  Q={:10.1f} J  rms={:.3e}  [{:s}]  {:.0f} us\n",
		c.trial_id, mat.name,
		c.Tc_init, c.Th_init, c.T_ambient,
		c.dt, c.N_radial,
		r.total_Q_loss_J, r.rms_final,
		tag, r.elapsed_us);
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
	int n_trials    = 48;       // default: 48 random trials
	uint32_t seed   = 2025;
	std::string out_dir = "out/reports/thermal_loss_explorer";

	// simple arg parsing
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--trials" && i + 1 < argc)  n_trials = std::stoi(argv[++i]);
		if (arg == "--seed"   && i + 1 < argc)  seed = static_cast<uint32_t>(std::stoi(argv[++i]));
		if (arg == "--out"    && i + 1 < argc)  out_dir = argv[++i];
	}

	fs::create_directories(out_dir);

	std::cout << "\n";
	std::cout << "========================================================================\n";
	std::cout << "  THERMAL LOSS EXPLORER -- Tc / Th / T_ambient energy tracking\n";
	std::cout << "  Trials: " << n_trials << "  Seed: " << seed << "\n";
	std::cout << "  dt sweep: 0.0001 .. 0.1 s    N_radial: 16..64\n";
	std::cout << "  Output: " << out_dir << "\n";
	std::cout << "========================================================================\n\n";

	auto configs = cast_random_net(n_trials, seed);
	std::vector<TrialResult> results;
	results.reserve(n_trials);

	for (auto& cfg : configs) {
		auto r = run_trial(cfg);
		print_trial_line(r);
		write_trial_csv(r, out_dir);
		results.push_back(std::move(r));
	}

	// --- summary ---
	write_summary_json(results, fs::path(out_dir) / "summary.json");

	int n_conv = 0;
	double best_rms = 1e30;
	int best_id = -1;
	for (const auto& r : results) {
		if (r.converged) ++n_conv;
		if (r.rms_final < best_rms) { best_rms = r.rms_final; best_id = r.config.trial_id; }
	}

	std::cout << "\n";
	std::cout << "========================================================================\n";
	std::cout << "  COMPLETE: " << n_conv << "/" << n_trials << " converged\n";
	std::cout << "  Best RMS: " << std::scientific << std::setprecision(4) << best_rms
			  << " (trial " << best_id << " -- "
			  << MATERIAL_DB[results[best_id].config.mat_index].name << ")\n";

	// dt sensitivity summary
	std::cout << "\n  dt SENSITIVITY:\n";
	std::map<double, std::pair<double, int>> dt_stats; // dt -> (sum_rms, count)
	for (const auto& r : results) {
		auto& [sum, cnt] = dt_stats[r.config.dt];
		sum += r.rms_final;
		cnt += 1;
	}
	for (const auto& [dt, pair] : dt_stats) {
		auto [sum, cnt] = pair;
		std::cout << std::format("    dt={:.0e}  avg_rms={:.3e}  n={}\n",
								  dt, sum / cnt, cnt);
	}

	// material summary
	std::cout << "\n  MATERIAL SUMMARY:\n";
	std::map<int, std::pair<double, int>> mat_stats;
	for (const auto& r : results) {
		auto& [sum, cnt] = mat_stats[r.config.mat_index];
		sum += r.rms_final;
		cnt += 1;
	}
	for (const auto& [idx, pair] : mat_stats) {
		auto [sum, cnt] = pair;
		std::cout << std::format("    {:20s}  avg_rms={:.3e}  n={}\n",
								  MATERIAL_DB[idx].name, sum / cnt, cnt);
	}

	std::cout << "\n  Output: " << out_dir << "\n";
	std::cout << "========================================================================\n\n";

	return 0;
}
