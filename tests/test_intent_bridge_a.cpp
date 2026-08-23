/**
 * tests/test_intent_bridge_a.cpp
 * ================================
 * Group 54 — Intent Runtime Bridge Basic (WO-VSIM-INTENT-BRIDGE-A)
 *
 * Acceptance tests:
 *
 *   IB-A-01  material NaCl creates 2 particles (Na + Cl from basis)
 *   IB-A-02  particle masses are non-zero and chemically plausible
 *   IB-A-03  particle charges: Na=+1, Cl=-1 (formal charge model)
 *   IB-A-04  environment temperature applied from [environment]
 *   IB-A-05  environment periodic flag propagated
 *   IB-A-06  environment boundary strings propagated from [boundary]
 *   IB-A-07  run mode / max_steps / dt_fs applied from [run]
 *   IB-A-08  empty [material] yields has_particles=false (graceful)
 *   IB-A-09  silicon (diamond, neutral) yields 2 particles, charge=0
 */

#include "vsim/intent/intent_bridge.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

static vsim::IntentSystem nacl_system() {
	vsim::VsimDocument doc;
	doc.material.formula   = "NaCl";
	doc.material.structure = "rocksalt";
	// Basis will be resolved via RegistryResolver (B1_NaCl)
	// Na:0,0,0; Cl:0.5,0.5,0.5
	return vsim::IntentBridge::apply(doc);
}

static vsim::IntentSystem si_system() {
	vsim::VsimDocument doc;
	doc.material.formula   = "Si";
	doc.material.structure = "diamond";
	return vsim::IntentBridge::apply(doc);
}

// ============================================================================
// Tests
// ============================================================================

static bool test_IB_A_01() {
	auto sys = nacl_system();
	if (!sys.has_particles) {
		std::puts("FAIL IB-A-01: has_particles is false");
		return false;
	}
	// B1_NaCl basis: "Na:0,0,0; Cl:0.5,0.5,0.5"  -> 2 particles
	if (sys.particles.size() < 2) {
		std::printf("FAIL IB-A-01: expected >=2 particles, got %zu\n", sys.particles.size());
		return false;
	}
	std::puts("PASS IB-A-01: NaCl creates >=2 particles");
	return true;
}

static bool test_IB_A_02() {
	auto sys = nacl_system();
	for (const auto& p : sys.particles) {
		if (p.mass <= 0.0) {
			std::printf("FAIL IB-A-02: particle %s has mass=%.3f\n", p.symbol.c_str(), p.mass);
			return false;
		}
	}
	// Na ~22.99, Cl ~35.45
	bool has_na = false, has_cl = false;
	for (const auto& p : sys.particles) {
		if (p.symbol == "Na" && std::abs(p.mass - 22.99) < 0.1) has_na = true;
		if (p.symbol == "Cl" && std::abs(p.mass - 35.45) < 0.1) has_cl = true;
	}
	if (!has_na || !has_cl) {
		std::puts("FAIL IB-A-02: Na or Cl mass out of expected range");
		return false;
	}
	std::puts("PASS IB-A-02: particle masses non-zero and plausible");
	return true;
}

static bool test_IB_A_03() {
	auto sys = nacl_system();
	bool na_ok = false, cl_ok = false;
	for (const auto& p : sys.particles) {
		if (p.symbol == "Na" && std::abs(p.charge - 1.0) < 1e-9) na_ok = true;
		if (p.symbol == "Cl" && std::abs(p.charge + 1.0) < 1e-9) cl_ok = true;
	}
	if (!na_ok || !cl_ok) {
		std::puts("FAIL IB-A-03: formal charges Na=+1/Cl=-1 not found");
		return false;
	}
	std::puts("PASS IB-A-03: formal charges Na=+1 Cl=-1");
	return true;
}

static bool test_IB_A_04() {
	vsim::VsimDocument doc;
	doc.material.formula   = "NaCl";
	doc.material.structure = "rocksalt";
	doc.environment.temperature = 900.0;
	auto sys = vsim::IntentBridge::apply(doc);
	if (std::abs(sys.env.temperature_K - 900.0) > 0.5) {
		std::printf("FAIL IB-A-04: expected T=900K, got %.1f\n", sys.env.temperature_K);
		return false;
	}
	std::puts("PASS IB-A-04: environment temperature applied");
	return true;
}

