#pragma once
/**
 * property_trainer.hpp  -  Property-based training session over the continual runner.
 *
 * Wraps the continual runner loop with:
 *   1. Randomized formula generation (property-based: draws from a parametric
 *      formula space with configurable element mix and atom count range)
 *   2. Physical invariant checking on every result (via property_invariant.hpp)
 *   3. Live ASCII terminal dashboard (refreshed every N formations)
 *   4. chart_data exports (CSV + JSON) for downstream analysis / plotting
 *
 * The trainer is self-contained: it owns the run loop, the accumulator, and
 * the visual output.  It is NOT a wrapper around continual_runner.cpp; it
 * reimplements the minimal run kernel inline to avoid a binary dependency.
 * (continual_runner.cpp is an app-level entry point, not a library.)
 *
 * Usage:
 *   #include "training/property_trainer.hpp"
 *   using namespace vsepr::training;
 *
 *   TrainerConfig cfg;
 *   cfg.max_formations = 500;
 *   cfg.dashboard_every = 20;
 *   cfg.export_dir = "train_out";
 *
 *   PropertyTrainer trainer(cfg);
 *   trainer.run();                // blocks until done or STOP file appears
 *
 * VSEPR-SIM v5.0.0-main | WO-VSIM-74-PROP-TRAIN
 */

#include "training/property_invariant.hpp"
#include "chart/chart_data.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <functional>
#include <thread>

namespace fs = std::filesystem;

namespace vsepr {
namespace training {

// ============================================================================
// TrainerConfig
// ============================================================================

struct TrainerConfig {
	// Run budget
	int    max_formations    = 200;       // 0 = run forever until STOP
	int    seeds_per_formula = 3;         // ensemble width
	int    fire_steps        = 1000;      // FIRE steps per formation
	double eps_force         = 1e-4;      // FIRE convergence criterion
	double eps_energy        = 1e-8;

	// Formula generator
	int    min_atoms         = 2;
	int    max_atoms         = 12;
	// Element pool (symbol → Z); drawn weighted by natural abundance proxy
	std::vector<std::pair<std::string,int>> element_pool = {
		{"H",1},{"C",6},{"N",7},{"O",8},{"F",9},
		{"S",16},{"Cl",17},{"Na",11},{"Mg",12},{"Al",13},{"Si",14}
	};

	// Visual output
	int    dashboard_every   = 10;        // refresh dashboard every N formations
	int    dashboard_width   = 72;        // terminal column width
	bool   color_ansi        = true;      // ANSI color codes in dashboard

	// Export
	std::string export_dir   = "property_train_out";
	bool        export_csv   = true;
	bool        export_json  = true;
	std::string stop_file    = "STOP";    // presence of this file ends the run

	// Invariant configuration
	bool   skip_vacuous      = true;      // skip invariants vacuously true (e.g. INV-03 on non-converged)
	double warn_margin_below = 0.05;      // highlight invariant if margin < this
};

// ============================================================================
// SessionStats  -  accumulated session statistics
// ============================================================================

struct SessionStats {
	int total_formations = 0;
	int total_converged  = 0;
	int total_stable     = 0;
	int total_failed_any_invariant = 0;

	// Per-invariant pass counts
	std::map<std::string, int> inv_pass;
	std::map<std::string, int> inv_total;
	std::map<std::string, double> inv_worst_margin;

	// Energy distribution (running moments)
	double sum_e_per_atom   = 0.0;
	double sum_e_per_atom_sq= 0.0;
	double min_e_per_atom   =  1e18;
	double max_e_per_atom   = -1e18;

	// RMS force distribution
	double sum_rms_force    = 0.0;
	double min_rms_force    =  1e18;
	double max_rms_force    = -1e18;

	// Wall time
	double sum_wall_ms      = 0.0;
	double max_wall_ms      = 0.0;

	// Session start
	std::chrono::steady_clock::time_point start =
		std::chrono::steady_clock::now();

