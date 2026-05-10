#pragma once
/**
 * vsim_output_layout.hpp
 * ----------------------
 * Canonical folder-as-project and numbered run output convention.
 *
 * Layout:
 *   <project_dir>/
 *     main.vsim             <- primary script (B10-13)
 *     output/
 *       run_001/            <- first run (B10-14)
 *         run_manifest.json
 *         trajectory.xyzFull
 *         report.md
 *         ...
 *       run_002/
 *         ...
 *
 * Runtime config paths (B10-15):
 *   %LOCALAPPDATA%\VSEPR-SIM\config\   <- user config
 *   %LOCALAPPDATA%\VSEPR-SIM\cache\    <- computed cache
 *   %LOCALAPPDATA%\VSEPR-SIM\logs\     <- run/error logs
 *
 * B10-13, B10-14, B10-15 | v5.0.0-beta.10
 */

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>

namespace fs = std::filesystem;

namespace vsim {

// ============================================================================
// RunPaths — resolved paths for a single numbered run
// ============================================================================

struct RunPaths {
	fs::path run_dir;         // output/run_NNN/
	fs::path manifest;        // output/run_NNN/run_manifest.json
	fs::path trajectory;      // output/run_NNN/trajectory.xyzFull
	fs::path report;          // output/run_NNN/report.md
	fs::path log;             // output/run_NNN/run.log
	int      run_number = 0;
};

// ============================================================================
// AppDataPaths — standardised AppData layout (B10-15)
// ============================================================================

struct AppDataPaths {
	fs::path root;    // %LOCALAPPDATA%\VSEPR-SIM\  (or ~/vsepr-sim/)
	fs::path config;  // root/config/
	fs::path cache;   // root/cache/
	fs::path logs;    // root/logs/
	fs::path examples; // root/examples/
	fs::path bin;     // root/bin/
};

// ============================================================================
// OutputLayout — utilities for creating and resolving project output structure
// ============================================================================

class OutputLayout {
public:
	// Resolve (and optionally create) AppData paths.
	static AppDataPaths app_data_paths(bool create_dirs = false) {
		AppDataPaths p;
#ifdef _WIN32
		const char* lad = std::getenv("LOCALAPPDATA");
		p.root = lad ? fs::path(lad) / "VSEPR-SIM" : fs::path("vsepr-sim");
#else
		const char* home = std::getenv("HOME");
		p.root = home ? fs::path(home) / ".vsepr-sim" : fs::path("vsepr-sim");
#endif
		p.config   = p.root / "config";
		p.cache    = p.root / "cache";
		p.logs     = p.root / "logs";
		p.examples = p.root / "examples";
		p.bin      = p.root / "bin";

		if (create_dirs) {
			fs::create_directories(p.config);
			fs::create_directories(p.cache);
			fs::create_directories(p.logs);
		}
		return p;
	}

	// Find the next available run_NNN number in <project_dir>/output/.
	static int next_run_number(const fs::path& project_dir) {
		fs::path output = project_dir / "output";
		if (!fs::exists(output)) return 1;
		int max_n = 0;
		for (auto& e : fs::directory_iterator(output)) {
			if (!e.is_directory()) continue;
			std::string name = e.path().filename().string();
			if (name.size() > 4 && name.substr(0, 4) == "run_") {
				try {
					int n = std::stoi(name.substr(4));
					if (n > max_n) max_n = n;
				} catch (...) {}
			}
		}
		return max_n + 1;
	}

	// Create the next numbered run folder and return resolved paths.
	static RunPaths create_run_folder(const fs::path& project_dir) {
		int n = next_run_number(project_dir);
		std::ostringstream ss;
		ss << "run_" << std::setw(3) << std::setfill('0') << n;

		RunPaths rp;
		rp.run_number = n;
		rp.run_dir    = project_dir / "output" / ss.str();
		rp.manifest   = rp.run_dir / "run_manifest.json";
		rp.trajectory = rp.run_dir / "trajectory.xyzFull";
		rp.report     = rp.run_dir / "report.md";
		rp.log        = rp.run_dir / "run.log";

		fs::create_directories(rp.run_dir);
		write_manifest(rp, project_dir);
		return rp;
	}

	// Resolve paths for an existing run_NNN folder (no creation).
	static RunPaths resolve_run(const fs::path& run_dir) {
		RunPaths rp;
		rp.run_dir    = run_dir;
		rp.manifest   = run_dir / "run_manifest.json";
		rp.trajectory = run_dir / "trajectory.xyzFull";
		rp.report     = run_dir / "report.md";
		rp.log        = run_dir / "run.log";
		std::string name = run_dir.filename().string();
		if (name.size() > 4)
			try { rp.run_number = std::stoi(name.substr(4)); } catch (...) {}
		return rp;
	}

private:
	static void write_manifest(const RunPaths& rp, const fs::path& project_dir) {
		std::ofstream f(rp.manifest);
		if (!f.is_open()) return;

		// ISO timestamp
		std::time_t t = std::time(nullptr);
		char tbuf[32] = {};
		std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));

		f << "{\n"
		  << "  \"vsepr_sim_version\": \"5.0.0-beta.10\",\n"
		  << "  \"run_number\": " << rp.run_number << ",\n"
		  << "  \"created_utc\": \"" << tbuf << "\",\n"
		  << "  \"project_dir\": " << "\"" << project_dir.string() << "\",\n"
		  << "  \"run_dir\": \"" << rp.run_dir.string() << "\",\n"
		  << "  \"files\": {\n"
		  << "    \"trajectory\": \"trajectory.xyzFull\",\n"
		  << "    \"report\": \"report.md\",\n"
		  << "    \"log\": \"run.log\"\n"
		  << "  }\n"
		  << "}\n";
	}
};

} // namespace vsim
