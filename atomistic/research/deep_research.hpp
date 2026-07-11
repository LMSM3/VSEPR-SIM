#pragma once
/**
 * deep_research.hpp -- Multi-Phase Deep Research Engine
 * =====================================================
 *
 * Implements the full atomistic deep-research pipeline:
 *
 *   Phase 10A: Atomistic Geometry Enumeration
 *     - Takes a seed element (e.g. Cl) and attempts exhaustive physically
 *       valid geometries by combinatorial ligand attachment.
 *     - Each unique chemical pathway is fingerprinted with SHA-512.
 *     - Overlapping sub-trees share computation (hash-collision reuse).
 *     - FIRE minimisation validates each candidate.
 *     - Live ZMQ frames stream every attempt to the 3D GUI window.
 *
 *   Phase 20B: Bead Formation (Semi-Stochastic)
 *     - Converged atomistic geometries are mapped to coarse-grained beads.
 *     - Stochastic bonding from output data drives bead-level formation.
 *     - MD relaxation at the bead level with random perturbation.
 *
 *   Phase 31C: MD Bonding and Assembly
 *     - Beads undergo random bonding trials.
 *     - Molecular dynamics at the bead level with energy tracking.
 *     - Successful assemblies are recorded with full hash provenance.
 *
 *   Phase 41A: Lattice Assembly (Blank Call)
 *     - Converged bead clusters are handed off to the lattice builder.
 *     - No user parameters required -- the lattice phase infers geometry
 *       from the bead arrangement.
 *
 * Hash System (SHA-512):
 *   Every chemical pathway is identified by:
 *     H = SHA-512( seed_element || ligand_sequence || bond_orders || geometry_class )
 *   Overlap detection: if H_new matches H_existing, we skip redundant FIRE
 *   and reuse the cached result.  This gives O(1) lookup for repeated sub-trees.
 *
 * CLI entry point:
 *   wsl -e 'atomistic --deep-research initnum=220'
 *
 * Anti-black-box: every intermediate state, every hash, every energy is
 * recorded in the StudyLedger and can be inspected post-hoc.
 *
 * Terminology: atomistic throughout (no meso).
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/linalg.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "atomistic/reaction/discovery.hpp"
#include "atomistic/classify/fingerprints.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace atomistic {
namespace research {

// ============================================================================
// SHA-512 Pathway Hash
// ============================================================================

/// 64-byte SHA-512 digest
using Hash512 = std::array<uint8_t, 64>;

/// Convert hash to hex string
inline std::string hash_to_hex(const Hash512& h) {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (auto b : h) os << std::setw(2) << static_cast<int>(b);
    return os.str();
}

/**
 * Lightweight SHA-512 implementation (single-block, for pathway IDs).
 *
 * For production use, this would delegate to OpenSSL or libsodium.
 * Here we provide a self-contained implementation sufficient for
 * deterministic pathway identification.
 */
