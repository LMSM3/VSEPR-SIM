/**
 * viz_server.hpp
 * --------------
 * Dual-port JSON visualisation stream server.
 *
 * Port 9999  -> Atomic View: live atomistic state, frame-by-frame
 * Port 10001 -> Analysis View: historical metrics, candidates, properties
 *
 * Both servers share one analysis engine. Atomic frames are lightweight
 * and published at up to 20 fps. Analysis frames carry full history buffers
 * and property panels at ~1 fps.
 *
 * Wire protocol: newline-delimited JSON (NDJSON).
 * Each line is one complete JSON frame. Clients read line-by-line.
 *
 * Anti-black-box: every published frame carries seed, cycle, and provenance.
 */

#pragma once

#include "gas2/gas2_engine.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <fstream>

namespace vsepr {
namespace viz {

// ============================================================================
// Atom record - the fundamental unit of the atomic view frame
// ============================================================================

struct AtomRecord {
    int         id;
    int         Z;              // atomic number / species index
    std::string symbol;
    double      x, y, z;       // position (Angstrom)
    double      vx, vy, vz;    // velocity (m/s), or pseudo-motion vector
    double      q;              // charge or scalar property
    int         state_tag;      // 0=normal, 1=active, 2=defect, 3=focus
    double      chi;            // confidence / local fit score [0,1]
    double      ke_Eh;          // local kinetic energy (Hartree)
    double      color_r, color_g, color_b;  // element colour
};

// ============================================================================
// Bond record
// ============================================================================

struct BondRecord {
    int    i, j;
    double weight;
    int    type;    // 0=chem, 1=neighbor, 2=crystal, 3=force
};

// ============================================================================
// Lattice vectors
// ============================================================================

struct LatticeRecord {
    double ax, ay, az;
    double bx, by, bz;
    double cx, cy, cz;
    bool   active;
};

// ============================================================================
// Atomic view frame (port 9999) - fast, lightweight
// ============================================================================

struct AtomicFrame {
    uint64_t    cycle;
    uint64_t    seed;
    std::string formula;
    double      T_K;
    double      energy_J;
    double      energy_Eh;
    std::string phase_guess;
    int         n_atoms;
    int         n_defects;
    double      convergence;
    int         run_mode;       // 0=sample, 1=relax, 2=explore, 3=crystal
    std::string run_mode_str;
    int         focus_atom;     // index of currently active atom (-1 = none)

    std::vector<AtomRecord>  atoms;
    std::vector<BondRecord>  bonds;
    LatticeRecord            lattice;

    std::string to_json() const;
};

// ============================================================================
// Candidate structure (for analysis view comparison panel)
// ============================================================================

struct CandidateRecord {
    int         id;
    std::string label;
    double      energy_Eh;
    double      density_gcm3;
    double      symmetry_score;
    double      bulk_modulus_GPa;
    double      thermal_expansion;
    int         n_atoms;
    std::string phase_class;    // "fcc","bcc","hcp","amorphous","gas","unknown"
};

// ============================================================================
// Property panel snapshot
// ============================================================================

struct PropertySnapshot {
    double density_gcm3;
    double bulk_modulus_GPa;
    double shear_modulus_GPa;
    double youngs_modulus_GPa;
    double poisson_ratio;
    double thermal_expansion_1e6K;
    double debye_temperature_K;
    double vickers_hardness;
    double melt_temperature_K;
    double electrical_resistivity_uOhm;
    double thermal_conductivity_WmK;
    double sound_speed_ms;
    bool   valid;
};

// ============================================================================
// Analysis view frame (port 10001) - full history, heavy
// ============================================================================

struct AnalysisFrame {
    uint64_t    cycle;
    std::string formula;
    double      T_K;

    // Time-series history (last N points)
    std::vector<double> history_energy;
    std::vector<double> history_T;
    std::vector<double> history_density;
    std::vector<double> history_convergence;
    std::vector<double> history_phi;         // phase score
    std::vector<double> history_bond_count;
    std::vector<double> history_defect_count;
    std::vector<double> history_lattice_fit;

    // Candidate comparison set
    std::vector<CandidateRecord> candidates;

    // Macro-property panel
    PropertySnapshot properties;

    // Speed/KE distribution (for gas2 phase)
    std::vector<double> speed_distribution;   // radial f(v), 64 bins
    std::vector<double> ke_distribution;      // KE histogram, 64 bins
    double v_rms, v_mean, v_mp;
    double ke_avg_Eh;

    // Crystal metrics
    double lattice_a, lattice_b, lattice_c;
    double lattice_alpha, lattice_beta, lattice_gamma;
    double density_fit;
    double coordination_avg;
    std::string space_group_guess;

    std::string to_json() const;
};

// ============================================================================
// Dual-port viz server
// ============================================================================

struct VizServerConfig {
    int    port_atomic   = 9999;
    int    port_analysis = 10001;
    int    atomic_fps    = 15;
    int    analysis_fps  = 2;
    int    history_len   = 300;
    bool   verbose       = false;
    std::string log_path;              // JSONL capture file (empty = no logging)
};

class VizServer {
public:
    explicit VizServer(const VizServerConfig& cfg = {});
    ~VizServer();

    // Non-blocking start — launches two listener threads
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Push a new atomic frame (called by the simulation engine)
    void push_atomic(const AtomicFrame& frame);
    // Push a new analysis frame
    void push_analysis(const AnalysisFrame& frame);

    // Query current frame counts
    uint64_t atomic_frame_count() const  { return atomic_count_.load(); }
    uint64_t analysis_frame_count() const { return analysis_count_.load(); }

private:
    VizServerConfig cfg_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> atomic_count_{0};
    std::atomic<uint64_t> analysis_count_{0};

    // Latest frames (guarded by mutexes)
    mutable std::mutex atomic_mtx_;
    mutable std::mutex analysis_mtx_;
    AtomicFrame   latest_atomic_;
    AnalysisFrame latest_analysis_;
    bool          has_atomic_{false};
    bool          has_analysis_{false};

    // Listener threads
    std::thread atomic_thread_;
    std::thread analysis_thread_;
    int atomic_fd_  = -1;
    int analysis_fd_ = -1;

    void serve_loop(int port, bool is_atomic);
    bool init_socket(int port, int& fd_out);
    void serve_client(int client_fd, bool is_atomic);
    void cleanup_socket(int fd);

    // JSONL capture log
    std::ofstream log_file_;
};

// ============================================================================
// Gas2-driven frame generator
// ============================================================================

// Generate an AtomicFrame from a gas2 MB sample (for gas/kinetic view)
AtomicFrame gas2_atomic_frame(const gas2::GasSpecies& sp, double T_K,
                               uint64_t cycle, uint64_t seed,
                               int n_atoms = 64);

// Generate an AnalysisFrame from a gas2 analysis result
AnalysisFrame gas2_analysis_frame(const gas2::Gas2Analysis& a,
                                   uint64_t cycle,
                                   const std::vector<double>& hist_E,
                                   const std::vector<double>& hist_T,
                                   const std::vector<double>& hist_rho);

// ============================================================================
// CLI dispatch: vsepr viz [options]
// ============================================================================

int viz_dispatch(int argc, char** argv);

} // namespace viz
} // namespace vsepr
