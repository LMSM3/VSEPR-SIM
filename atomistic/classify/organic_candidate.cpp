#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/ring_detector.hpp"
#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/classify/aromaticity.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace atomistic {
namespace classify {

// ============================================================================
// organic_family_name
// ============================================================================

const char* organic_family_name(OrganicFamily f) {
	switch (f) {
		case OrganicFamily::ALKANE:               return "alkane";
		case OrganicFamily::ALKENE:               return "alkene";
		case OrganicFamily::ALKYNE:               return "alkyne";
		case OrganicFamily::AROMATIC:             return "aromatic";
		case OrganicFamily::HETEROAROMATIC:       return "heteroaromatic";
		case OrganicFamily::ALCOHOL:              return "alcohol";
		case OrganicFamily::ETHER:                return "ether";
		case OrganicFamily::KETONE:               return "ketone";
		case OrganicFamily::ALDEHYDE:             return "aldehyde";
		case OrganicFamily::CARBOXYLIC_ACID:      return "carboxylic_acid";
		case OrganicFamily::ESTER:                return "ester";
		case OrganicFamily::AMIDE:                return "amide";
		case OrganicFamily::AMINE:                return "amine";
		case OrganicFamily::NITRILE:              return "nitrile";
		case OrganicFamily::HALOGENATED_ORGANIC:  return "halogenated_organic";
		case OrganicFamily::ORGANOMETALLIC:       return "organometallic";
		case OrganicFamily::POLYMER_PRECURSOR:    return "polymer_precursor";
		case OrganicFamily::CROSSLINKABLE_SPECIES: return "crosslinkable_species";
		case OrganicFamily::RADICAL_PRONE:        return "radical_prone";
		case OrganicFamily::CONJUGATED:           return "conjugated";
		case OrganicFamily::RIGID_RING:           return "rigid_ring";
		case OrganicFamily::FLEXIBLE_CHAIN:       return "flexible_chain";
		case OrganicFamily::DONOR_RICH:           return "donor_rich";
		case OrganicFamily::ACCEPTOR_RICH:        return "acceptor_rich";
		case OrganicFamily::MIXED_ORGANIC:        return "mixed_organic";
		case OrganicFamily::UNKNOWN_ORGANIC:      return "unknown_organic";
		default:                                  return "unknown";
	}
}

const char* to_string(FamilySource s) {
	switch (s) {
		case FamilySource::Inferred: return "inferred";
		case FamilySource::Manual:   return "manual";
		case FamilySource::Default:  return "default";
		default:                     return "unknown";
	}
}

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

static bool is_carbon(uint32_t z)        { return z == 6; }
static bool is_hydrogen(uint32_t z)      { return z == 1; }
static bool is_nitrogen(uint32_t z)      { return z == 7; }
static bool is_oxygen(uint32_t z)        { return z == 8; }
static bool is_halogen(uint32_t z)       { return z == 9 || z == 17 || z == 35 || z == 53; }
static bool is_heteroatom(uint32_t z)    { return z != 6 && z != 1; }

static std::vector<std::vector<std::size_t>> build_adjacency(const State& state) {
	std::vector<std::vector<std::size_t>> adj(state.N);
	for (const auto& e : state.B) {
		if (e.i < state.N && e.j < state.N) {
			adj[e.i].push_back(e.j);
			adj[e.j].push_back(e.i);
		}
	}
	return adj;
}

// Molecular formula (Hill order: C first, H second, then alphabetical by Z)
static std::string build_formula(const State& state) {
	int c_count = 0, h_count = 0;
	std::map<uint32_t, int> others;
	for (uint32_t z : state.type) {
		if (z == 6)      ++c_count;
		else if (z == 1) ++h_count;
		else             ++others[z];
	}

	std::ostringstream ss;
	if (c_count > 0) { ss << "C"; if (c_count > 1) ss << c_count; }
	if (h_count > 0) { ss << "H"; if (h_count > 1) ss << h_count; }
	for (auto& [z, cnt] : others) {
		ss << "Z" << z;
		if (cnt > 1) ss << cnt;
	}
	return ss.str();
}

// Simple deterministic hash of connectivity (not cryptographic)
static std::string build_id_hash(const State& state) {
	std::size_t h = state.N * 2654435761ULL;
	for (uint32_t z : state.type)  h ^= (h << 5) + (h >> 2) + static_cast<std::size_t>(z);
	for (const auto& e : state.B)  h ^= (h << 5) + (h >> 2) + e.i * 31 + e.j;
	std::ostringstream ss;
	ss << std::hex << h;
	return ss.str();
}

// Count heavy-atom bonds (excludes H-H and X-H where X is specified)
static int count_heavy_bonds(const State& state, const std::vector<std::vector<std::size_t>>& adj) {
	int count = 0;
	for (const auto& e : state.B) {
		if (e.i < state.N && e.j < state.N) {
			if (!is_hydrogen(state.type[e.i]) && !is_hydrogen(state.type[e.j])) {
				++count;
			}
		}
	}
	return count;
}

// Estimate whether a bond is rotatable (single bond, not terminal,
// not in a ring).  WO-84C: ring bonds are excluded via `ring_system` when a
// detector result is supplied; otherwise no ring exclusion is applied and the
// caller relies on the pre-WO-84C behaviour.
static int count_rotatable_bonds_impl(const State& state,
									  const std::vector<std::vector<std::size_t>>& adj,
									  const RingSystem* ring_system = nullptr) {
	int count = 0;
	for (const auto& e : state.B) {
		if (e.i >= state.N || e.j >= state.N) continue;
		if (is_hydrogen(state.type[e.i]) || is_hydrogen(state.type[e.j])) continue;
		// WO-84C: ring bonds are not rotatable.
		if (ring_system && ring_system->is_ring_bond(e.i, e.j)) continue;
		// Both endpoints must have >1 heavy neighbour (not terminal)
		const auto count_heavy = [&](std::size_t idx) {
			int n = 0;
			for (std::size_t nb : adj[idx]) {
				if (!is_hydrogen(state.type[nb])) ++n;
			}
			return n;
		};
		if (count_heavy(e.i) < 2 || count_heavy(e.j) < 2) continue;
		++count;
	}
	return count;
}

// Heuristic: donor atoms are N and O bonded to H
// Acceptor atoms are N and O with a lone pair (Z = 7 or 8)
static std::vector<OrganicCandidate::DonorAcceptor> build_donor_acceptor(
	const State& state,
	const std::vector<std::vector<std::size_t>>& adj
) {
	std::vector<OrganicCandidate::DonorAcceptor> result;
	for (std::size_t i = 0; i < state.N; ++i) {
		const uint32_t z = state.type[i];
		if (!is_nitrogen(z) && !is_oxygen(z)) continue;

		bool bonded_to_h = false;
		for (std::size_t nb : adj[i]) {
			if (is_hydrogen(state.type[nb])) { bonded_to_h = true; break; }
		}

		const bool donor    = bonded_to_h;
		const bool acceptor = (z == 8 || z == 7);  // conservative
		const double strength = (z == 8) ? 0.8 : 0.6;

		if (donor || acceptor) {
			result.push_back({
				static_cast<uint32_t>(i),
				donor,
				acceptor,
				strength
			});
		}
	}
	return result;
}

// Stub reactive site map (Fukui functions unavailable without QM data)
static std::vector<OrganicCandidate::ReactiveSite> build_reactive_sites(
	const State& state
) {
	std::vector<OrganicCandidate::ReactiveSite> result;
	for (std::size_t i = 0; i < state.N; ++i) {
		const uint32_t z = state.type[i];
		// Flag halogens and carbonyls as electrophilic centres heuristically
		if (is_halogen(z)) {
			result.push_back({
				static_cast<uint32_t>(i), "halogen",
				0.0, 0.4, 0.3, "electrophilic_centre"
			});
		}
	}
	return result;
}

} // namespace