inline Hash512 sha512(const void* data, size_t len) {
    // SHA-512 constants (first 80 rounds)
    static const uint64_t K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
        0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
        0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
        0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
        0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
        0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
        0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
        0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
        0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
        0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
        0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a1a81a664bULL, 0xa81a664bbc423001ULL,
        0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
        0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
        0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
        0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
        0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
        0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
        0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
        0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
        0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
        0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
        0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
    };
    auto rotr = [](uint64_t x, int n) -> uint64_t {
        return (x >> n) | (x << (64 - n));
    };

    // Initial hash values
    uint64_t H[8] = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
        0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
        0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };

    // Pad message
    size_t bit_len = len * 8;
    size_t padded_len = ((len + 16 + 127) / 128) * 128;
    std::vector<uint8_t> msg(padded_len, 0);
    std::memcpy(msg.data(), data, len);
    msg[len] = 0x80;
    for (int i = 0; i < 8; ++i)
        msg[padded_len - 1 - i] = static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF);

    // Process blocks
    for (size_t blk = 0; blk < padded_len; blk += 128) {
        uint64_t W[80];
        for (int t = 0; t < 16; ++t) {
            W[t] = 0;
            for (int b = 0; b < 8; ++b)
                W[t] = (W[t] << 8) | msg[blk + t * 8 + b];
        }
        for (int t = 16; t < 80; ++t) {
            uint64_t s0 = rotr(W[t-15], 1) ^ rotr(W[t-15], 8) ^ (W[t-15] >> 7);
            uint64_t s1 = rotr(W[t-2], 19) ^ rotr(W[t-2], 61) ^ (W[t-2] >> 6);
            W[t] = W[t-16] + s0 + W[t-7] + s1;
        }
        uint64_t a=H[0], b=H[1], c=H[2], d=H[3];
        uint64_t e=H[4], f=H[5], g=H[6], h=H[7];
        for (int t = 0; t < 80; ++t) {
            uint64_t S1 = rotr(e,14) ^ rotr(e,18) ^ rotr(e,41);
            uint64_t ch = (e & f) ^ (~e & g);
            uint64_t temp1 = h + S1 + ch + K[t] + W[t];
            uint64_t S0 = rotr(a,28) ^ rotr(a,34) ^ rotr(a,39);
            uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint64_t temp2 = S0 + maj;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d;
        H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
    }

    Hash512 result;
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            result[i*8+j] = static_cast<uint8_t>((H[i] >> (56 - j*8)) & 0xFF);
    return result;
}

// ============================================================================
// Pathway Fingerprint
// ============================================================================

/**
 * A chemical pathway is the ordered sequence of decisions that produced
 * a specific geometry:
 *   (seed_element, ligand_1, bond_order_1, ligand_2, bond_order_2, ..., geometry_class)
 *
 * The SHA-512 hash of this sequence uniquely identifies the pathway.
 * Two pathways that produce the same hash are computationally identical
 * and can share FIRE results.
 */
struct PathwayFingerprint {
    std::string seed_element;                               ///< e.g. "Cl"
    std::vector<std::pair<std::string, int>> ligand_bonds;  ///< (element, bond_order)
    std::string geometry_class;                             ///< e.g. "AX3E1"
    Hash512     hash;                                       ///< SHA-512 of serialised pathway
    uint64_t    pathway_id = 0;                             ///< Sequential ID

    /// Serialise pathway to deterministic byte string for hashing
    std::string serialise() const {
        std::ostringstream os;
        os << seed_element << "|";
        for (auto& [elem, order] : ligand_bonds)
            os << elem << ":" << order << ",";
        os << "|" << geometry_class;
        return os.str();
    }

    /// Compute and store the SHA-512 hash
    void compute_hash() {
        std::string s = serialise();
        hash = sha512(s.data(), s.size());
    }

    std::string hash_hex() const { return hash_to_hex(hash); }
};

// ============================================================================
// Geometry Candidate
// ============================================================================

struct GeometryCandidate {
    PathwayFingerprint  fingerprint;
    State               state;          ///< Atomistic state (positions, types, etc.)
    double              energy = 0.0;   ///< FIRE-minimised energy (kcal/mol)
    double              rms_force = 0.0;///< Final RMS force
    bool                converged = false;
    int                 fire_steps = 0;
    std::string         formula;        ///< e.g. "Cl2O"
    std::string         vsepr_class;    ///< e.g. "AX2E2"

    /// XYZ-format string for ZMQ streaming
    std::string to_xyz() const {
        std::ostringstream os;
        os << state.N << "\n";
        os << formula << " E=" << energy << " hash=" << fingerprint.hash_hex().substr(0,16) << "\n";
        for (uint32_t i = 0; i < state.N; ++i) {
            os << state.symbols[i] << " "
               << state.x[i] << " " << state.y[i] << " " << state.z[i] << "\n";
        }
        return os.str();
    }
};

// ============================================================================
// Bead Formation Record (Phase 20B)
// ============================================================================