	double elapsed_s() const {
		return std::chrono::duration<double>(
			std::chrono::steady_clock::now() - start).count();
	}
	double rate() const {
		double t = elapsed_s();
		return t > 0.0 ? total_formations / t : 0.0;
	}
	double conv_rate() const {
		return total_formations > 0
			? 100.0 * total_converged / total_formations : 0.0;
	}
	double stable_rate() const {
		return total_formations > 0
			? 100.0 * total_stable / total_formations : 0.0;
	}
	double inv_pass_rate(const std::string& id) const {
		auto it = inv_total.find(id);
		if (it == inv_total.end() || it->second == 0) return 0.0;
		auto ip = inv_pass.find(id);
		int p = (ip != inv_pass.end()) ? ip->second : 0;
		return 100.0 * p / it->second;
	}
	double mean_e_per_atom() const {
		return total_formations > 0 ? sum_e_per_atom / total_formations : 0.0;
	}
	double mean_rms_force() const {
		return total_formations > 0 ? sum_rms_force / total_formations : 0.0;
	}
};

// ============================================================================
// FormationRecord  -  one row in the training ledger (for chart export)
// ============================================================================

struct FormationRecord {
	int         idx;
	std::string formula;
	uint32_t    seed;
	int         num_atoms;
	int         steps_taken;
	double      energy_per_atom;
	double      rms_force;
	double      wall_time_ms;
	bool        converged;
	std::string classification;
	int         invariants_passed;
	int         invariants_total;
	std::string timestamp;
};

// ============================================================================
// Minimal self-contained FIRE integrator kernel
//   (mirrors continual_runner's run_formation() without the binary dep)
// ============================================================================

namespace detail {

static const std::map<std::string,int> SYM_Z = {
	{"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
	{"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
	{"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Fe",26},{"Cu",29},
	{"Zn",30},{"Br",35},
};
static const std::map<int,double> ATOMIC_MASS = {
	{1,1.008},{2,4.003},{3,6.941},{4,9.012},{5,10.81},{6,12.011},{7,14.007},
	{8,15.999},{9,18.998},{10,20.18},{11,22.99},{12,24.305},{13,26.982},
	{14,28.086},{15,30.974},{16,32.065},{17,35.453},{18,39.948},{19,39.098},
	{20,40.078},{26,55.845},{29,63.546},{30,65.38},{35,79.904},
};

// LJ parameters (epsilon kcal/mol, sigma Å) keyed by Z
struct LJParam { double eps, sigma; };
static const std::map<int,LJParam> LJ = {
	{1, {0.030, 2.50}}, {6, {0.086, 3.40}}, {7, {0.170, 3.25}},
	{8, {0.210, 3.07}}, {9, {0.061, 2.90}}, {11,{0.030, 2.27}},
	{12,{0.111, 2.72}}, {13,{0.505, 4.01}}, {14,{0.402, 3.80}},
	{15,{0.200, 3.70}}, {16,{0.395, 3.56}}, {17,{0.265, 3.47}},
	{18,{0.238, 3.40}}, {19,{0.035, 2.71}}, {20,{0.238, 2.83}},
	{26,{0.013, 2.91}}, {29,{0.005, 2.87}}, {30,{0.125, 2.76}},
	{35,{0.320, 3.73}},
};

struct Atom { int Z; double x,y,z,fx,fy,fz,vx,vy,vz,mass; };

inline double lj_energy_force(std::vector<Atom>& atoms) {
	double E = 0.0;
	for (auto& a : atoms) { a.fx = a.fy = a.fz = 0.0; }
	int N = (int)atoms.size();
	for (int i = 0; i < N-1; ++i) {
		auto& ai = atoms[i];
		auto lji_it = LJ.find(ai.Z);
		const LJParam& lji = (lji_it != LJ.end()) ? lji_it->second : LJ.at(6);
		for (int j = i+1; j < N; ++j) {
			auto& aj = atoms[j];
			auto ljj_it = LJ.find(aj.Z);
			const LJParam& ljj = (ljj_it != LJ.end()) ? ljj_it->second : LJ.at(6);
			// Lorentz-Berthelot mixing
			double eps   = std::sqrt(lji.eps * ljj.eps);
			double sigma = 0.5*(lji.sigma + ljj.sigma);
			double dx = ai.x - aj.x, dy = ai.y - aj.y, dz = ai.z - aj.z;
			double r2 = dx*dx + dy*dy + dz*dz;
			if (r2 < 1e-4) r2 = 1e-4;
			double sr2  = sigma*sigma / r2;
			double sr6  = sr2*sr2*sr2;
			double sr12 = sr6*sr6;
			E += 4.0*eps*(sr12 - sr6);
			double dE = 24.0*eps*(2.0*sr12 - sr6) / r2;
			ai.fx += dE*dx; ai.fy += dE*dy; ai.fz += dE*dz;
			aj.fx -= dE*dx; aj.fy -= dE*dy; aj.fz -= dE*dz;
		}
	}
	return E;
}

// Parse formula string e.g. "H2O", "C6H12O6" into element counts
inline std::vector<std::pair<std::string,int>> parse_formula(const std::string& f) {
	std::vector<std::pair<std::string,int>> result;
	int i = 0, n = (int)f.size();
	while (i < n) {
		if (!std::isupper((unsigned char)f[i])) { ++i; continue; }
		std::string sym(1, f[i++]);
		while (i < n && std::islower((unsigned char)f[i])) sym += f[i++];
		int cnt = 0;
		while (i < n && std::isdigit((unsigned char)f[i]))
			cnt = cnt*10 + (f[i++]-'0');
		if (cnt == 0) cnt = 1;
		// merge with existing entry
		bool found = false;
		for (auto& p : result) if (p.first == sym) { p.second += cnt; found = true; break; }
		if (!found) result.push_back({sym, cnt});
	}
	return result;
}

struct FireResult {
	double energy;
	double energy_per_atom;
	double rms_force;
	double max_force;
	double alpha_final;
	double dt_final;
	int    steps_taken;
	bool   converged;
	std::string classification;
	double wall_ms;
};

inline FireResult run_fire(
	const std::string& formula,
	uint32_t seed,
	int max_steps,
	double eps_force,
	double /*eps_energy*/)
{
	auto t0 = std::chrono::steady_clock::now();

	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> pos_dist(-5.0, 5.0);

	// Build atom list from formula
	auto elems = parse_formula(formula);
	std::vector<Atom> atoms;
	for (auto& [sym, cnt] : elems) {
		auto it = SYM_Z.find(sym);
		int Z = (it != SYM_Z.end()) ? it->second : 6;
		auto mit = ATOMIC_MASS.find(Z);
		double mass = (mit != ATOMIC_MASS.end()) ? mit->second : 12.0;
		for (int k = 0; k < cnt; ++k) {
			Atom a{};
			a.Z = Z; a.mass = mass;
			a.x = pos_dist(rng); a.y = pos_dist(rng); a.z = pos_dist(rng);
			atoms.push_back(a);
		}
	}

	if (atoms.empty()) {
		return {0,0,0,0,0.1,0.01,0,false,"fragment",0.0};
	}

	int N = (int)atoms.size();

	// FIRE parameters
	double dt     = 0.01;
	double alpha  = 0.1;
	double dt_max = 0.1;
	int    N_min  = 5;
	double f_inc  = 1.1, f_dec = 0.5, alpha_start = 0.1, f_alpha = 0.99;
	int    n_pos  = 0;
	double E      = lj_energy_force(atoms);

	bool converged = false;
	int step = 0;
	for (; step < max_steps; ++step) {
		// Velocity Verlet half-step
		for (auto& a : atoms) {
			a.vx += 0.5*dt*a.fx/a.mass;
			a.vy += 0.5*dt*a.fy/a.mass;
			a.vz += 0.5*dt*a.fz/a.mass;
			a.x  += dt*a.vx;
			a.y  += dt*a.vy;
			a.z  += dt*a.vz;
		}
		double E_new = lj_energy_force(atoms);
		for (auto& a : atoms) {
			a.vx += 0.5*dt*a.fx/a.mass;
			a.vy += 0.5*dt*a.fy/a.mass;
			a.vz += 0.5*dt*a.fz/a.mass;
		}

		// FIRE mixing
		double P = 0.0, vnorm = 0.0, fnorm = 0.0;
		for (auto& a : atoms) {
			P     += a.vx*a.fx + a.vy*a.fy + a.vz*a.fz;
			vnorm += a.vx*a.vx + a.vy*a.vy + a.vz*a.vz;
			fnorm += a.fx*a.fx + a.fy*a.fy + a.fz*a.fz;
		}
		vnorm = std::sqrt(vnorm); fnorm = std::sqrt(fnorm);

		if (fnorm > 1e-30) {
			double scale = alpha * vnorm / fnorm;
			for (auto& a : atoms) {
				a.vx = (1.0-alpha)*a.vx + scale*a.fx;
				a.vy = (1.0-alpha)*a.vy + scale*a.fy;
				a.vz = (1.0-alpha)*a.vz + scale*a.fz;
			}
		}

		if (P > 0.0) {
			if (++n_pos >= N_min) {
				dt = std::min(dt*f_inc, dt_max);
				alpha *= f_alpha;
			}
		} else {
			n_pos = 0;
			dt    *= f_dec;
			alpha  = alpha_start;
			for (auto& a : atoms) a.vx = a.vy = a.vz = 0.0;
		}

		double rms_f = (N > 0) ? fnorm / std::sqrt((double)N) : fnorm;
		if (rms_f < eps_force) { converged = true; E = E_new; break; }
		E = E_new;
	}

	// Compute final forces/stats
	lj_energy_force(atoms);
	double rms_f = 0.0, max_f = 0.0;
	for (auto& a : atoms) {
		double f2 = a.fx*a.fx + a.fy*a.fy + a.fz*a.fz;
		rms_f += f2;
		max_f  = std::max(max_f, std::sqrt(f2));
	}
	rms_f = std::sqrt(rms_f / std::max(N,1));

	double E_per = (N > 0) ? E / N : E;
	auto wall_ms = std::chrono::duration<double,std::milli>(
		std::chrono::steady_clock::now() - t0).count();

	std::string cls;
	if (step >= max_steps)         cls = "timeout";
	else if (!std::isfinite(E))    cls = "collapsed";
	else if (E_per > 1000.0)       cls = "collapsed";
	else if (rms_f < 1e-3 && E_per < 0.0) cls = "stable";
	else if (rms_f < 1e-2)        cls = "metastable";
	else                           cls = "unstable";

	return { E, E_per, rms_f, max_f, alpha, dt, step, converged, cls, wall_ms };
}

} // namespace detail

// ============================================================================
// ASCII Dashboard
// ============================================================================

namespace dashboard {

static const char* ANSI_RESET  = "\033[0m";
static const char* ANSI_BOLD   = "\033[1m";
static const char* ANSI_GREEN  = "\033[32m";
static const char* ANSI_YELLOW = "\033[33m";
static const char* ANSI_RED    = "\033[31m";
static const char* ANSI_CYAN   = "\033[36m";
static const char* ANSI_GREY   = "\033[90m";

inline std::string color(const char* code, bool enabled) {
	return enabled ? code : "";
}

inline std::string bar(double fraction, int width, char fill='#', char empty=' ') {
	int filled = static_cast<int>(std::round(std::clamp(fraction, 0.0, 1.0) * width));
	return std::string(filled, fill) + std::string(width - filled, empty);
}

inline void render(
	const SessionStats& stats,
	const InvariantRegistry& reg,
	const TrainerConfig& cfg,
	const FormationSnapshot& last,
	const InvariantSummary& last_summary)
{
	bool c = cfg.color_ansi;
	int W = cfg.dashboard_width;
	std::string sep(W, '=');
	std::string sep2(W, '-');

	// Clear screen (ANSI move-to-top; works in most terminals)
	if (c) std::cout << "\033[2J\033[H";
	else   std::cout << "\n\n";

	// Header
	std::cout << color(ANSI_BOLD, c) << color(ANSI_CYAN, c)
			  << sep << "\n"
			  << "  VSEPR-SIM  |  PROPERTY-BASED TRAINING MONITOR\n"
			  << sep << "\n"
			  << color(ANSI_RESET, c);

	// Session overview
	std::cout << std::fixed << std::setprecision(1);
	std::cout << "  Formations : " << color(ANSI_BOLD,c)
			  << stats.total_formations << color(ANSI_RESET,c);
	if (cfg.max_formations > 0)
		std::cout << " / " << cfg.max_formations;
	std::cout << "   |   Rate: " << stats.rate() << "/s"
			  << "   |   Elapsed: " << stats.elapsed_s() << "s\n";

	// Convergence bar
	double conv_frac  = stats.total_formations > 0
						? (double)stats.total_converged  / stats.total_formations : 0.0;
	double stable_frac= stats.total_formations > 0
						? (double)stats.total_stable / stats.total_formations : 0.0;
	int bw = W - 22;
	std::cout << "  Converged  : [" << color(ANSI_GREEN,c)
			  << bar(conv_frac, bw) << color(ANSI_RESET,c) << "] "
			  << std::setprecision(1) << conv_frac*100.0 << "%\n";
	std::cout << "  Stable     : [" << color(ANSI_CYAN,c)
			  << bar(stable_frac, bw) << color(ANSI_RESET,c) << "] "
			  << stable_frac*100.0 << "%\n";

	// Invariant pass rates
	std::cout << "\n" << color(ANSI_BOLD,c)
			  << "  INVARIANTS\n" << color(ANSI_RESET,c)
			  << "  " << sep2 << "\n";
	for (const auto& inv : reg.invariants) {
		double pr = stats.inv_pass_rate(inv.id);
		auto wm_it = stats.inv_worst_margin.find(inv.id);
		double wm = (wm_it != stats.inv_worst_margin.end()) ? wm_it->second : 0.0;

		const char* col = ANSI_GREEN;
		if (pr < 99.0) col = ANSI_YELLOW;
		if (pr < 90.0) col = ANSI_RED;

		std::cout << "  " << color(ANSI_BOLD,c) << inv.id << color(ANSI_RESET,c)
				  << "  " << std::left << std::setw(28) << inv.name
				  << color(col,c)
				  << std::right << std::setw(6) << std::setprecision(1) << pr << "%"
				  << color(ANSI_RESET,c)
				  << color(ANSI_GREY,c)
				  << "  margin=" << std::setprecision(4) << wm
				  << color(ANSI_RESET,c) << "\n";
	}

	// Energy distribution
	std::cout << "\n" << color(ANSI_BOLD,c)
			  << "  ENERGY / ATOM  (kcal/mol)\n" << color(ANSI_RESET,c);
	if (stats.total_formations > 0) {
		double mn = stats.min_e_per_atom;
		double mx = stats.max_e_per_atom;
		double me = stats.mean_e_per_atom();
		double range = std::max(mx - mn, 1e-6);
		// 5-bucket histogram from running stats
		std::cout << "  min=" << std::setprecision(3) << mn
				  << "  mean=" << me
				  << "  max=" << mx << "\n";
		// Simple ASCII number line
		int nl = W - 12;
		double mean_pos = (me - mn) / range;
		int mpos = static_cast<int>(mean_pos * nl);
		std::string line(nl, '-');
		if (mpos >= 0 && mpos < nl) line[mpos] = '^';
		std::cout << "  [" << line << "]\n";
		std::cout << "  " << std::setprecision(2) << mn
				  << std::string(nl/2 - 4, ' ') << "  mean  "
				  << std::string(nl/2 - 4, ' ') << mx << "\n";
	}

	// Last result
	if (stats.total_formations > 0) {
		std::cout << "\n" << color(ANSI_BOLD,c)
				  << "  LAST RESULT\n" << color(ANSI_RESET,c);
		const char* cls_col =
			last.classification == "stable"     ? ANSI_GREEN  :
			last.classification == "metastable" ? ANSI_YELLOW : ANSI_RED;
		std::cout << "  Formula : " << last.formula
				  << "  N=" << last.num_atoms
				  << "  seed=" << last.seed << "\n"
				  << "  Class   : " << color(cls_col,c) << last.classification
				  << color(ANSI_RESET,c)
				  << "  E/N=" << std::setprecision(3) << last.energy_per_atom
				  << " kcal/mol  rms_F=" << std::scientific << std::setprecision(2)
				  << last.rms_force << "\n"
				  << "  Invs    : " << last_summary.passed << "/"
				  << last_summary.total << " passed";
		if (last_summary.failed > 0)
			std::cout << color(ANSI_RED,c) << "  [" << last_summary.failed
					  << " FAILED]" << color(ANSI_RESET,c);
		std::cout << "\n";
	}

	// Failed invariant details (last result)
	for (const auto& r : last_summary.results) {
		if (!r.passed) {
			std::cout << color(ANSI_RED,c)
					  << "    ! " << r.invariant_id << " FAIL: " << r.message
					  << color(ANSI_RESET,c) << "\n";
		}
	}

	std::cout << color(ANSI_GREY,c)
			  << "\n  Export: " << cfg.export_dir
			  << "   Stop: place file '" << cfg.stop_file << "' to halt\n"
			  << color(ANSI_RESET,c)
			  << sep << "\n" << std::flush;
}

} // namespace dashboard

// ============================================================================
// PropertyTrainer
// ============================================================================

class PropertyTrainer {
public:
	explicit PropertyTrainer(TrainerConfig cfg = {})
		: cfg_(std::move(cfg))
		, reg_(InvariantRegistry::default_registry())
		, rng_(std::random_device{}())
	{}

	// Run the training loop.  Blocks until max_formations reached or STOP file.
	void run() {
		fs::create_directories(cfg_.export_dir);
		fs::create_directories(fs::path(cfg_.export_dir) / "results");

		open_ledger();

		FormationSnapshot last_snap;
		InvariantSummary  last_summary;

		while (should_continue()) {
			// Generate a random formula
			std::string formula = generate_formula();

			for (int s = 0; s < cfg_.seeds_per_formula && should_continue(); ++s) {
				uint32_t seed = static_cast<uint32_t>(
					rng_() ^ (uint32_t)(stats_.total_formations * 7 + s * 13));

				auto fr = detail::run_fire(
					formula, seed, cfg_.fire_steps,
					cfg_.eps_force, cfg_.eps_energy);

				FormationSnapshot snap;
				snap.formula         = formula;
				snap.seed            = seed;
				snap.tier            = "medium";
				snap.num_atoms       = count_atoms(formula);
				snap.steps_taken     = fr.steps_taken;
				snap.max_steps       = cfg_.fire_steps;
				snap.energy          = fr.energy;
				snap.energy_per_atom = fr.energy_per_atom;
				snap.rms_force       = fr.rms_force;
				snap.max_force       = fr.max_force;
				snap.alpha_final     = fr.alpha_final;
				snap.dt_final        = fr.dt_final;
				snap.converged       = fr.converged;
				snap.classification  = fr.classification;
				snap.wall_time_ms    = fr.wall_ms;

				auto summary = check_all(reg_, snap);
				accumulate(snap, summary);

				last_snap    = snap;
				last_summary = summary;

				// Append ledger row
				append_ledger(snap, summary);

				// Dashboard refresh
				if (stats_.total_formations % cfg_.dashboard_every == 0) {
					dashboard::render(stats_, reg_, cfg_, last_snap, last_summary);
				}
			}
		}

		// Final render + export
		dashboard::render(stats_, reg_, cfg_, last_snap, last_summary);
		export_charts();
		close_ledger();

		print_session_summary();
	}

	const SessionStats& stats() const { return stats_; }

private:
	TrainerConfig     cfg_;
	InvariantRegistry reg_;
	SessionStats      stats_;
	std::mt19937      rng_;
	std::ofstream     ledger_file_;
	std::vector<FormationRecord> records_;

	bool should_continue() const {
		if (fs::exists(fs::path(cfg_.export_dir) / cfg_.stop_file)) return false;
		if (cfg_.max_formations > 0 &&
			stats_.total_formations >= cfg_.max_formations)     return false;
		return true;
	}

	std::string generate_formula() {
		int n_elements = std::uniform_int_distribution<int>(1, 3)(rng_);
		int total_atoms= std::uniform_int_distribution<int>(
							 cfg_.min_atoms, cfg_.max_atoms)(rng_);
		// Pick n_elements distinct from pool
		std::vector<int> idxs(cfg_.element_pool.size());
		std::iota(idxs.begin(), idxs.end(), 0);
		std::shuffle(idxs.begin(), idxs.end(), rng_);
		idxs.resize(std::min(n_elements, (int)cfg_.element_pool.size()));

		// Distribute total_atoms across selected elements
		std::string formula;
		int remaining = total_atoms;
		for (int i = 0; i < (int)idxs.size(); ++i) {
			int cnt = (i == (int)idxs.size()-1)
					  ? remaining
					  : std::uniform_int_distribution<int>(1, remaining)(rng_);
			remaining -= cnt;
			formula += cfg_.element_pool[idxs[i]].first;
			if (cnt > 1) formula += std::to_string(cnt);
			if (remaining <= 0) break;
		}
		return formula;
	}

	int count_atoms(const std::string& formula) {
		auto elems = detail::parse_formula(formula);
		int n = 0;
		for (auto& [sym, cnt] : elems) n += cnt;
		return n;
	}

	void accumulate(const FormationSnapshot& s, const InvariantSummary& sum) {
		++stats_.total_formations;
		if (s.converged)              ++stats_.total_converged;
		if (s.classification=="stable") ++stats_.total_stable;
		if (!sum.all_passed())        ++stats_.total_failed_any_invariant;

		for (const auto& r : sum.results) {
			++stats_.inv_total[r.invariant_id];
			if (r.passed) ++stats_.inv_pass[r.invariant_id];
			auto& wm = stats_.inv_worst_margin[r.invariant_id];
			if (r.margin < wm || stats_.inv_total.at(r.invariant_id) == 1)
				wm = r.margin;
		}

		if (std::isfinite(s.energy_per_atom)) {
			stats_.sum_e_per_atom    += s.energy_per_atom;
			stats_.sum_e_per_atom_sq += s.energy_per_atom * s.energy_per_atom;
			stats_.min_e_per_atom = std::min(stats_.min_e_per_atom, s.energy_per_atom);
			stats_.max_e_per_atom = std::max(stats_.max_e_per_atom, s.energy_per_atom);
		}
		stats_.sum_rms_force += s.rms_force;
		stats_.min_rms_force = std::min(stats_.min_rms_force, s.rms_force);
		stats_.max_rms_force = std::max(stats_.max_rms_force, s.rms_force);
		stats_.sum_wall_ms   += s.wall_time_ms;
		stats_.max_wall_ms    = std::max(stats_.max_wall_ms, s.wall_time_ms);

		records_.push_back({
			stats_.total_formations,
			s.formula, s.seed, s.num_atoms, s.steps_taken,
			s.energy_per_atom, s.rms_force, s.wall_time_ms,
			s.converged, s.classification,
			sum.passed, sum.total,
			now_iso()
		});
	}

	static std::string now_iso() {
		auto t  = std::time(nullptr);
		auto tm = *std::localtime(&t);
		std::ostringstream ss;
		ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
		return ss.str();
	}

	void open_ledger() {
		std::string path = (fs::path(cfg_.export_dir) / "training_ledger.csv").string();
		ledger_file_.open(path, std::ios::app);
		bool empty = (fs::file_size(path) == 0);
		if (empty) {
			ledger_file_ << "idx,formula,seed,num_atoms,steps,energy_per_atom,"
							"rms_force,wall_ms,converged,classification,"
							"inv_passed,inv_total,timestamp\n";
		}
	}

	void append_ledger(const FormationSnapshot& s, const InvariantSummary& sum) {
		ledger_file_ << std::fixed << std::setprecision(6)
			<< stats_.total_formations << ","
			<< s.formula << ","
			<< s.seed << ","
			<< s.num_atoms << ","
			<< s.steps_taken << ","
			<< s.energy_per_atom << ","
			<< s.rms_force << ","
			<< s.wall_time_ms << ","
			<< (s.converged ? 1 : 0) << ","
			<< s.classification << ","
			<< sum.passed << ","
			<< sum.total << ","
			<< now_iso() << "\n";
		ledger_file_.flush();
	}

	void close_ledger() { ledger_file_.close(); }

	void export_charts() {
		using namespace vsepr::chart;

		// DataTable: full record table
		DataTable dt("training_results");
		dt.add_column("idx",              ColumnType::Int);
		dt.add_column("formula",          ColumnType::String);
		dt.add_column("num_atoms",        ColumnType::Int);
		dt.add_column("energy_per_atom",  ColumnType::Float);
		dt.add_column("rms_force",        ColumnType::Float);
		dt.add_column("wall_ms",          ColumnType::Float);
		dt.add_column("converged",        ColumnType::Int);
		dt.add_column("classification",   ColumnType::String);
		dt.add_column("inv_passed",       ColumnType::Int);
		dt.add_column("inv_total",        ColumnType::Int);

		for (const auto& rec : records_) {
			dt.add_row_map({
				{"idx",             std::to_string(rec.idx)},
				{"formula",         rec.formula},
				{"num_atoms",       std::to_string(rec.num_atoms)},
				{"energy_per_atom", std::to_string(rec.energy_per_atom)},
				{"rms_force",       std::to_string(rec.rms_force)},
				{"wall_ms",         std::to_string(rec.wall_time_ms)},
				{"converged",       std::to_string(rec.converged ? 1 : 0)},
				{"classification",  rec.classification},
				{"inv_passed",      std::to_string(rec.invariants_passed)},
				{"inv_total",       std::to_string(rec.invariants_total)},
			});
		}

		// 3. PropertyCard: session summary
		PropertyCard pc("session_summary");
		pc.add("total_formations",   std::to_string(stats_.total_formations),     "count");
		pc.add("converged_rate_pct", std::to_string(stats_.conv_rate()),           "%");
		pc.add("stable_rate_pct",    std::to_string(stats_.stable_rate()),         "%");
		pc.add("mean_energy_per_atom",std::to_string(stats_.mean_e_per_atom()),    "kcal/mol");
		pc.add("mean_rms_force",     std::to_string(stats_.mean_rms_force()),      "kcal/mol/A");
		pc.add("total_wall_s",       std::to_string(stats_.elapsed_s()),           "s");
		pc.add("rate_per_s",         std::to_string(stats_.rate()),                "/s");
		for (const auto& inv : reg_.invariants) {
			pc.add(inv.id + "_pass_rate",
				   std::to_string(stats_.inv_pass_rate(inv.id)), "%");
		}

		// Export
		std::string base = cfg_.export_dir;
		if (cfg_.export_csv) {
			{
				std::ofstream f((fs::path(base) / "training_results.csv").string());
				f << dt.to_csv();
			}
			{
				std::ofstream f((fs::path(base) / "session_summary.csv").string());
				f << pc.to_csv();
			}
		}
		if (cfg_.export_json) {
			{
				std::ofstream f((fs::path(base) / "training_results.json").string());
				f << dt.to_json();
			}
			{
				std::ofstream f((fs::path(base) / "session_summary.json").string());
				f << pc.to_json();
			}
		}
	}

	void print_session_summary() {
		std::cout << "\n=== PROPERTY TRAINING SESSION COMPLETE ===\n"
				  << "  Formations    : " << stats_.total_formations << "\n"
				  << "  Converged     : " << stats_.total_converged
				  << " (" << std::fixed << std::setprecision(1)
				  << stats_.conv_rate() << "%)\n"
				  << "  Stable        : " << stats_.total_stable
				  << " (" << stats_.stable_rate() << "%)\n"
				  << "  Inv failures  : " << stats_.total_failed_any_invariant << "\n"
				  << "  Wall time     : " << stats_.elapsed_s() << "s\n"
				  << "  Rate          : " << stats_.rate() << " formations/s\n"
				  << "  Ledger        : " << cfg_.export_dir << "/training_ledger.csv\n"
				  << "  Results CSV   : " << cfg_.export_dir << "/training_results.csv\n"
				  << "  Results JSON  : " << cfg_.export_dir << "/training_results.json\n"
				  << "\n  Invariant Pass Rates:\n";
		for (const auto& inv : reg_.invariants) {
			std::cout << "    " << inv.id << "  "
					  << std::left << std::setw(28) << inv.name
					  << std::right << std::setprecision(1)
					  << stats_.inv_pass_rate(inv.id) << "%\n";
		}
		std::cout << std::endl;
	}
};

} // namespace training
} // namespace vsepr
