/**
 * test_bio_object.cpp  —  WO-75E: Biological Object Layer Tests
 * ==============================================================
 *
 * Gate criteria (from STAGE.md WO-75E):
 *   Identity   : BioObject round-trips through registry without geometry fields
 *   Composition: cellulose, lignin, pectin resolve from a [plant] object
 *   Projection : project() produces a GeomMesh; not called unless explicitly requested
 *   Schema     : [bio] section parses object/class/projection keys correctly
 *
 * VSEPR-SIM  |  WO-75E  |  v5.13.5
 */

#include "bio/bio_object.hpp"
#include "bio/bio_geom_projection.hpp"
#include "bio/organic_class.hpp"
#include "bio/plant_composition.hpp"
#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

// ============================================================================
// Test 1 — Identity: no geometry fields, correct tags
// ============================================================================

static void test_identity_leaf() {
	auto obj = vsepr::bio::BioObject::from_name("leaf");
	assert(obj.kind      == vsepr::bio::BioObjectKind::Leaf);
	assert(obj.domain    == "bio/nongeom");
	assert(obj.class_tag == "plant/object");
	assert(obj.nongeom   == true);
	assert(obj.resolved  == false);
	std::printf("PASS  test_identity_leaf\n");
}

static void test_identity_stem() {
	auto obj = vsepr::bio::BioObject::from_name("stem");
	assert(obj.kind    == vsepr::bio::BioObjectKind::Stem);
	assert(obj.nongeom == true);
	std::printf("PASS  test_identity_stem\n");
}

static void test_identity_plant() {
	auto obj = vsepr::bio::BioObject::from_name("plant");
	assert(obj.kind    == vsepr::bio::BioObjectKind::Plant);
	assert(obj.nongeom == true);
	std::printf("PASS  test_identity_plant\n");
}

static void test_identity_unknown() {
	auto obj = vsepr::bio::BioObject::from_name("notabiobject");
	assert(obj.kind == vsepr::bio::BioObjectKind::Unknown);
	std::printf("PASS  test_identity_unknown\n");
}

// ============================================================================
// Test 2 — Composition: cellulose / lignin / pectin resolve from plant objects
// ============================================================================

static void test_composition_leaf() {
	auto obj = vsepr::bio::BioObject::from_name("leaf");
	obj.resolve();
	assert(obj.resolved == true);
	double c = obj.weight_of(vsepr::bio::OrganicClass::Cellulose);
	double l = obj.weight_of(vsepr::bio::OrganicClass::Lignin);
	double p = obj.weight_of(vsepr::bio::OrganicClass::Pectin);
	double total = c + l + p;
	assert(total > 0.99 && total < 1.01);
	(void)total;
	assert(c > l && c > p);
	std::printf("PASS  test_composition_leaf  (cel=%.3f  lig=%.3f  pec=%.3f)\n", c, l, p);
}

static void test_composition_stem() {
	auto obj = vsepr::bio::BioObject::from_name("stem");
	obj.resolve();
	double c = obj.weight_of(vsepr::bio::OrganicClass::Cellulose);
	double l = obj.weight_of(vsepr::bio::OrganicClass::Lignin);
	double p = obj.weight_of(vsepr::bio::OrganicClass::Pectin);
	assert(l > p);
	assert(c + l + p > 0.99);
	std::printf("PASS  test_composition_stem  (cel=%.3f  lig=%.3f  pec=%.3f)\n", c, l, p);
}

static void test_composition_plant() {
	auto obj = vsepr::bio::BioObject::from_name("plant");
	obj.resolve();
	double c = obj.weight_of(vsepr::bio::OrganicClass::Cellulose);
	double l = obj.weight_of(vsepr::bio::OrganicClass::Lignin);
	double p = obj.weight_of(vsepr::bio::OrganicClass::Pectin);
	assert(c > 0.0 && l > 0.0 && p > 0.0);
	std::printf("PASS  test_composition_plant (cel=%.3f  lig=%.3f  pec=%.3f)\n", c, l, p);
}

// ============================================================================
// Test 3 — Organic class descriptors
// ============================================================================

static void test_organic_class_cellulose() {
	auto d = vsepr::bio::organic_class_descriptor(vsepr::bio::OrganicClass::Cellulose);
	assert(d.name   == "cellulose");
	assert(d.domain == "organic/polymer");
	assert(vsepr::bio::has_flag(d.bonding, vsepr::bio::BondingCharacter::HBond));
	assert(vsepr::bio::has_flag(d.bonding, vsepr::bio::BondingCharacter::SigmaNet));
	(void)d;
	std::printf("PASS  test_organic_class_cellulose\n");
}

