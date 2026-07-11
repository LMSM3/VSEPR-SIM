// =============================================================================
// tests/test_wo_03b.cpp   WO-VSIM-03B — Kill Explicit Object Authoring (Group 36)
// =============================================================================
// L0A  Level 0: [project] + [material] formula + [run] mode round-trips
// L0B  Level 0: structure alias "rocksalt" resolves to prototype "B1_NaCl"
// L1A  Level 1: prototype key stored directly
// L1B  Level 1: space_group + basis + cell parsed
// L2A  Level 2: [environment] periodic + temperature parsed
// L2B  Level 2: [excite.laser] axis/intensity/pulse_width_fs parsed
// L2C  Level 2: [observe] metrics list parsed
// L3A  Level 3: [[override.particle]] id + velocity parsed
// L4A  Level 4: [[raw.object]] id + species + position + velocity parsed
// L4B  Level 4: two raw objects produce a vector of size 2
// ALIAS  resolve_structure_alias covers all ~70 documented canonical aliases
//        across: ionic/salts, metals, covalent, oxides, molecular geometry,
//        polymers/organics, porous/framework, bead/premacro, and pass-through
// =============================================================================

#include <cassert>
#include <cstdio>
#include <string>
#include <cmath>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"

using namespace vsim;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static VsimDocument parse(const std::string& script) {
	return VsimParser::parse_string(script, "<test>");
}

static bool approx(double a, double b, double eps = 1e-9) {
	return std::fabs(a - b) < eps;
}