static bool test_IB_A_05() {
	vsim::VsimDocument doc;
	doc.material.formula = "NaCl";
	doc.environment.periodic = true;
	auto sys = vsim::IntentBridge::apply(doc);
	if (!sys.env.periodic) {
		std::puts("FAIL IB-A-05: periodic flag not propagated");
		return false;
	}
	std::puts("PASS IB-A-05: environment periodic propagated");
	return true;
}

static bool test_IB_A_06() {
	vsim::VsimDocument doc;
	doc.material.formula = "NaCl";
	doc.boundary.x = "periodic";
	doc.boundary.y = "reflecting";
	doc.boundary.z = "open";
	auto sys = vsim::IntentBridge::apply(doc);
	if (sys.env.boundary_x != "periodic" ||
		sys.env.boundary_y != "reflecting" ||
		sys.env.boundary_z != "open") {
		std::printf("FAIL IB-A-06: boundary x=%s y=%s z=%s\n",
			sys.env.boundary_x.c_str(),
			sys.env.boundary_y.c_str(),
			sys.env.boundary_z.c_str());
		return false;
	}
	std::puts("PASS IB-A-06: boundary strings propagated");
	return true;
}

static bool test_IB_A_07() {
	vsim::VsimDocument doc;
	doc.material.formula = "NaCl";
	doc.run.mode      = "md";
	doc.run.max_steps = 1234;
	doc.run.dt_fs     = 0.5;
	doc.run.converge  = false;
	auto sys = vsim::IntentBridge::apply(doc);
	if (sys.run.mode != "md" || sys.run.max_steps != 1234 ||
		std::abs(sys.run.dt_fs - 0.5) > 1e-9 || sys.run.converge) {
		std::printf("FAIL IB-A-07: mode=%s steps=%d dt=%.2f converge=%d\n",
			sys.run.mode.c_str(), sys.run.max_steps, sys.run.dt_fs, (int)sys.run.converge);
		return false;
	}
	std::puts("PASS IB-A-07: run mode/steps/dt/converge applied");
	return true;
}

static bool test_IB_A_08() {
	vsim::VsimDocument doc;
	// No material set
	auto sys = vsim::IntentBridge::apply(doc);
	if (sys.has_particles) {
		std::puts("FAIL IB-A-08: empty material should yield has_particles=false");
		return false;
	}
	std::puts("PASS IB-A-08: empty material gracefully yields no particles");
	return true;
}

static bool test_IB_A_09() {
	auto sys = si_system();
	// diamond Si basis: "X:0,0,0; X:0.25,0.25,0.25" -> 2 Si particles
	if (!sys.has_particles || sys.particles.size() < 2) {
		std::printf("FAIL IB-A-09: expected >=2 Si particles, got %zu\n", sys.particles.size());
		return false;
	}
	for (const auto& p : sys.particles) {
		if (std::abs(p.charge) > 1e-9) {
			std::printf("FAIL IB-A-09: Si particle %s has charge=%.2f (expected 0)\n",
				p.symbol.c_str(), p.charge);
			return false;
		}
	}
	std::puts("PASS IB-A-09: diamond Si yields >=2 neutral particles");
	return true;
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::puts("\n=== Group 54 — Intent Runtime Bridge Basic ===\n");

	int pass = 0, fail = 0;
	auto run = [&](bool (*fn)()) {
		if (fn()) ++pass; else ++fail;
	};

	run(test_IB_A_01);
	run(test_IB_A_02);
	run(test_IB_A_03);
	run(test_IB_A_04);
	run(test_IB_A_05);
	run(test_IB_A_06);
	run(test_IB_A_07);
	run(test_IB_A_08);
	run(test_IB_A_09);

	std::printf("\n  Results: %d passed  %d failed\n\n", pass, fail);
	return (fail == 0) ? 0 : 1;
}
