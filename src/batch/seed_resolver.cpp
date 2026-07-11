/**
 * src/batch/seed_resolver.cpp
 * =============================
 * WO-VSIM-62C — Seed Derivation Implementation
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/seed_resolver.hpp"

namespace vsim {
namespace batch {

SeedSection SeedResolver::resolve(const SeedSection& declared) {
	SeedSection out = declared;
	if (out.defect    == 0) out.resolved_defect    = out.foundation + 3000;
	else                    out.resolved_defect    = out.defect;
	if (out.formation == 0) out.resolved_formation = out.foundation + 6000;
	else                    out.resolved_formation = out.formation;
	if (out.thermal   == 0) out.resolved_thermal   = out.foundation + 8000;
	else                    out.resolved_thermal   = out.thermal;
	if (out.placement == 0) out.resolved_placement = out.foundation + 11000;
	else                    out.resolved_placement = out.placement;
	return out;
}

uint64_t SeedResolver::replicate_seed(uint64_t base_seed,
									   int       replicate,
									   const std::string& policy) {
	if (policy == "split") return base_seed + static_cast<uint64_t>(replicate);
	if (policy == "shift") return base_seed + static_cast<uint64_t>(replicate) * 1000u;
	return base_seed;
}

} // namespace batch
} // namespace vsim
