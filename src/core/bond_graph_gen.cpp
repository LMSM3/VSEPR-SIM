/**
 * bond_graph_gen.cpp
 * ------------------
 * C++ backend: random molecule generation, covalent-radius bond detection,
 * JSON serialisation, and process launchers for the bond-graph web viewer.
 *
 * Replaces tools/bond_graph_randomizer.py and the orchestration logic in
 * tools/bond_graph_auto.py entirely.  Python files are retained as thin
 * dev-convenience wrappers only.
 *
 * VSEPR-SIM v5.1.4  |  WO-AUTO-01
 */

#include "core/bond_graph_gen.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <sys/wait.h>
#endif

namespace vsepr {
namespace bond_graph {

// ============================================================================
// Element data  (symbol, Z, covalent radius Å, typical valence)
// ============================================================================

struct ElemData {
    const char* sym;
    int         Z;
    double      cov_rad;  // Angstrom
    int         valence;
};

static constexpr ElemData ELEMENTS[] = {
    {"H",   1, 0.31, 1},
    {"C",   6, 0.76, 4},
    {"N",   7, 0.71, 3},
    {"O",   8, 0.66, 2},
    {"F",   9, 0.57, 1},
    {"S",  16, 1.05, 2},
    {"Cl", 17, 1.02, 1},
    {"P",  15, 1.07, 3},
    {"Na", 11, 1.66, 1},
    {"Mg", 12, 1.41, 2},
    {"Ca", 20, 1.76, 2},
    {"Fe", 26, 1.32, 3},
    {"Zn", 30, 1.22, 2},
    {"Br", 35, 1.20, 1},
    {"I",  53, 1.39, 1},
    {"Si", 14, 1.11, 4},
    {"B",   5, 0.84, 3},
    {"Al", 13, 1.21, 3},
    {"Ar", 18, 1.06, 0},
    {"Kr", 36, 1.16, 0},
};
static constexpr int N_ELEMENTS = static_cast<int>(sizeof(ELEMENTS)/sizeof(ELEMENTS[0]));

static double cov_radius(int Z) {
    for (int i = 0; i < N_ELEMENTS; ++i)
        if (ELEMENTS[i].Z == Z) return ELEMENTS[i].cov_rad;
    return 0.77;
}

static double cov_radius_sym(const std::string& sym) {
    for (int i = 0; i < N_ELEMENTS; ++i)
        if (sym == ELEMENTS[i].sym) return ELEMENTS[i].cov_rad;
    return 0.77;
}

static int Z_for_sym(const std::string& sym) {
    for (int i = 0; i < N_ELEMENTS; ++i)
        if (sym == ELEMENTS[i].sym) return ELEMENTS[i].Z;
    return 0;
}

// ============================================================================
// JSON helpers
// ============================================================================

static std::string json_str(const std::string& s) {
    // Simple escaping — no Unicode transforms needed here
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else           out += c;
    }
    out += '"';
    return out;
}

// ============================================================================
// BondGraphFrame::to_json
// ============================================================================

std::string BondGraphFrame::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);

    ss << "{\"frame_id\":" << frame_id
       << ",\"formula\":" << json_str(formula)
       << ",\"source\":"  << json_str(source)
       << ",\"n_atoms\":" << atoms.size()
       << ",\"n_bonds\":" << bonds.size()
       << ",\"atoms\":[";
    for (size_t i = 0; i < atoms.size(); ++i) {
        if (i) ss << ',';
        const auto& a = atoms[i];
        ss << "{\"sym\":" << json_str(a.symbol)
           << ",\"Z\":"   << a.Z
           << ",\"x\":"   << a.x
           << ",\"y\":"   << a.y
           << ",\"z\":"   << a.z
           << "}";
    }
    ss << "],\"bonds\":[";
    for (size_t i = 0; i < bonds.size(); ++i) {
        if (i) ss << ',';
        const auto& b = bonds[i];
        ss << "{\"a\":" << b.a
           << ",\"b\":" << b.b
           << ",\"len\":" << b.length_ang
           << ",\"order\":" << b.order
           << "}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================================
// BondGraphGenerator
// ============================================================================

BondGraphGenerator::BondGraphGenerator(uint64_t seed)
    : seed_(seed == 0 ? static_cast<uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count())
                      : seed)
{}

