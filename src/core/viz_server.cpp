/**
 * viz_server.cpp
 * --------------
 * Dual-port NDJSON visualisation stream server.
 * Port 9999  -> Atomic live view
 * Port 10001 -> Analysis / deep render view
 */

#include "core/viz_server.hpp"
#include "gas2/gas2_kinetic.hpp"
#include "gas2/gas2_constants.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <random>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib,"ws2_32.lib")
  using sock_t = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
  #define CLOSE_SOCK(s) closesocket(s)
  static void init_winsock() {
      static bool done = false;
      if (!done) { WSADATA w; WSAStartup(MAKEWORD(2,2),&w); done=true; }
  }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  using sock_t = int;
  #define INVALID_SOCK (-1)
  #define CLOSE_SOCK(s) ::close(s)
  static void init_winsock() {}
#endif

namespace vsepr {
namespace viz {

// ============================================================================
// Element colours (CPK-ish: index = Z)
// ============================================================================

static const double ELEM_R[] = {0.9,0.9,0.8,0.5,0.7,0.2,0.1,0.1,0.6,0.7,
                                  0.7,0.5,0.6,0.6,0.8,0.9,0.1,0.5,0.7,0.5};
static const double ELEM_G[] = {0.9,0.9,0.9,0.5,0.7,0.2,0.1,0.3,0.6,0.7,
                                  0.7,0.5,0.7,0.4,0.5,0.8,0.4,0.5,0.5,0.5};
static const double ELEM_B[] = {0.9,0.9,1.0,0.5,0.7,0.9,0.9,0.9,0.9,0.7,
                                  0.7,0.5,0.3,0.3,0.3,0.5,0.1,0.5,0.5,0.5};

static inline void elem_colour(int Z, double& r, double& g, double& b) {
    int idx = (Z >= 1 && Z <= 18) ? (Z-1) : 0;
    r = ELEM_R[idx]; g = ELEM_G[idx]; b = ELEM_B[idx];
}

// ============================================================================
// AtomicFrame serialisation
// ============================================================================

std::string AtomicFrame::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{"
       << "\"type\":\"atomic\","
       << "\"cycle\":" << cycle << ","
       << "\"seed\":" << seed << ","
       << "\"formula\":\"" << formula << "\","
       << "\"T_K\":" << T_K << ","
       << "\"energy_Eh\":" << energy_Eh << ","
       << "\"phase\":\"" << phase_guess << "\","
       << "\"n_atoms\":" << n_atoms << ","
       << "\"n_defects\":" << n_defects << ","
       << "\"convergence\":" << convergence << ","
       << "\"run_mode\":\"" << run_mode_str << "\","
       << "\"focus_atom\":" << focus_atom << ","
       << "\"lattice\":{"
         << "\"active\":" << (lattice.active ? "true" : "false") << ","
         << "\"ax\":" << lattice.ax << ",\"ay\":" << lattice.ay << ",\"az\":" << lattice.az << ","
         << "\"bx\":" << lattice.bx << ",\"by\":" << lattice.by << ",\"bz\":" << lattice.bz << ","
         << "\"cx\":" << lattice.cx << ",\"cy\":" << lattice.cy << ",\"cz\":" << lattice.cz
       << "},"
       << "\"atoms\":[";
    for (int i = 0; i < (int)atoms.size(); ++i) {
        if (i) ss << ",";
        const auto& a = atoms[i];
        ss << "{\"id\":" << a.id << ",\"Z\":" << a.Z << ",\"sym\":\"" << a.symbol << "\","
           << "\"x\":" << a.x << ",\"y\":" << a.y << ",\"z\":" << a.z << ","
           << "\"vx\":" << a.vx << ",\"vy\":" << a.vy << ",\"vz\":" << a.vz << ","
           << "\"q\":" << a.q << ",\"st\":" << a.state_tag << ","
           << "\"chi\":" << a.chi << ",\"ke\":" << a.ke_Eh << ","
           << "\"cr\":" << a.color_r << ",\"cg\":" << a.color_g << ",\"cb\":" << a.color_b << "}";
    }
    ss << "],\"bonds\":[";
    for (int i = 0; i < (int)bonds.size(); ++i) {
        if (i) ss << ",";
        const auto& b = bonds[i];
        ss << "{\"i\":" << b.i << ",\"j\":" << b.j << ",\"w\":" << b.weight << ",\"t\":" << b.type << "}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// AnalysisFrame serialisation
// ============================================================================

static std::string vec_json(const std::vector<double>& v, int prec = 4) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << "[";
    for (int i = 0; i < (int)v.size(); ++i) { if (i) ss << ","; ss << v[i]; }
    ss << "]";
    return ss.str();
}

std::string AnalysisFrame::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{"
       << "\"type\":\"analysis\","
       << "\"cycle\":" << cycle << ","
       << "\"formula\":\"" << formula << "\","
       << "\"T_K\":" << T_K << ","
       << "\"v_rms\":" << v_rms << ","
       << "\"v_mean\":" << v_mean << ","
       << "\"v_mp\":" << v_mp << ","
       << "\"ke_avg_Eh\":" << ke_avg_Eh << ","
       << "\"lattice_a\":" << lattice_a << ","
       << "\"lattice_b\":" << lattice_b << ","
       << "\"lattice_c\":" << lattice_c << ","
       << "\"density_fit\":" << density_fit << ","
       << "\"coord_avg\":" << coordination_avg << ","
       << "\"space_group\":\"" << space_group_guess << "\","
       << "\"hist_E\":" << vec_json(history_energy) << ","
       << "\"hist_T\":" << vec_json(history_T) << ","
       << "\"hist_rho\":" << vec_json(history_density) << ","
       << "\"hist_conv\":" << vec_json(history_convergence) << ","
       << "\"hist_phi\":" << vec_json(history_phi) << ","
       << "\"hist_bond\":" << vec_json(history_bond_count) << ","
       << "\"hist_defect\":" << vec_json(history_defect_count) << ","
       << "\"speed_dist\":" << vec_json(speed_distribution) << ","
       << "\"ke_dist\":" << vec_json(ke_distribution) << ","
       << "\"props\":{"
         << "\"density\":" << properties.density_gcm3 << ","
         << "\"bulk_mod\":" << properties.bulk_modulus_GPa << ","
         << "\"shear_mod\":" << properties.shear_modulus_GPa << ","
         << "\"youngs_mod\":" << properties.youngs_modulus_GPa << ","
         << "\"poisson\":" << properties.poisson_ratio << ","
         << "\"therm_exp\":" << properties.thermal_expansion_1e6K << ","
         << "\"debye_T\":" << properties.debye_temperature_K << ","
         << "\"sound_speed\":" << properties.sound_speed_ms << ","
         << "\"melt_T\":" << properties.melt_temperature_K
       << "},"
       << "\"candidates\":[";
    for (int i = 0; i < (int)candidates.size(); ++i) {
        if (i) ss << ",";
        const auto& c = candidates[i];
        ss << "{\"id\":" << c.id << ",\"label\":\"" << c.label << "\","
           << "\"E\":" << c.energy_Eh << ",\"rho\":" << c.density_gcm3 << ","
           << "\"sym\":" << c.symmetry_score << ",\"phase\":\"" << c.phase_class << "\"}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// Gas2-driven frame generators
// ============================================================================

AtomicFrame gas2_atomic_frame(const gas2::GasSpecies& sp, double T_K,
                               uint64_t cycle, uint64_t seed, int n_atoms) {
    using namespace gas2;
    AtomicFrame f{};
    f.cycle      = cycle;
    f.seed       = seed;
    f.formula    = sp.formula;
    f.T_K        = T_K;
    f.n_atoms    = n_atoms;
    f.n_defects  = 0;
    f.focus_atom = static_cast<int>(cycle % n_atoms);
    f.run_mode_str = "sample";
    f.lattice.active = false;

    double M_kg  = sp.molar_mass_kg();
    double m_mol = M_kg / N_A;
    auto vels    = sample_mb(T_K, M_kg, n_atoms, seed + cycle);

    // Simple cubic lattice arrangement for display
    int side = static_cast<int>(std::ceil(std::cbrt(n_atoms)));
    double spacing = sp.d_kinetic_pm * 1e-10 * 1e10 * 3.5; // Angstrom, ~3.5 diameters
    if (spacing < 2.5) spacing = 2.5;

    double ke_sum = 0.0;
    f.atoms.reserve(n_atoms);
    for (int i = 0; i < n_atoms; ++i) {
        int ix = i % side;
        int iy = (i / side) % side;
        int iz = i / (side * side);
        double ke = 0.5 * m_mol * (vels[i].vx*vels[i].vx + vels[i].vy*vels[i].vy + vels[i].vz*vels[i].vz);
        ke_sum += ke;
        double ke_Eh = ke / Hartree_J;

        // Jitter position by thermal displacement
        std::mt19937 rng(seed + cycle + i);
        std::normal_distribution<double> jitter(0.0, spacing * 0.08);

        AtomRecord a{};
        a.id = i;
        a.Z  = (sp.n_atoms == 1) ? 18 : 7;   // noble -> Ar-like, else N-like
        a.symbol = sp.formula;
        a.x  = (ix - side/2.0) * spacing + jitter(rng);
        a.y  = (iy - side/2.0) * spacing + jitter(rng);
        a.z  = (iz - side/2.0) * spacing + jitter(rng);
        a.vx = vels[i].vx; a.vy = vels[i].vy; a.vz = vels[i].vz;
        a.q  = 0.0;
        a.state_tag = (i == f.focus_atom) ? 1 : 0;
        a.chi = ke_Eh / (avg_translational_ke_Eh(T_K) * 3.0 + 1e-30);
        if (a.chi > 1.0) a.chi = 1.0;
        a.ke_Eh = ke_Eh;
        elem_colour(a.Z, a.color_r, a.color_g, a.color_b);
        // Tint towards hot (red) based on KE
        a.color_r = std::min(1.0, a.color_r + a.chi * 0.4);
        a.color_b = std::max(0.0, a.color_b - a.chi * 0.4);
        f.atoms.push_back(a);
    }
    f.energy_Eh = ke_sum / Hartree_J;

    // Build neighbor bonds (< 1.5 * spacing)
    double bond_cutoff = spacing * 1.5;
    for (int i = 0; i < n_atoms; ++i) {
        for (int j = i+1; j < n_atoms; ++j) {
            double dx = f.atoms[j].x - f.atoms[i].x;
            double dy = f.atoms[j].y - f.atoms[i].y;
            double dz = f.atoms[j].z - f.atoms[i].z;
            double d  = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d < bond_cutoff) {
                BondRecord b{};
                b.i = i; b.j = j;
                b.weight = 1.0 - (d / bond_cutoff);
                b.type   = 1; // neighbor graph
                f.bonds.push_back(b);
            }
        }
    }

    // Phase guess from T/Tc
    double Tc = sp.Tc_K;
    if (T_K > 1.2 * Tc)   f.phase_guess = "gas";
    else if (T_K > 0.9 * Tc) f.phase_guess = "supercritical";
    else if (T_K > 0.5 * Tc) f.phase_guess = "vapor";
    else                   f.phase_guess = "liquid";

    f.convergence = std::abs(ke_sum / n_atoms / (1.5 * kB * T_K * N_A) - 1.0);
    return f;
}

AnalysisFrame gas2_analysis_frame(const gas2::Gas2Analysis& a,
                                   uint64_t cycle,
                                   const std::vector<double>& hist_E,
                                   const std::vector<double>& hist_T,
                                   const std::vector<double>& hist_rho) {
    using namespace gas2;
    AnalysisFrame f{};
    f.cycle    = cycle;
    f.formula  = a.formula;
    f.T_K      = a.T_K;
    f.v_rms    = a.v_rms;
    f.v_mean   = a.v_mean;
    f.v_mp     = a.v_mp;
    f.ke_avg_Eh = a.ke_translational_Eh;

    f.history_energy      = hist_E;
    f.history_T           = hist_T;
    f.history_density     = hist_rho;
    f.history_convergence.assign(hist_E.size(), 0.01);
    f.history_phi.assign(hist_E.size(), 0.5);
    f.history_bond_count.assign(hist_E.size(), 0.0);
    f.history_defect_count.assign(hist_E.size(), 0.0);
    f.history_lattice_fit.assign(hist_E.size(), 0.0);

    // Speed distribution from Maxwell-Boltzmann (64 bins)
    const int N_BINS = 64;
    f.speed_distribution.resize(N_BINS, 0.0);
    f.ke_distribution.resize(N_BINS, 0.0);
    double M_kg = a.species ? a.species->molar_mass_kg() : 0.04;
    double vmax = a.v_rms * 2.5;
    for (int b = 0; b < N_BINS; ++b) {
        double v = (b + 0.5) / N_BINS * vmax;
        // f(v) = 4*pi*(m/2pi*kT)^(3/2) * v^2 * exp(-mv^2/2kT)
        double m = M_kg / N_A;
        double alpha = m / (2.0 * kB * a.T_K);
        f.speed_distribution[b] = 4.0 * PI * std::pow(alpha / PI, 1.5)
                                   * v * v * std::exp(-alpha * v * v);
        double ke = 0.5 * m * v * v;
        double ke_Eh = ke / Hartree_J;
        f.ke_distribution[b] = f.speed_distribution[b] * ke_Eh;
    }

    // Crystal / lattice metrics (representative for gas phase)
    double Vm = a.eos_ideal.V_m3;
    double rho = (M_kg * N_A / Vm) * 1e-3;  // g/cm^3 (1e-6/1e-3)
    f.lattice_a = std::cbrt(Vm / N_A) * 1e10;  // Angstrom
    f.lattice_b = f.lattice_a;
    f.lattice_c = f.lattice_a;
    f.lattice_alpha = 90.0; f.lattice_beta = 90.0; f.lattice_gamma = 90.0;
    f.density_fit    = 1.0;
    f.coordination_avg = 12.0;
    f.space_group_guess = (a.species && a.species->n_atoms == 1) ? "Fm-3m" : "P21/c";

    // Properties (kinetic-theory-level estimates)
    f.properties.valid                = true;
    f.properties.density_gcm3        = rho;
    f.properties.bulk_modulus_GPa    = a.P_Pa / 1e9;
    f.properties.shear_modulus_GPa   = 0.0;
    f.properties.youngs_modulus_GPa  = 0.0;
    f.properties.poisson_ratio       = 0.0;
    f.properties.thermal_expansion_1e6K = 1.0 / a.T_K * 1e6;
    f.properties.debye_temperature_K = 0.0;
    f.properties.sound_speed_ms      = a.c_sound;
    f.properties.melt_temperature_K  = a.species ? a.species->Tc_K * 0.7 : 0.0;
    f.properties.thermal_conductivity_WmK = a.species ? a.species->k_thermal_mWmK * 1e-3 : 0.0;

    // Candidates: 3 EOS variants as candidate structures
    auto add_cand = [&](int id, const std::string& lbl, double V, double Z) {
        CandidateRecord c{};
        c.id = id; c.label = lbl;
        double rho_c = M_kg * N_A / V * 1e-3;
        c.density_gcm3  = rho_c;
        c.energy_Eh     = a.ke_translational_Eh;
        c.symmetry_score = Z;
        c.n_atoms        = a.species ? a.species->n_atoms : 1;
        c.phase_class    = "gas";
        f.candidates.push_back(c);
    };
    add_cand(0, "Ideal",      a.eos_ideal.V_m3, 1.0);
    add_cand(1, "VdW",        a.eos_vdw.V_m3,   a.eos_vdw.Z);
    add_cand(2, "Redlich-K",  a.eos_rk.V_m3,    a.eos_rk.Z);

    return f;
}

// ============================================================================
// VizServer — socket machinery
// ============================================================================

VizServer::VizServer(const VizServerConfig& cfg) : cfg_(cfg) {
    init_winsock();
    if (!cfg_.log_path.empty()) {
        log_file_.open(cfg_.log_path, std::ios::app);
        if (log_file_.is_open()) {
            std::cout << "  Log capture: " << cfg_.log_path << "\n";
        } else {
            std::cerr << "  WARNING: Could not open log file: " << cfg_.log_path << "\n";
        }
    }
}

VizServer::~VizServer() { stop(); }

bool VizServer::start() {
    if (running_.load()) return true;
    if (!init_socket(cfg_.port_atomic,   atomic_fd_))   return false;
    if (!init_socket(cfg_.port_analysis, analysis_fd_)) { cleanup_socket(atomic_fd_); return false; }
    running_.store(true);
    atomic_thread_   = std::thread([this]{ serve_loop(cfg_.port_atomic,   true);  });
    analysis_thread_ = std::thread([this]{ serve_loop(cfg_.port_analysis, false); });
    return true;
}

void VizServer::stop() {
    running_.store(false);
    cleanup_socket(atomic_fd_);
    cleanup_socket(analysis_fd_);
    if (atomic_thread_.joinable())   atomic_thread_.join();
    if (analysis_thread_.joinable()) analysis_thread_.join();
}

void VizServer::push_atomic(const AtomicFrame& frame) {
    std::lock_guard<std::mutex> lk(atomic_mtx_);
    latest_atomic_ = frame;
    has_atomic_ = true;
    ++atomic_count_;
}

void VizServer::push_analysis(const AnalysisFrame& frame) {
    std::lock_guard<std::mutex> lk(analysis_mtx_);
    latest_analysis_ = frame;
    has_analysis_ = true;
    ++analysis_count_;
    if (log_file_.is_open()) {
        log_file_ << frame.to_json() << "\n";
        log_file_.flush();
    }
}

bool VizServer::init_socket(int port, int& fd_out) {
#ifdef _WIN32
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
#endif
    int opt = 1;
#ifdef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) { CLOSE_SOCK(s); return false; }
    if (listen(s, 8) != 0) { CLOSE_SOCK(s); return false; }
    fd_out = static_cast<int>(s);
    return true;
}

void VizServer::cleanup_socket(int fd) {
    if (fd != -1) { CLOSE_SOCK(static_cast<sock_t>(fd)); }
}

void VizServer::serve_loop(int port, bool is_atomic) {
    (void)port;
    int server_fd = is_atomic ? atomic_fd_ : analysis_fd_;
    while (running_.load()) {
        struct timeval tv{0, 200000};  // 200 ms timeout
        fd_set fds; FD_ZERO(&fds); FD_SET(static_cast<sock_t>(server_fd), &fds);
        int sel = select(server_fd + 1, &fds, nullptr, nullptr, &tv);
        if (sel <= 0) continue;
        struct sockaddr_in caddr{}; socklen_t clen = sizeof(caddr);
        int cfd = static_cast<int>(accept(server_fd, (struct sockaddr*)&caddr, &clen));
        if (cfd < 0) continue;
        std::thread([this, cfd, is_atomic]{ serve_client(cfd, is_atomic); }).detach();
    }
}

void VizServer::serve_client(int client_fd, bool is_atomic) {
    int ms = is_atomic ? (1000 / std::max(1, cfg_.atomic_fps))
                       : (1000 / std::max(1, cfg_.analysis_fps));
    while (running_.load()) {
        std::string payload;
        if (is_atomic) {
            std::lock_guard<std::mutex> lk(atomic_mtx_);
            if (has_atomic_) payload = latest_atomic_.to_json() + "\n";
        } else {
            std::lock_guard<std::mutex> lk(analysis_mtx_);
            if (has_analysis_) payload = latest_analysis_.to_json() + "\n";
        }
        if (!payload.empty()) {
            const char* buf = payload.c_str();
            int total = static_cast<int>(payload.size());
            int sent  = 0;
            while (sent < total) {
                int r = send(static_cast<sock_t>(client_fd), buf + sent, total - sent, 0);
                if (r <= 0) goto done;
                sent += r;
            }
        }
#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
done:
    CLOSE_SOCK(static_cast<sock_t>(client_fd));
}

// ============================================================================
// CLI dispatch: vsepr viz [options]
// ============================================================================

int viz_dispatch(int argc, char** argv) {
    using namespace gas2;

    std::string formula = "Ar";
    double T_start = 200.0, T_end = 600.0;
    int    n_atoms = 64;
    int    n_frames = 0;       // 0 = infinite
    int    atomic_fps = 15;
    int    analysis_fps = 2;
    bool   verbose = false;
    std::string log_path = "viz_log.jsonl";  // default capture file

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-T" || a == "--temp") && i+1 < argc)         { T_start = std::stod(argv[++i]); T_end = T_start; }
        if ((a == "--T-start") && i+1 < argc)                   { T_start = std::stod(argv[++i]); }
        if ((a == "--T-end") && i+1 < argc)                     { T_end   = std::stod(argv[++i]); }
        if ((a == "-N" || a == "--atoms") && i+1 < argc)        { n_atoms = std::stoi(argv[++i]); }
        if ((a == "--frames") && i+1 < argc)                    { n_frames = std::stoi(argv[++i]); }
        if ((a == "--atomic-fps") && i+1 < argc)                { atomic_fps = std::stoi(argv[++i]); }
        if ((a == "--analysis-fps") && i+1 < argc)              { analysis_fps = std::stoi(argv[++i]); }
        if (a == "--verbose" || a == "-v")                      { verbose = true; }
        if ((a == "--log") && i+1 < argc)                       { log_path = argv[++i]; }
        if (a == "--no-log")                                    { log_path.clear(); }
        if (a[0] != '-' && i == 2)                              { formula = a; }
    }

    VizServerConfig cfg{};
    cfg.port_atomic   = 9999;
    cfg.port_analysis = 10001;
    cfg.atomic_fps    = atomic_fps;
    cfg.analysis_fps  = analysis_fps;
    cfg.verbose       = verbose;
    cfg.log_path      = log_path;

    VizServer server(cfg);
    if (!server.start()) {
        std::cerr << "ERROR: Failed to bind ports 9999 / 10001.\n";
        return 1;
    }

    std::cout << "\033[1;36m"
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║  ⚛  VSEPR-SIM Dual-Port Visualisation Stream Server     ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << "\033[0m"
              << "  Port 9999  → Atomic View    (" << atomic_fps   << " fps)\n"
              << "  Port 10001 → Analysis View  (" << analysis_fps << " fps)\n"
              << "  Formula: " << formula << "  T: " << T_start << " – " << T_end << " K\n"
              << "  Atoms: " << n_atoms << "\n\n"
              << "  Connect viewers:\n"
              << "    python tools/viz_atomic.py\n"
              << "    python tools/viz_analysis.py\n\n"
              << "  Press Ctrl+C to stop.\n\n";

    auto* sp = find_species(formula);
    if (!sp) {
        std::cerr << "Unknown species: " << formula
                  << "  (run 'vsepr gas2 species' for list)\n";
        server.stop(); return 1;
    }

    // History buffers for analysis frames
    const int HIST = 300;
    std::deque<double> hist_E, hist_T, hist_rho;

    uint64_t seed = 42;
    uint64_t cycle = 0;

    while (n_frames == 0 || (int)cycle < n_frames) {
        double progress = (n_frames > 1) ? ((double)cycle / (n_frames - 1)) : 0.0;
        double T_cur = T_start + (T_end - T_start) * progress;

        // --- Atomic frame ---
        AtomicFrame af = gas2_atomic_frame(*sp, T_cur, cycle, seed + cycle, n_atoms);
        server.push_atomic(af);

        // --- Analysis frame (every ~5 atomic frames) ---
        if (cycle % 5 == 0) {
            auto ga = analyze(formula, T_cur, 1.0, 1.0, seed + cycle);
            double M_kg = sp->molar_mass_kg();
            double Vm   = ga.eos_ideal.V_m3;
            double rho  = M_kg * N_A / Vm * 1e-3;

            hist_E.push_back(ga.ke_translational_Eh);
            hist_T.push_back(T_cur);
            hist_rho.push_back(rho);
            while ((int)hist_E.size() > HIST) { hist_E.pop_front(); hist_T.pop_front(); hist_rho.pop_front(); }

            std::vector<double> ve(hist_E.begin(), hist_E.end());
            std::vector<double> vt(hist_T.begin(), hist_T.end());
            std::vector<double> vr(hist_rho.begin(), hist_rho.end());

            AnalysisFrame anf = gas2_analysis_frame(ga, cycle, ve, vt, vr);
            server.push_analysis(anf);
        }

        if (verbose) {
            std::cout << "\r  cycle " << cycle << "  T=" << std::fixed << std::setprecision(1)
                      << T_cur << " K  atoms=" << n_atoms
                      << "  clients (atomic=" << server.atomic_frame_count()
                      << " analysis=" << server.analysis_frame_count() << ")"
                      << std::flush;
        }

        ++cycle;
        int sleep_ms = 1000 / std::max(1, atomic_fps);
#ifdef _WIN32
        Sleep(sleep_ms);
#else
        usleep(sleep_ms * 1000);
#endif
    }

    std::cout << "\n  Done. " << cycle << " frames published.\n";
    server.stop();
    return 0;
}

} // namespace viz
} // namespace vsepr