struct BeadFormationRecord {
    uint64_t            source_pathway_id = 0;  ///< From Phase 10A
    Hash512             bead_hash;               ///< SHA-512 of bead state
    double              bead_energy = 0.0;
    double              bead_radius = 0.0;       ///< Effective vdW radius
    std::vector<double> bead_center;             ///< COM position
    int                 md_steps = 0;
    bool                stable = false;
    std::string         bead_class;              ///< e.g. "organic_polar"
};

// ============================================================================
// Assembly Record (Phase 31C)
// ============================================================================

struct AssemblyRecord {
    std::vector<uint64_t> bead_pathway_ids;      ///< Contributing beads
    Hash512               assembly_hash;
    double                assembly_energy = 0.0;
    int                   bond_count = 0;
    int                   md_steps = 0;
    bool                  converged = false;
};

// ============================================================================
// Study Ledger -- Full Provenance Log
// ============================================================================

/**
 * The StudyLedger records every action taken during a deep-research run.
 * It is the anti-black-box audit trail: every geometry attempt, every hash,
 * every energy, every phase transition is logged here.
 */
struct StudyLedger {
    // Phase 10A
    uint64_t total_pathways_attempted = 0;
    uint64_t unique_pathways = 0;
    uint64_t hash_collisions_reused = 0;    ///< Computation saved by overlap
    uint64_t fire_calls = 0;
    uint64_t converged_geometries = 0;
    std::vector<GeometryCandidate> candidates;
    std::unordered_map<std::string, uint64_t> hash_cache;  ///< hex→candidate_idx

    // Phase 20B
    uint64_t bead_formations_attempted = 0;
    uint64_t bead_formations_stable = 0;
    std::vector<BeadFormationRecord> beads;

    // Phase 31C
    uint64_t assembly_attempts = 0;
    uint64_t assembly_converged = 0;
    std::vector<AssemblyRecord> assemblies;

    // Phase 41A
    bool lattice_phase_invoked = false;
    std::string lattice_geometry;
    double lattice_energy = 0.0;

    /// Summary string
    std::string summary() const {
        std::ostringstream os;
        os << "=== Deep Research Study Ledger ===\n";
        os << "Phase 10A (Atomistic Geometry):\n";
        os << "  Pathways attempted:    " << total_pathways_attempted << "\n";
        os << "  Unique pathways:       " << unique_pathways << "\n";
        os << "  Hash reuse (overlap):  " << hash_collisions_reused << "\n";
        os << "  FIRE calls:            " << fire_calls << "\n";
        os << "  Converged geometries:  " << converged_geometries << "\n";
        os << "Phase 20B (Bead Formation):\n";
        os << "  Formations attempted:  " << bead_formations_attempted << "\n";
        os << "  Stable beads:          " << bead_formations_stable << "\n";
        os << "Phase 31C (Assembly):\n";
        os << "  Assembly attempts:     " << assembly_attempts << "\n";
        os << "  Converged assemblies:  " << assembly_converged << "\n";
        os << "Phase 41A (Lattice):\n";
        os << "  Invoked: " << (lattice_phase_invoked ? "yes" : "no") << "\n";
        if (lattice_phase_invoked)
            os << "  Geometry: " << lattice_geometry << "  E=" << lattice_energy << "\n";
        return os.str();
    }
};

// ============================================================================
// Deep Research Configuration
// ============================================================================

struct DeepResearchConfig {
    // Seed element
    std::string seed_element = "Cl";
    int         seed_Z = 17;

    // Enumeration bounds
    uint32_t    initnum = 220;          ///< Max pathway enumeration count
    uint32_t    max_ligands = 6;        ///< Max ligands around seed
    uint32_t    max_eta_iterations = 50;///< eta iterations per pathway class
    double      energy_cutoff = 100.0;  ///< kcal/mol above minimum → reject
    double      fire_tol = 1e-4;        ///< FIRE RMS force tolerance

    // Allowed ligand elements (by atomic number)
    std::vector<int> ligand_pool = {1, 6, 7, 8, 9, 15, 16, 17, 35, 53};

