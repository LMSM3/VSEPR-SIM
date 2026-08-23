/**
 * cmd_batch.cpp  -  vsepr batch inlet
 * ====================================
 * Resolves .vsim inputs, runs each through cmd_run_vsim(), reports timing,
 * and writes a JSON summary.  Supports parallel jobs via std::thread.
 *
 * WO-BATCH-INLET  |  v5.0.0
 */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif
#include "cli/cmd_batch.hpp"
#include "cli/cmd_batch_sweep.hpp"
#include "cli/cmd_run_vsim.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace vsepr::cli {

// ---------------------------------------------------------------------------
// Colour helpers (ANSI – disabled on Windows unless VT enabled)
// ---------------------------------------------------------------------------
namespace {
#ifdef _WIN32
    static bool vt_enabled = []() -> bool {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD m = 0;
        if (GetConsoleMode(h, &m)) {
            SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            return true;
        }
        return false;
    }();
#endif

const char* C_RESET  = "\033[0m";
const char* C_BOLD   = "\033[1m";
const char* C_DIM    = "\033[2m";
const char* C_GREEN  = "\033[0;32m";
const char* C_RED    = "\033[0;31m";
const char* C_YELLOW = "\033[1;33m";
const char* C_CYAN   = "\033[0;36m";
const char* C_WHITE  = "\033[0;37m";

// ---------------------------------------------------------------------------
// Job record
// ---------------------------------------------------------------------------
struct Job {
    std::string path;           // absolute .vsim path
    std::string label;          // display name (filename only)
    int         exit_code = -1;
    double      elapsed_ms = 0.0;
    bool        ran = false;
};

// ---------------------------------------------------------------------------
// Input resolution
// ---------------------------------------------------------------------------
static bool has_vsim_ext(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    return ext == ".vsim";
}

static void collect_from_dir(const std::string& dir, std::vector<std::string>& out)
{
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (e.is_regular_file(ec) && has_vsim_ext(e.path()))
            out.push_back(e.path().string());
    }
    std::sort(out.begin(), out.end());
}

static void collect_from_manifest(const std::string& file, std::vector<std::string>& out)
{
    std::ifstream in(file);
    if (!in) {
        std::cerr << "[batch] cannot open manifest: " << file << "\n";
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        // strip comments and whitespace
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        while (!line.empty() && std::isspace((unsigned char)line.back()))  line.pop_back();
        while (!line.empty() && std::isspace((unsigned char)line.front())) line.erase(line.begin());
        if (line.empty()) continue;
        out.push_back(line);
    }
}