// ---------------------------------------------------------------------------
// L0A  Level 0 minimal round-trip: formula + run mode
// ---------------------------------------------------------------------------
static void L0A() {
	auto doc = parse(
		"[project]\nname = \"nacl_relax\"\n"
		"[material]\nformula = \"NaCl\"\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.project.name      == "nacl_relax");
	assert(doc.material.formula  == "NaCl");
	assert(doc.run.mode          == "relax");
	assert(doc.run.has_mode());
	printf("L0A OK\n");
}

// ---------------------------------------------------------------------------
// L0B  Structure alias "rocksalt" resolves to prototype "B1_NaCl"
// ---------------------------------------------------------------------------
static void L0B() {
	auto doc = parse(
		"[project]\nname = \"r\"\n"
		"[material]\nformula = \"NaCl\"\nstructure = \"rocksalt\"\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.material.structure          == "rocksalt");
	assert(doc.material.prototype          == "B1_NaCl");
	assert(doc.material.resolved_prototype()== "B1_NaCl");
	printf("L0B OK\n");
}

// ---------------------------------------------------------------------------
// L1A  Level 1: explicit prototype key takes precedence over alias
// ---------------------------------------------------------------------------
static void L1A() {
	auto doc = parse(
		"[project]\nname = \"si_diamond\"\n"
		"[material]\nformula = \"Si\"\nprototype = \"A4_Si\"\ncell = \"4x4x4\"\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.material.prototype == "A4_Si");
	assert(doc.material.cell      == "4x4x4");
	assert(doc.material.resolved_prototype() == "A4_Si");
	printf("L1A OK\n");
}

// ---------------------------------------------------------------------------
// L1B  Level 1: space_group + basis + lattice parsed
// ---------------------------------------------------------------------------
static void L1B() {
	auto doc = parse(
		"[project]\nname = \"lab\"\n"
		"[material]\n"
		"formula = \"NaCl\"\n"
		"space_group = \"Fm-3m\"\n"
		"lattice = \"fcc_ionic\"\n"
		"basis = \"Na:0,0,0; Cl:0.5,0.5,0.5\"\n"
		"cell = \"4x4x4\"\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.material.space_group == "Fm-3m");
	assert(doc.material.lattice     == "fcc_ionic");
	assert(doc.material.basis       == "Na:0,0,0; Cl:0.5,0.5,0.5");
	assert(doc.material.cell        == "4x4x4");
	// explicit basis + space_group path
	assert(doc.material.resolved_prototype() == "__explicit_basis__");
	printf("L1B OK\n");
}

// ---------------------------------------------------------------------------
// L2A  Level 2: [environment] periodic + temperature
// ---------------------------------------------------------------------------
static void L2A() {
	auto doc = parse(
		"[project]\nname = \"env\"\n"
		"[material]\nformula = \"Si\"\n"
		"[environment]\nperiodic = true\ntemperature = 300\n"
		"[run]\nmode = \"md\"\n"
	);
	assert(doc.environment.periodic    == true);
	assert(approx(doc.environment.temperature, 300.0));
	printf("L2A OK\n");
}

// ---------------------------------------------------------------------------
// L2B  Level 2: [excite.laser] fields parsed into ExciteSection registry
// ---------------------------------------------------------------------------
static void L2B() {
	auto doc = parse(
		"[project]\nname = \"laser\"\n"
		"[material]\nformula = \"Si\"\ncell = \"6x6x6\"\n"
		"[excite.laser]\n"
		"axis = \"z\"\n"
		"polarization = \"x\"\n"
		"intensity = 1.0\n"
		"pulse_width_fs = 100\n"
		"[run]\nmode = \"md\"\n"
	);
	assert(doc.excite.has("laser"));
	const auto* e = doc.excite.get("laser");
	assert(e != nullptr);
	assert(e->axis          == "z");
	assert(e->polarization  == "x");
	assert(approx(e->intensity,      1.0));
	assert(approx(e->pulse_width_fs, 100.0));
	printf("L2B OK\n");
}

// ---------------------------------------------------------------------------
// L2C  Level 2: [observe] metrics list parsed
// ---------------------------------------------------------------------------
static void L2C() {
	auto doc = parse(
		"[project]\nname = \"obs\"\n"
		"[material]\nformula = \"Si\"\n"
		"[observe]\nmetrics = [\"energy_map\", \"interference\", \"spectral_response\"]\n"
		"[run]\nmode = \"md\"\n"
	);
	assert(doc.observe.metrics.size() == 3);
	assert(doc.observe.metrics[0] == "energy_map");
	assert(doc.observe.metrics[1] == "interference");
	assert(doc.observe.metrics[2] == "spectral_response");
	printf("L2C OK\n");
}

// ---------------------------------------------------------------------------
// L3A  Level 3: [[override.particle]] id + velocity + charge
// ---------------------------------------------------------------------------
static void L3A() {
	auto doc = parse(
		"[project]\nname = \"ov\"\n"
		"[material]\nformula = \"Si\"\n"
		"[[override.particle]]\n"
		"id = 14\n"
		"velocity = [0.0, 0.0, 3.0]\n"
		"charge = -1.0\n"
		"[run]\nmode = \"md\"\n"
	);
	assert(doc.overrides.size() == 1);
	const auto& ov = doc.overrides[0];
	assert(ov.id == 14);
	assert(ov.has_velocity);
	assert(approx(ov.velocity[0], 0.0));
	assert(approx(ov.velocity[1], 0.0));
	assert(approx(ov.velocity[2], 3.0));
	assert(ov.has_charge);
	assert(approx(ov.charge, -1.0));
	printf("L3A OK\n");
}

// ---------------------------------------------------------------------------
// L4A  Level 4: [[raw.object]] explicit particle fields
// ---------------------------------------------------------------------------
static void L4A() {
	auto doc = parse(
		"[project]\nname = \"raw\"\n"
		"[[raw.object]]\n"
		"id = \"debug_particle_001\"\n"
		"species = \"C\"\n"
		"position = [0, 0, 0]\n"
		"velocity = [0, 0, 1]\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.raw_objects.size() == 1);
	const auto& ro = doc.raw_objects[0];
	assert(ro.id      == "debug_particle_001");
	assert(ro.species == "C");
	assert(approx(ro.position[0], 0.0));
	assert(approx(ro.velocity[2], 1.0));
	printf("L4A OK\n");
}

// ---------------------------------------------------------------------------
// L4B  Two [[raw.object]] blocks produce vector of size 2
// ---------------------------------------------------------------------------
static void L4B() {
	auto doc = parse(
		"[project]\nname = \"raw2\"\n"
		"[[raw.object]]\nid = \"p1\"\nspecies = \"H\"\n"
		"[[raw.object]]\nid = \"p2\"\nspecies = \"O\"\n"
		"[run]\nmode = \"relax\"\n"
	);
	assert(doc.raw_objects.size() == 2);
	assert(doc.raw_objects[0].id      == "p1");
	assert(doc.raw_objects[0].species == "H");
	assert(doc.raw_objects[1].id      == "p2");
	assert(doc.raw_objects[1].species == "O");
	printf("L4B OK\n");
}

// ---------------------------------------------------------------------------
// ALIAS  resolve_structure_alias covers documented entries
// ---------------------------------------------------------------------------
static void ALIAS() {
	// ── ionic / salts ─────────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("rocksalt")         == "B1_NaCl");
	assert(vsim::resolve_structure_alias("halite")           == "B1_NaCl");
	assert(vsim::resolve_structure_alias("nacl")             == "B1_NaCl");
	assert(vsim::resolve_structure_alias("cesium_chloride")  == "B2_CsCl");
	assert(vsim::resolve_structure_alias("cscl")             == "B2_CsCl");
	assert(vsim::resolve_structure_alias("fluorite")         == "C1_CaF2");
	assert(vsim::resolve_structure_alias("antifluorite")     == "Anti_C1_Li2O");
	assert(vsim::resolve_structure_alias("zincblende")       == "B3_ZnS");
	assert(vsim::resolve_structure_alias("sphalerite")       == "B3_ZnS");
	assert(vsim::resolve_structure_alias("wurtzite")         == "B4_ZnS");
	assert(vsim::resolve_structure_alias("rutile")           == "C4_TiO2");
	assert(vsim::resolve_structure_alias("perovskite")       == "ABO3_perovskite");
	assert(vsim::resolve_structure_alias("spinel")           == "AB2O4_spinel");
	// ── elemental metals ──────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("simple_cubic")     == "A_cP1");
	assert(vsim::resolve_structure_alias("sc")               == "A_cP1");
	assert(vsim::resolve_structure_alias("bcc")              == "A2_bcc");
	assert(vsim::resolve_structure_alias("body_centered_cubic") == "A2_bcc");
	assert(vsim::resolve_structure_alias("fcc")              == "A1_fcc");
	assert(vsim::resolve_structure_alias("face_centered_cubic") == "A1_fcc");
	assert(vsim::resolve_structure_alias("hcp")              == "A3_hcp");
	assert(vsim::resolve_structure_alias("hexagonal_close_packed") == "A3_hcp");
	assert(vsim::resolve_structure_alias("diamond")          == "A4_diamond");
	assert(vsim::resolve_structure_alias("diamond_cubic")    == "A4_diamond");
	assert(vsim::resolve_structure_alias("silicon")          == "A4_diamond");
	assert(vsim::resolve_structure_alias("germanium")        == "A4_diamond");
	assert(vsim::resolve_structure_alias("graphite")         == "A9_graphite");
	assert(vsim::resolve_structure_alias("graphene")         == "A9_graphene_2D");
	// ── covalent / semiconductor ──────────────────────────────────────────
	assert(vsim::resolve_structure_alias("zinc_sulfide")     == "B3_ZnS");
	assert(vsim::resolve_structure_alias("cadmium_sulfide")  == "B4_CdS");
	// ── oxides / ceramics ─────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("alpha_alumina")    == "D5_Al2O3_corundum");
	assert(vsim::resolve_structure_alias("corundum")         == "D5_Al2O3_corundum");
	assert(vsim::resolve_structure_alias("magnesia")         == "B1_MgO");
	assert(vsim::resolve_structure_alias("ceria")            == "C1_CeO2_fluorite");
	assert(vsim::resolve_structure_alias("zirconia")         == "C1_ZrO2_fluorite_like");
	assert(vsim::resolve_structure_alias("uraninite")        == "C1_UO2_fluorite");
	assert(vsim::resolve_structure_alias("thoria")           == "C1_ThO2_fluorite");
	// ── molecular geometry ────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("linear")           == "geom_linear");
	assert(vsim::resolve_structure_alias("bent")             == "geom_bent");
	assert(vsim::resolve_structure_alias("trigonal_planar")  == "geom_trigonal_planar");
	assert(vsim::resolve_structure_alias("tetrahedral")      == "geom_tetrahedral");
	assert(vsim::resolve_structure_alias("trigonal_pyramidal") == "geom_trigonal_pyramidal");
	assert(vsim::resolve_structure_alias("octahedral")       == "geom_octahedral");
	assert(vsim::resolve_structure_alias("square_planar")    == "geom_square_planar");
	assert(vsim::resolve_structure_alias("see_saw")          == "geom_seesaw");
	assert(vsim::resolve_structure_alias("t_shaped")         == "geom_t_shaped");
	// ── polymers / organics ───────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("linear_chain")     == "polymer_linear_chain");
	assert(vsim::resolve_structure_alias("branched_chain")   == "polymer_branched");
	assert(vsim::resolve_structure_alias("aromatic_ring")    == "organic_aromatic_ring");
	assert(vsim::resolve_structure_alias("benzene_ring")     == "organic_benzene");
	assert(vsim::resolve_structure_alias("alkane_chain")     == "organic_alkane_chain");
	assert(vsim::resolve_structure_alias("cycloalkane")      == "organic_cycloalkane");
	// ── porous / framework ────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("zeolite")          == "framework_zeolite");
	assert(vsim::resolve_structure_alias("mof")              == "framework_mof");
	assert(vsim::resolve_structure_alias("cof")              == "framework_cof");
	assert(vsim::resolve_structure_alias("pba")              == "framework_prussian_blue_analog");
	assert(vsim::resolve_structure_alias("prussian_blue")    == "framework_prussian_blue");
	// ── bead / premacro ───────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("bead_chain")       == "bead_linear_chain");
	assert(vsim::resolve_structure_alias("bead_cluster")     == "bead_cluster_random");
	assert(vsim::resolve_structure_alias("powder_bed")       == "premacro_powder_bed");
	assert(vsim::resolve_structure_alias("packed_bed")       == "premacro_packed_bed");
	assert(vsim::resolve_structure_alias("granular_column")  == "premacro_granular_column");
	assert(vsim::resolve_structure_alias("fiber_bundle")     == "premacro_fiber_bundle");
	assert(vsim::resolve_structure_alias("pipe_flow")        == "premacro_pipe_flow");
	// ── pass-through ──────────────────────────────────────────────────────
	assert(vsim::resolve_structure_alias("unknown_xyz")      == "unknown_xyz");
	printf("ALIAS OK\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
	printf("=== Group 36: WO-VSIM-03B Intent-Based Authoring ===\n");
	L0A();
	L0B();
	L1A();
	L1B();
	L2A();
	L2B();
	L2C();
	L3A();
	L4A();
	L4B();
	ALIAS();
	printf("=== All Group 36 tests passed ===\n");
	return 0;
}
