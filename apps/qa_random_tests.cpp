/**
 * qa_random_tests.cpp
 * --------------------
 * Expanded QA Random Molecular Test Suite
 *
 * Generates 5–101 random molecular structures, relaxes them with the
 * composite model (bonded + LJ nonbonded with 1-2/1-3 exclusions),
 * and produces three classes of output:
 *
 *   3/3 (always)  → .xyz file with cartoon interpretation
 *   2/3           → connectivity / energy graph (DOT format)
 *   ~1/3          → protein-like chain structures (helix / sheet / loop)
 *
 * GPU detection runs first and is reported in the manifest.
 *
 * Usage:
 *   qa_random_tests [--count N] [--seed S] [--output dir] [--portable|--strict]
 *
 * NO VIBES. Only facts.
 */

#include "atomistic/core/linalg.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/models/bonded.hpp"
#include "atomistic/models/composite.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "src/io/xyz_format.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <random>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <functional>
#include <array>
#include <cstring>

using namespace atomistic;
namespace fs = std::filesystem;

// ============================================================================
// ELEMENT DATABASE (inline, self-contained)
// ============================================================================

struct ElementInfo {
    int Z;
    const char* symbol;
    double mass;
    double covalent_radius;   // Å
    int max_bonds;            // typical max coordination
};

static const ElementInfo ELEMENTS[] = {
    { 1, "H",  1.008, 0.31, 1},
    { 5, "B", 10.81,  0.84, 3},
    { 6, "C", 12.01,  0.77, 4},
    { 7, "N", 14.01,  0.75, 3},
    { 8, "O", 16.00,  0.73, 2},
    { 9, "F", 19.00,  0.72, 1},
    {14, "Si",28.09,  1.17, 4},
    {15, "P", 30.97,  1.10, 5},
    {16, "S", 32.06,  1.04, 6},
    {17, "Cl",35.45,  0.99, 1},
    {26, "Fe",55.85,  1.25, 6},
    {29, "Cu",63.55,  1.32, 4},
    {30, "Zn",65.38,  1.22, 4},
    {35, "Br",79.90,  1.14, 1},
    {53, "I",126.90,  1.33, 1},
};
static constexpr int N_ELEMENTS = sizeof(ELEMENTS) / sizeof(ELEMENTS[0]);

static const ElementInfo& element_by_Z(int Z) {
    for (int i = 0; i < N_ELEMENTS; ++i)
        if (ELEMENTS[i].Z == Z) return ELEMENTS[i];
    static const ElementInfo fallback{6, "C", 12.01, 0.77, 4};
    return fallback;
}

static const char* symbol_for_Z(int Z) {
    static const char* table[] = {
        "X","H","He","Li","Be","B","C","N","O","F","Ne",
        "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
        "Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
        "Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr",
        "Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
        "Sb","Te","I","Xe"
    };
    if (Z >= 0 && Z <= 54) return table[Z];
    return "X";
}

// ============================================================================
// CHARGE POLICY
// ============================================================================

static double get_charge(int Z) {
    switch (Z) {
        case 11: return +1.0;  // Na
        case 12: return +2.0;  // Mg
        case 17: return -1.0;  // Cl
        case 20: return +2.0;  // Ca
        case 55: return +1.0;  // Cs
        default: return 0.0;
    }
}

// ============================================================================
// STATE ADAPTER
// ============================================================================

struct CoreState {
    std::vector<int> atomic_numbers;
    std::vector<Vec3> positions;
    std::vector<atomistic::Edge> bonds;
    bool pbc_enabled = false;
    Vec3 box_lengths = {0, 0, 0};
};

inline atomistic::State to_atomistic_state(const CoreState& core) {
    atomistic::State s;
    s.N = (uint32_t)core.positions.size();
    s.X = core.positions;
    s.V.resize(s.N, {0, 0, 0});
    s.F.resize(s.N, {0, 0, 0});
    s.Q.resize(s.N, 0.0);
    s.M.resize(s.N, 1.0);
    s.type.resize(s.N, 1);
    for (uint32_t i = 0; i < s.N; ++i) {
        s.type[i] = (uint32_t)core.atomic_numbers[i];
        s.Q[i] = get_charge(core.atomic_numbers[i]);
        s.M[i] = element_by_Z(core.atomic_numbers[i]).mass;
    }
    s.B = core.bonds;
    if (core.pbc_enabled)
        s.box = BoxPBC(core.box_lengths.x, core.box_lengths.y, core.box_lengths.z);
    return s;
}

inline void sync_from_atomistic(CoreState& core, const atomistic::State& s) {
    core.positions = s.X;
}

// ============================================================================
// MODEL WRAPPER (composite: bonded + LJ with 1-2/1-3 exclusions)
// ============================================================================

class CompositeModelWrapper {
    std::unique_ptr<IModel> impl_;
    std::unique_ptr<IModel> composite_;
    ModelParams params_;
    bool built_ = false;

    IModel* select(atomistic::State& s) {
        if (!s.B.empty()) {
            if (!built_) {
                composite_ = create_composite_model(s);
                built_ = true;
            }
            return composite_.get();
        }
        return impl_.get();
    }
public:
    CompositeModelWrapper() {
        impl_ = create_lj_coulomb_model();
        params_.rc = 10.0;
        params_.k_coul = 138.935;
        params_.sigma = 0.0;
        params_.eps = 0.0;
    }
    void reset() { composite_.reset(); built_ = false; }

