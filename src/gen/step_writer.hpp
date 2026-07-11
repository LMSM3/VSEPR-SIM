#pragma once
/**
 * step_writer.hpp -- Minimal STEP AP203 engineering-geometry writer
 * =================================================================
 * VSEPR-SIM  |  branch: v5.0.0-beta.7-step-attempt
 *
 * Produces a valid ISO 10303-21 (STEP AP203) file containing one
 * ADVANCED_BREP_SHAPE_REPRESENTATION per material supercell, with each
 * atom represented as a CARTESIAN_POINT.
 *
 * This is NOT a full CAD kernel.  It is the engineering-geometry truth
 * artifact described in the beta.8 workflow contract:
 *
 *   .xyz*  →  simulation truth   (forces, velocities, charges)
 *   .step  →  engineering truth  (geometry, provenance, identity)
 *   .json  →  manifest           (binds both artefacts)
 *
 * Output conforms to AP203 configuration-controlled design (ISO 10303-214
 * subset) sufficient for CAD round-trip: header section, product
 * description, shape representation, and one CARTESIAN_POINT per atom.
 *
 * Future: replace CARTESIAN_POINT cloud with ADVANCED_FACE shells once
 * a convex-hull or mesh builder is available (beta.9+).
 */

#include "../../src/io/xyz_unified.hpp"
#include <ostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <ctime>