// ============================================================================
// OrganicClassifier method implementations
// ============================================================================

OrganicCandidate OrganicClassifier::classify(const State& state) const {
	OrganicCandidate c;
	c.id_hash = build_id_hash(state);
	c.formula  = build_formula(state);

	const auto adj = build_adjacency(state);

	// Heteroatom count
	for (uint32_t z : state.type) {
		if (is_heteroatom(z) && !is_hydrogen(z)) {
			++c.heteroatom_count;
		}
	}

	// WO-84C: ring detection.  Prefer a wired RingProvider; otherwise run the
	// deterministic detector once and reuse its result for both ring counts and
	// rotatable-bond exclusion so the two stay consistent.
	RingSystem ring_system = detect_ring_system(state);
	const RingInfo rings = count_rings(state);
	c.ring_count          = rings.total;

	// WO-84D: aromatic ring count via the Huckel gate (bond-order aware).
	const BondOrderTable bond_orders = BondOrderProviderBuilder::infer(state);
	const AromaticityReport aroma =
		analyze_aromaticity(state, ring_system, bond_orders);
	c.aromatic_ring_count = aroma.aromatic_ring_count;

	// Rotatable bonds (WO-84C: exclude ring bonds).
	c.rotatable_bond_count = count_rotatable_bonds_impl(state, adj, &ring_system);

	// VSEPR-powered hybridisation counts and strain.
	// WO-84A: thread the wired provider set into VSEPR so an available
	// lone-pair provider overrides element/geometry inference deterministically.
	VSEPROptions vsepr_options;
	vsepr_options.providers = providers_;
	const VSEPRReport vsepr = classify_vsepr_sites(state, vsepr_options);
	c.sp_count  = static_cast<int>(vsepr.linear_count);
	c.sp2_count = static_cast<int>(vsepr.planar_count);
	c.sp3_count = static_cast<int>(vsepr.tetrahedral_count);

	// WO-84A: propagate provider/fallback provenance for CLI/report narration.
	c.provider_set_available = providers_.lone_pair.available() ||
							   providers_.bond_order.available() ||
							   providers_.ring.available() ||
							   providers_.formal_charge.available();
	c.used_provider_lone_pair = vsepr.provider_lone_pair_sites > 0;
	c.used_fallback_lone_pair = vsepr.fallback_lone_pair_sites > 0;

	// Strain score from mean VSEPR rms_deviation
	double total_rms = 0.0;
	for (const auto& site : vsepr.sites) {
		total_rms += site.angle_stats.rms_deviation_deg;
	}
	if (!vsepr.sites.empty()) {
		c.strain_score = std::clamp(
			total_rms / (45.0 * static_cast<double>(vsepr.sites.size())),
			0.0, 1.0
		);
	}

	// Polarity from partial charges (if present)
	c.polarity_score = compute_polarity(state);

	// Families, functional groups, donor/acceptor, reactive sites
	c.families           = identify_families(state);
	// WO-84D: promote AROMATIC family when the Huckel gate found an aromatic ring.
	if (aroma.any_aromatic() &&
		std::find(c.families.begin(), c.families.end(), OrganicFamily::AROMATIC)
			== c.families.end()) {
		c.families.insert(c.families.begin(), OrganicFamily::AROMATIC);
	}
	c.primary_family     = c.families.empty() ? OrganicFamily::UNKNOWN_ORGANIC : c.families.front();
	c.functional_groups  = detect_functional_groups(state);
	c.donor_acceptor_map = build_donor_acceptor(state, adj);
	c.reactive_sites     = build_reactive_sites(state);

	// hbond scores from donor/acceptor map
	for (const auto& da : c.donor_acceptor_map) {
		if (da.is_donor)    c.hbond_donor_score    += da.strength;
		if (da.is_acceptor) c.hbond_acceptor_score += da.strength;
	}
	c.hbond_donor_score    = std::clamp(c.hbond_donor_score,    0.0, 1.0);
	c.hbond_acceptor_score = std::clamp(c.hbond_acceptor_score, 0.0, 1.0);

	c.conjugation_score    = compute_conjugation(state);
	c.steric_score         = compute_steric(state);
	c.decomposition_risk   = estimate_decomposition_risk(state);
	c.lipid_like_score     = compute_lipid_like_score(state);  // WO-83N
	c.family_source        = c.families.empty()               // WO-83M
		? FamilySource::Default
		: FamilySource::Inferred;

	return c;
}

