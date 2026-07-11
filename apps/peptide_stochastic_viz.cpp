// peptide_stochastic_viz.cpp — Stochastic Peptide Formation Visualization Runner
// Day 48A: "Create 2D and 3D window spam of complete simulations and
//           relaxations of random and high stochasticity in choosing"
//
// This runner:
//   1. Generates N random peptide chains (random residue sequences, environments,
//      torsion angles, partial charges, sidechain classes)
//   2. Runs the full formation pipeline for each chain
//   3. Outputs JSON frames to stdout for the Python visualization clients
//   4. Spawns both the 2D energy/score timeline viewer and the 3D atomic viewer
//
// Each simulation run uses a different random seed, producing high-stochasticity
// exploration of the peptide formation landscape.
//
// Usage:
//     peptide-stochastic-viz                        (default: 8 runs, 3-8 residues each)
//     peptide-stochastic-viz --runs 16 --max-res 12
//     peptide-stochastic-viz --seed 42              (reproducible)

#include "../chem/peptide/peptide_formation.hpp"
#include "../chem/peptide/vsepr_peptide_types.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define CLOSE_SOCKET close
#define INVALID_SOCKET (-1)
#endif

using namespace vsepr::chem;

// ============================================================================
// Amino acid library — the 20 standard residues
// ============================================================================

struct AminoAcidDef {
    const char* three_letter;
    const char* one_letter;
    VSEPR_PeptideSidechainClass sidechain_class;
    int sidechain_heavy_atoms;   // non-H heavy atoms in sidechain
    double sidechain_mass_u;     // approximate sidechain mass
};

static constexpr AminoAcidDef AMINO_ACIDS[] = {
    {"GLY", "G", VSEPR_SIDECHAIN_SPECIAL,      0,   1.008},
    {"ALA", "A", VSEPR_SIDECHAIN_NONPOLAR,      1,  15.035},
    {"VAL", "V", VSEPR_SIDECHAIN_NONPOLAR,      3,  43.089},
    {"LEU", "L", VSEPR_SIDECHAIN_NONPOLAR,      4,  57.116},
    {"ILE", "I", VSEPR_SIDECHAIN_NONPOLAR,      4,  57.116},
    {"PRO", "P", VSEPR_SIDECHAIN_SPECIAL,       3,  42.081},
    {"PHE", "F", VSEPR_SIDECHAIN_AROMATIC,      7,  91.132},
    {"TRP", "W", VSEPR_SIDECHAIN_AROMATIC,     10, 130.170},
    {"MET", "M", VSEPR_SIDECHAIN_NONPOLAR,      4,  75.154},
    {"CYS", "C", VSEPR_SIDECHAIN_POLAR,         2,  47.099},
    {"SER", "S", VSEPR_SIDECHAIN_POLAR,         2,  31.034},
    {"THR", "T", VSEPR_SIDECHAIN_POLAR,         3,  45.061},
    {"ASN", "N", VSEPR_SIDECHAIN_POLAR,         4,  58.060},
    {"GLN", "Q", VSEPR_SIDECHAIN_POLAR,         5,  72.087},
    {"TYR", "Y", VSEPR_SIDECHAIN_AROMATIC,      8, 107.131},
    {"ASP", "D", VSEPR_SIDECHAIN_NEGATIVE,      4,  59.044},
    {"GLU", "E", VSEPR_SIDECHAIN_NEGATIVE,      5,  73.071},
    {"LYS", "K", VSEPR_SIDECHAIN_POSITIVE,      5,  72.130},
    {"ARG", "R", VSEPR_SIDECHAIN_POSITIVE,      7, 100.144},
    {"HIS", "H", VSEPR_SIDECHAIN_AROMATIC,      6,  81.097},
};
static constexpr int N_AMINO_ACIDS = sizeof(AMINO_ACIDS) / sizeof(AMINO_ACIDS[0]);

static constexpr VSEPR_EnvironmentClass ENVIRONMENTS[] = {
    VSEPR_ENV_VACUUM,
    VSEPR_ENV_DRY_CONDENSED,
    VSEPR_ENV_POLAR_SOLVENT,
    VSEPR_ENV_NONPOLAR_SOLVENT,
    VSEPR_ENV_REACTIVE_FIELD,
};
static constexpr int N_ENVIRONMENTS = sizeof(ENVIRONMENTS) / sizeof(ENVIRONMENTS[0]);