// xorshift64
uint64_t BondGraphGenerator::next_rand() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 7;
    seed_ ^= seed_ << 17;
    return seed_;
}

// Random double in [0, 1)
static double rnd01(uint64_t r) {
    return static_cast<double>(r >> 11) / static_cast<double>(1ULL << 53);
}

// ── Random frame ──────────────────────────────────────────────────────────

BondGraphFrame BondGraphGenerator::random_frame(uint64_t frame_id) {
    // Choose a random molecule template
    static const struct { const char* formula; int n; int Zs[12]; } TEMPLATES[] = {
        {"H2O",    3, {8, 1, 1}},
        {"CH4",    5, {6, 1, 1, 1, 1}},
        {"CO2",    3, {6, 8, 8}},
        {"NH3",    4, {7, 1, 1, 1}},
        {"C2H6",   8, {6, 6, 1, 1, 1, 1, 1, 1}},
        {"C6H6",  12, {6, 6, 6, 6, 6, 6, 1, 1, 1, 1, 1, 1}},
        {"HCl",    2, {1, 17}},
        {"N2",     2, {7, 7}},
        {"O2",     2, {8, 8}},
        {"SO2",    3, {16, 8, 8}},
        {"CH3OH",  6, {6, 8, 1, 1, 1, 1}},
        {"SF6",    7, {16, 9, 9, 9, 9, 9, 9}},
    };
    static constexpr int N_TEMPLATES = static_cast<int>(sizeof(TEMPLATES)/sizeof(TEMPLATES[0]));

    int ti = static_cast<int>(next_rand() % static_cast<uint64_t>(N_TEMPLATES));
    const auto& tmpl = TEMPLATES[ti];

    std::vector<BondAtom> atoms;
    atoms.reserve(tmpl.n);

    // Place central atom at origin, others randomly within bond-length range
    for (int i = 0; i < tmpl.n; ++i) {
        BondAtom a;
        a.Z = tmpl.Zs[i];
        // find symbol
        for (int e = 0; e < N_ELEMENTS; ++e)
            if (ELEMENTS[e].Z == a.Z) { a.symbol = ELEMENTS[e].sym; break; }
        if (a.symbol.empty()) a.symbol = "X";

        if (i == 0) {
            a.x = a.y = a.z = 0.0;
        } else {
            double r = cov_radius(tmpl.Zs[0]) + cov_radius(a.Z);
            r += 0.2 * rnd01(next_rand());
            double theta = rnd01(next_rand()) * 3.14159265358979323846;
            double phi   = rnd01(next_rand()) * 6.28318530717958647692;
            a.x = r * std::sin(theta) * std::cos(phi);
            a.y = r * std::sin(theta) * std::sin(phi);
            a.z = r * std::cos(theta);
        }
        atoms.push_back(a);
    }

    return from_atoms(atoms, frame_id, "random");
}

// ── from_atoms ────────────────────────────────────────────────────────────

BondGraphFrame BondGraphGenerator::from_atoms(const std::vector<BondAtom>& atoms,
                                               uint64_t frame_id,
                                               const std::string& source) {
    BondGraphFrame f;
    f.frame_id = frame_id;
    f.source   = source;
    f.atoms    = atoms;
    f.bonds    = detect_bonds(atoms);
    f.formula  = derive_formula(atoms);
    return f;
}

// ── from_xyz_string ───────────────────────────────────────────────────────

BondGraphFrame BondGraphGenerator::from_xyz_string(const std::string& xyz,
                                                    uint64_t frame_id) {
    std::vector<BondAtom> atoms;
    std::istringstream ss(xyz);
    std::string line;

    int n_atoms = 0;
    int line_idx = 0;
    while (std::getline(ss, line)) {
        if (line_idx == 0) {
            try { n_atoms = std::stoi(line); } catch(...) {}
            ++line_idx; continue;
        }
        if (line_idx == 1) { ++line_idx; continue; } // skip comment
        if (line.empty()) { ++line_idx; continue; }

        std::istringstream ls(line);
        BondAtom a;
        if (ls >> a.symbol >> a.x >> a.y >> a.z) {
            a.Z = Z_for_sym(a.symbol);
            atoms.push_back(a);
        }
        ++line_idx;
        if (n_atoms > 0 && static_cast<int>(atoms.size()) >= n_atoms) break;
    }

    return from_atoms(atoms, frame_id, "xyz");
}