std::vector<OrganicFamily> OrganicClassifier::identify_families(const State& state) const {
	std::vector<OrganicFamily> result;

	int c_count = 0, n_count = 0, o_count = 0, halogen_count = 0;
	for (uint32_t z : state.type) {
		if (is_carbon(z))   ++c_count;
		if (is_nitrogen(z)) ++n_count;
		if (is_oxygen(z))   ++o_count;
		if (is_halogen(z))  ++halogen_count;
	}

	const int total_non_h = c_count + n_count + o_count + halogen_count;
	if (total_non_h == 0) return result;

	if (halogen_count > 0) result.push_back(OrganicFamily::HALOGENATED_ORGANIC);
	if (n_count > 0 && o_count == 0) result.push_back(OrganicFamily::AMINE);
	if (n_count > 0 && o_count > 0)  result.push_back(OrganicFamily::AMIDE);
	if (o_count >= 2)                result.push_back(OrganicFamily::CARBOXYLIC_ACID);
	if (o_count == 1)                result.push_back(OrganicFamily::ALCOHOL);
	if (c_count > 0 && n_count == 0 && o_count == 0 && halogen_count == 0)
		result.push_back(OrganicFamily::ALKANE);

	const auto adj = build_adjacency(state);
	const RingSystem ring_system = detect_ring_system(state);
	const int rotatable = count_rotatable_bonds_impl(state, adj, &ring_system);
	if (rotatable >= 4) result.push_back(OrganicFamily::FLEXIBLE_CHAIN);

	// WO-84C: a detected ring implies a (rigid) ring-bearing species.
	if (ring_system.total_rings() > 0) {
		result.push_back(OrganicFamily::RIGID_RING);
	}

	if (result.empty()) result.push_back(OrganicFamily::UNKNOWN_ORGANIC);
	return result;
}