static const char* env_name(VSEPR_EnvironmentClass e) {
    switch (e) {
        case VSEPR_ENV_VACUUM:          return "vacuum";
        case VSEPR_ENV_DRY_CONDENSED:   return "dry_condensed";
        case VSEPR_ENV_POLAR_SOLVENT:   return "polar_solvent";
        case VSEPR_ENV_NONPOLAR_SOLVENT: return "nonpolar_solvent";
        case VSEPR_ENV_REACTIVE_FIELD:  return "reactive_field";
        case VSEPR_ENV_RADIATIVE:       return "radiative";
        default:                        return "unknown";
    }
}

static const char* state_name(VSEPR_FormationState s) {
    switch (s) {
        case VSEPR_STATE_PREFORM:              return "PREFORM";
        case VSEPR_STATE_LOCAL_GROUP_FORMED:    return "LOCAL_GROUP_FORMED";
        case VSEPR_STATE_MOLECULE_FORMED:       return "MOLECULE_FORMED";
        case VSEPR_STATE_LINKED:               return "LINKED";
        case VSEPR_STATE_CONFORMATIONAL_SOLVE:  return "CONFORMATIONAL_SOLVE";
        case VSEPR_STATE_LOCAL_FOLDED:          return "LOCAL_FOLDED";
        case VSEPR_STATE_MACRO_STABLE:          return "MACRO_STABLE";
        case VSEPR_STATE_DEGRADED:             return "DEGRADED";
    }
    return "UNKNOWN";
}

// ============================================================================
// Stochastic chain generator
// ============================================================================

struct StochasticRun {
    int run_id;
    std::string sequence;           // e.g. "GLY-ALA-SER"
    VSEPR_EnvironmentClass env;
    std::vector<Atom> atoms;
    std::vector<Residue> residues;
    FormationSummary summary;
    bool success;
    std::string error_message;
};