    double energy(CoreState& cs) {
        auto s = to_atomistic_state(cs);
        select(s)->eval(s, params_);
        sync_from_atomistic(cs, s);
        return s.E.total();
    }
    std::vector<Vec3> forces(CoreState& cs) {
        auto s = to_atomistic_state(cs);
        select(s)->eval(s, params_);
        sync_from_atomistic(cs, s);
        return s.F;
    }
    double energy(atomistic::State& s) {
        select(s)->eval(s, params_);
        return s.E.total();
    }
    std::vector<Vec3> forces(atomistic::State& s) {
        select(s)->eval(s, params_);
        return s.F;
    }
};

// ============================================================================
// FIRE MINIMIZER
// ============================================================================

struct FIREResult {
    bool converged = false;
    int iterations = 0;
    double final_max_force = 0.0;
    double final_energy = 0.0;
    std::vector<double> energy_trace;
};

template<typename Model>
FIREResult fire_minimize(CoreState& cs, Model& model,
                         int max_steps = 2000, double f_tol = 1e-3)
{
    atomistic::State state = to_atomistic_state(cs);
    FIREResult res;

    if (state.X.empty()) return res;

    const size_t N = state.X.size();
    std::vector<Vec3> vel(N, {0,0,0});

    // Adaptive timestep with conservative initial dt and max-displacement limiter
    double dt = 0.001;        // very small initial dt for safety
    double dt_max = 0.1;
    double alpha = 0.25;
    const double alpha_start = 0.25;
    const double max_disp = 0.2;  // max displacement per atom per step (Å)
    int n_pos = 0;
    double prev_fmax = 1e30;
    int div_count = 0;

    for (int step = 0; step < max_steps; ++step) {
        auto F = model.forces(state);
        double E = model.energy(state);
        res.energy_trace.push_back(E);

        double fmax = 0;
        for (auto& f : F) {
            double m = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
            fmax = std::max(fmax, m);
        }
        res.final_max_force = fmax;
        res.final_energy = E;

        if (!std::isfinite(E) || !std::isfinite(fmax)) {
            res.iterations = step;
            sync_from_atomistic(cs, state);
            return res;
        }

        if (fmax > prev_fmax * 5.0) {
            if (++div_count > 20) { res.iterations = step; sync_from_atomistic(cs, state); return res; }
            // Hard reset on divergence
            dt = 0.001;
            alpha = alpha_start;
            n_pos = 0;
            for (auto& v : vel) v = {0,0,0};
        } else div_count = 0;
        prev_fmax = fmax;

        if (fmax < f_tol) {
            res.converged = true;
            res.iterations = step;
            sync_from_atomistic(cs, state);
            return res;
        }

        double P = 0, vnorm = 0, fnorm = 0;
        for (size_t i = 0; i < N; ++i) {
            P += vel[i].x*F[i].x + vel[i].y*F[i].y + vel[i].z*F[i].z;
            vnorm += vel[i].x*vel[i].x + vel[i].y*vel[i].y + vel[i].z*vel[i].z;
            fnorm += F[i].x*F[i].x + F[i].y*F[i].y + F[i].z*F[i].z;
        }
        vnorm = std::sqrt(vnorm);
        fnorm = std::sqrt(fnorm);

        if (P > 0) {
            if (++n_pos > 5) { dt = std::min(dt * 1.1, dt_max); alpha *= 0.99; }
            double mix = alpha * vnorm / (fnorm + 1e-10);
            for (size_t i = 0; i < N; ++i) {
                vel[i].x = (1-alpha)*vel[i].x + mix*F[i].x;
                vel[i].y = (1-alpha)*vel[i].y + mix*F[i].y;
                vel[i].z = (1-alpha)*vel[i].z + mix*F[i].z;
            }
        } else {
            n_pos = 0; dt = 0.001; alpha = alpha_start;
            for (auto& v : vel) v = {0,0,0};
        }

        // Velocity Verlet with max-displacement limiter
        for (size_t i = 0; i < N; ++i) {
            vel[i].x += dt * F[i].x;
            vel[i].y += dt * F[i].y;
            vel[i].z += dt * F[i].z;

            double dx = dt * vel[i].x;
            double dy = dt * vel[i].y;
            double dz = dt * vel[i].z;
            double disp = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (disp > max_disp) {
                double scale = max_disp / disp;
                dx *= scale; dy *= scale; dz *= scale;
                vel[i].x *= scale; vel[i].y *= scale; vel[i].z *= scale;
            }
            state.X[i].x += dx;
            state.X[i].y += dy;
            state.X[i].z += dz;
        }
    }

    res.iterations = max_steps;
    sync_from_atomistic(cs, state);
    return res;
}

// ============================================================================
// GPU DETECTION
// ============================================================================

struct GPUInfo {
    bool available = false;
    std::string name = "N/A";
    std::string driver = "N/A";
    int vram_mb = 0;
    int compute_units = 0;
};

#ifdef _WIN32
#include <windows.h>
#endif