std::vector<std::string> OrganicClassifier::detect_functional_groups(const State& state) const {
	std::vector<std::string> groups;
	const auto adj = build_adjacency(state);

	for (std::size_t i = 0; i < state.N; ++i) {
		const uint32_t z = state.type[i];
		if (is_oxygen(z)) {
			bool bonded_to_h = false;
			for (std::size_t nb : adj[i]) {
				if (is_hydrogen(state.type[nb])) { bonded_to_h = true; break; }
			}
			if (bonded_to_h) {
				groups.push_back("hydroxyl");
			}
		}
		if (is_nitrogen(z)) {
			int h_count = 0;
			for (std::size_t nb : adj[i]) {
				if (is_hydrogen(state.type[nb])) ++h_count;
			}
			if (h_count >= 1) groups.push_back("amine");
		}
		if (is_halogen(z)) {
			groups.push_back("halide");
		}
	}

	// Deduplicate
	std::sort(groups.begin(), groups.end());
	groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
	return groups;
}

OrganicClassifier::RingInfo OrganicClassifier::count_rings(const State& state) const {
	RingInfo info;

	// WO-84C: prefer provider-backed ring data when a RingProvider is wired.
	if (providers_.ring.available()) {
		if (auto total = providers_.ring.total_rings()) {
			info.total = *total;
			int strained = 0;
			for (std::size_t i = 0; i < state.N; ++i) {
				if (providers_.ring.in_strained_ring(i)) { strained = 1; break; }
			}
			// Count strained rings by scanning per-atom ring sizes for size<=4.
			// A distinct strained ring is hard to isolate from membership alone,
			// so report a conservative presence flag as a non-zero count.
			info.strained = 0;
			for (std::size_t i = 0; i < state.N; ++i) {
				for (int s : providers_.ring.ring_sizes(i)) {
					if (s <= 4) { info.strained = std::max(info.strained, 1); }
				}
			}
			(void)strained;
			return info;
		}
	}

	// Fallback: run the deterministic detector directly (still exact SSSR-lite),
	// so ring counts are correct even without an externally-wired provider.
	const RingSystem system = detect_ring_system(state);
	info.total    = system.total_rings();
	info.strained = system.strained_rings();

	// Final fallback if detection somehow produced nothing: circuit rank.
	if (info.total == 0 && system.circuit_rank > 0) {
		info.total = system.circuit_rank;
	}
	return info;
}

int OrganicClassifier::count_rotatable_bonds(const State& state) const {
	const auto adj = build_adjacency(state);
	// WO-84C: exclude ring bonds from rotatable-bond counts.
	const RingSystem system = detect_ring_system(state);
	return count_rotatable_bonds_impl(state, adj, &system);
}

