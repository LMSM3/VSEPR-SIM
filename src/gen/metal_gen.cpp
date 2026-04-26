/**
 * metal_gen.cpp -- Metal/alloy supercell generator (beta.7 attempt)
 * ==================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Drives the full artifact pipeline for each material preset:
 *
 *   <tag>.xyz    — static geometry     (write_xyz)
 *   <tag>.xyza   — + charge/vel/e cols (write_xyza)
 *   <tag>.xyzc   — checkpoint frame    (write_xyzc)
 *   <tag>.xyzf   — 3-frame trajectory  (write_xyzf)
 *   <tag>.step   — AP203 geometry truth (write_step_ap203)
 *   geometry_map.json — provenance manifest
 *
 * Build as a standalone executable (no CMake integration needed for the
 * attempt branch):
 *
 *   g++ -std=c++20 -O2 -I../../ -o metal_gen metal_gen.cpp
 *
 * Run:
 *   ./metal_gen [output_dir]   (default: ./gen_out)
 */

// ---- project helpers (relative to src/gen/) ----------------------------
#include "metal_presets.hpp"
#include "lattice_builder.hpp"
#include "step_writer.hpp"

// ---- project I/O --------------------------------------------------------
#include "../../src/io/xyz_writer.hpp"    // write_xyz, write_xyza, write_xyzc, write_xyzf

// ---- stdlib -------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;
using namespace vsepr;
using namespace vsepr::gen;
using namespace vsepr::io;

// ---------------------------------------------------------------------------
// Supercell size for demo output (3×3×3 gives a tractable atom count)
// ---------------------------------------------------------------------------
static constexpr int NX = 3, NY = 3, NZ = 3;

