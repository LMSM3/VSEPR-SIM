/**
 * kernel_viz_demo.cpp — Kernel Event Log Terminal Visualization
 * ==============================================================
 *
 * Renders a live terminal dashboard of the kernel event log produced
 * by a combined pipeline + direct event run.
 *
 * No GUI. No external dependencies. ANSI terminal only.
 * Headless-safe: --headless suppresses ANSI codes, emits plain text.
 *
 * Dashboard panels (in order):
 *
 *   Panel 1 — Event Timeline
 *       Chronological lane per KernelEventKind.
 *       Each event is a block on its lane at its frame_id.
 *       Valid = filled block ▮, Invalid = hollow block ▯.
 *
 *   Panel 2 — Per-Kind Stacked Summary Bars
 *       Horizontal bar per event kind, scaled to event count.
 *       Shows distribution of event types for this run.
 *
 *   Panel 3 — Symbolic Trace Printout
 *       For every event with a non-empty equation, print the full
 *       symbolic and numeric trace in documentation format.
 *       This is the WO-56C anti-black-box panel.
 *
 *   Panel 4 — Pipeline Trace Expressions
 *       Loads a synthetic pipeline run (Al, Fe, C6H12, C)
 *       and prints all AnalysisRecord symbolic traces.
 *
 *   Panel 5 — Animation Cue Timeline
 *       Lists all collected AnimationCues in stage order
 *       with their timing and easing parameters.
 *
 *   Panel 6 — Final Audit Table
 *       event_id | kind | frame | formula | result | unit | valid
 *
 * Usage:
 *   kernel_viz_demo [--headless] [--help]
 *
 * Architecture position:
 *   KernelEventLog (kernel spine)
 *         ↓
 *   pipeline stages (trace injection)
 *         ↓
 *   kernel_viz_demo (terminal dashboard)
 *         ↓
 *   later: SVG / dashboard JSON export (beta-8)
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "include/kernel/kernel_event.hpp"
#include "include/kernel/kernel_event_log.hpp"
#include "include/pipeline/pipeline_record.hpp"
#include "include/pipeline/pipeline_stages.hpp"
#include "include/pipeline/pipeline_trace.hpp"
#include "include/pipeline/expression_generators.hpp"
#include "include/pipeline/animation_generators.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// ANSI / headless toggle
// ============================================================================

static bool g_headless = false;

namespace col {
	inline const char* rst()   { return g_headless ? "" : "\033[0m";  }
	inline const char* bold()  { return g_headless ? "" : "\033[1m";  }
	inline const char* dim()   { return g_headless ? "" : "\033[2m";  }
	inline const char* grn()   { return g_headless ? "" : "\033[32m"; }
	inline const char* yel()   { return g_headless ? "" : "\033[33m"; }
	inline const char* cyn()   { return g_headless ? "" : "\033[36m"; }
	inline const char* mag()   { return g_headless ? "" : "\033[35m"; }
	inline const char* red()   { return g_headless ? "" : "\033[31m"; }
	inline const char* blu()   { return g_headless ? "" : "\033[34m"; }
	inline const char* wht()   { return g_headless ? "" : "\033[37m"; }
}

// ============================================================================
// Layout helpers
// ============================================================================

static void ruler(const char* ch = "-", int width = 60) {
	for (int i = 0; i < width; ++i) std::fputs(ch, stdout);
	std::fputc('\n', stdout);
}

static void panel_header(const char* title, int index) {
	std::printf("\n%s%s", col::bold(), col::cyn());
	ruler("\xe2\x95\x90");  // UTF-8 ═
	std::printf("  Panel %d \xe2\x80\x94 %s\n", index, title);
	ruler("\xe2\x95\x90");  // UTF-8 ═
	std::printf("%s", col::rst());
}

static void section_line(const char* title) {
	std::printf("%s%s  %s  %s\n", col::dim(), col::wht(), title, col::rst());
	ruler("\xe2\x94\x80", 56);  // UTF-8 ─
}

// ============================================================================
// Synthetic FormationRecord builder (no file I/O)
// ============================================================================

// Forward-declare the v4 namespace types we need
namespace v4 { enum class LatticeClass : uint8_t; struct FormationRecord; }

static v4::FormationRecord make_formation(
	const std::string& symbol,
	const std::string& name,
	v4::LatticeClass   lc,
	int n_beads, int steps, bool converged,
	double energy, double rms, double eta, double rho, double C,
	double rigidity, double ductility,
	int n_l3)
{
	v4::FormationRecord f{};
	f.symbol          = symbol;
	f.name            = name;
	f.structure       = lc;
	f.n_beads         = n_beads;
	f.steps           = steps;
	f.converged       = converged;
	f.final_energy    = energy;
	f.rms_force       = rms;
	f.avg_eta         = eta;
	f.avg_rho         = rho;
	f.avg_C           = C;
	f.macro_rigidity  = rigidity;
	f.macro_ductility = ductility;
	f.n_l3_domains    = n_l3;
	return f;
}

// ============================================================================
// Panel 1 — Event Timeline
// ============================================================================

static void panel_timeline(const vsepr::kernel::KernelEventLog& log) {
	panel_header("Event Timeline", 1);

	using vsepr::kernel::KernelEventKind;
	using vsepr::kernel::kind_name;

	const KernelEventKind kinds[] = {
		KernelEventKind::Reaction,
		KernelEventKind::ChemicalState,
		KernelEventKind::Formation,
		KernelEventKind::Defect,
		KernelEventKind::Transport,
		KernelEventKind::ContinualReport,
	};
	const char* lane_colors[] = {
		"\033[35m", "\033[36m", "\033[32m",
		"\033[31m", "\033[34m", "\033[33m",
	};

	auto all = log.snapshot();
	if (all.empty()) { std::printf("  (no events)\n"); return; }

	// Find max frame
	uint64_t max_frame = 0;
	for (const auto& e : all) max_frame = std::max(max_frame, e.frame_id);

	const int   LANE_WIDTH = 50;
	const float scale = max_frame > 0
		? static_cast<float>(LANE_WIDTH) / static_cast<float>(max_frame)
		: 1.0f;

	std::printf("  frame 0");
	for (int i = 8; i < LANE_WIDTH - 4; ++i) std::fputc(' ', stdout);
	std::printf("%llu\n", (unsigned long long)max_frame);
	std::printf("  ");
	for (int i = 0; i < LANE_WIDTH; ++i) std::fputs("\xc2\xb7", stdout);  // UTF-8 ·
	std::fputc('\n', stdout);

	for (int ki = 0; ki < 6; ++ki) {
		KernelEventKind k = kinds[ki];
		const char* lc = g_headless ? "" : lane_colors[ki];

		// Build lane char array
		std::vector<char> lane(LANE_WIDTH, ' ');
		for (const auto& e : all) {
			if (e.kind != k) continue;
			int pos = static_cast<int>(static_cast<float>(e.frame_id) * scale);
			pos = std::max(0, std::min(LANE_WIDTH - 1, pos));
			lane[pos] = e.is_valid ? '\xE2' : 'x';  // will print manually
			(void)lane[pos];  // suppress; we store valid flag separately
		}

		std::printf("  %s%-16s%s ", lc, kind_name(k), col::rst());

		for (int x = 0; x < LANE_WIDTH; ++x) {
			// Check if any event lands in this cell
			bool hit = false, valid = true;
			for (const auto& e : all) {
				if (e.kind != k) continue;
				int pos = static_cast<int>(static_cast<float>(e.frame_id) * scale);
				pos = std::max(0, std::min(LANE_WIDTH - 1, pos));
				if (pos == x) { hit = true; valid = e.is_valid; }
			}
			if (hit) {
				std::printf("%s%s%s", lc, valid ? "▮" : "▯", col::rst());
			} else {
				std::fputs("\xc2\xb7", stdout);  // UTF-8 ·
			}
		}
		std::fputc('\n', stdout);
	}
}

// ============================================================================
// Panel 2 — Per-Kind Summary Bars
// ============================================================================

static void panel_summary_bars(const vsepr::kernel::KernelEventLog& log) {
	panel_header("Per-Kind Event Count Bars", 2);

	using vsepr::kernel::KernelEventKind;
	using vsepr::kernel::kind_name;

	const KernelEventKind kinds[] = {
		KernelEventKind::Reaction,
		KernelEventKind::ChemicalState,
		KernelEventKind::Formation,
		KernelEventKind::Defect,
		KernelEventKind::Transport,
		KernelEventKind::ContinualReport,
	};
	const char* lane_colors[] = {
		"\033[35m", "\033[36m", "\033[32m",
		"\033[31m", "\033[34m", "\033[33m",
	};

	int counts[6] = {};
	int max_count = 0;
	for (int ki = 0; ki < 6; ++ki) {
		auto evs = log.filter_by_kind(kinds[ki]);
		counts[ki] = static_cast<int>(evs.size());
		max_count = std::max(max_count, counts[ki]);
	}
	if (max_count == 0) { std::printf("  (no events)\n"); return; }

	const int BAR_WIDTH = 40;
	for (int ki = 0; ki < 6; ++ki) {
		int bar_len = counts[ki] * BAR_WIDTH / max_count;
		const char* lc = g_headless ? "" : lane_colors[ki];
		std::printf("  %s%-18s%s %s", lc, kind_name(kinds[ki]), col::rst(), col::yel());
		for (int b = 0; b < bar_len; ++b) std::printf("█");
		std::printf("%s %d\n", col::rst(), counts[ki]);
	}
}

// ============================================================================
// Panel 3 — Symbolic Trace Printout
// ============================================================================

static void panel_symbolic_traces(const vsepr::kernel::KernelEventLog& log) {
	panel_header("Symbolic Trace Printout (Anti-Black-Box)", 3);

	for (const auto& e : log.snapshot()) {
		if (e.equation_symbolic.empty()) continue;

		std::printf("\n  %s%s[event %llu] %s  formula=%s  frame=%llu%s\n",
			col::bold(), col::wht(),
			(unsigned long long)e.event_id,
			vsepr::kernel::kind_name(e.kind),
			e.source_formula.c_str(),
			(unsigned long long)e.frame_id,
			col::rst());

		std::printf("  %s  Equation:      %s%s%s\n",
			col::dim(), col::cyn(), e.equation_symbolic.c_str(), col::rst());
		std::printf("  %s  Substitution:  %s%s%s\n",
			col::dim(), col::yel(), e.equation_numeric.c_str(), col::rst());
		std::printf("  %s  Result:        %s%.6g %s%s\n",
			col::dim(), col::grn(), e.result_value, e.result_unit.c_str(), col::rst());
		if (!e.is_valid)
			std::printf("  %s  Warning:       %s%s%s\n",
				col::dim(), col::red(), e.warning.c_str(), col::rst());
	}
}

// ============================================================================
// Panel 4 — Pipeline Trace Expressions
// ============================================================================

static void panel_pipeline_traces() {
	panel_header("Pipeline Symbolic Trace Expressions", 4);

	// Build a synthetic 4-case pipeline run
	using namespace vsepr::pipeline;
	using v4::LatticeClass;

	std::vector<v4::FormationRecord> formations;
	formations.push_back(make_formation(
		"Al", "Aluminium", LatticeClass::FCC,
		64, 312, true, -123.45, 0.0008, 0.71, 10.2, 38.4, 0.82, 0.41, 1));
	formations.push_back(make_formation(
		"Fe", "Iron", LatticeClass::BCC,
		64, 440, true, -110.1, 0.0012, 0.68, 9.8, 36.0, 0.91, 0.22, 3));
	formations.push_back(make_formation(
		"C6H12", "Cyclohexane", LatticeClass::FCC,
		24, 180, true, -48.3, 0.0005, 0.59, 6.1, 22.0, 0.33, 0.77, 0));
	formations.push_back(make_formation(
		"C", "Graphene", LatticeClass::HCP,
		96, 800, false, 8.6, 0.048, 0.43, 4.2, 15.3, 0.55, 0.12, 12));

	auto [records, dash] = run_pipeline(formations, "kernel_viz_demo");

	for (const auto& pr : records) {
		const auto& sym    = pr.analysis.symbol;
		const auto& traces = pr.analysis.trace.expressions;

		std::printf("\n  %s%s%s — %zu symbolic traces\n",
			col::bold(), sym.c_str(), col::rst(), traces.size());
		section_line("");

		for (const auto& tr : traces) {
			std::printf("  %s%s%s\n",
				col::bold(), tr.metric_name.c_str(), col::rst());
			std::printf("    %sEquation:      %s%s%s\n",
				col::dim(), col::cyn(), tr.symbolic_expression.c_str(), col::rst());
			std::printf("    %sSubstitution:  %s%s%s\n",
				col::dim(), col::yel(), tr.substituted_expression.c_str(), col::rst());
			std::printf("    %sResult:        %s%s%s\n",
				col::dim(), col::grn(), tr.result_expression.c_str(), col::rst());
			std::printf("    %sUnits:         %s%s  — %s%s%s\n",
				col::dim(), col::wht(), tr.units.c_str(),
				col::dim(), tr.interpretation.c_str(), col::rst());
			std::fputc('\n', stdout);
		}
	}
}

// ============================================================================
// Panel 5 — Animation Cue Timeline
// ============================================================================

static void panel_animation_cues() {
	panel_header("Animation Cue Timeline (Declarative — Not Rendered Here)", 5);

	using namespace vsepr::pipeline;
	using v4::LatticeClass;

	// Reuse a single-case pipeline to collect cues
	std::vector<v4::FormationRecord> formations;
	formations.push_back(make_formation(
		"Al", "Aluminium", LatticeClass::FCC,
		64, 312, true, -123.45, 0.0008, 0.71, 10.2, 38.4, 0.82, 0.41, 1));
	formations.push_back(make_formation(
		"C", "Graphene", LatticeClass::HCP,
		96, 800, false, 8.6, 0.048, 0.43, 4.2, 15.3, 0.55, 0.12, 12));

	auto [records, dash] = run_pipeline(formations, "anim-cue-demo");

	// Collect all cues from dashboard trace
	const auto& cues = dash.trace.animations;

	std::printf("  %zu total animation cues\n\n", cues.size());

	std::printf("  %s%-42s %-18s %-14s %-5s %-5s %-10s%s\n",
		col::bold(), "id", "stage", "cue_type", "t0", "t1", "easing", col::rst());
	ruler("\xe2\x94\x80", 110);  // UTF-8 ─

	for (const auto& c : cues) {
		const char* lc = col::cyn();
		if (c.cue_type == "flash")        lc = col::red();
		if (c.cue_type == "gauge_fill")   lc = col::grn();
		if (c.cue_type == "network_grow") lc = col::mag();
		if (c.cue_type == "bar_grow")     lc = col::yel();
		if (c.cue_type == "pulse")        lc = col::blu();

		std::printf("  %s%-42s%s %-18s %-14s %-5.2f %-5.2f %-10s\n",
			lc, c.id.c_str(), col::rst(),
			c.stage.c_str(),
			c.cue_type.c_str(),
			c.t0, c.t1,
			c.easing.c_str());

		if (!c.notes.empty())
			std::printf("    %s↳ %s%s\n", col::dim(), c.notes.c_str(), col::rst());
	}
}

// ============================================================================
// Panel 6 — Final Audit Table
// ============================================================================

static void panel_audit_table(const vsepr::kernel::KernelEventLog& log) {
	panel_header("Final Audit Table", 6);

	std::printf("  %s%-6s %-16s %-8s %-12s %-12s %-14s %-6s%s\n",
		col::bold(),
		"ID", "Kind", "Frame", "Formula", "Result", "Unit", "Valid",
		col::rst());
	ruler("\xe2\x94\x80", 80);  // UTF-8 ─

	for (const auto& e : log.snapshot()) {
		const char* vc = e.is_valid ? col::grn() : col::red();
		std::printf("  %-6llu %-16s %-8llu %-12s %s%-12.4g%s %-14s %s%s%s\n",
			(unsigned long long)e.event_id,
			vsepr::kernel::kind_name(e.kind),
			(unsigned long long)e.frame_id,
			e.source_formula.c_str(),
			col::yel(), e.result_value, col::rst(),
			e.result_unit.c_str(),
			vc, e.is_valid ? "✓" : "✗", col::rst());
	}
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--headless") == 0) g_headless = true;
		if (std::strcmp(argv[i], "--help") == 0) {
			std::printf(
				"kernel_viz_demo — Kernel Event Log Terminal Visualization\n\n"
				"Usage: kernel_viz_demo [options]\n\n"
				"Options:\n"
				"  --headless   Suppress ANSI colors (plain text output)\n"
				"  --help       Show this message\n\n"
				"Panels:\n"
				"  1  Event Timeline       (frame-position lanes per kind)\n"
				"  2  Per-Kind Bars        (count histogram)\n"
				"  3  Symbolic Traces      (equation + substitution + result)\n"
				"  4  Pipeline Traces      (analysis stage expressions)\n"
				"  5  Animation Cues       (declarative dashboard instructions)\n"
				"  6  Audit Table          (final event registry)\n"
			);
			return 0;
		}
	}

	// ----------------------------------------------------------------
	// Populate kernel event log
	// ----------------------------------------------------------------

	auto& log = vsepr::kernel::KernelEventLog::instance();
	log.clear();

	// Reaction
	{
		vsepr::kernel::ReactionEvent ev;
		ev.source_formula    = "C6H12";
		ev.frame_id          = 10;
		ev.reactants         = {"C6H12", "HCl"};
		ev.products          = {"C6H13Cl"};
		ev.reactant_energies = {-168.4, -22.1};
		ev.product_energies  = {-194.9};
		ev.reaction_rule     = "electrophilic_addition";
		ev.compute_delta_E();
		log.record(ev);
	}
	// Chemical state
	{
		vsepr::kernel::ChemicalStateEvent ev;
		ev.source_formula      = "C";
		ev.frame_id            = 22;
		ev.particle_i          = 12;
		ev.particle_j          = 13;
		ev.coordination_before = 2.0;
		ev.coordination_after  = 3.0;
		ev.local_energy_before = -3.41;
		ev.local_energy_after  = -3.87;
		ev.bond_length_ang     = 1.42;
		ev.state_tag_before    = "sp2_open";
		ev.state_tag_after     = "sp2_ring";
		ev.compute();
		log.record(ev);
	}
	// Formation (converged)
	{
		vsepr::kernel::FormationEvent ev;
		ev.source_formula   = "Al";
		ev.frame_id         = 0;
		ev.n_beads          = 64;
		ev.fire_steps       = 312;
		ev.converged        = true;
		ev.final_energy     = -123.45;
		ev.packing_fraction = 0.741;
		ev.lattice_class    = "FCC";
		ev.formation_preset = "metallic";
		ev.compute();
		log.record(ev);
	}
	// Formation (not converged)
	{
		vsepr::kernel::FormationEvent ev;
		ev.source_formula   = "AmorphTest";
		ev.frame_id         = 0;
		ev.n_beads          = 32;
		ev.fire_steps       = 800;
		ev.converged        = false;
		ev.final_energy     = 18.6;
		ev.packing_fraction = 0.43;
		ev.lattice_class    = "amorphous";
		ev.compute();
		log.record(ev);
	}
	// Defect
	{
		vsepr::kernel::DefectEvent ev;
		ev.source_formula   = "Fe";
		ev.frame_id         = 50;
		ev.defect_type      = vsepr::kernel::DefectType::Vacancy;
		ev.site_id          = 47;
		ev.formation_energy = 2.04;
		ev.migration_energy = 0.68;
		ev.host_element     = "Fe";
		ev.compute();
		log.record(ev);
	}
	// Transport
	{
		vsepr::kernel::TransportEvent ev;
		ev.source_formula   = "Li";
		ev.frame_id         = 80;
		ev.particle_id      = 7;
		ev.displacement_ang = 4.88;
		ev.msd              = 23.8;
		ev.diffusivity      = 0.0014;
		ev.transport_mode   = "diffusion";
		ev.compute();
		log.record(ev);
	}
	// Continual report snapshots
	for (int i = 0; i < 5; ++i) {
		vsepr::kernel::ContinualReportEvent ev;
		ev.source_formula   = "C6H12";
		ev.frame_id         = static_cast<uint64_t>((i + 1) * 50);
		ev.total_energy     = -168.4 - i * 0.7;
		ev.temperature_K    = 300.0 + i * 2.0;
		ev.packing_fraction = 0.61 + i * 0.008;
		ev.mean_coord_num   = 4.1 + i * 0.04;
		ev.rmsd_ang         = 0.22 - i * 0.02;
		ev.n_active_beads   = 24;
		ev.report_interval  = 50;
		ev.compute();
		log.record(ev);
	}

	// ----------------------------------------------------------------
	// Render panels
	// ----------------------------------------------------------------

	std::printf("\n%s%s", col::bold(), col::cyn());
	ruler("\xe2\x95\x90", 60);  // UTF-8 ═
	std::printf("  VSEPR-SIM  |  kernel_viz_demo  |  WO-56C  |  beta-7\n");
	ruler("\xe2\x95\x90", 60);  // UTF-8 ═
	std::printf("%s\n", col::rst());

	panel_timeline(log);
	panel_summary_bars(log);
	panel_symbolic_traces(log);
	panel_pipeline_traces();
	panel_animation_cues();
	panel_audit_table(log);

	// ----------------------------------------------------------------
	// Footer
	// ----------------------------------------------------------------

	std::printf("\n%s%s", col::bold(), col::cyn());
	ruler("\xe2\x95\x90", 60);  // UTF-8 ═
	std::printf("  %s events in log — spine is clean — panels complete\n",
		std::to_string(log.size()).c_str());
	ruler("\xe2\x95\x90", 60);  // UTF-8 ═
	std::printf("%s\n", col::rst());

	return 0;
}