// ── detect_bonds ──────────────────────────────────────────────────────────

std::vector<BondEdge>
BondGraphGenerator::detect_bonds(const std::vector<BondAtom>& atoms,
                                  double tolerance) {
    std::vector<BondEdge> bonds;
    int n = static_cast<int>(atoms.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dx = atoms[j].x - atoms[i].x;
            double dy = atoms[j].y - atoms[i].y;
            double dz = atoms[j].z - atoms[i].z;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            double thresh = (cov_radius_sym(atoms[i].symbol)
                           + cov_radius_sym(atoms[j].symbol)) * tolerance;
            if (dist > 0.4 && dist <= thresh) {
                BondEdge e;
                e.a = i; e.b = j; e.length_ang = dist;
                // Rough order heuristic from distance ratio
                double ratio = dist / (cov_radius_sym(atoms[i].symbol)
                                     + cov_radius_sym(atoms[j].symbol));
                e.order = (ratio < 0.80) ? 3 : (ratio < 0.90) ? 2 : 1;
                bonds.push_back(e);
            }
        }
    }
    return bonds;
}

// ── derive_formula ────────────────────────────────────────────────────────

std::string BondGraphGenerator::derive_formula(const std::vector<BondAtom>& atoms) {
    std::map<std::string, int> counts;
    for (const auto& a : atoms) counts[a.symbol]++;

    std::string out;
    // Hill order: C first, H second, then alphabetical
    auto emit = [&](const std::string& sym) {
        auto it = counts.find(sym);
        if (it == counts.end()) return;
        out += sym;
        if (it->second > 1) out += std::to_string(it->second);
        counts.erase(it);
    };
    emit("C"); emit("H");
    for (const auto& [sym, cnt] : counts) {
        out += sym;
        if (cnt > 1) out += std::to_string(cnt);
    }
    return out.empty() ? "?" : out;
}

// ============================================================================
// Launcher helpers
// ============================================================================

// ── TCP probe ────────────────────────────────────────────────────────────

static bool port_open(const char* host, int port) {
#ifdef _WIN32
    static bool wsa_init = false;
    if (!wsa_init) {
        WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
        wsa_init = true;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in sa{}; sa.sin_family = AF_INET;
    sa.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host, &sa.sin_addr);
    // 500 ms timeout
    DWORD tv = 500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    bool ok = (connect(s, (sockaddr*)&sa, sizeof(sa)) == 0);
    closesocket(s);
    return ok;
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_in sa{}; sa.sin_family = AF_INET;
    sa.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host, &sa.sin_addr);
    bool ok = (::connect(s, (sockaddr*)&sa, sizeof(sa)) == 0);
    ::close(s);
    return ok;
#endif
}

// ── launch_viz_web ────────────────────────────────────────────────────────

uint32_t launch_viz_web(const std::string& tools_dir, int port) {
    std::string script = tools_dir + "/viz_web.py";

#ifdef _WIN32
    std::string params = "\"" + script + "\" --http-port " + std::to_string(port);
    SHELLEXECUTEINFOA sei{};
    sei.cbSize      = sizeof(sei);
    sei.fMask       = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb      = "open";
    sei.lpFile      = "python";
    sei.lpParameters = params.c_str();
    sei.nShow       = SW_HIDE;
    if (!ShellExecuteExA(&sei)) return 0;
    uint32_t pid = GetProcessId(sei.hProcess);
    CloseHandle(sei.hProcess);
    return pid;
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child
        execlp("python3", "python3", script.c_str(),
               "--http-port", std::to_string(port).c_str(), nullptr);
        execlp("python", "python", script.c_str(),
               "--http-port", std::to_string(port).c_str(), nullptr);
        _exit(1);
    }
    if (pid < 0) return 0;
    return static_cast<uint32_t>(pid);
#endif
}

// ── open_graph_browser ────────────────────────────────────────────────────

void open_graph_browser(const std::string& host, int port, int timeout_ms) {
    std::string url = "http://" + host + ":" + std::to_string(port) + "/graph";

    // Poll until server responds or timeout
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (port_open(host.c_str(), port)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        elapsed += 400;
    }

#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    // Try xdg-open, then open (macOS fallback)
    if (fork() == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
        execlp("open",     "open",     url.c_str(), nullptr);
        _exit(0);
    }
#endif
}

} // namespace bond_graph
} // namespace vsepr