// ---------------------------------------------------------------------------
// JSON manifest helper — builds a geometry_map.json entry for one material
// ---------------------------------------------------------------------------
static std::string json_entry(const MaterialPreset& mat,
							   int n_atoms,
							   const std::string& out_dir)
{
	std::ostringstream j;
	j << "  {\n";
	j << "    \"tag\": \""         << mat.tag         << "\",\n";
	j << "    \"name\": \""        << mat.name        << "\",\n";
	j << "    \"composition\": \"" << mat.composition << "\",\n";
	j << "    \"lattice\": \"";
	switch (mat.lattice) {
		case LatticeType::BCC: j << "BCC"; break;
		case LatticeType::FCC: j << "FCC"; break;
		case LatticeType::HCP: j << "HCP"; break;
		case LatticeType::B2:  j << "B2";  break;
	}
	j << "\",\n";
	j << "    \"a_ang\": "         << std::fixed << std::setprecision(4) << mat.a  << ",\n";
	j << "    \"c_ang\": "         << std::fixed << std::setprecision(4) << mat.c  << ",\n";
	j << "    \"density_gcc\": "   << mat.density_gcc << ",\n";
	j << "    \"supercell\": \""   << NX << "x" << NY << "x" << NZ << "\",\n";
	j << "    \"n_atoms\": "       << n_atoms << ",\n";
	j << "    \"artifacts\": {\n";
	j << "      \"xyz\":  \"" << mat.tag << ".xyz\",\n";
	j << "      \"xyza\": \"" << mat.tag << ".xyza\",\n";
	j << "      \"xyzc\": \"" << mat.tag << ".xyzc\",\n";
	j << "      \"xyzf\": \"" << mat.tag << ".xyzf\",\n";
	j << "      \"step\": \"" << mat.tag << ".step\"\n";
	j << "    }\n";
	j << "  }";
	return j.str();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	std::string out_dir = (argc > 1) ? argv[1] : "gen_out";

	// Create output directory
	std::error_code ec;
	fs::create_directories(out_dir, ec);
	if (ec) {
		std::cerr << "[metal_gen] Cannot create output directory: "
				  << out_dir << " — " << ec.message() << '\n';
		return 1;
	}

	std::cout << "[metal_gen] Output → " << fs::absolute(out_dir).string() << '\n';

	auto presets = default_presets();

	XYZWriterConfig cfg;
	cfg.coord_precision  = 6;
	cfg.prop_precision   = 6;
	cfg.write_charge     = true;
	cfg.write_velocity   = true;
	cfg.write_force      = false;   // no force data at build time
	cfg.write_energy_col = true;

	std::vector<std::string> manifest_entries;

	for (const auto& mat : presets) {
		std::cout << "\n[" << mat.tag << "] " << mat.name
				  << "  lattice=" << [&]{
						switch(mat.lattice){
							case LatticeType::BCC: return "BCC";
							case LatticeType::FCC: return "FCC";
							case LatticeType::HCP: return "HCP";
							case LatticeType::B2:  return "B2";
						}
						return "?";
					}()
				  << "  a=" << mat.a << " Å\n";

		// ----------------------------------------------------------------
		// 1. Build supercell
		// ----------------------------------------------------------------
		XYZFrame frame = build_supercell(mat, NX, NY, NZ, /*apply_cycle=*/true);

		// Set a descriptive comment for the frame
		{
			std::ostringstream cmt;
			cmt << mat.name << " | " << mat.composition
				<< " | supercell " << NX << "x" << NY << "x" << NZ
				<< " | E = " << std::fixed << std::setprecision(2)
				<< *frame.energy << " kcal/mol"
				<< " | T = " << *frame.temperature << " K";
			frame.comment = cmt.str();
		}

		std::cout << "  atoms: " << frame.N << '\n';

		auto p = [&](const std::string& ext) {
			return (fs::path(out_dir) / (mat.tag + ext)).string();
		};

		// ----------------------------------------------------------------
		// 2. .xyz — coordinates only
		// ----------------------------------------------------------------
		if (write_xyz(p(".xyz"), frame, cfg))
			std::cout << "  wrote " << mat.tag << ".xyz\n";
		else
			std::cerr << "  ERROR writing .xyz\n";

		// ----------------------------------------------------------------
		// 3. .xyza — + charge, velocity, energy columns
		// ----------------------------------------------------------------
		if (write_xyza(p(".xyza"), frame, cfg))
			std::cout << "  wrote " << mat.tag << ".xyza\n";
		else
			std::cerr << "  ERROR writing .xyza\n";

		// ----------------------------------------------------------------
		// 4. .xyzc — checkpoint header + xyza frame
		// ----------------------------------------------------------------
		{
			XYZData data;
			data.frames.push_back(frame);

			CheckpointState ck;
			ck.step     = 0;
			ck.time     = 0.0;
			ck.dt       = 1.0;   // 1 fs
			ck.T_target = 300.0;
			ck.seed     = 42;
			ck.box      = frame.box;
			data.checkpoint = ck;

			if (write_xyzc(p(".xyzc"), data, cfg))
				std::cout << "  wrote " << mat.tag << ".xyzc\n";
			else
				std::cerr << "  ERROR writing .xyzc\n";
		}

		// ----------------------------------------------------------------
		// 5. .xyzf — 3-frame thermal trajectory
		// ----------------------------------------------------------------
		{
			auto traj = build_thermal_trajectory(mat, NX, NY, NZ,
												  {0.0, 300.0, 600.0});
			if (write_xyzf(p(".xyzf"), traj, /*append=*/false, cfg))
				std::cout << "  wrote " << mat.tag << ".xyzf  ("
						  << traj.size() << " frames)\n";
			else
				std::cerr << "  ERROR writing .xyzf\n";
		}

		// ----------------------------------------------------------------
		// 6. .step — STEP AP203 geometry artifact
		// ----------------------------------------------------------------
		{
			std::string step_name = mat.name + " " + mat.tag
				+ " " + std::to_string(NX) + "x"
				+ std::to_string(NY) + "x"
				+ std::to_string(NZ);
			if (write_step_ap203(p(".step"), frame, step_name))
				std::cout << "  wrote " << mat.tag << ".step\n";
			else
				std::cerr << "  ERROR writing .step\n";
		}

		manifest_entries.push_back(json_entry(mat, frame.N, out_dir));
	}

	// ----------------------------------------------------------------
	// 7. geometry_map.json — provenance manifest
	// ----------------------------------------------------------------
	{
		auto map_path = (fs::path(out_dir) / "geometry_map.json").string();
		std::ofstream jf(map_path);
		if (jf) {
			jf << "{\n";
			jf << "  \"generator\": \"VSEPR-SIM metal_gen\",\n";
			jf << "  \"branch\": \"v5.0.0-beta.7-step-attempt\",\n";
			jf << "  \"supercell\": \"" << NX << "x" << NY << "x" << NZ << "\",\n";
			jf << "  \"unit_coords\": \"angstrom\",\n";
			jf << "  \"unit_energy\": \"kcal/mol\",\n";
			jf << "  \"unit_velocity\": \"angstrom/fs\",\n";
			jf << "  \"materials\": [\n";
			for (int i = 0; i < static_cast<int>(manifest_entries.size()); ++i) {
				jf << manifest_entries[i];
				if (i + 1 < static_cast<int>(manifest_entries.size())) jf << ',';
				jf << '\n';
			}
			jf << "  ]\n}\n";
			std::cout << "\n  wrote geometry_map.json\n";
		} else {
			std::cerr << "  ERROR writing geometry_map.json\n";
		}
	}

	std::cout << "\n[metal_gen] Done.\n";
	return 0;
}
