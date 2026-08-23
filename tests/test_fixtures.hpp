#pragma once
/**
 * tests/test_fixtures.hpp  -  Canonical test fixtures for VSEPR-SIM module tests
 * ================================================================================
 *
 * Provides reusable VsimDocument and SimState builders for unit tests.
 * Reference: FinalChapter/VSIM_TEST_FIXTURES.md
 *
 * Fixtures:
 *   make_ch4_doc()              -  CH4 relax VsimDocument
 *   make_nacl_doc()             -  NaCl crystal VsimDocument
 *   make_observe_doc(metrics)   -  VsimDocument with [observe] metrics list
 *
 * WO-75A  |  v5.0.0-main
 */

#include "vsim/vsim_document.hpp"

#include <string>
#include <vector>

namespace vsim_test {

// ---------------------------------------------------------------------------
// make_ch4_doc  -  Minimal CH4 relax document
// ---------------------------------------------------------------------------

inline vsim::VsimDocument make_ch4_doc()
{
	vsim::VsimDocument doc;

	doc.project.name    = "ch4_test";
	doc.project.version = "v5.0.0";
	doc.project.seed_base = 101;

	doc.material.formula   = "CH4";
	doc.material.prototype = "noble_gas";
	doc.material.phase     = "gas";

	doc.run.mode      = "relax";
	doc.run.max_steps = 100;
	doc.run.dt_fs     = 1.0;
	doc.run.converge  = true;

	vsim::MoleculeEntry mol;
	mol.formula       = "CH4";
	mol.count         = 1;
	mol.temperature_K = 0.0;
	mol.lattice       = "none";
	doc.simulation.molecules.push_back(mol);

	doc.exports.output_dir      = "out/ch4_test";
	doc.exports.write_xyz       = true;

	return doc;
}

// ---------------------------------------------------------------------------
// make_nacl_doc  -  NaCl B1 crystal document
// ---------------------------------------------------------------------------

inline vsim::VsimDocument make_nacl_doc()
{
	vsim::VsimDocument doc;

	doc.project.name    = "nacl_test";
	doc.project.version = "v5.0.0";
	doc.project.seed_base = 202;

	doc.material.formula    = "NaCl";
	doc.material.prototype  = "B1_NaCl";
	doc.material.phase      = "solid";

	doc.run.mode      = "relax";
	doc.run.max_steps = 200;
	doc.run.converge  = true;

	vsim::MoleculeEntry mol;
	mol.formula       = "NaCl";
	mol.count         = 8;
	mol.temperature_K = 300.0;
	mol.lattice       = "rocksalt";
	doc.simulation.molecules.push_back(mol);

	return doc;
}

// ---------------------------------------------------------------------------
// make_observe_doc  -  Document with an [observe] block for metric tests
// ---------------------------------------------------------------------------

inline vsim::VsimDocument make_observe_doc(const std::vector<std::string>& metrics)
{
	vsim::VsimDocument doc = make_ch4_doc();
	doc.observe.metrics         = metrics;
	doc.observe.output_format   = "auto";
	doc.observe.every_n_steps   = 1;
	return doc;
}

// ---------------------------------------------------------------------------
// make_excite_laser_doc  -  Document with [excite.laser] block
// ---------------------------------------------------------------------------

inline vsim::VsimDocument make_excite_laser_doc()
{
	vsim::VsimDocument doc = make_ch4_doc();
	doc.run.mode      = "md";
	doc.run.max_steps = 50;

	vsim::ExciteEntry laser;
	laser.type             = "laser";
	laser.axis             = "z";
	laser.polarization     = "z";
	laser.intensity        = 2.5;
	laser.pulse_width_fs   = 80.0;
	laser.photon_energy_eV = 1.55;
	laser.profile          = "gaussian";
	doc.excite.entries["laser"] = laser;

	return doc;
}

} // namespace vsim_test