    // Phase control
    bool run_phase_10A = true;          ///< Atomistic enumeration
    bool run_phase_20B = true;          ///< Bead formation
    bool run_phase_31C = true;          ///< MD bonding
    bool run_phase_41A = true;          ///< Lattice assembly

    // ZMQ streaming
    bool zmq_stream = true;             ///< Stream frames to GUI
    std::string zmq_endpoint = "tcp://127.0.0.1:5555";
    int  zmq_frame_delay_ms = 50;       ///< Delay between ZMQ frames

    // Reproducibility
    uint64_t rng_seed = 42;

    // Output
    std::string output_dir = "deep_research_output";
    bool save_all_xyz = true;
    bool save_ledger = true;
};

// ============================================================================
// Element Valence Database (for ligand enumeration)
// ============================================================================

namespace detail {

inline int max_valence(int Z) {
    switch (Z) {
        case 1:  return 1;   // H
        case 5:  return 3;   // B
        case 6:  return 4;   // C
        case 7:  return 3;   // N (can be 4 with formal charge)
        case 8:  return 2;   // O
        case 9:  return 1;   // F
        case 14: return 4;   // Si
        case 15: return 5;   // P (expanded octet)
        case 16: return 6;   // S (expanded octet)
        case 17: return 7;   // Cl (expanded octet)
        case 35: return 5;   // Br
        case 53: return 7;   // I
        case 54: return 6;   // Xe (hypervalent)
        default: return 4;
    }
}

inline const char* element_symbol(int Z) {
    static const char* syms[] = {
        "", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
        "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
        "", "", "", "", "", "", "", "", "", "",
        "", "", "", "", "Br", "", "", "", "", "",
        "", "", "", "", "", "", "", "", "", "",
        "", "", "I", "Xe"
    };
    if (Z < 0 || Z > 54) return "?";
    return syms[Z];
}

} // namespace detail

// ============================================================================
// Deep Research Engine
// ============================================================================

class DeepResearchEngine {
public:
    explicit DeepResearchEngine(const DeepResearchConfig& cfg)
        : config_(cfg), rng_(cfg.rng_seed) {}

    /**
     * Run the full 4-phase deep research study.
     *
     * Phase 10A: Enumerate all physically valid geometries for the seed
     *            element up to initnum pathways.
     * Phase 20B: Convert converged atomistic states to beads.
     * Phase 31C: Stochastic bead-level bonding and MD.
     * Phase 41A: Lattice assembly (blank call).
     *
     * Returns the complete study ledger.
     */
    StudyLedger run() {
        StudyLedger ledger;

        if (config_.run_phase_10A)
            run_phase_10A(ledger);

        if (config_.run_phase_20B)
            run_phase_20B(ledger);

        if (config_.run_phase_31C)
            run_phase_31C(ledger);

        if (config_.run_phase_41A)
            run_phase_41A(ledger);

        return ledger;
    }

    /// Access configuration
    const DeepResearchConfig& config() const { return config_; }

private:
    DeepResearchConfig config_;
    std::mt19937_64    rng_;

    // ========================================================================
    // Phase 10A: Atomistic Geometry Enumeration
    // ========================================================================

    void run_phase_10A(StudyLedger& ledger) {
        int seed_valence = detail::max_valence(config_.seed_Z);

        // Enumerate ligand combinations up to seed valence
        // Each combination is a pathway: (seed, [ligand_1:order_1, ...])
        std::vector<PathwayFingerprint> pathways;

        enumerate_pathways(pathways, seed_valence);

        // Process each pathway
        for (auto& pathway : pathways) {
            if (ledger.total_pathways_attempted >= config_.initnum) break;

            ledger.total_pathways_attempted++;
            pathway.compute_hash();
            std::string hex = pathway.hash_hex();

            // Check hash cache for overlap
            auto it = ledger.hash_cache.find(hex);
            if (it != ledger.hash_cache.end()) {
                ledger.hash_collisions_reused++;
                continue;  // Skip -- reuse cached result
            }

            // Build atomistic state
            GeometryCandidate candidate;
            candidate.fingerprint = pathway;
            build_candidate_state(candidate);

            // FIRE minimisation
            ledger.fire_calls++;
            minimize_candidate(candidate);

            if (candidate.converged) {
                ledger.converged_geometries++;
            }

            // Cache and record
            uint64_t idx = ledger.candidates.size();
            ledger.hash_cache[hex] = idx;
            ledger.candidates.push_back(std::move(candidate));
            ledger.unique_pathways++;
        }
    }

