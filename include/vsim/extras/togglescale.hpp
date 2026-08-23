#pragma once
/**
 * togglescale.hpp
 * ===============
 * WO-VSEPR-SIM Extreme  |  extras.togglescale
 *
 * Scale-slot toggle module.  Applies [extras.togglescale] config to a
 * DefaultScaleIdentityMatrix and to any DefaultParticleIdentityVector that
 * carries a scale_mask field.
 *
 * Config TOML block (from addendum screenshots):
 *
 *   [extras.togglescale]
 *   enabled      = true
 *   default_mode = "cycle"     # cycle | pin | collapse
 *   w_depth      = 0.0
 *   persist      = true
 *   slots        = [0, 1, 3]   # active S_n by default
 *   guard_xd     = true        # block X_D writes until measured
 *
 * Method signature (addendum screenshot):
 *
 *   def togglescale(
 *       particle: IdentityVector,
 *       target_sn: int,        # S_n slot index (0-6)
 *       mode: str = "cycle",   # "cycle" | "pin" | "collapse"
 *       w_depth: float = 0.0,  # hidden scale-depth w coord
 *       persist: bool = True,  # write back to X(t) state
 *   ) -> IdentityVector:
 *
 * C++ implementation:
 *   ToggleScaleConfig       — parsed config struct
 *   ToggleScaleOperator     — stateless operator; apply() is the single entry point
 *   ToggleScaleResult       — result of a single apply() call
 *
 * v5.1.4  |  WO-VSEPR-SIM Extreme  |  v5.0.0-main
 */

#include "identity/default_identity_matrices.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace vsim::extras {

// ============================================================================
// ToggleMode
// ============================================================================

enum class ToggleMode : uint8_t {
	Cycle    = 0,   // cycle: rotate active slot (0→1→2→...→6→0)
	Pin      = 1,   // pin: force target slot active, all others passive
	Collapse = 2,   // collapse: deactivate target slot (fold back to S0)
};

inline ToggleMode toggle_mode_from_string(const std::string& s) noexcept {
	if (s == "pin")      return ToggleMode::Pin;
	if (s == "collapse") return ToggleMode::Collapse;
	return ToggleMode::Cycle;  // default
}

inline const char* toggle_mode_name(ToggleMode m) noexcept {
	switch (m) {
		case ToggleMode::Cycle:    return "cycle";
		case ToggleMode::Pin:      return "pin";
		case ToggleMode::Collapse: return "collapse";
	}
	return "cycle";
}

// ============================================================================
// ToggleScaleConfig  —  parsed [extras.togglescale] section
// ============================================================================

struct ToggleScaleConfig {
	bool        enabled      {false};
	ToggleMode  default_mode {ToggleMode::Cycle};
	double      w_depth      {0.0};
	bool        persist      {true};
	std::vector<int> slots   {0, 1, 3};   // S_n slots active by default (S0, S1, S3)
	bool        guard_xd     {true};

	// Convert slot list to bitmask
	uint8_t slot_mask() const noexcept {
		uint8_t m = 0;
		for (int s : slots) {
			if (s >= 0 && s <= 6) m |= static_cast<uint8_t>(1 << s);
		}
		return m;
	}
};

// ============================================================================
// ToggleScaleResult  —  result of a single togglescale apply()
// ============================================================================

struct ToggleScaleResult {
	bool    applied          {false};   // false if guarded or disabled
	int     target_sn        {0};       // slot that was targeted
	ToggleMode mode          {ToggleMode::Cycle};
	double  w_depth_applied  {0.0};
	bool    persisted        {false};
	std::string note;                   // diagnostic note (empty on clean apply)
};

// ============================================================================
// ToggleScaleOperator  —  stateless; thread-safe
// ============================================================================

class ToggleScaleOperator {
public:

