// src/v4/uff/uff_autocreate.cpp
// Formation Engine v4.1.0 -- UFF auto-creator implementation

#include "uff_autocreate.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace vsepr::uff {

// ---------------------------------------------------------------------------
// Period-scaling table for universal stub
// r1 base, x1 base, D1 base, zeta base -- one entry per period 1-7
// ---------------------------------------------------------------------------
struct PeriodDefaults {
	double r1, x1, D1, zeta;
};

constexpr std::array<PeriodDefaults, 8> k_period_defaults = {{
	{ 0.0,  0.0,  0.0,   0.0   },  // [0] unused (1-based)
	{ 0.35, 2.90, 0.044, 12.0  },  // period 1
	{ 0.90, 3.00, 0.080, 12.0  },  // period 2
	{ 1.20, 3.50, 0.150, 12.0  },  // period 3
	{ 1.40, 3.70, 0.200, 12.0  },  // period 4
	{ 1.50, 3.90, 0.250, 12.0  },  // period 5
	{ 1.55, 3.00, 0.100, 12.0  },  // period 6
	{ 1.70, 3.10, 0.080, 12.0  },  // period 7
}};

static int period_for_Z(int Z) noexcept {
	if (Z <=  2) return 1;
	if (Z <= 10) return 2;
	if (Z <= 18) return 3;
	if (Z <= 36) return 4;
	if (Z <= 54) return 5;
	if (Z <= 86) return 6;
	return 7;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UFFAutoCreator::UFFAutoCreator(const UFFReferenceProvider& reference_provider)
	: ref_(reference_provider)
{}

// ---------------------------------------------------------------------------
// Primary entry point
// ---------------------------------------------------------------------------

UFFEntry UFFAutoCreator::create_missing_entry(
	const std::string& atom_type,
	const ChemicalContext& ctx) const
{
	// 1. Try interpolation first.
	auto interp = interpolate_from_neighbours_(atom_type, ctx);
	if (interp.has_value()) {
		return *interp;
	}

	// 2. Fall back to universal period stub.
	return universal_stub_(atom_type, ctx);
}

// ---------------------------------------------------------------------------
// Interpolation: look up two published types by element, average their params.
// Rev 1: simple element-only neighbour search via lookup_by_element().
// ---------------------------------------------------------------------------

std::optional<UFFEntry> UFFAutoCreator::interpolate_from_neighbours_(
	const std::string& atom_type,
	const ChemicalContext& ctx) const
{
	if (ctx.element.empty()) return std::nullopt;

	// Try to find any published entry for the same element.
	auto base = ref_.lookup(ctx.element);
	if (!base.has_value()) return std::nullopt;

	UFFEntry e = *base;
	e.atom_type          = atom_type;
	e.coordination_number = ctx.coordination > 0 ? ctx.coordination : e.coordination_number;
	e.geometry_tag       = ctx.geometry_tag.empty() ? e.geometry_tag : ctx.geometry_tag;
	e.confidence         = ParamConfidence::Derived;
	e.source_id          = "autocreate_interpolate";
	e.source_note        = "Derived from published same-element entry; "
						   "use caution for quantitative evaluation.";
	return e;
}

// ---------------------------------------------------------------------------
// Universal stub: period-scaled defaults, physically plausible topology
// but NOT suitable for quantitative energy or gradient evaluation.
// ---------------------------------------------------------------------------

UFFEntry UFFAutoCreator::universal_stub_(
	const std::string& atom_type,
	const ChemicalContext& ctx) const
{
	const int period = (ctx.atomic_number > 0)
		? period_for_Z(ctx.atomic_number)
		: 4;  // guess period 4 if Z unknown

	const auto& pd = k_period_defaults[static_cast<std::size_t>(
		std::clamp(period, 1, 7))];

	UFFEntry e;
	e.atom_type           = atom_type;
	e.element             = ctx.element;
	e.coordination_number = ctx.coordination;
	e.geometry_tag        = ctx.geometry_tag;
	e.r1                  = pd.r1;
	e.theta0              = 109.47;
	e.x1                  = pd.x1;
	e.D1                  = pd.D1;
	e.zeta                = pd.zeta;
	e.Z1                  = 1.0;
	e.Vi                  = 0.0;
	e.Uj                  = 0.0;
	e.Xi                  = 0.0;
	e.Hard                = 0.0;
	e.Radius              = pd.r1;
	e.confidence          = ParamConfidence::Estimated;
	e.source_id           = "autocreate_stub";
	e.source_note         = "Universal period stub; NOT suitable for quantitative evaluation.";
	return e;
}

} // namespace vsepr::uff