static GPUInfo detect_gpu() {
    GPUInfo info;

#ifdef _WIN32
    // Attempt nvidia-smi detection
    FILE* pipe = _popen("nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader,nounits 2>nul", "r");
    if (pipe) {
        char buf[512] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            info.available = true;
            // Parse: "NVIDIA GeForce RTX 3090, 528.49, 24576"
            std::string line(buf);
            // Trim trailing newline
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            size_t p1 = line.find(',');
            if (p1 != std::string::npos) {
                info.name = line.substr(0, p1);
                size_t p2 = line.find(',', p1+1);
                if (p2 != std::string::npos) {
                    info.driver = line.substr(p1+2, p2-p1-2);
                    try { info.vram_mb = std::stoi(line.substr(p2+2)); } catch(...) {}
                }
            }
        }
        _pclose(pipe);
    }

    // Fallback: try AMD via rocm-smi
    if (!info.available) {
        pipe = _popen("rocm-smi --showproductname 2>nul", "r");
        if (pipe) {
            char buf[512] = {};
            if (fgets(buf, sizeof(buf), pipe)) {
                std::string line(buf);
                if (line.find("GPU") != std::string::npos || line.find("Radeon") != std::string::npos) {
                    info.available = true;
                    info.name = line;
                    while (!info.name.empty() && (info.name.back() == '\n' || info.name.back() == '\r'))
                        info.name.pop_back();
                }
            }
            _pclose(pipe);
        }
    }
#else
    // Linux/macOS: nvidia-smi
    FILE* pipe = popen("nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader,nounits 2>/dev/null", "r");
    if (pipe) {
        char buf[512] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            info.available = true;
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            size_t p1 = line.find(',');
            if (p1 != std::string::npos) {
                info.name = line.substr(0, p1);
                size_t p2 = line.find(',', p1+1);
                if (p2 != std::string::npos) {
                    info.driver = line.substr(p1+2, p2-p1-2);
                    try { info.vram_mb = std::stoi(line.substr(p2+2)); } catch(...) {}
                }
            }
        }
        pclose(pipe);
    }
#endif

    return info;
}

// ============================================================================
// OUTPUT MODE
// ============================================================================

enum class OutputMode {
    XYZ_CARTOON,     // 3/3 — always: .xyz file
    GRAPH,           // 2/3 — connectivity + energy graph (DOT)
    PROTEIN          // ~1/3 — protein-like chain output
};

// ============================================================================
// STRUCTURE GENERATORS
// ============================================================================

// --- Hub-and-spoke (central atom + ligands) ---
struct HubSpoke {
    int center_Z;
    int ligand_Z;
    int n_ligands;
    double bond_length;
};