static StochasticRun generate_random_chain(int run_id, std::mt19937& rng,
                                            int min_res, int max_res) {
    StochasticRun run;
    run.run_id = run_id;
    run.success = false;

    std::uniform_int_distribution<int> res_count_dist(min_res, max_res);
    std::uniform_int_distribution<int> aa_dist(0, N_AMINO_ACIDS - 1);
    std::uniform_int_distribution<int> env_dist(0, N_ENVIRONMENTS - 1);
    std::uniform_real_distribution<double> phi_dist(-180.0, 180.0);
    std::uniform_real_distribution<double> psi_dist(-180.0, 180.0);
    std::uniform_real_distribution<double> omega_noise(-5.0, 5.0);
    std::uniform_real_distribution<double> charge_noise(-0.3, 0.3);
    std::uniform_real_distribution<double> pos_noise(-0.1, 0.1);

    int n_residues = res_count_dist(rng);
    run.env = ENVIRONMENTS[env_dist(rng)];

    int atom_id = 1;
    std::ostringstream seq_ss;

    for (int i = 0; i < n_residues; ++i) {
        const auto& aa = AMINO_ACIDS[aa_dist(rng)];
        if (i > 0) seq_ss << "-";
        seq_ss << aa.three_letter;

        double x_offset = i * 3.8; // ~3.8 A per residue along chain

        int base_id = atom_id;

        // Backbone: N, CA, C, O
        run.atoms.push_back(Atom{
            .atom_id = atom_id++, .atomic_number = 7, .isotope = 14,
            .atom_name = std::string(aa.three_letter) + "_N",
            .element_symbol = "N",
            .position = {x_offset + pos_noise(rng), pos_noise(rng), pos_noise(rng)},
            .chem_role = VSEPR_ROLE_BACKBONE_N,
            .partial_charge = -0.42 + charge_noise(rng),
            .covalent_radius_pm = 71.0, .vdw_radius_pm = 155.0, .mass_u = 14.007
        });
        run.atoms.push_back(Atom{
            .atom_id = atom_id++, .atomic_number = 6, .isotope = 12,
            .atom_name = std::string(aa.three_letter) + "_CA",
            .element_symbol = "C",
            .position = {x_offset + 1.47 + pos_noise(rng), pos_noise(rng), pos_noise(rng)},
            .chem_role = VSEPR_ROLE_ALPHA_C,
            .partial_charge = 0.02 + charge_noise(rng),
            .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
        });
        run.atoms.push_back(Atom{
            .atom_id = atom_id++, .atomic_number = 6, .isotope = 12,
            .atom_name = std::string(aa.three_letter) + "_C",
            .element_symbol = "C",
            .position = {x_offset + 2.99 + pos_noise(rng), pos_noise(rng), pos_noise(rng)},
            .chem_role = VSEPR_ROLE_CARBONYL_C,
            .partial_charge = 0.51 + charge_noise(rng),
            .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = 12.011
        });
        run.atoms.push_back(Atom{
            .atom_id = atom_id++, .atomic_number = 8, .isotope = 16,
            .atom_name = std::string(aa.three_letter) + "_O",
            .element_symbol = "O",
            .position = {x_offset + 2.99 + pos_noise(rng), 1.23 + pos_noise(rng), pos_noise(rng)},
            .chem_role = VSEPR_ROLE_CARBONYL_O,
            .partial_charge = -0.51 + charge_noise(rng),
            .covalent_radius_pm = 66.0, .vdw_radius_pm = 152.0, .mass_u = 15.999
        });

        int sidechain_root = -1;
        // Add sidechain heavy atoms (simplified: placed near CA)
        if (aa.sidechain_heavy_atoms > 0) {
            sidechain_root = atom_id;
            std::uniform_real_distribution<double> sc_offset(-2.0, 2.0);
            for (int s = 0; s < aa.sidechain_heavy_atoms; ++s) {
                int sc_Z = 6; // default carbon
                double sc_mass = 12.011;
                if (aa.sidechain_class == VSEPR_SIDECHAIN_NONPOLAR && s == aa.sidechain_heavy_atoms - 1
                    && std::string(aa.three_letter) == "MET") {
                    sc_Z = 16; sc_mass = 32.06;
                } else if (aa.sidechain_class == VSEPR_SIDECHAIN_POLAR && s == aa.sidechain_heavy_atoms - 1) {
                    sc_Z = 8; sc_mass = 15.999;
                } else if (aa.sidechain_class == VSEPR_SIDECHAIN_NEGATIVE && s >= aa.sidechain_heavy_atoms - 2) {
                    sc_Z = 8; sc_mass = 15.999;
                }

                run.atoms.push_back(Atom{
                    .atom_id = atom_id++, .atomic_number = sc_Z,
                    .atom_name = std::string(aa.three_letter) + "_SC" + std::to_string(s),
                    .element_symbol = (sc_Z == 6 ? "C" : (sc_Z == 8 ? "O" : "S")),
                    .position = {x_offset + 1.47 + sc_offset(rng),
                                 -1.0 + sc_offset(rng),
                                 sc_offset(rng)},
                    .chem_role = VSEPR_ROLE_SIDECHAIN,
                    .partial_charge = charge_noise(rng),
                    .covalent_radius_pm = 76.0, .vdw_radius_pm = 170.0, .mass_u = sc_mass
                });
            }
        }

        Residue res {
            .residue_id = i + 1,
            .residue_name = aa.three_letter,
            .backbone_N  = base_id,
            .backbone_CA = base_id + 1,
            .backbone_C  = base_id + 2,
            .backbone_O  = base_id + 3,
            .sidechain_root = sidechain_root,
            .sidechain_class = aa.sidechain_class,
            .phi_deg = phi_dist(rng),
            .psi_deg = psi_dist(rng),
            .omega_deg = 180.0 + omega_noise(rng),
        };

        // Build atom_ids list
        for (int a = base_id; a < atom_id; ++a) {
            res.atom_ids.push_back(a);
        }

        run.residues.push_back(std::move(res));
    }

    run.sequence = seq_ss.str();

    // Run the pipeline
    PeptideFormationPipeline pipeline{run.env};
    auto graph_result = pipeline.build_from_residues(run.residues, run.atoms);
    if (!graph_result) {
        run.error_message = graph_result.error().message;
        return run;
    }

    auto solve_result = pipeline.solve(std::move(*graph_result));
    if (!solve_result) {
        run.error_message = solve_result.error().message;
        return run;
    }

    run.summary = *solve_result;
    run.success = true;
    return run;
}