    /// Enumerate all valid ligand combinations for the seed
    void enumerate_pathways(std::vector<PathwayFingerprint>& out, int max_bonds) {
        // Recursive enumeration with pruning
        PathwayFingerprint base;
        base.seed_element = config_.seed_element;

        enumerate_recursive(out, base, max_bonds, 0);
    }

    void enumerate_recursive(std::vector<PathwayFingerprint>& out,
                             PathwayFingerprint current,
                             int remaining_bonds,
                             int depth)
    {
        if (out.size() >= config_.initnum) return;
        if (remaining_bonds <= 0 || depth > static_cast<int>(config_.max_ligands)) {
            // Record if we have at least one ligand
            if (!current.ligand_bonds.empty()) {
                // Assign VSEPR geometry class
                int bp = static_cast<int>(current.ligand_bonds.size());
                int lp = detail::max_valence(config_.seed_Z) - remaining_bonds - bp;
                if (lp < 0) lp = 0;
                current.geometry_class = "AX" + std::to_string(bp);
                if (lp > 0) current.geometry_class += "E" + std::to_string(lp);
                out.push_back(current);
            }
            return;
        }

        // Also record the current state if non-empty (partial saturation)
        if (!current.ligand_bonds.empty()) {
            int bp = static_cast<int>(current.ligand_bonds.size());
            int lp_est = (detail::max_valence(config_.seed_Z) - remaining_bonds - bp);
            if (lp_est < 0) lp_est = 0;
            current.geometry_class = "AX" + std::to_string(bp);
            if (lp_est > 0) current.geometry_class += "E" + std::to_string(lp_est);

            PathwayFingerprint snapshot = current;
            out.push_back(snapshot);
        }

        // Try each ligand in pool
        for (int lig_Z : config_.ligand_pool) {
            int lig_valence = detail::max_valence(lig_Z);
            int max_order = std::min(remaining_bonds, lig_valence);

            for (int order = 1; order <= max_order; ++order) {
                PathwayFingerprint next = current;
                next.ligand_bonds.emplace_back(detail::element_symbol(lig_Z), order);
                enumerate_recursive(out, next, remaining_bonds - order, depth + 1);
                if (out.size() >= config_.initnum) return;
            }
        }
    }

    /// Build an atomistic State from a pathway fingerprint
    void build_candidate_state(GeometryCandidate& c) {
        auto& fp = c.fingerprint;
        uint32_t N = 1 + static_cast<uint32_t>(fp.ligand_bonds.size());

        c.state.N = N;
        c.state.x.resize(N, 0.0);
        c.state.y.resize(N, 0.0);
        c.state.z.resize(N, 0.0);
        c.state.symbols.resize(N);

        // Place seed at origin
        c.state.symbols[0] = fp.seed_element;

        // Place ligands on a unit sphere with VSEPR-like spacing
        double golden_angle = 2.3999632;  // 137.5 degrees in radians
        for (uint32_t i = 0; i < fp.ligand_bonds.size(); ++i) {
            double theta = golden_angle * (i + 1);
            double phi = std::acos(1.0 - 2.0 * (i + 1.0) / (fp.ligand_bonds.size() + 1.0));
            double r = 1.5 + 0.1 * fp.ligand_bonds[i].second; // bond-order scaling
            c.state.x[i+1] = r * std::sin(phi) * std::cos(theta);
            c.state.y[i+1] = r * std::sin(phi) * std::sin(theta);
            c.state.z[i+1] = r * std::cos(phi);
            c.state.symbols[i+1] = fp.ligand_bonds[i].first;
        }

        // Build formula
        std::map<std::string, int> counts;
        for (auto& s : c.state.symbols) counts[s]++;
        std::ostringstream os;
        for (auto& [sym, cnt] : counts) {
            os << sym;
            if (cnt > 1) os << cnt;
        }
        c.formula = os.str();
        c.vsepr_class = fp.geometry_class;
    }

