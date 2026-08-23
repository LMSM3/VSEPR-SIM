#pragma once
/**
 * include/vsim/validation/surface_warnings.hpp
 * ==============================================
 * 72-floating-12  |  V5.1.4
 *
 * Quiet validation helpers for surface-mode simulations.
 *
 * Warnings are emitted as structured records, not console spam.
 * They are visible in audit mode or when the caller explicitly checks them.
 *
 * Primary check:
 *   if surface_mode && bead_count < SURFACE_BEAD_MIN_DEFAULT:
 *       emit SurfaceWarning::UnderpopulatedSurface
 *
 * Design:
 *   - check_surface_bead_count() returns a SurfaceWarningResult — never throws.
 *   - The caller decides whether to log, assert, or ignore.
 *   - Audit mode (audit=true) writes to the provided ostream.
 *   - Default threshold: 100 beads (configurable per call).
 *
 * 72-floating-12  |  V5.1.4  |  v5.0.0-main
 */

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace vsim {
namespace validation {

// ============================================================================
// Constants
// ============================================================================

inline constexpr uint32_t SURFACE_BEAD_MIN_DEFAULT = 100;

// ============================================================================
// SurfaceWarningKind
// ============================================================================

enum class SurfaceWarningKind : uint8_t {
	UnderpopulatedSurface = 0,  // bead_count < threshold in surface mode
	ZeroBeads             = 1,  // bead_count == 0
	NegativeBeads         = 2,  // pathological: negative count supplied
};

inline const char* surface_warning_kind_name(SurfaceWarningKind k) noexcept {
	switch (k) {
		case SurfaceWarningKind::UnderpopulatedSurface: return "UnderpopulatedSurface";
		case SurfaceWarningKind::ZeroBeads:             return "ZeroBeads";
		case SurfaceWarningKind::NegativeBeads:         return "NegativeBeads";
	}
	return "Unknown";
}

// ============================================================================
// SurfaceWarning  —  one structured warning record
// ============================================================================

struct SurfaceWarning {
	SurfaceWarningKind kind;
	std::string        context;      // e.g. script name or section label
	int64_t            bead_count;
	uint32_t           threshold;
	std::string        message;      // human-readable summary

	static SurfaceWarning make(SurfaceWarningKind k,
							   const std::string& ctx,
							   int64_t            count,
							   uint32_t           thresh) {
		SurfaceWarning w;
		w.kind       = k;
		w.context    = ctx;
		w.bead_count = count;
		w.threshold  = thresh;

		char buf[256];
		switch (k) {
			case SurfaceWarningKind::UnderpopulatedSurface:
				std::snprintf(buf, sizeof(buf),
					"[SURFACE WARN] %s: bead_count=%lld < threshold=%u "
					"— surface statistics may be unreliable",
					ctx.c_str(), (long long)count, thresh);
				break;
			case SurfaceWarningKind::ZeroBeads:
				std::snprintf(buf, sizeof(buf),
					"[SURFACE WARN] %s: bead_count=0 — empty surface",
					ctx.c_str());
				break;
			case SurfaceWarningKind::NegativeBeads:
				std::snprintf(buf, sizeof(buf),
					"[SURFACE WARN] %s: bead_count=%lld — invalid (negative)",
					ctx.c_str(), (long long)count);
				break;
		}
		w.message = buf;
		return w;
	}
};

// ============================================================================
// SurfaceWarningResult  —  returned by check functions
// ============================================================================

struct SurfaceWarningResult {
	bool                        ok;        // true = no warnings
	std::vector<SurfaceWarning> warnings;

	void push(SurfaceWarning w) {
		ok = false;
		warnings.push_back(std::move(w));
	}

	// Write all warnings to a stream (audit mode).
	void audit(std::ostream& out) const {
		for (const auto& w : warnings)
			out << w.message << "\n";
	}
};

// ============================================================================
// check_surface_bead_count()  —  primary validation entry point
// ============================================================================

inline SurfaceWarningResult check_surface_bead_count(
	int64_t            bead_count,
	bool               is_surface_mode,
	const std::string& context   = "",
	uint32_t           threshold = SURFACE_BEAD_MIN_DEFAULT,
	std::ostream*      audit_out = nullptr
) {
	SurfaceWarningResult r;
	r.ok = true;

	if (!is_surface_mode) return r;   // only relevant in surface mode

	if (bead_count < 0) {
		r.push(SurfaceWarning::make(SurfaceWarningKind::NegativeBeads,
									context, bead_count, threshold));
	} else if (bead_count == 0) {
		r.push(SurfaceWarning::make(SurfaceWarningKind::ZeroBeads,
									context, bead_count, threshold));
	} else if (static_cast<uint32_t>(bead_count) < threshold) {
		r.push(SurfaceWarning::make(SurfaceWarningKind::UnderpopulatedSurface,
									context, bead_count, threshold));
	}

	if (audit_out && !r.ok)
		r.audit(*audit_out);

	return r;
}

} // namespace validation
} // namespace vsim