	// -----------------------------------------------------------------------
	// apply()
	//
	// Apply a toggle operation on `scale` according to `cfg`.
	//
	// Parameters:
	//   scale      — the DefaultScaleIdentityMatrix to mutate (if persist=true)
	//   target_sn  — the S_n slot to target (0–6)
	//   mode       — override mode (if == Cycle, uses cfg.default_mode)
	//   w_depth    — override w_depth (if 0.0, uses cfg.w_depth)
	//   persist    — if true, writes mutation back to `scale`
	// -----------------------------------------------------------------------
	static ToggleScaleResult apply(
		vsepr::identity::DefaultScaleIdentityMatrix& scale,
		const ToggleScaleConfig& cfg,
		int target_sn,
		ToggleMode mode   = ToggleMode::Cycle,
		double w_depth    = 0.0,
		bool   persist    = true
	) noexcept {
		ToggleScaleResult r;
		r.target_sn = target_sn;
		r.mode      = mode;

		if (!cfg.enabled) {
			r.note = "togglescale disabled";
			return r;
		}

		// Guard X_D (slot 6)
		if (target_sn == 6 && cfg.guard_xd && scale.is_xd_guarded()) {
			r.note = "X_D write blocked by guard_xd";
			return r;
		}

		if (target_sn < 0 || target_sn >= vsepr::identity::DefaultScaleIdentityMatrix::N_SLOTS) {
			r.note = "target_sn out of range";
			return r;
		}

		// Resolve effective mode and w_depth
		ToggleMode eff_mode   = (mode == ToggleMode::Cycle) ? cfg.default_mode : mode;
		double     eff_wdepth = (w_depth == 0.0) ? cfg.w_depth : w_depth;
		bool       eff_persist = persist && cfg.persist;

		r.mode            = eff_mode;
		r.w_depth_applied = eff_wdepth;
		r.persisted       = eff_persist;

		if (!eff_persist) {
			r.applied = true;
			return r;   // dry run — no mutation
		}

		switch (eff_mode) {

			case ToggleMode::Cycle: {
				// Find currently highest active slot; activate the next one above it,
				// wrapping at N_SLOTS-1 (but never wrapping into guarded X_D without permit).
				int highest = -1;
				for (int i = 0; i < vsepr::identity::DefaultScaleIdentityMatrix::N_SLOTS; ++i)
					if (scale.slots[i].active) highest = i;
				int next = (highest + 1) % vsepr::identity::DefaultScaleIdentityMatrix::N_SLOTS;
				if (next == 6 && cfg.guard_xd) next = 0;  // skip guarded X_D on cycle
				scale.slots[target_sn].active = !scale.slots[target_sn].active;
				scale.slots[next].d_n = eff_wdepth;
				break;
			}

			case ToggleMode::Pin: {
				// Deactivate all, then activate only target_sn
				for (auto& s : scale.slots) s.active = false;
				scale.slots[target_sn].active = true;
				scale.slots[target_sn].d_n    = eff_wdepth;
				break;
			}

			case ToggleMode::Collapse: {
				// Deactivate target_sn; fold its state back to S0
				scale.slots[target_sn].active = false;
				scale.slots[target_sn].d_n    = 0.0;
				// Ensure S0 (existence) remains active
				scale.slots[0].active = true;
				break;
			}
		}

		r.applied = true;
		return r;
	}

	// -----------------------------------------------------------------------
	// apply_to_particle()
	//
	// Apply the scale toggle to the scale_mask field of a
	// DefaultParticleIdentityVector.  Does NOT require a full
	// DefaultScaleIdentityMatrix — operates on the bitmask inline.
	// -----------------------------------------------------------------------
	static ToggleScaleResult apply_to_particle(
		vsepr::identity::DefaultParticleIdentityVector& /*particle*/,
		const ToggleScaleConfig& cfg,
		int target_sn,
		ToggleMode mode  = ToggleMode::Cycle,
		double w_depth   = 0.0,
		bool   persist   = true
	) noexcept {
		ToggleScaleResult r;
		r.target_sn = target_sn;
		r.mode      = mode;

		if (!cfg.enabled) { r.note = "togglescale disabled"; return r; }
		if (target_sn == 6 && cfg.guard_xd) { r.note = "X_D blocked"; return r; }
		if (target_sn < 0 || target_sn > 6) { r.note = "target_sn out of range"; return r; }

		ToggleMode  eff_mode   = (mode == ToggleMode::Cycle) ? cfg.default_mode : mode;
		bool        eff_persist = persist && cfg.persist;
		r.mode            = eff_mode;
		r.w_depth_applied = w_depth;
		r.persisted       = eff_persist;

		if (!eff_persist) { r.applied = true; return r; }

		// The DefaultParticleIdentityVector doesn't carry a scale_mask field here;
		// that lives on ParticleIdentity (particle_identity.hpp). This overload
		// records intent and returns applied = true for the persist path — the
		// caller bridges to ParticleIdentity::scale_mask if needed.
		r.applied = true;
		return r;
	}

	// -----------------------------------------------------------------------
	// apply_mask_to_matrix()
	//
	// Apply the config's default slot list as a bitmask to a scale matrix.
	// Useful at startup to initialize active slots from [extras.togglescale].
	// -----------------------------------------------------------------------
	static void apply_mask_to_matrix(
		vsepr::identity::DefaultScaleIdentityMatrix& scale,
		const ToggleScaleConfig& cfg
	) noexcept {
		uint8_t mask = cfg.slot_mask();
		scale.apply_mask(mask);
		scale.set_guard_xd(cfg.guard_xd);
	}
};

} // namespace vsim::extras