double OrganicClassifier::compute_steric(const State& state) const {
	if (state.N == 0) return 0.0;

	const auto adj = build_adjacency(state);
	double total = 0.0;
	int heavy_count = 0;
	for (std::size_t i = 0; i < state.N; ++i) {
		if (is_hydrogen(state.type[i])) continue;
		int heavy_nbr = 0;
		for (std::size_t nb : adj[i]) {
			if (!is_hydrogen(state.type[nb])) ++heavy_nbr;
		}
		total += static_cast<double>(heavy_nbr);
		++heavy_count;
	}
	if (heavy_count == 0) return 0.0;
	// Normalise: coordination 4 -> score 1.0, coordination 1 -> score 0.25
	return std::clamp(total / (4.0 * static_cast<double>(heavy_count)), 0.0, 1.0);
}

double OrganicClassifier::compute_polarity(const State& state) const {
	if (state.Q.empty() || state.N == 0) {
		// Heuristic from element types
		int heteroatom_count = 0;
		for (uint32_t z : state.type) {
			if (is_heteroatom(z) && !is_hydrogen(z)) ++heteroatom_count;
		}
		const int total_non_h = [&] {
			int n = 0;
			for (uint32_t z : state.type) { if (!is_hydrogen(z)) ++n; }
			return n;
		}();
		if (total_non_h == 0) return 0.0;
		return std::clamp(
			static_cast<double>(heteroatom_count) / static_cast<double>(total_non_h),
			0.0, 1.0
		);
	}

	// Charge-based polarity: rms of partial charges normalised to [0,1]
	double sum_sq = 0.0;
	for (double q : state.Q) sum_sq += q * q;
	const double rms = std::sqrt(sum_sq / static_cast<double>(state.N));
	return std::clamp(rms / 0.5, 0.0, 1.0);  // 0.5 e is "fully polar"
}

double OrganicClassifier::compute_conjugation(const State& state) const {
	if (state.N == 0) return 0.0;

	// sp2 fraction as a proxy for conjugation
	const VSEPRReport vsepr = classify_vsepr_sites(state);
	const std::size_t sp2 = vsepr.planar_count;
	const std::size_t total_heavy = [&] {
		std::size_t n = 0;
		for (uint32_t z : state.type) { if (!is_hydrogen(z)) ++n; }
		return n;
	}();
	if (total_heavy == 0) return 0.0;
	return std::clamp(static_cast<double>(sp2) / static_cast<double>(total_heavy), 0.0, 1.0);
}

double OrganicClassifier::compute_strain(const State& state) const {
	if (state.N == 0) return 0.0;

	const VSEPRReport vsepr = classify_vsepr_sites(state);
	if (vsepr.sites.empty()) return 0.0;

	double total_rms = 0.0;
	for (const auto& site : vsepr.sites) {
		total_rms += site.angle_stats.rms_deviation_deg;
	}
	return std::clamp(
		total_rms / (45.0 * static_cast<double>(vsepr.sites.size())),
		0.0, 1.0
	);
}

std::vector<OrganicCandidate::DonorAcceptor>
OrganicClassifier::map_donors_acceptors(const State& state) const {
	const auto adj = build_adjacency(state);
	return build_donor_acceptor(state, adj);
}

std::vector<OrganicCandidate::ReactiveSite>
OrganicClassifier::map_reactive_sites(const State& state) const {
	return build_reactive_sites(state);
}

double OrganicClassifier::estimate_decomposition_risk(const State& state) const {
	double risk = 0.0;

	// High strain -> higher decomposition risk
	risk += compute_strain(state) * 0.4;

	// High heteroatom fraction -> elevated risk
	int heteroatom_count = 0;
	int total_non_h = 0;
	for (uint32_t z : state.type) {
		if (!is_hydrogen(z)) {
			++total_non_h;
			if (is_heteroatom(z)) ++heteroatom_count;
		}
	}
	if (total_non_h > 0) {
		risk += 0.3 * (static_cast<double>(heteroatom_count) /
					   static_cast<double>(total_non_h));
	}

	// Halogens elevate risk slightly
	int halogen_count = 0;
	for (uint32_t z : state.type) { if (is_halogen(z)) ++halogen_count; }
	if (halogen_count > 0) risk += 0.15;

	return std::clamp(risk, 0.0, 1.0);
}