// ============================================================================
// JSON frame serialization
// ============================================================================

static std::string run_to_json(const StochasticRun& run) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"run_id\":" << run.run_id << ",";
    ss << "\"sequence\":\"" << run.sequence << "\",";
    ss << "\"environment\":\"" << env_name(run.env) << "\",";
    ss << "\"success\":" << (run.success ? "true" : "false") << ",";

    if (!run.success) {
        ss << "\"error\":\"" << run.error_message << "\"";
    } else {
        const auto& s = run.summary;
        ss << "\"state\":\"" << state_name(s.formation_state) << "\",";
        ss << "\"atom_count\":" << s.atom_count << ",";
        ss << "\"residue_count\":" << s.residue_count << ",";
        ss << "\"bond_count\":" << s.bond_count << ",";
        ss << "\"hbond_count\":" << s.hbond_count << ",";
        ss << "\"energy\":{";
        ss << "\"total\":" << s.energy.total_kj_mol << ",";
        ss << "\"bond\":" << s.energy.bond_kj_mol << ",";
        ss << "\"vdw\":" << s.energy.vdw_kj_mol << ",";
        ss << "\"coulomb\":" << s.energy.coulomb_kj_mol << ",";
        ss << "\"solvation\":" << s.energy.solvation_kj_mol << ",";
        ss << "\"formation\":" << s.energy.formation_kj_mol;
        ss << "},";
        ss << "\"scores\":{";
        ss << "\"steric\":" << s.score.steric_score << ",";
        ss << "\"hbond\":" << s.score.hbond_score << ",";
        ss << "\"electrostatic\":" << s.score.electrostatic_score << ",";
        ss << "\"hydrophobic\":" << s.score.hydrophobic_score << ",";
        ss << "\"planarity\":" << s.score.planarity_score << ",";
        ss << "\"confidence\":" << s.score.confidence_score;
        ss << "},";
        ss << "\"validity\":{";
        ss << "\"chemical\":" << (s.chemical_validity_pass ? "true" : "false") << ",";
        ss << "\"valence\":" << (s.valence_validity_pass ? "true" : "false");
        ss << "},";

        // Atom positions for 3D rendering
        ss << "\"atoms\":[";
        for (std::size_t i = 0; i < run.atoms.size(); ++i) {
            if (i > 0) ss << ",";
            const auto& a = run.atoms[i];
            ss << "{\"id\":" << a.atom_id;
            ss << ",\"Z\":" << a.atomic_number;
            ss << ",\"x\":" << a.position.x;
            ss << ",\"y\":" << a.position.y;
            ss << ",\"z\":" << a.position.z;
            ss << ",\"q\":" << a.partial_charge;
            ss << ",\"role\":" << static_cast<int>(a.chem_role);
            ss << "}";
        }
        ss << "],";

        // Residue info
        ss << "\"residues\":[";
        for (std::size_t i = 0; i < run.residues.size(); ++i) {
            if (i > 0) ss << ",";
            const auto& r = run.residues[i];
            ss << "{\"id\":" << r.residue_id;
            ss << ",\"name\":\"" << r.residue_name << "\"";
            ss << ",\"phi\":" << r.phi_deg;
            ss << ",\"psi\":" << r.psi_deg;
            ss << ",\"omega\":" << r.omega_deg;
            ss << ",\"sc_class\":" << static_cast<int>(r.sidechain_class);
            ss << "}";
        }
        ss << "]";

        // Functional groups
        if (!s.functional_groups.empty()) {
            ss << ",\"functional_groups\":[";
            for (std::size_t i = 0; i < s.functional_groups.size(); ++i) {
                if (i > 0) ss << ",";
                ss << "\"" << s.functional_groups[i].label << "\"";
            }
            ss << "]";
        }
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Network socket helper (non-blocking UDP broadcast to viz ports)
// Base ports: 9999 (3D atomic) and 8899 (2D analysis)
// Also broadcasts to all 999X ports for unified viz_host.py
// ============================================================================

struct VizBroadcaster {
    static constexpr int PORT_3D   = 9999;  // 3D atomic view (primary)
    static constexpr int PORT_2D   = 8899;  // 2D analysis (base)
    static constexpr int PORT_999X_START = 9990;
    static constexpr int PORT_999X_END   = 9998;  // 9990-9998 (9999 already covered)

    socket_t sock;
    sockaddr_in addr_3d;
    sockaddr_in addr_2d;
    sockaddr_in addr_999x[9]; // 9990-9998
    bool active;

    VizBroadcaster() : sock(INVALID_SOCKET), active(false) {}

    bool init() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;
#endif
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return false;

        auto make_addr = [](int port) -> sockaddr_in {
            sockaddr_in a {};
            a.sin_family = AF_INET;
            a.sin_port = htons(static_cast<uint16_t>(port));
            inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
            return a;
        };

        addr_3d = make_addr(PORT_3D);
        addr_2d = make_addr(PORT_2D);
        for (int i = 0; i < 9; ++i) {
            addr_999x[i] = make_addr(PORT_999X_START + i);
        }

        active = true;
        return true;
    }

    void send_3d(const std::string& json) {
        if (!active) return;
        auto* buf = json.c_str();
        auto len = static_cast<int>(json.size());
        // Primary: port 9999
        sendto(sock, buf, len, 0,
               reinterpret_cast<sockaddr*>(&addr_3d), sizeof(addr_3d));
        // Broadcast to 9990-9998 (viz_host.py picks these up)
        for (int i = 0; i < 9; ++i) {
            sendto(sock, buf, len, 0,
                   reinterpret_cast<sockaddr*>(&addr_999x[i]), sizeof(addr_999x[i]));
        }
    }

    void send_2d(const std::string& json) {
        if (!active) return;
        auto* buf = json.c_str();
        auto len = static_cast<int>(json.size());
        // Primary: port 8899
        sendto(sock, buf, len, 0,
               reinterpret_cast<sockaddr*>(&addr_2d), sizeof(addr_2d));
    }

    void close() {
        if (sock != INVALID_SOCKET) CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        active = false;
    }
};

