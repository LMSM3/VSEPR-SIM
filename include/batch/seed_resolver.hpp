#pragma once
/**
 * include/batch/seed_resolver.hpp
 * =================================
 * WO-VSIM-62C — Seed Derivation
 *
 * SeedResolver fills the resolved_* fields of SeedSection from the
 * declared foundation and sub-seeds.  If a sub-seed is 0, it is
 * derived from the foundation using the fixed offsets from the spec.
 *
 * replicate_seed() computes a per-replicate seed from a base seed
 * using the declared seed_policy ("split" | "shift").
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/vsim/vsim_document.hpp"
#include <cstdint>
#include <string>

namespace vsim {
namespace batch {

class SeedResolver {
public:
	// Fill resolved_* fields; explicit non-zero sub-seeds are preserved.
	static SeedSection resolve(const SeedSection& declared);

	// Derive per-replicate seed from a base seed.
	// policy = "split" → base + replicate
	// policy = "shift" → base + replicate * 1000
	static uint64_t replicate_seed(uint64_t base_seed,
								   int       replicate,
								   const std::string& policy);
};

} // namespace batch
} // namespace vsim