// ============================================================================
// WO-83N: compute_lipid_like_score — composite descriptor
// ============================================================================

double OrganicClassifier::compute_lipid_like_score(const State& state) const {
	if (state.N < 4) return 0.0;

	double score = 0.0;

	// Count carbon and hydrogen for hydrocarbon chain evidence
	int c_count = 0, h_count = 0, n_count = 0, o_count = 0, halogen_count = 0;
	for (uint32_t z : state.type) {
		if (z == 6)        ++c_count;
		else if (z == 1)   ++h_count;
		else if (z == 7)   ++n_count;
		else if (z == 8)   ++o_count;
		else if (z == 9 || z == 17 || z == 35 || z == 53) ++halogen_count;
	}
	const int total_heavy = static_cast<int>(state.N) - h_count;
	if (total_heavy == 0) return 0.0;

	// 1. Long hydrocarbon chain evidence: high C fraction among heavy atoms
	const double c_fraction = static_cast<double>(c_count) / total_heavy;
	if (c_fraction >= 0.85) score += 0.35;
	else if (c_fraction >= 0.70) score += 0.20;
	else if (c_fraction >= 0.50) score += 0.08;

	// 2. Flexible chain evidence: high rotatable bond count relative to heavy atoms
	const auto adj = build_adjacency(state);
	const int rot = count_rotatable_bonds_impl(state, adj);
	const double rot_density = static_cast<double>(rot) / total_heavy;
	if (rot_density >= 0.4) score += 0.20;
	else if (rot_density >= 0.2) score += 0.10;

	// 3. Ester/carboxylic evidence: O present, small heteroatom count
	if (o_count >= 1 && o_count <= 4 && n_count == 0) score += 0.15;

	// 4. Polarity balance: moderate O/N fraction (amphiphilic, not ionic)
	const double hetero_fraction =
		static_cast<double>(n_count + o_count) / total_heavy;
	if (hetero_fraction >= 0.02 && hetero_fraction <= 0.20) score += 0.10;

	// 5. Excessive heteroatom penalty
	if (hetero_fraction > 0.35) score -= 0.20;

	// 6. Halogen penalty (halogens are unusual in neutral lipids)
	if (halogen_count > 0) score -= 0.10;

	return std::clamp(score, 0.0, 1.0);
}

// ============================================================================
// WO-83Q: format_organic_candidate
// ============================================================================

std::string format_organic_candidate(const OrganicCandidate& c) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3);

	oss << "organic_candidate:\n";
	oss << "  formula:         " << c.formula << "\n";
	oss << "  primary_family:  " << organic_family_name(c.primary_family);
	oss << "  [" << to_string(c.family_source) << "]\n";

	if (!c.families.empty()) {
		oss << "  families:";
		for (auto f : c.families) oss << "  " << organic_family_name(f);
		oss << "\n";
	}
	if (!c.functional_groups.empty()) {
		oss << "  functional_groups:";
		for (const auto& g : c.functional_groups) oss << "  " << g;
		oss << "\n";
	}

	oss << "  sp_count:        " << c.sp_count << "\n";
	oss << "  sp2_count:       " << c.sp2_count << "\n";
	oss << "  sp3_count:       " << c.sp3_count << "\n";
	oss << "  ring_count:      " << c.ring_count << "\n";
	oss << "  aromatic_rings:  " << c.aromatic_ring_count << "\n";
	oss << "  rotatable_bonds: " << c.rotatable_bond_count << "\n";
	oss << "  heteroatom_count:" << c.heteroatom_count << "\n";
	oss << "  strain_score:    " << c.strain_score << "\n";
	oss << "  lipid_like_score:" << c.lipid_like_score << "\n";
	oss << "  polarity_score:  " << c.polarity_score << "\n";
	oss << "  conjugation_score:" << c.conjugation_score << "\n";
	oss << "  hbond_donor:     " << c.hbond_donor_score << "\n";
	oss << "  hbond_acceptor:  " << c.hbond_acceptor_score << "\n";
	oss << "  decomp_risk:     " << c.decomposition_risk << "\n";

	return oss.str();
}

} // namespace classify
} // namespace atomistic