// ============================================================================
// TUI output — ANSI colored terminal visualization
// ============================================================================

static void print_header() {
    std::printf("\033[1;35m");
    std::puts("╔═══════════════════════════════════════════════════════════════════╗");
    std::puts("║  VSEPR-SIM  Day 48A  |  Stochastic Peptide Formation Runner     ║");
    std::puts("║  High-stochasticity chain generation + formation + scoring       ║");
    std::puts("╚═══════════════════════════════════════════════════════════════════╝");
    std::printf("\033[0m\n");
}

static void print_run_result(const StochasticRun& run) {
    const char* color = run.success ? "\033[1;32m" : "\033[1;31m";
    const char* reset = "\033[0m";
    const char* dim   = "\033[90m";
    const char* cyan  = "\033[1;36m";
    const char* gold  = "\033[1;33m";

    std::printf("%s[Run %02d]%s ", color, run.run_id, reset);
    std::printf("%s%s%s ", cyan, run.sequence.c_str(), reset);
    std::printf("%sin %s%s\n", dim, env_name(run.env), reset);

    if (!run.success) {
        std::printf("         %sERROR: %s%s\n", "\033[1;31m", run.error_message.c_str(), reset);
        return;
    }

    const auto& s = run.summary;

    // Energy bar
    double e = s.energy.total_kj_mol;
    std::printf("         E_total = %s%+10.2f kJ/mol%s  ", gold, e, reset);
    int bar_len = std::clamp(static_cast<int>(std::abs(e) / 5.0), 1, 40);
    std::printf("%s", e < 0 ? "\033[32m" : "\033[31m");
    for (int i = 0; i < bar_len; ++i) std::putchar('█');
    std::printf("%s\n", reset);

    // Energy components
    std::printf("         %sbond=%+.1f  vdw=%+.1f  coul=%+.1f  solv=%+.1f  form=%+.1f%s\n",
                dim,
                s.energy.bond_kj_mol, s.energy.vdw_kj_mol,
                s.energy.coulomb_kj_mol, s.energy.solvation_kj_mol,
                s.energy.formation_kj_mol, reset);

    // Scores
    auto score_bar = [&](const char* name, double val) {
        int filled = static_cast<int>(val * 20);
        std::printf("         %s%-14s%s ", dim, name, reset);
        std::printf("\033[36m");
        for (int i = 0; i < 20; ++i) std::putchar(i < filled ? '▓' : '░');
        std::printf("%s %.3f\n", reset, val);
    };

    score_bar("steric", s.score.steric_score);
    score_bar("electrostatic", s.score.electrostatic_score);
    score_bar("hydrophobic", s.score.hydrophobic_score);
    score_bar("planarity", s.score.planarity_score);
    score_bar("confidence", s.score.confidence_score);

    // Stats line
    std::printf("         %satoms=%d  residues=%d  bonds=%d  hbonds=%d  "
                "chem_valid=%s  valence_valid=%s%s\n",
                dim, s.atom_count, s.residue_count, s.bond_count, s.hbond_count,
                s.chemical_validity_pass ? "YES" : "NO",
                s.valence_validity_pass ? "YES" : "NO",
                reset);

    // Functional groups
    if (!s.functional_groups.empty()) {
        std::printf("         %sfunc_groups: ", dim);
        for (std::size_t i = 0; i < s.functional_groups.size(); ++i) {
            if (i > 0) std::printf(", ");
            std::printf("%s", s.functional_groups[i].label.c_str());
        }
        std::printf("%s\n", reset);
    }

    std::puts("");
}