static CoreState generate_hub_spoke(const HubSpoke& hs, std::mt19937_64& rng) {
    CoreState cs;
    cs.atomic_numbers.push_back(hs.center_Z);
    cs.positions.push_back({0, 0, 0});

    // Distribute ligands on a sphere with slight random perturbation
    std::uniform_real_distribution<double> noise(-0.05, 0.05);
    for (int i = 0; i < hs.n_ligands; ++i) {
        double theta, phi;
        if (hs.n_ligands == 1) {
            theta = 0; phi = 0;
        } else if (hs.n_ligands == 2) {
            theta = (i == 0) ? 0.0 : M_PI;
            phi = 0;
        } else if (hs.n_ligands == 3) {
            theta = M_PI / 2.0;
            phi = i * 2.0 * M_PI / 3.0;
        } else if (hs.n_ligands == 4) {
            // Tetrahedral
            static const double tet[][3] = {
                { 1, 1, 1}, { 1,-1,-1}, {-1, 1,-1}, {-1,-1, 1}
            };
            double r = hs.bond_length;
            double n3 = 1.0 / std::sqrt(3.0);
            cs.atomic_numbers.push_back(hs.ligand_Z);
            cs.positions.push_back({tet[i][0]*n3*r + noise(rng),
                                     tet[i][1]*n3*r + noise(rng),
                                     tet[i][2]*n3*r + noise(rng)});
            cs.bonds.push_back({0, (uint32_t)(i+1)});
            continue;
        } else if (hs.n_ligands == 5) {
            // Trigonal bipyramidal
            if (i < 3) { theta = M_PI/2.0; phi = i * 2.0*M_PI/3.0; }
            else { theta = (i == 3) ? 0.0 : M_PI; phi = 0; }
        } else { // 6 = octahedral
            static const double oct[][3] = {
                {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
            };
            double r = hs.bond_length;
            cs.atomic_numbers.push_back(hs.ligand_Z);
            cs.positions.push_back({oct[i%6][0]*r + noise(rng),
                                     oct[i%6][1]*r + noise(rng),
                                     oct[i%6][2]*r + noise(rng)});
            cs.bonds.push_back({0, (uint32_t)(i+1)});
            continue;
        }

        double r = hs.bond_length;
        double x = r * std::sin(theta) * std::cos(phi) + noise(rng);
        double y = r * std::sin(theta) * std::sin(phi) + noise(rng);
        double z = r * std::cos(theta) + noise(rng);
        cs.atomic_numbers.push_back(hs.ligand_Z);
        cs.positions.push_back({x, y, z});
        cs.bonds.push_back({0, (uint32_t)(i+1)});
    }
    return cs;
}

// --- Linear chain (–A–B–A–B–) ---
static CoreState generate_chain(int length, int Z_a, int Z_b,
                                 double bond_len, std::mt19937_64& rng)
{
    CoreState cs;
    std::uniform_real_distribution<double> angle_noise(-0.15, 0.15);
    double theta = 109.5 * M_PI / 180.0; // tetrahedral-ish angle

    for (int i = 0; i < length; ++i) {
        int Z = (i % 2 == 0) ? Z_a : Z_b;
        cs.atomic_numbers.push_back(Z);

        if (i == 0) {
            cs.positions.push_back({0, 0, 0});
        } else {
            // Build along a zig-zag backbone with noise
            double a = theta + angle_noise(rng);
            double phi = (i % 2 == 0) ? 0.0 : M_PI;
            Vec3 prev = cs.positions.back();
            double dx = bond_len * std::sin(a) * std::cos(phi + i * 0.3);
            double dy = bond_len * std::sin(a) * std::sin(phi + i * 0.3);
            double dz = bond_len * std::cos(a);
            cs.positions.push_back({prev.x + dx, prev.y + dy, prev.z + dz});
            cs.bonds.push_back({(uint32_t)(i-1), (uint32_t)i});
        }
    }
    return cs;
}

// --- Protein-like backbone (N-Cα-C-N-Cα-C...) with helix/sheet ---

enum class SecondaryStructure { HELIX, SHEET, LOOP };

static CoreState generate_protein_backbone(int n_residues, SecondaryStructure ss,
                                            std::mt19937_64& rng)
{
    CoreState cs;
    std::uniform_real_distribution<double> noise(-0.02, 0.02);

    // Backbone dihedral angles (Ramachandran)
    double phi_angle, psi_angle;
    switch (ss) {
        case SecondaryStructure::HELIX:
            phi_angle = -57.0 * M_PI / 180.0;  // α-helix
            psi_angle = -47.0 * M_PI / 180.0;
            break;
        case SecondaryStructure::SHEET:
            phi_angle = -120.0 * M_PI / 180.0; // β-sheet
            psi_angle =  130.0 * M_PI / 180.0;
            break;
        case SecondaryStructure::LOOP:
        default:
            phi_angle = -65.0 * M_PI / 180.0;
            psi_angle = 150.0 * M_PI / 180.0;
            break;
    }

    // Bond lengths (Å)
    const double r_N_CA  = 1.47;
    const double r_CA_C  = 1.53;
    const double r_C_N   = 1.33;
    const double theta_bb = 111.0 * M_PI / 180.0; // backbone angle

    // Build backbone as N–Cα–C repeats
    Vec3 pos = {0, 0, 0};
    Vec3 dir = {1, 0, 0};
    Vec3 perp = {0, 1, 0};

    for (int res = 0; res < n_residues; ++res) {
        double lengths[] = {r_N_CA, r_CA_C, r_C_N};
        int atom_Z[] = {7, 6, 6};  // N, Cα (C), C'
        double dihedrals[] = {phi_angle, psi_angle, 0.0}; // ω = 0 (trans)

        for (int a = 0; a < 3; ++a) {
            cs.atomic_numbers.push_back(atom_Z[a]);
            cs.positions.push_back(pos);
            int idx = (int)cs.positions.size() - 1;

            if (idx > 0) {
                cs.bonds.push_back({(uint32_t)(idx-1), (uint32_t)idx});
            }

            // Advance position along backbone with rotation
            double r = lengths[a];
            double dih = dihedrals[a] + noise(rng);

            // Simple rotation model: rotate dir around perp, then twist
            double ct = std::cos(theta_bb), st = std::sin(theta_bb);
            Vec3 new_dir = {
                dir.x * ct + perp.x * st * std::cos(dih) + noise(rng),
                dir.y * ct + perp.y * st * std::sin(dih) + noise(rng),
                dir.z * ct + st * std::cos(dih + res * 0.5) + noise(rng)
            };
            double nd = std::sqrt(new_dir.x*new_dir.x + new_dir.y*new_dir.y + new_dir.z*new_dir.z);
            if (nd > 1e-10) { new_dir.x /= nd; new_dir.y /= nd; new_dir.z /= nd; }

            pos.x += r * new_dir.x;
            pos.y += r * new_dir.y;
            pos.z += r * new_dir.z;

            // Update direction basis
            perp = {-dir.y * new_dir.z + dir.z * new_dir.y,
                     dir.x * new_dir.z - dir.z * new_dir.x,
                    -dir.x * new_dir.y + dir.y * new_dir.x};
            double pn = std::sqrt(perp.x*perp.x + perp.y*perp.y + perp.z*perp.z);
            if (pn > 1e-10) { perp.x /= pn; perp.y /= pn; perp.z /= pn; }
            else perp = {0, 1, 0};
            dir = new_dir;
        }
    }
    return cs;
}

// --- Ring structures (benzene-like, cyclopentane, etc.) ---
static CoreState generate_ring(int ring_size, int Z_ring, int Z_sub,
                                double bond_len, std::mt19937_64& rng)
{
    CoreState cs;
    std::uniform_real_distribution<double> noise(-0.03, 0.03);
    double angle_step = 2.0 * M_PI / ring_size;
    double radius = bond_len / (2.0 * std::sin(M_PI / ring_size));

    // Ring atoms
    for (int i = 0; i < ring_size; ++i) {
        double a = i * angle_step;
        cs.atomic_numbers.push_back(Z_ring);
        cs.positions.push_back({radius * std::cos(a) + noise(rng),
                                 radius * std::sin(a) + noise(rng),
                                 noise(rng)});
        cs.bonds.push_back({(uint32_t)i, (uint32_t)((i+1) % ring_size)});
    }

    // Substituent H on each ring atom (radially outward)
    for (int i = 0; i < ring_size; ++i) {
        double a = i * angle_step;
        double r_sub = radius + 1.08;
        int idx = (int)cs.positions.size();
        cs.atomic_numbers.push_back(Z_sub);
        cs.positions.push_back({r_sub * std::cos(a) + noise(rng),
                                 r_sub * std::sin(a) + noise(rng),
                                 noise(rng)});
        cs.bonds.push_back({(uint32_t)i, (uint32_t)idx});
    }
    return cs;
}

// ============================================================================
// RANDOM TEST GENERATOR
// ============================================================================

struct RandomTest {
    std::string name;
    std::string category;      // "molecule", "chain", "protein", "ring"
    CoreState state;
    uint64_t seed;
    std::vector<OutputMode> outputs;   // which outputs to produce
    SecondaryStructure ss = SecondaryStructure::LOOP;
};

static std::vector<RandomTest> generate_random_tests(int count, uint64_t master_seed)
{
    std::mt19937_64 rng(master_seed);
    std::vector<RandomTest> tests;
    tests.reserve(count);

    // Hub-spoke templates
    const HubSpoke hub_templates[] = {
        { 6, 1, 4, 1.09},  // CH4
        { 7, 1, 3, 1.01},  // NH3
        { 8, 1, 2, 0.96},  // H2O
        { 5, 9, 3, 1.31},  // BF3
        {14, 1, 4, 1.48},  // SiH4
        {15,17, 5, 2.04},  // PCl5
        {16, 9, 6, 1.56},  // SF6
        { 6,17, 4, 1.77},  // CCl4
        {26, 7, 6, 2.00},  // Fe(N)6 — octahedral complex
        {30, 8, 4, 1.95},  // Zn(O)4 — tetrahedral complex
    };
    const int n_hub = sizeof(hub_templates) / sizeof(hub_templates[0]);

    // Chain atom pairs
    const int chain_pairs[][2] = {
        {6, 6}, {6, 7}, {6, 8}, {14,8}, {15,8}
    };
    const int n_chain_pairs = 5;

    // Ring sizes
    const int ring_sizes[] = {3, 4, 5, 6, 7, 8};
    const int n_ring_sizes = 6;

    std::uniform_int_distribution<int> type_dist(0, 3);  // 0=hub, 1=chain, 2=ring, 3=protein
    std::uniform_int_distribution<int> hub_dist(0, n_hub - 1);
    std::uniform_int_distribution<int> chain_len_dist(3, 20);
    std::uniform_int_distribution<int> chain_pair_dist(0, n_chain_pairs - 1);
    std::uniform_int_distribution<int> ring_dist(0, n_ring_sizes - 1);
    std::uniform_int_distribution<int> prot_res_dist(3, 15);
    std::uniform_int_distribution<int> ss_dist(0, 2);
    std::uniform_real_distribution<double> output_roll(0.0, 1.0);

    for (int i = 0; i < count; ++i) {
        RandomTest t;
        t.seed = rng();
        std::mt19937_64 local_rng(t.seed);

        int type = type_dist(rng);

        // Force ~1/3 to be protein-like
        double prot_roll = output_roll(rng);
        if (prot_roll < 0.33) type = 3;

        switch (type) {
            case 0: { // hub-spoke
                int idx = hub_dist(rng);
                t.state = generate_hub_spoke(hub_templates[idx], local_rng);
                t.name = "Hub_" + std::string(symbol_for_Z(hub_templates[idx].center_Z))
                        + std::to_string(hub_templates[idx].n_ligands)
                        + "_" + std::to_string(i);
                t.category = "molecule";
                break;
            }
            case 1: { // chain
                int len = chain_len_dist(rng);
                int cp = chain_pair_dist(rng);
                double bl = element_by_Z(chain_pairs[cp][0]).covalent_radius
                          + element_by_Z(chain_pairs[cp][1]).covalent_radius;
                t.state = generate_chain(len, chain_pairs[cp][0], chain_pairs[cp][1],
                                          bl, local_rng);
                t.name = "Chain_" + std::string(symbol_for_Z(chain_pairs[cp][0]))
                        + std::string(symbol_for_Z(chain_pairs[cp][1]))
                        + "_n" + std::to_string(len) + "_" + std::to_string(i);
                t.category = "chain";
                break;
            }
            case 2: { // ring
                int rs = ring_sizes[ring_dist(rng)];
                t.state = generate_ring(rs, 6, 1, 1.40, local_rng);
                t.name = "Ring_C" + std::to_string(rs) + "H" + std::to_string(rs)
                        + "_" + std::to_string(i);
                t.category = "ring";
                break;
            }
            case 3: { // protein backbone
                int n_res = prot_res_dist(rng);
                SecondaryStructure ss = (SecondaryStructure)ss_dist(rng);
                t.state = generate_protein_backbone(n_res, ss, local_rng);
                t.ss = ss;
                const char* ss_names[] = {"helix", "sheet", "loop"};
                t.name = "Protein_" + std::string(ss_names[(int)ss])
                        + "_r" + std::to_string(n_res) + "_" + std::to_string(i);
                t.category = "protein";
                break;
            }
        }

        // Assign output modes
        // 3/3: always XYZ
        t.outputs.push_back(OutputMode::XYZ_CARTOON);

        // 2/3: graph
        if (output_roll(rng) < 0.667)
            t.outputs.push_back(OutputMode::GRAPH);

        // ~1/3: protein mode (already handled by type=3, but also output for any type)
        if (type == 3)
            t.outputs.push_back(OutputMode::PROTEIN);

        tests.push_back(std::move(t));
    }

    return tests;
}

// ============================================================================
// OUTPUT WRITERS
// ============================================================================

// --- XYZ with cartoon comment header ---
static void write_xyz_cartoon(const CoreState& cs, const std::string& name,
                               double energy, const std::string& path)
{
    std::ofstream ofs(path);
    ofs << cs.positions.size() << "\n";
    ofs << "# " << name << " | E=" << std::fixed << std::setprecision(4) << energy
        << " kcal/mol | bonds=" << cs.bonds.size()
        << " | cartoon=ball-and-stick\n";

    for (size_t i = 0; i < cs.positions.size(); ++i) {
        ofs << std::setw(3) << std::left << symbol_for_Z(cs.atomic_numbers[i])
            << " " << std::fixed << std::setprecision(6)
            << std::setw(12) << cs.positions[i].x
            << " " << std::setw(12) << cs.positions[i].y
            << " " << std::setw(12) << cs.positions[i].z
            << "\n";
    }
    ofs.close();
}

// --- DOT graph: connectivity + energy labels ---
static void write_graph_dot(const CoreState& cs, const std::string& name,
                             double energy, const std::string& path)
{
    std::ofstream ofs(path);
    ofs << "// " << name << " connectivity graph\n";
    ofs << "// Total energy: " << std::fixed << std::setprecision(4) << energy << " kcal/mol\n";
    ofs << "graph " << "G {\n";
    ofs << "  rankdir=LR;\n";
    ofs << "  node [shape=circle, style=filled];\n";

    // Color atoms by element
    for (size_t i = 0; i < cs.atomic_numbers.size(); ++i) {
        const char* color = "lightgray";
        switch (cs.atomic_numbers[i]) {
            case 1:  color = "white"; break;
            case 6:  color = "gray30"; break;
            case 7:  color = "royalblue"; break;
            case 8:  color = "red"; break;
            case 9:  color = "palegreen"; break;
            case 15: color = "orange"; break;
            case 16: color = "yellow"; break;
            case 17: color = "green"; break;
            case 26: color = "darkorange"; break;
        }
        ofs << "  " << i << " [label=\"" << symbol_for_Z(cs.atomic_numbers[i])
            << i << "\", fillcolor=\"" << color << "\"];\n";
    }

    // Bonds
    for (const auto& b : cs.bonds) {
        double r = 0;
        Vec3 d = {cs.positions[b.j].x - cs.positions[b.i].x,
                   cs.positions[b.j].y - cs.positions[b.i].y,
                   cs.positions[b.j].z - cs.positions[b.i].z};
        r = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
        ofs << "  " << b.i << " -- " << b.j
            << " [label=\"" << std::fixed << std::setprecision(2) << r << " A\"];\n";
    }

    // Energy annotation
    ofs << "  label=\"" << name << "\\nE=" << std::fixed << std::setprecision(4)
        << energy << " kcal/mol\";\n";
    ofs << "  labelloc=t;\n";
    ofs << "}\n";
    ofs.close();
}

// --- Protein annotation output (PDB-like summary) ---
static void write_protein_annotation(const CoreState& cs, const std::string& name,
                                      SecondaryStructure ss, double energy,
                                      const std::string& path)
{
    std::ofstream ofs(path);
    const char* ss_labels[] = {"HELIX", "SHEET", "LOOP"};
    int n_residues = (int)cs.positions.size() / 3; // N-Cα-C per residue

    ofs << "REMARK  VSEPR-SIM Protein-like Structure Report\n";
    ofs << "REMARK  Name: " << name << "\n";
    ofs << "REMARK  Energy: " << std::fixed << std::setprecision(4) << energy << " kcal/mol\n";
    ofs << "REMARK  Residues: " << n_residues << "\n";
    ofs << "REMARK  Secondary: " << ss_labels[(int)ss] << "\n";
    ofs << "REMARK  Atoms: " << cs.positions.size() << "\n";
    ofs << "REMARK  Bonds: " << cs.bonds.size() << "\n";
    ofs << "REMARK\n";

    // HELIX / SHEET records
    if (ss == SecondaryStructure::HELIX && n_residues >= 4) {
        ofs << "HELIX    1   H  ALA     1  ALA   "
            << std::setw(4) << n_residues << "  1\n";
    } else if (ss == SecondaryStructure::SHEET && n_residues >= 2) {
        ofs << "SHEET    1   S  ALA     1  ALA   "
            << std::setw(4) << n_residues << "  0\n";
    }

    // ATOM records
    for (size_t i = 0; i < cs.positions.size(); ++i) {
        int res_num = (int)(i / 3) + 1;
        const char* atom_names[] = {"N  ", "CA ", "C  "};
        const char* aname = atom_names[i % 3];

        ofs << "ATOM  " << std::setw(5) << (i+1) << " "
            << aname << " ALA A"
            << std::setw(4) << res_num << "    "
            << std::fixed << std::setprecision(3)
            << std::setw(8) << cs.positions[i].x
            << std::setw(8) << cs.positions[i].y
            << std::setw(8) << cs.positions[i].z
            << "  1.00  0.00           "
            << symbol_for_Z(cs.atomic_numbers[i]) << "\n";
    }

    // CONECT records
    for (const auto& b : cs.bonds) {
        ofs << "CONECT" << std::setw(5) << (b.i+1) << std::setw(5) << (b.j+1) << "\n";
    }

    ofs << "END\n";
    ofs.close();
}

// ============================================================================
// TEST RUNNER
// ============================================================================

struct TestResult {
    std::string name;
    std::string category;
    bool converged = false;
    int iterations = 0;
    double energy = 0.0;
    double max_force = 0.0;
    int n_atoms = 0;
    int n_bonds = 0;
    std::vector<std::string> outputs_written;
};

static TestResult run_single_test(RandomTest& test, CompositeModelWrapper& model,
                                   const std::string& out_dir)
{
    TestResult res;
    res.name = test.name;
    res.category = test.category;
    res.n_atoms = (int)test.state.positions.size();
    res.n_bonds = (int)test.state.bonds.size();

    model.reset();

    // FIRE minimization
    auto fire = fire_minimize(test.state, model, 500, 1e-3);
    res.converged = fire.converged;
    res.iterations = fire.iterations;
    res.max_force = fire.final_max_force;

    // Energy at relaxed state
    res.energy = model.energy(test.state);

    // Write outputs
    for (auto mode : test.outputs) {
        switch (mode) {
            case OutputMode::XYZ_CARTOON: {
                std::string path = out_dir + "/" + test.name + ".xyz";
                write_xyz_cartoon(test.state, test.name, res.energy, path);
                res.outputs_written.push_back(path);
                break;
            }
            case OutputMode::GRAPH: {
                std::string path = out_dir + "/" + test.name + ".dot";
                write_graph_dot(test.state, test.name, res.energy, path);
                res.outputs_written.push_back(path);
                break;
            }
            case OutputMode::PROTEIN: {
                std::string path = out_dir + "/" + test.name + ".pdb";
                write_protein_annotation(test.state, test.name, test.ss, res.energy, path);
                res.outputs_written.push_back(path);
                break;
            }
        }
    }

    return res;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    // --- Parse arguments ---
    int test_count = 42;       // default: 42 random tests
    uint64_t master_seed = 12345;
    std::string output_dir = "out/qa_random/run_" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--count" || arg == "-n") && i+1 < argc) {
            test_count = std::atoi(argv[++i]);
            test_count = std::max(5, std::min(101, test_count));
        } else if ((arg == "--seed" || arg == "-s") && i+1 < argc) {
            master_seed = std::stoull(argv[++i]);
        } else if ((arg == "--output" || arg == "-o") && i+1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: qa_random_tests [options]\n\n"
                      << "Options:\n"
                      << "  --count N, -n N   Number of random tests (5-101, default 42)\n"
                      << "  --seed S, -s S    Master RNG seed (default 12345)\n"
                      << "  --output dir      Output directory\n"
                      << "  --help            Show this help\n\n"
                      << "Output modes (per test):\n"
                      << "  3/3 (always)  .xyz file with cartoon header\n"
                      << "  2/3           .dot connectivity/energy graph\n"
                      << "  ~1/3          .pdb protein-like annotation\n\n";
            return 0;
        }
    }

    fs::create_directories(output_dir);
    fs::create_directories(output_dir + "/xyz");
    fs::create_directories(output_dir + "/graph");
    fs::create_directories(output_dir + "/protein");

    // --- Banner ---
    std::cout << "╔══════════════════════════════════════════════════════════╗\n"
              << "║  QA Random Molecular Test Suite                         ║\n"
              << "╠══════════════════════════════════════════════════════════╣\n"
              << "║  Deterministic randomized structure generation          ║\n"
              << "║  Composite model: bonded + LJ (1-2/1-3 exclusions)     ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n\n";

    std::cout << "  Tests:  " << test_count << "\n";
    std::cout << "  Seed:   " << master_seed << "\n";
    std::cout << "  Output: " << output_dir << "\n\n";

    // --- GPU Detection ---
    std::cout << "🖥️  GPU Detection...\n";
    GPUInfo gpu = detect_gpu();
    if (gpu.available) {
        std::cout << "  ✅ GPU: " << gpu.name << "\n";
        std::cout << "     Driver: " << gpu.driver << "\n";
        if (gpu.vram_mb > 0)
            std::cout << "     VRAM: " << gpu.vram_mb << " MB\n";
        std::cout << "     Status: MOUNTED (available for future acceleration)\n";
    } else {
        std::cout << "  ⚠️  No GPU detected — running CPU-only\n";
    }
    std::cout << "\n";

    // --- Save manifest ---
    {
        std::ofstream mf(output_dir + "/manifest.json");
        mf << "{\n"
           << "  \"test_count\": " << test_count << ",\n"
           << "  \"master_seed\": " << master_seed << ",\n"
           << "  \"gpu_available\": " << (gpu.available ? "true" : "false") << ",\n"
           << "  \"gpu_name\": \"" << gpu.name << "\",\n"
           << "  \"gpu_driver\": \"" << gpu.driver << "\",\n"
           << "  \"gpu_vram_mb\": " << gpu.vram_mb << ",\n"
           << "  \"model\": \"CompositeModel(Bonded+LJ, 1-2/1-3 excl)\",\n"
           << "  \"fire_max_steps\": 500,\n"
           << "  \"fire_f_tol\": 0.001\n"
           << "}\n";
        mf.close();
        std::cout << "📝 Manifest: " << output_dir << "/manifest.json\n\n";
    }

    // --- Generate tests ---
    std::cout << "🎲 Generating " << test_count << " random structures...\n";
    auto tests = generate_random_tests(test_count, master_seed);

    // Count categories
    int n_mol = 0, n_chain = 0, n_ring = 0, n_prot = 0;
    int n_xyz = 0, n_graph = 0, n_pdb = 0;
    for (const auto& t : tests) {
        if (t.category == "molecule") n_mol++;
        else if (t.category == "chain") n_chain++;
        else if (t.category == "ring") n_ring++;
        else if (t.category == "protein") n_prot++;
        for (auto m : t.outputs) {
            if (m == OutputMode::XYZ_CARTOON) n_xyz++;
            else if (m == OutputMode::GRAPH) n_graph++;
            else if (m == OutputMode::PROTEIN) n_pdb++;
        }
    }
    std::cout << "  Molecules:  " << n_mol << "\n"
              << "  Chains:     " << n_chain << "\n"
              << "  Rings:      " << n_ring << "\n"
              << "  Proteins:   " << n_prot << "  (~"
              << (int)(100.0 * n_prot / test_count) << "% of total)\n"
              << "  Outputs: .xyz=" << n_xyz << " (3/3), .dot=" << n_graph
              << " (~2/3), .pdb=" << n_pdb << " (~1/3)\n\n";

    // --- Run tests ---
    std::cout << "⚡ Running FIRE minimization on all structures...\n\n";

    CompositeModelWrapper model;
    std::vector<TestResult> results;
    results.reserve(test_count);

    int passed = 0, failed = 0;
    auto t_start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < tests.size(); ++i) {
        auto& test = tests[i];

        // Route output files to subdirectories
        std::string sub_dir = output_dir;
        if (test.category == "protein") sub_dir += "/protein";
        else sub_dir += "/xyz";

        auto result = run_single_test(test, model, sub_dir);

        // Also write graph to graph/ subdir if GRAPH mode
        for (auto m : test.outputs) {
            if (m == OutputMode::GRAPH) {
                std::string gpath = output_dir + "/graph/" + test.name + ".dot";
                write_graph_dot(test.state, test.name, result.energy, gpath);
            }
        }

        // Print inline status
        const char* status = result.converged ? "✅" : "⚠️ ";
        std::cout << "  [" << std::setw(3) << (i+1) << "/" << test_count << "] "
                  << status << " " << std::left << std::setw(40) << result.name
                  << " atoms=" << std::setw(4) << result.n_atoms
                  << " bonds=" << std::setw(4) << result.n_bonds
                  << " E=" << std::fixed << std::setprecision(2) << std::setw(10)
                  << result.energy
                  << " iter=" << result.iterations << "\n";

        if (result.converged) passed++; else failed++;
        results.push_back(result);
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // --- Summary ---
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n"
              << "║  SUMMARY                                                 ║\n"
              << "╠══════════════════════════════════════════════════════════╣\n"
              << "║  Total:      " << std::setw(4) << test_count << "                                       ║\n"
              << "║  Converged:  " << std::setw(4) << passed << "                                       ║\n"
              << "║  Unconverged:" << std::setw(4) << failed << "                                       ║\n"
              << "║  Wall time:  " << std::fixed << std::setprecision(1) << std::setw(8) << wall_ms
              << " ms                                ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n\n";

    // --- Generate report ---
    {
        std::string rpath = output_dir + "/report.md";
        std::ofstream rpt(rpath);
        rpt << "# QA Random Test Report\n\n";
        rpt << "**Tests:** " << test_count << "  \n";
        rpt << "**Seed:** " << master_seed << "  \n";
        rpt << "**GPU:** " << (gpu.available ? gpu.name : "None") << "  \n";
        rpt << "**Wall time:** " << std::fixed << std::setprecision(1) << wall_ms << " ms  \n\n";

        rpt << "## Distribution\n\n";
        rpt << "| Category | Count |\n|---|---|\n";
        rpt << "| Molecules | " << n_mol << " |\n";
        rpt << "| Chains | " << n_chain << " |\n";
        rpt << "| Rings | " << n_ring << " |\n";
        rpt << "| Proteins | " << n_prot << " |\n\n";

        rpt << "## Output Modes\n\n";
        rpt << "| Mode | Count | Fraction |\n|---|---|---|\n";
        rpt << "| .xyz (cartoon) | " << n_xyz << " | " << std::setprecision(0) << (100.0*n_xyz/test_count) << "% |\n";
        rpt << "| .dot (graph) | " << n_graph << " | " << (100.0*n_graph/test_count) << "% |\n";
        rpt << "| .pdb (protein) | " << n_pdb << " | " << (100.0*n_pdb/test_count) << "% |\n\n";

        rpt << "## Results\n\n";
        rpt << "| # | Name | Category | Atoms | Bonds | Energy | Converged | Iter |\n";
        rpt << "|---|------|----------|-------|-------|--------|-----------|------|\n";
        for (size_t i = 0; i < results.size(); ++i) {
            auto& r = results[i];
            rpt << "| " << (i+1) << " | " << r.name << " | " << r.category
                << " | " << r.n_atoms << " | " << r.n_bonds
                << " | " << std::fixed << std::setprecision(4) << r.energy
                << " | " << (r.converged ? "✅" : "⚠️") << " | " << r.iterations << " |\n";
        }

        rpt << "\n---\n*Generated by VSEPR-SIM qa_random_tests*\n";
        rpt.close();
        std::cout << "📄 Report: " << rpath << "\n";
    }

    // --- List output files ---
    int total_files = 0;
    for (const auto& r : results) total_files += (int)r.outputs_written.size();
    std::cout << "📁 Files written: " << total_files << " across xyz/, graph/, protein/\n\n";

    if (failed == 0)
        std::cout << "✅ ALL " << test_count << " STRUCTURES CONVERGED\n\n";
    else
        std::cout << "⚠️  " << failed << " structure(s) did not converge (may need more FIRE steps)\n\n";

    return (failed == 0) ? 0 : 1;
}