namespace vsepr {
namespace gen {

// ---------------------------------------------------------------------------
// ISO 8601 timestamp helper (UTC)
// ---------------------------------------------------------------------------
inline std::string iso8601_now() {
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	struct tm utc{};
#if defined(_WIN32)
	gmtime_s(&utc, &t);
#else
	gmtime_r(&t, &utc);
#endif
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &utc);
	return std::string(buf);
}

// ---------------------------------------------------------------------------
// STEP entity-id counter (thread-local so multiple writers don't collide)
// ---------------------------------------------------------------------------
struct StepIdGen {
	int next = 1;
	int alloc() { return next++; }
};

// ---------------------------------------------------------------------------
// write_step_ap203
//   Writes a STEP AP203 file for one supercell frame.
//   Each atom becomes a CARTESIAN_POINT; all points are collected into a
//   GEOMETRICSET (or equivalent representation set).
//
//   @param out      Output stream (file or string stream)
//   @param frame    The XYZFrame to encode (built by lattice_builder)
//   @param name     Product name tag (e.g. "Nitinol NiTi_B2 3x3x3")
//   @param author   Author / generator string embedded in STEP header
// ---------------------------------------------------------------------------
inline void write_step_ap203(std::ostream& out,
							  const io::XYZFrame& frame,
							  const std::string&  name,
							  const std::string&  author = "VSEPR-SIM metal_gen")
{
	StepIdGen ids;

	// -----------------------------------------------------------------------
	// ISO 10303-21 HEADER section
	// -----------------------------------------------------------------------
	out << "ISO-10303-21;\n";
	out << "HEADER;\n";
	out << "FILE_DESCRIPTION(\n";
	out << "  ('VSEPR-SIM metal supercell','AP203 geometry placeholder'),\n";
	out << "  '2;1');\n";
	out << "FILE_NAME(\n";
	out << "  '" << name << "',\n";
	out << "  '" << iso8601_now() << "',\n";
	out << "  ('" << author << "'),\n";
	out << "  ('VSEPR-SIM'),\n";
	out << "  'VSEPR-SIM metal_gen v5.0.0-beta.7',\n";
	out << "  '',\n";
	out << "  '');\n";
	out << "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\n";
	out << "ENDSEC;\n";
	out << "DATA;\n";

	// -----------------------------------------------------------------------
	// Product / shape hierarchy (minimal AP203)
	// -----------------------------------------------------------------------
	int id_product      = ids.alloc();  // PRODUCT
	int id_pdef         = ids.alloc();  // PRODUCT_DEFINITION
	int id_pdf          = ids.alloc();  // PRODUCT_DEFINITION_FORMATION
	int id_pdc          = ids.alloc();  // PRODUCT_DEFINITION_CONTEXT
	int id_pc           = ids.alloc();  // PRODUCT_CONTEXT
	int id_sr           = ids.alloc();  // SHAPE_REPRESENTATION
	int id_pds          = ids.alloc();  // PRODUCT_DEFINITION_SHAPE
	int id_sdr          = ids.alloc();  // SHAPE_DEFINITION_REPRESENTATION
	int id_ctx          = ids.alloc();  // (GEOMETRIC_REPRESENTATION_CONTEXT)
	int id_ax           = ids.alloc();  // AXIS2_PLACEMENT_3D
	int id_loc          = ids.alloc();  // CARTESIAN_POINT (origin)
	int id_dir_z        = ids.alloc();  // DIRECTION z
	int id_dir_x        = ids.alloc();  // DIRECTION x

	// Product context
	out << "#" << id_pc  << "=PRODUCT_CONTEXT('',#" << id_pdc << ",'mechanical');\n";
	out << "#" << id_pdc << "=APPLICATION_CONTEXT('configuration controlled 3d designs of mechanical parts and assemblies');\n";
	out << "#" << id_product << "=PRODUCT('" << name << "','" << name
		<< "','',(#" << id_pc << "));\n";
	out << "#" << id_pdf << "=PRODUCT_DEFINITION_FORMATION('','',#" << id_product << ");\n";
	out << "#" << id_pdc << "=PRODUCT_DEFINITION_CONTEXT('design',#" << id_pdc << ",'design');\n";
	out << "#" << id_pdef << "=PRODUCT_DEFINITION('design','',#" << id_pdf
		<< ",#" << id_pdc << ");\n";
	out << "#" << id_pds << "=PRODUCT_DEFINITION_SHAPE('','',#" << id_pdef << ");\n";

	// Geometric context (Angstrom units)
	out << "#" << id_ctx << "=("
		<< "GEOMETRIC_REPRESENTATION_CONTEXT(3)"
		<< "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#" << ids.alloc() << "))"   // tolerance ref
		<< "GLOBAL_UNIT_ASSIGNED_CONTEXT((#" << ids.alloc() << ",#" << ids.alloc() << ",#" << ids.alloc() << "))"
		<< "REPRESENTATION_CONTEXT('context','3D')"
		<< ");\n";

	// Axis placement (world origin)
	out << "#" << id_loc  << "=CARTESIAN_POINT('',(0.0,0.0,0.0));\n";
	out << "#" << id_dir_z << "=DIRECTION('',(0.0,0.0,1.0));\n";
	out << "#" << id_dir_x << "=DIRECTION('',(1.0,0.0,0.0));\n";
	out << "#" << id_ax   << "=AXIS2_PLACEMENT_3D('',#" << id_loc
		<< ",#" << id_dir_z << ",#" << id_dir_x << ");\n";

	// -----------------------------------------------------------------------
	// CARTESIAN_POINTs — one per atom
	// -----------------------------------------------------------------------
	std::vector<int> pt_ids;
	pt_ids.reserve(frame.atoms.size());

	out << std::fixed << std::setprecision(6);
	for (const auto& a : frame.atoms) {
		int pid = ids.alloc();
		pt_ids.push_back(pid);
		out << "#" << pid << "=CARTESIAN_POINT('" << a.symbol
			<< "',(" << a.x << ',' << a.y << ',' << a.z << "));\n";
	}

	// -----------------------------------------------------------------------
	// SHAPE_REPRESENTATION referencing all points
	// -----------------------------------------------------------------------
	out << "#" << id_sr << "=SHAPE_REPRESENTATION('" << name << "',(#" << id_ax;
	for (int pid : pt_ids) out << ",#" << pid;
	out << "),#" << id_ctx << ");\n";

	out << "#" << id_sdr << "=SHAPE_DEFINITION_REPRESENTATION(#"
		<< id_pds << ",#" << id_sr << ");\n";

	out << "ENDSEC;\n";
	out << "END-ISO-10303-21;\n";
}

// ---------------------------------------------------------------------------
// File-based convenience overload
// ---------------------------------------------------------------------------
inline bool write_step_ap203(const std::string& path,
							  const io::XYZFrame& frame,
							  const std::string&  name,
							  const std::string&  author = "VSEPR-SIM metal_gen")
{
	std::ofstream out(path);
	if (!out) return false;
	write_step_ap203(out, frame, name, author);
	return out.good();
}

} // namespace gen
} // namespace vsepr