static void print_summary(const std::vector<StochasticRun>& runs) {
    int success = 0, fail = 0;
    double min_e = 1e30, max_e = -1e30, sum_e = 0;
    double max_conf = 0;
    int best_run = -1;

    for (const auto& r : runs) {
        if (r.success) {
            ++success;
            double e = r.summary.energy.total_kj_mol;
            sum_e += e;
            if (e < min_e) min_e = e;
            if (e > max_e) max_e = e;
            if (r.summary.score.confidence_score > max_conf) {
                max_conf = r.summary.score.confidence_score;
                best_run = r.run_id;
            }
        } else {
            ++fail;
        }
    }

    std::printf("\033[1;35m");
    std::puts("╔═══════════════════════════════════════════════════════════════════╗");
    std::puts("║  Stochastic Formation Summary                                   ║");
    std::puts("╠═══════════════════════════════════════════════════════════════════╣");
    std::printf("\033[0m");

    std::printf("  Runs: %d total, \033[32m%d OK\033[0m, \033[31m%d FAIL\033[0m\n",
                static_cast<int>(runs.size()), success, fail);

    if (success > 0) {
        std::printf("  Energy range: [%+.2f, %+.2f] kJ/mol   mean=%+.2f kJ/mol\n",
                    min_e, max_e, sum_e / success);
        std::printf("  Best confidence: %.4f  \033[1;33m(run %d: %s)\033[0m\n",
                    max_conf, best_run,
                    best_run >= 0 ? runs[best_run - 1].sequence.c_str() : "?");
    }

    std::printf("\033[1;35m");
    std::puts("╚═══════════════════════════════════════════════════════════════════╝");
    std::printf("\033[0m");
}

// ============================================================================
// Ramachandran-style 2D scatter (ASCII)
// ============================================================================

static void print_ramachandran(const std::vector<StochasticRun>& runs) {
    std::printf("\n\033[1;36m  Ramachandran Plot (phi/psi, all residues)\033[0m\n");

    // 36x18 grid covering phi=[-180,180], psi=[-180,180]
    constexpr int W = 36, H = 18;
    int grid[H][W] = {};

    for (const auto& r : runs) {
        if (!r.success) continue;
        for (const auto& res : r.residues) {
            int gx = static_cast<int>((res.phi_deg + 180.0) / 360.0 * W);
            int gy = static_cast<int>((res.psi_deg + 180.0) / 360.0 * H);
            gx = std::clamp(gx, 0, W - 1);
            gy = std::clamp(gy, 0, H - 1);
            grid[gy][gx]++;
        }
    }

    std::printf("  psi\n");
    for (int y = H - 1; y >= 0; --y) {
        std::printf("  %+4.0f |", -180.0 + (y + 0.5) * 360.0 / H);
        for (int x = 0; x < W; ++x) {
            int v = grid[y][x];
            if (v == 0)      std::printf(" ");
            else if (v == 1) std::printf("\033[90m·\033[0m");
            else if (v <= 3) std::printf("\033[36m●\033[0m");
            else if (v <= 6) std::printf("\033[33m●\033[0m");
            else             std::printf("\033[31m●\033[0m");
        }
        std::printf("|\n");
    }
    std::printf("       ");
    for (int x = 0; x < W; ++x) std::printf("-");
    std::printf("\n       -180%*s+180  phi\n", W - 7, "");
}