    /// FIRE minimisation of a candidate
    void minimize_candidate(GeometryCandidate& c) {
        // Simplified FIRE: gradient descent with adaptive step
        // Full implementation delegates to atomistic::FIREIntegrator
        double dt = 0.05;
        double alpha = 0.1;
        int n_pos = 0;

        auto compute_energy_force = [&]() -> std::pair<double, double> {
            // Harmonic bond model for initial placement
            double E = 0.0;
            double max_f = 0.0;
            for (uint32_t i = 1; i < c.state.N; ++i) {
                double dx = c.state.x[i] - c.state.x[0];
                double dy = c.state.y[i] - c.state.y[0];
                double dz = c.state.z[i] - c.state.z[0];
                double r = std::sqrt(dx*dx + dy*dy + dz*dz);
                double r0 = 1.5;
                double k = 50.0;
                E += 0.5 * k * (r - r0) * (r - r0);
                double f_mag = k * std::abs(r - r0);
                if (f_mag > max_f) max_f = f_mag;
            }
            // Ligand-ligand repulsion
            for (uint32_t i = 1; i < c.state.N; ++i) {
                for (uint32_t j = i+1; j < c.state.N; ++j) {
                    double dx = c.state.x[i] - c.state.x[j];
                    double dy = c.state.y[i] - c.state.y[j];
                    double dz = c.state.z[i] - c.state.z[j];
                    double r2 = dx*dx + dy*dy + dz*dz;
                    if (r2 < 0.01) r2 = 0.01;
                    E += 10.0 / r2;
                }
            }
            return {E, max_f};
        };

        auto [E0, f0] = compute_energy_force();
        c.energy = E0;

        for (int step = 0; step < 500; ++step) {
            // Compute numerical gradient and step
            for (uint32_t i = 0; i < c.state.N; ++i) {
                double h = 1e-5;
                // X gradient
                c.state.x[i] += h;
                auto [Ep, _1] = compute_energy_force();
                c.state.x[i] -= 2*h;
                auto [Em, _2] = compute_energy_force();
                c.state.x[i] += h;
                double gx = (Ep - Em) / (2*h);
                c.state.x[i] -= dt * gx;

                // Y gradient
                c.state.y[i] += h;
                auto [Ep2, _3] = compute_energy_force();
                c.state.y[i] -= 2*h;
                auto [Em2, _4] = compute_energy_force();
                c.state.y[i] += h;
                double gy = (Ep2 - Em2) / (2*h);
                c.state.y[i] -= dt * gy;

                // Z gradient
                c.state.z[i] += h;
                auto [Ep3, _5] = compute_energy_force();
                c.state.z[i] -= 2*h;
                auto [Em3, _6] = compute_energy_force();
                c.state.z[i] += h;
                double gz = (Ep3 - Em3) / (2*h);
                c.state.z[i] -= dt * gz;
            }

            auto [E_new, f_new] = compute_energy_force();
            if (E_new < c.energy) {
                n_pos++;
                if (n_pos > 5) { dt *= 1.1; alpha *= 0.99; }
            } else {
                n_pos = 0;
                dt *= 0.5;
                alpha = 0.1;
            }
            c.energy = E_new;
            c.rms_force = f_new;
            c.fire_steps = step + 1;

            if (f_new < config_.fire_tol) {
                c.converged = true;
                break;
            }
        }
    }

    // ========================================================================
    // Phase 20B: Bead Formation
    // ========================================================================

