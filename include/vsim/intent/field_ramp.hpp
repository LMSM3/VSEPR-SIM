#pragma once
/**
 * include/vsim/intent/field_ramp.hpp
 * =====================================
 * WO-VSIM-FORMATION-FIELDRAMP  |  Phase 7  |  v5.1.x
 *
 * FieldRamp: linear electric field ramp over a formation stage.
 *
 *   E(t) = E0 + (E1 - E0) * (t / t_stage)
 *
 * where:
 *   E0  = FormationStage::from_field_V_A   (V/Å)
 *   E1  = FormationStage::to_field_V_A     (V/Å)
 *   t   = current time within the stage    (ps)
 *   t_stage = FormationStage::duration_ps  (ps, 0 = clamp to E0)
 *
 * Axis: field is applied along FormationStage::field_axis ("x"|"y"|"z"|"").
 *       Empty axis defaults to "z".
 *
 * Error conditions:
 *   - duration_ps < 0.0   → invalid; evaluate() returns {0,0,0} and ok=false
 *   - t < 0.0             → clamped to 0
 *   - t > duration_ps     → clamped to duration_ps
 *   - duration_ps == 0.0  → clamp to E0 on all time values
 *
 * Group 55 — Formation FieldRamp
 */

#include "vsim/vsim_document.hpp"

#include <array>
#include <string>

namespace vsim {

// ============================================================================
// FieldRampResult  —  output of a single evaluation
// ============================================================================
struct FieldRampResult {
	std::array<double, 3> field_V_A = {0, 0, 0};  // Field vector (V/Å)
	double  magnitude   = 0.0;                      // |E| (V/Å)
	double  t_fraction  = 0.0;                      // t / t_stage  ∈ [0,1]
	bool    ok          = true;                     // false on invalid stage
	std::string axis;                               // resolved axis ("x"|"y"|"z")
};

// ============================================================================
// FieldRampEvaluator
// ============================================================================
class FieldRampEvaluator {
public:
	// -----------------------------------------------------------------------
	// is_field_ramp  —  true when a stage carries a non-trivial field ramp.
	// -----------------------------------------------------------------------
	static bool is_field_ramp(const FormationStage& stage) {
		return stage.kind == FormationStageKind::FieldRamp;
	}

	// -----------------------------------------------------------------------
	// evaluate  —  compute E(t) for time t_ps within the stage.
	// -----------------------------------------------------------------------
	static FieldRampResult evaluate(const FormationStage& stage, double t_ps) {
		FieldRampResult r;
		r.axis = resolve_axis(stage.field_axis);

		if (stage.duration_ps < 0.0) {
			r.ok = false;
			return r;
		}

		double frac = 0.0;
		if (stage.duration_ps > 0.0) {
			double tc = t_ps < 0.0 ? 0.0 : (t_ps > stage.duration_ps ? stage.duration_ps : t_ps);
			frac = tc / stage.duration_ps;
		}
		// t_ps with duration_ps == 0 → frac remains 0 → E = E0

		double E = stage.from_field_V_A + (stage.to_field_V_A - stage.from_field_V_A) * frac;
		r.t_fraction = frac;
		r.magnitude  = E;
		r.field_V_A  = axis_vector(r.axis, E);
		return r;
	}

	// -----------------------------------------------------------------------
	// log_frame  —  print one line of field ramp progress to stdout.
	// Format: "[field_ramp] step=<s>  t=<t>ps  E=<E> V/Å  axis=<a>"
	// -----------------------------------------------------------------------
	static void log_frame(int step, double t_ps, const FieldRampResult& r) {
		std::printf("[field_ramp]  step=%-6d  t=%8.3f ps  E=%+10.4f V/A  axis=%s  "
					"progress=%.1f%%\n",
			step, t_ps, r.magnitude, r.axis.c_str(), r.t_fraction * 100.0);
	}

private:
	static std::string resolve_axis(const std::string& s) {
		if (s == "x" || s == "X") return "x";
		if (s == "y" || s == "Y") return "y";
		return "z";  // default
	}

	static std::array<double, 3> axis_vector(const std::string& ax, double E) {
		if (ax == "x") return {E, 0.0, 0.0};
		if (ax == "y") return {0.0, E, 0.0};
		return {0.0, 0.0, E};
	}
};

} // namespace vsim