// ============================================================================
// Energy landscape 2D heatmap (ASCII)
// ============================================================================

static void print_energy_landscape(const std::vector<StochasticRun>& runs) {
    std::printf("\n\033[1;33m  Energy Landscape (run vs component)\033[0m\n");

    std::printf("  %4s  %10s %10s %10s %10s %10s  %s\n",
                "Run", "Bond", "VdW", "Coulomb", "Solv", "Form", "Total Bar");
    std::printf("  ────  ────────── ────────── ────────── ────────── ──────────  ──────────\n");

    for (const auto& r : runs) {
        if (!r.success) continue;
        const auto& e = r.summary.energy;

        std::printf("  %4d  %+10.2f %+10.2f %+10.2f %+10.2f %+10.2f  ",
                    r.run_id, e.bond_kj_mol, e.vdw_kj_mol,
                    e.coulomb_kj_mol, e.solvation_kj_mol, e.formation_kj_mol);

        // Mini inline bar for total
        double t = e.total_kj_mol;
        int bar = std::clamp(static_cast<int>(std::abs(t) / 10.0), 0, 30);
        std::printf("%s", t < 0 ? "\033[32m" : "\033[31m");
        for (int i = 0; i < bar; ++i) std::putchar('█');
        std::printf("\033[0m %+.1f\n", t);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int n_runs     = 8;
    int min_res    = 3;
    int max_res    = 8;
    std::uint64_t seed = 0;
    bool has_seed  = false;
    bool broadcast = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--runs" && i + 1 < argc) {
            n_runs = std::atoi(argv[++i]);
        } else if (arg == "--min-res" && i + 1 < argc) {
            min_res = std::atoi(argv[++i]);
        } else if (arg == "--max-res" && i + 1 < argc) {
            max_res = std::atoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            has_seed = true;
        } else if (arg == "--no-broadcast") {
            broadcast = false;
        } else if (arg == "--help" || arg == "-h") {
            std::puts("peptide-stochastic-viz [OPTIONS]");
            std::puts("  --runs N        Number of stochastic runs (default: 8)");
            std::puts("  --min-res N     Min residues per chain (default: 3)");
            std::puts("  --max-res N     Max residues per chain (default: 8)");
            std::puts("  --seed N        Random seed (default: time-based)");
            std::puts("  --no-broadcast  Disable UDP viz broadcast");
            return 0;
        }
    }

    if (!has_seed) {
        seed = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    print_header();

    std::printf("  Configuration: %d runs, %d-%d residues, seed=%llu\n\n",
                n_runs, min_res, max_res, static_cast<unsigned long long>(seed));

    VizBroadcaster viz;
    if (broadcast) {
        if (viz.init()) {
            std::printf("  \033[32mViz broadcast active\033[0m (3D: UDP %d, 2D: UDP %d, 999X: 9990-9998)\n",
                        VizBroadcaster::PORT_3D, VizBroadcaster::PORT_2D);
            std::printf("  Connect: python tools/viz_host.py\n\n");
        } else {
            std::printf("  \033[33mViz broadcast unavailable (continuing headless)\033[0m\n\n");
            broadcast = false;
        }
    }

    std::mt19937 rng(seed);
    std::vector<StochasticRun> runs;
    runs.reserve(n_runs);

    auto t_start = std::chrono::steady_clock::now();

    for (int i = 0; i < n_runs; ++i) {
        auto run = generate_random_chain(i + 1, rng, min_res, max_res);
        print_run_result(run);

        if (broadcast) {
            auto json = run_to_json(run);
            viz.send_3d(json);
            viz.send_2d(json);
        }

        runs.push_back(std::move(run));

        // Brief pause between runs for viz to render
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    print_ramachandran(runs);
    print_energy_landscape(runs);
    std::puts("");
    print_summary(runs);

    std::printf("\n  Elapsed: %.1f ms (%.1f runs/sec)\n", elapsed_ms,
                n_runs / (elapsed_ms / 1000.0));

    if (broadcast) viz.close();

    return 0;
}
