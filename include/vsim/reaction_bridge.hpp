#pragma once
/**
 * include/vsim/reaction_bridge.hpp
 * =================================
 * Adapter between the VSIM document layer and the atomistic reaction engine.
 *
 * Reactions are ambient physics: this bridge is called after every simulation
 * step, not only in "reaction_event" mode.  The ChemistrySection controls
 * rule selection, heat-gate level, and event verbosity — but the engine
 * always runs whenever two or more molecule species are present.
 *
 * Architecture (WO-56C doctrine):
 *
 *   VsimDocument (chemistry + environment + simulation.molecules)
 *         ↓
 *   ReactionBridge::run_pass(doc, frame_id)
 *         ├── build_state_from_entry()   ← formula string → atomistic::State stub
 *         ├── ReactionEngine::identify_reactive_sites()
 *         ├── ReactionEngine::match_reactive_sites()
 *         ├── ReactionEngine::score_reaction()
 *         └── KernelEventLog::instance().record(ReactionEvent)
 *
 * Truth-carrier rule: this bridge NEVER writes to .xyz / .xyzFull.
 * It only emits KernelEvents.  State files are written by the FIRE/MD runner.
 *
 * WO-56C | v5.0.0-beta.9
 */

#include "vsim_document.hpp"
#include "kernel/kernel_event.hpp"
#include "kernel/kernel_event_log.hpp"

#include "atomistic/reaction/engine.hpp"
#include "atomistic/reaction/heat_gate.hpp"
#include "atomistic/core/state.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace vsim {

// ============================================================================
// ReactionPassResult — lightweight summary returned to the caller
// ============================================================================

struct ReactionPassResult {
	int    reactions_evaluated  = 0;  // Pairs evaluated by the engine
	int    reactions_emitted    = 0;  // Events recorded in KernelEventLog
	double best_score           = 0.0;
	bool   any_exothermic       = false;
};

// ============================================================================
// ReactionBridge
// ============================================================================

class ReactionBridge {
public:

	// -----------------------------------------------------------------------
	// run_pass — evaluate reactions for all molecule pairs in the document.
	//
	// Called after each FIRE/MD step.  Uses the document's chemistry section
	// and environment temperature to configure the heat gate, then runs the
	// reaction engine over every ordered pair of distinct molecule species.
	//
	// frame_id — the simulation step counter (for KernelEvent provenance).
	// -----------------------------------------------------------------------
	static ReactionPassResult run_pass(const VsimDocument& doc,
									   uint64_t frame_id)
	{
		ReactionPassResult result;

		// Reactions are always evaluated; [chemistry] just shapes the rules.
		// An empty chemistry string also runs — it picks up standard templates.
		const auto& chem = doc.chemistry;
		const auto& mols = doc.simulation.molecules;

		if (mols.empty()) return result;

		// Build the engine with the document-level heat configuration.
		atomistic::reaction::ReactionEngine engine;
		engine.load_standard_templates();

		const double T_K   = (doc.environment.temperature > 0.0)
							   ? doc.environment.temperature
							   : doc.run.temperature_K;
		const int    heat  = chem.effective_heat(T_K);
		const atomistic::reaction::HeatConfig hcfg(static_cast<uint16_t>(
			std::clamp(heat, 0, 999)));

		auto& log = vsepr::kernel::KernelEventLog::instance();

		// Evaluate all ordered pairs (i, j) with i < j.
		// Each pair represents a potential bimolecular reaction.
		int reactions_this_step = 0;

		for (size_t i = 0; i < mols.size(); ++i) {
			if (chem.max_reactions_per_step > 0 &&
				reactions_this_step >= chem.max_reactions_per_step) break;

			atomistic::State st_a = build_state_from_entry(mols[i]);
			auto sites_a = engine.identify_reactive_sites(st_a);
			if (sites_a.empty()) continue;

			for (size_t j = i + 1; j < mols.size(); ++j) {
				if (chem.max_reactions_per_step > 0 &&
					reactions_this_step >= chem.max_reactions_per_step) break;

				atomistic::State st_b = build_state_from_entry(mols[j]);
				auto sites_b = engine.identify_reactive_sites(st_b);
				if (sites_b.empty()) continue;

				// Use the first available reaction template for site matching.
				// The engine's template list is populated by load_standard_templates().
				const auto& templates = engine.get_templates();
				if (templates.empty()) continue;

				for (const auto& tmpl : templates) {
					++result.reactions_evaluated;

					auto proposals = engine.match_reactive_sites(
						st_a, st_b, sites_a, sites_b, tmpl);

					for (auto& prop : proposals) {
						engine.estimate_energetics(prop);
						engine.score_reaction(prop);

						if (prop.overall_score < chem.min_score_threshold) continue;
						if (!prop.mass_balanced || !prop.charge_balanced)   continue;

						// Gate through heat activation
						if (!is_heat_activated(prop, hcfg)) continue;

						if (!chem.reaction_events) continue;

						// Emit ReactionEvent into the spine
						vsepr::kernel::ReactionEvent ev;
						ev.frame_id        = frame_id;
						ev.source_formula  = mols[i].formula + "+" + mols[j].formula;
						ev.reactants       = { mols[i].formula, mols[j].formula };
						ev.products        = products_from_proposal(prop, mols[j]);
						ev.reaction_rule   = prop.description;
						ev.delta_E         = prop.reaction_energy;
						ev.exothermic      = prop.reaction_energy < 0.0;
						ev.result_value    = prop.reaction_energy;
						ev.result_unit     = "kcal/mol";
						ev.equation_symbolic = mols[i].formula + " + " +
											  mols[j].formula + " -> " +
											  ev.products[0];
						ev.equation_numeric  = "Delta_E = " +
											  std::to_string(prop.reaction_energy) +
											  " kcal/mol";

						log.record(ev);

						++result.reactions_emitted;
						++reactions_this_step;
						result.best_score    = std::max(result.best_score, prop.overall_score);
						result.any_exothermic |= ev.exothermic;

						// Emit ChemicalStateEvent if species tracking is on
						if (chem.track_species_state) {
							emit_chemical_state(mols[i].formula, frame_id,
												prop.reaction_energy, log);
						}
					}
				}
			}
		}

		return result;
	}

private:

	// -----------------------------------------------------------------------
	// build_state_from_entry — construct a minimal atomistic::State from a
	// MoleculeEntry.  This is a formula-level stub: no 3D geometry is placed.
	// The reaction engine uses element Z numbers and bond topology; the bridge
	// derives Z from the formula string, producing a single-atom representative
	// per species.  Full geometry injection is deferred to beta-10 generator
	// wiring when material structures are fully resolved.
	// -----------------------------------------------------------------------
	static atomistic::State build_state_from_entry(
		const vsim::MoleculeEntry& entry)
	{
		atomistic::State s;
		// Represent this molecule as a single pseudo-particle whose type
		// encodes the first element Z parsed from the formula.
		s.N    = 1;
		s.X    = { {0.0, 0.0, 0.0} };
		s.V    = { {0.0, 0.0, 0.0} };
		s.T    = { entry.temperature_K > 0.0 ? entry.temperature_K : 300.0 };
		s.Q    = { 0.0 };
		s.M    = { 1.0 };
		s.type = { first_element_z(entry.formula) };
		s.F    = { {0.0, 0.0, 0.0} };
		// Store formula in event log (type 0 used as formula proxy)
		return s;
	}

	// Parse first numeric Z from formula string (e.g., "H2O" → 1, "NaCl" → 11).
	// Falls back to 1 (H) for unknown formulas.
	static uint32_t first_element_z(const std::string& formula) {
		// Simple lookup: first 1-2 capital-letter symbol
		static const struct { const char* sym; uint32_t Z; } tbl[] = {
			{"He",2},{"Li",3},{"Be",4},{"Ne",10},{"Na",11},{"Mg",12},
			{"Al",13},{"Si",14},{"Cl",17},{"Ar",18},{"Ca",20},{"Fe",26},
			{"Co",27},{"Ni",28},{"Cu",29},{"Zn",30},{"Br",35},{"Kr",36},
			{"Xe",54},{"Cs",55},{"Au",79},{"Pb",82},
			{"H",1},{"B",5},{"C",6},{"N",7},{"O",8},{"F",9},
			{"P",15},{"S",16},{"K",19},{"V",23},{"I",53},
		};
		for (const auto& e : tbl) {
			if (formula.rfind(e.sym, 0) == 0) return e.Z;
		}
		return 1; // fallback
	}

	// -----------------------------------------------------------------------
	// is_heat_activated — check whether the heat gate permits this reaction.
	// Uses the activation_barrier as the energetic gate: higher barrier
	// requires higher heat (h) for the gate function to open.
	// -----------------------------------------------------------------------
	static bool is_heat_activated(
		const atomistic::reaction::ProposedReaction& prop,
		const atomistic::reaction::HeatConfig& hcfg)
	{
		// Barrier threshold: each 1 kcal/mol of activation needs ~h=100
		// Scale: barrier_kcal / 30.0 mapped to [0,1] heat fraction needed
		const double barrier_fraction = std::clamp(prop.activation_barrier / 30.0, 0.0, 1.0);
		const double x0 = barrier_fraction * 0.8;
		const double x1 = std::min(barrier_fraction + 0.1, 1.0);
		const double g  = atomistic::reaction::gate(hcfg.x_normalized, x0, x1);
		return g > 0.0;
	}

	// -----------------------------------------------------------------------
	// products_from_proposal — derive a product formula string.
	// -----------------------------------------------------------------------
	static std::vector<std::string> products_from_proposal(
		const atomistic::reaction::ProposedReaction& prop,
		const vsim::MoleculeEntry& mol_b)
	{
		// If a product State was generated, use mol_b formula as proxy.
		// Full product formula generation requires the beta-10 generator.
		(void)prop;
		return { mol_b.formula + "*" };  // '*' marks "reaction-modified"
	}

	// -----------------------------------------------------------------------
	// emit_chemical_state — record a ChemicalStateEvent for species tracking.
	// -----------------------------------------------------------------------
	static void emit_chemical_state(
		const std::string& formula,
		uint64_t frame_id,
		double delta_E,
		vsepr::kernel::KernelEventLog& log)
	{
		vsepr::kernel::ChemicalStateEvent ev;
		ev.frame_id       = frame_id;
		ev.source_formula = formula;
		ev.result_value   = delta_E;
		ev.result_unit    = "kcal/mol";
		ev.equation_symbolic = formula + " state_change";
		ev.equation_numeric  = "delta_E = " + std::to_string(delta_E);
		log.record(ev);
	}
};

} // namespace vsim