static std::vector<Job> resolve_jobs(const std::vector<std::string>& raw)
{
    std::vector<Job> jobs;
    std::vector<std::string> paths;

    // Deduplicate while preserving insertion order
    auto add_path = [&](const std::string& p) {
        auto abs = fs::weakly_canonical(p).string();
        for (auto& existing : paths) if (existing == abs) return;
        paths.push_back(abs);
    };

    for (const auto& p : raw) add_path(p);

    for (auto& p : paths) {
        Job j;
        j.path  = p;
        j.label = fs::path(p).filename().string();
        jobs.push_back(std::move(j));
    }
    return jobs;
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
static std::string json_escape(const std::string& s)
{
    std::string r;
    for (char c : s) {
        if (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else r += c;
    }
    return r;
}

static void write_json_report(const std::string& path,
                               const std::vector<Job>& jobs,
                               const std::string& label,
                               double total_ms)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "[batch] cannot write report: " << path << "\n"; return; }

    int passed = 0, failed = 0, skipped = 0;
    for (auto& j : jobs) {
        if (!j.ran)            ++skipped;
        else if (j.exit_code == 0) ++passed;
        else                   ++failed;
    }

    auto now = std::time(nullptr);
    char tsbuf[32];
    std::strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

    f << "{\n";
    f << "  \"label\": \"" << json_escape(label) << "\",\n";
    f << "  \"timestamp\": \"" << tsbuf << "\",\n";
    f << "  \"total_ms\": " << std::fixed << std::setprecision(1) << total_ms << ",\n";
    f << "  \"summary\": { \"passed\": " << passed
      << ", \"failed\": " << failed
      << ", \"skipped\": " << skipped << " },\n";
    f << "  \"jobs\": [\n";
    for (size_t i = 0; i < jobs.size(); ++i) {
        const auto& j = jobs[i];
        f << "    { \"path\": \"" << json_escape(j.path) << "\""
          << ", \"label\": \"" << json_escape(j.label) << "\""
          << ", \"exit_code\": " << j.exit_code
          << ", \"elapsed_ms\": " << std::fixed << std::setprecision(1) << j.elapsed_ms
          << ", \"ran\": " << (j.ran ? "true" : "false")
          << " }";
        if (i + 1 < jobs.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
}

// ---------------------------------------------------------------------------
// Progress printer (mutex-guarded)
// ---------------------------------------------------------------------------
static std::mutex g_print_mtx;

static void print_job_result(size_t idx, size_t total,
                              const Job& j, bool quiet)
{
    if (quiet) return;
    std::lock_guard<std::mutex> lk(g_print_mtx);
    const char* status_c  = (j.exit_code == 0) ? C_GREEN : C_RED;
    const char* status_s  = (j.exit_code == 0) ? "PASS" : "FAIL";
    std::fprintf(stdout,
        "  %s[%zu/%zu]%s  %s%-4s%s  %s%.0fms%s  %s\n",
        C_DIM, idx, total, C_RESET,
        status_c, status_s, C_RESET,
        C_DIM, j.elapsed_ms, C_RESET,
        j.label.c_str());
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// Run a single job (called from worker thread or main thread)
// ---------------------------------------------------------------------------
static void run_job(Job& job)
{
    auto t0 = std::chrono::steady_clock::now();
    job.ran = true;
    job.exit_code = vsepr::cli::cmd_run_vsim({ job.path });
    auto t1 = std::chrono::steady_clock::now();
    job.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Help text
// ---------------------------------------------------------------------------
static void print_help(const char* prog)
{
    std::cout
        << C_BOLD << "vsepr-batch" << C_RESET
        << "  \u2014  .vsim batch inlet  (v5.0.0)\n\n"
        << C_BOLD << "Usage\n" << C_RESET
        << "  " << prog << " [options] [script.vsim ...]\n\n"
        << C_BOLD << "Input\n" << C_RESET
        << "  <script.vsim>          one or more .vsim paths (glob-expanded by shell)\n"
        << "  --manifest <file>      text file, one .vsim path per line  (# = comment)\n"
        << "  --dir      <path>      recurse for *.vsim files under <path>\n\n"
        << C_BOLD << "Options\n" << C_RESET
        << "  --jobs     N           parallel worker threads  (default: 1)\n"
        << "  --stop-on-fail         abort remaining jobs after first failure\n"
        << "  --dry-run              print job list without executing\n"
        << "  --report   <path>      write JSON summary to <path>\n"
        << "  --label    <text>      tag printed in the summary header\n"
        << "  --verbose  / -v        echo each job's full output\n"
        << "  --quiet    / -q        suppress per-job progress lines\n"
        << "  --help     / -h        print this help\n\n"
        << C_BOLD << "Exit codes\n" << C_RESET
        << "  0   all jobs passed\n"
        << "  1   one or more jobs failed\n"
        << "  2   bad arguments / no jobs found\n\n"
        << C_BOLD << "Examples\n" << C_RESET
        << "  " << prog << " scripts/demo_01_nacl.vsim\n"
        << "  " << prog << " --dir scripts/ --jobs 4 --report out/batch.json\n"
        << "  " << prog << " --manifest ci/nightly.txt --stop-on-fail\n"
        << "  " << prog << " *.vsim --dry-run\n";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
int cmd_batch(const std::vector<std::string>& args)
{
    // ---- parse arguments ---------------------------------------------------
    struct Opts {
        std::vector<std::string> positionals;
        std::string  manifest;
        std::string  dir;
        std::string  report_path;
        std::string  label = "vsepr-batch";
        int          jobs      = 1;
        bool         stop_fail = false;
        bool         dry_run   = false;
        bool         verbose   = false;
        bool         quiet     = false;
    } opts;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        auto next = [&]() -> std::string {
            if (i + 1 < args.size()) return args[++i];
            std::cerr << "[batch] " << a << " requires an argument\n";
            std::exit(2);
        };

        if (a == "--help" || a == "-h") {
            print_help("vsepr-batch");
            return 0;
        } else if (a == "--manifest")     opts.manifest    = next();
        else if (a == "--dir")            opts.dir         = next();
        else if (a == "--report")         opts.report_path = next();
        else if (a == "--label")          opts.label       = next();
        else if (a == "--jobs")           opts.jobs        = std::stoi(next());
        else if (a == "--stop-on-fail")   opts.stop_fail   = true;
        else if (a == "--dry-run")        opts.dry_run     = true;
        else if (a == "--verbose" || a == "-v") opts.verbose = true;
        else if (a == "--quiet"   || a == "-q") opts.quiet   = true;
        // ---- sweep passthrough detection ----
        else if (a == "--base" || a == "--sweep-config" || a == "--axis" ||
                 a == "--sweep-design" || a == "--sweep-out" || a == "--sweep-seed" ||
                 a == "--replicates" || a == "--max-jobs" || a == "--keep-generated") {
            // Forward entire arg list to the sweep engine
            return vsepr::cli::cmd_batch_sweep(args);
        }
        else if (!a.empty() && a[0] != '-')      opts.positionals.push_back(a);
        else {
            std::cerr << "[batch] unknown option: " << a << "\n";
            return 2;
        }
    }

    if (opts.jobs < 1)  opts.jobs = 1;
    if (opts.jobs > 64) opts.jobs = 64;

    // ---- collect raw paths -------------------------------------------------
    std::vector<std::string> raw;
    for (auto& p : opts.positionals) raw.push_back(p);
    if (!opts.manifest.empty()) collect_from_manifest(opts.manifest, raw);
    if (!opts.dir.empty())      collect_from_dir(opts.dir, raw);

    if (raw.empty()) {
        std::cerr << "[batch] no .vsim scripts found — use --help for usage\n";
        return 2;
    }

    // ---- resolve to job list -----------------------------------------------
    auto jobs = resolve_jobs(raw);

    // ---- header ------------------------------------------------------------
    if (!opts.quiet) {
        std::cout << C_BOLD << "VSEPR Batch Inlet" << C_RESET
                  << "  [" << opts.label << "]"
                  << "  " << jobs.size() << " job" << (jobs.size() != 1 ? "s" : "")
                  << "  threads=" << opts.jobs
                  << "\n" << std::string(60, '-') << "\n";
    }

    // ---- dry run -----------------------------------------------------------
    if (opts.dry_run) {
        for (size_t i = 0; i < jobs.size(); ++i)
            std::cout << "  [" << (i+1) << "] " << jobs[i].path << "\n";
        std::cout << "(dry-run: no execution)\n";
        return 0;
    }

    // ---- execute -----------------------------------------------------------
    auto wall_t0 = std::chrono::steady_clock::now();
    std::atomic<bool> abort_flag{false};
    std::atomic<size_t> next_job{0};
    const size_t total = jobs.size();

    auto worker = [&]() {
        while (true) {
            if (abort_flag.load()) break;
            size_t idx = next_job.fetch_add(1);
            if (idx >= total) break;

            auto& j = jobs[idx];
            if (!opts.verbose) {
                // suppress child stdout during batch run (capture via redirect)
            }
            run_job(j);
            print_job_result(idx + 1, total, j, opts.quiet);

            if (j.exit_code != 0 && opts.stop_fail)
                abort_flag.store(true);
        }
    };

    if (opts.jobs == 1) {
        worker();
    } else {
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(opts.jobs));
        for (int t = 0; t < opts.jobs; ++t)
            threads.emplace_back(worker);
        for (auto& t : threads) t.join();
    }

    auto wall_t1 = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_t1 - wall_t0).count();

    // Mark any skipped jobs (stop-on-fail cut them off)
    for (auto& j : jobs) if (!j.ran) j.exit_code = -1;

    // ---- summary -----------------------------------------------------------
    int passed = 0, failed = 0, skipped = 0;
    for (auto& j : jobs) {
        if (!j.ran)            ++skipped;
        else if (j.exit_code == 0) ++passed;
        else                   ++failed;
    }

    if (!opts.quiet) {
        std::cout << std::string(60, '-') << "\n";
        std::printf(
            "  %s%-7s%s  %s%-7s%s  %s%-7s%s  wall %.0fms\n",
            C_GREEN,  (std::to_string(passed)  + " pass").c_str(),  C_RESET,
            C_RED,    (std::to_string(failed)  + " fail").c_str(),  C_RESET,
            C_YELLOW, (std::to_string(skipped) + " skip").c_str(),  C_RESET,
            wall_ms);

        if (failed > 0) {
            std::cout << "\n" << C_RED << C_BOLD << "Failed jobs:\n" << C_RESET;
            for (auto& j : jobs)
                if (j.ran && j.exit_code != 0)
                    std::cout << "  " << j.path << "\n";
        }
        std::cout << "\n";
    }

    // ---- JSON report -------------------------------------------------------
    if (!opts.report_path.empty())
        write_json_report(opts.report_path, jobs, opts.label, wall_ms);

    return (failed > 0) ? 1 : 0;
}

} // namespace vsepr::cli