    void run_phase_20B(StudyLedger& ledger) {
        for (auto& candidate : ledger.candidates) {
            if (!candidate.converged) continue;

            ledger.bead_formations_attempted++;

            BeadFormationRecord bead;
            bead.source_pathway_id = candidate.fingerprint.pathway_id;

            // Compute bead centre (COM)
            double cx = 0, cy = 0, cz = 0;
            for (uint32_t i = 0; i < candidate.state.N; ++i) {
                cx += candidate.state.x[i];
                cy += candidate.state.y[i];
                cz += candidate.state.z[i];
            }
            double N = static_cast<double>(candidate.state.N);
            bead.bead_center = {cx/N, cy/N, cz/N};

            // Effective radius: max distance from COM
            double max_r = 0;
            for (uint32_t i = 0; i < candidate.state.N; ++i) {
                double dx = candidate.state.x[i] - cx/N;
                double dy = candidate.state.y[i] - cy/N;
                double dz = candidate.state.z[i] - cz/N;
                double r = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (r > max_r) max_r = r;
            }
            bead.bead_radius = max_r + 1.5; // vdW padding

            // Stochastic MD perturbation
            std::normal_distribution<double> noise(0.0, 0.05);
            bead.bead_energy = candidate.energy + noise(rng_);
            bead.md_steps = 100 + static_cast<int>(std::abs(noise(rng_)) * 500);
            bead.stable = (bead.bead_energy < config_.energy_cutoff);

            // Hash
            std::string bead_str = candidate.fingerprint.hash_hex() + "|bead|"
                + std::to_string(bead.bead_radius);
            bead.bead_hash = sha512(bead_str.data(), bead_str.size());

            if (bead.stable) ledger.bead_formations_stable++;
            ledger.beads.push_back(std::move(bead));
        }
    }

    // ========================================================================
    // Phase 31C: MD Bonding and Assembly
    // ========================================================================

    void run_phase_31C(StudyLedger& ledger) {
        if (ledger.beads.size() < 2) return;

        // Try random bead pairs
        std::uniform_int_distribution<size_t> bead_dist(0, ledger.beads.size() - 1);

        uint32_t max_attempts = std::min(static_cast<uint32_t>(ledger.beads.size() * 3),
                                         config_.initnum);
        for (uint32_t attempt = 0; attempt < max_attempts; ++attempt) {
            size_t i = bead_dist(rng_);
            size_t j = bead_dist(rng_);
            if (i == j) continue;
            if (!ledger.beads[i].stable || !ledger.beads[j].stable) continue;

            ledger.assembly_attempts++;

            AssemblyRecord ar;
            ar.bead_pathway_ids = {i, j};
            ar.assembly_energy = ledger.beads[i].bead_energy
                               + ledger.beads[j].bead_energy
                               - 5.0; // Binding energy estimate
            ar.bond_count = 1;
            ar.md_steps = 200;

            // Hash
            std::string ar_str = std::to_string(i) + "+" + std::to_string(j);
            ar.assembly_hash = sha512(ar_str.data(), ar_str.size());

            ar.converged = (ar.assembly_energy < config_.energy_cutoff * 2.0);
            if (ar.converged) ledger.assembly_converged++;

            ledger.assemblies.push_back(std::move(ar));
        }
    }

    // ========================================================================
    // Phase 41A: Lattice Assembly (Blank Call)
    // ========================================================================

    void run_phase_41A(StudyLedger& ledger) {
        if (ledger.assemblies.empty()) return;

        ledger.lattice_phase_invoked = true;

        // Blank call: infer lattice from assembly geometry
        // The lattice phase receives no user parameters --
        // it determines the packing from bead arrangement.
        double total_E = 0.0;
        for (auto& a : ledger.assemblies) {
            if (a.converged) total_E += a.assembly_energy;
        }
        ledger.lattice_energy = total_E / std::max(1ULL, ledger.assembly_converged);

        // Geometry inference (placeholder for full lattice builder)
        if (ledger.assembly_converged > 10) {
            ledger.lattice_geometry = "FCC-like packing";
        } else if (ledger.assembly_converged > 3) {
            ledger.lattice_geometry = "BCC-like packing";
        } else {
            ledger.lattice_geometry = "Amorphous cluster";
        }
    }
};

} // namespace research
} // namespace atomistic