static void test_organic_class_lignin() {
	auto d = vsepr::bio::organic_class_descriptor(vsepr::bio::OrganicClass::Lignin);
	assert(d.name   == "lignin");
	assert(d.domain == "organic/ar/polymer");
	assert(vsepr::bio::has_flag(d.bonding, vsepr::bio::BondingCharacter::ArPi));
	(void)d;
	std::printf("PASS  test_organic_class_lignin\n");
}

static void test_organic_class_pectin() {
	auto d = vsepr::bio::organic_class_descriptor(vsepr::bio::OrganicClass::Pectin);
	assert(d.name   == "pectin");
	assert(d.domain == "organic/gel");
	assert(vsepr::bio::has_flag(d.bonding, vsepr::bio::BondingCharacter::ChargedSite));
	assert(vsepr::bio::has_flag(d.bonding, vsepr::bio::BondingCharacter::HBond));
	(void)d;
	std::printf("PASS  test_organic_class_pectin\n");
}

// ============================================================================
// Test 4 — Geometry projection is opt-in; BioObject itself has no mesh data
// ============================================================================

static void test_projection_opt_in() {
	auto obj = vsepr::bio::BioObject::from_name("leaf");
	obj.resolve();
	assert(obj.nongeom == true);

	auto mesh = vsepr::bio::project(obj);
	assert(mesh.domain          == "geom/mesh");
	assert(mesh.source_name     == "leaf");
	assert(mesh.is_placeholder  == true);
	assert(mesh.vertices.empty());
	assert(mesh.faces.empty());
	std::printf("PASS  test_projection_opt_in\n");
}

// ============================================================================
// Test 5 — [bio] VSIM schema section
// ============================================================================

static void test_vsim_bio_section_parse() {
	const std::string script =
		"[project]\n"
		"name    = \"bio_test\"\n"
		"version = \"v5.13.5\"\n"
		"\n"
		"[[simulation.molecule]]\n"
		"formula = \"H2O\"\n"
		"count   = 1\n"
		"\n"
		"[bio]\n"
		"object     = \"leaf\"\n"
		"class      = \"plant/object\"\n"
		"projection = false\n";

	vsim::VsimDocument doc = vsim::VsimParser::parse_string(script);
	assert(doc.bio.present    == true);
	assert(doc.bio.object     == "leaf");
	assert(doc.bio.class_tag  == "plant/object");
	assert(doc.bio.projection == false);
	std::printf("PASS  test_vsim_bio_section_parse\n");
}

static void test_vsim_bio_projection_true() {
	const std::string script =
		"[project]\n"
		"name    = \"bio_proj\"\n"
		"version = \"v5.13.5\"\n"
		"\n"
		"[[simulation.molecule]]\n"
		"formula = \"H2O\"\n"
		"count   = 1\n"
		"\n"
		"[bio]\n"
		"object     = \"stem\"\n"
		"projection = true\n";

	vsim::VsimDocument doc = vsim::VsimParser::parse_string(script);
	assert(doc.bio.object     == "stem");
	assert(doc.bio.projection == true);
	std::printf("PASS  test_vsim_bio_projection_true\n");
}

static void test_vsim_bio_absent() {
	const std::string script =
		"[project]\n"
		"name    = \"no_bio\"\n"
		"version = \"v5.13.5\"\n"
		"\n"
		"[[simulation.molecule]]\n"
		"formula = \"H2O\"\n"
		"count   = 1\n";

	vsim::VsimDocument doc = vsim::VsimParser::parse_string(script);
	assert(doc.bio.present    == false);
	assert(doc.bio.projection == false);
	std::printf("PASS  test_vsim_bio_absent\n");
}

} // namespace

// ============================================================================
// main
// ============================================================================
int main() {
	std::printf("=== test_bio_object (WO-75E) ===\n");

	test_identity_leaf();
	test_identity_stem();
	test_identity_plant();
	test_identity_unknown();

	test_composition_leaf();
	test_composition_stem();
	test_composition_plant();

	test_organic_class_cellulose();
	test_organic_class_lignin();
	test_organic_class_pectin();

	test_projection_opt_in();

	test_vsim_bio_section_parse();
	test_vsim_bio_projection_true();
	test_vsim_bio_absent();

	std::printf("=== ALL PASS ===\n");
	return 0;
}
