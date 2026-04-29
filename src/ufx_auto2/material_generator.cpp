// src/ufx_auto2/material_generator.cpp
// UFX_AUTO_2 Phase 2 -- Material Record Generator Implementation

#include "ufx_auto2/material_generator.hpp"
#include "v4/uff/uff_reference_provider.hpp"
#include "v4/uff/uff_autocreate.hpp"

#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>

namespace vsepr::ufx {

// ============================================================================
// Element -> atomic number table (Z=1..118 subset used by Phase 2)
// ============================================================================

static const struct { const char* sym; int Z; } k_element_Z[] = {
	{"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
	{"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
	{"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Sc",21},{"Ti",22},
	{"V",23},{"Cr",24},{"Mn",25},{"Fe",26},{"Co",27},{"Ni",28},{"Cu",29},
	{"Zn",30},{"Ga",31},{"Ge",32},{"As",33},{"Se",34},{"Br",35},{"Kr",36},
	{"Rb",37},{"Sr",38},{"Y",39},{"Zr",40},{"Nb",41},{"Mo",42},{"Tc",43},
	{"Ru",44},{"Rh",45},{"Pd",46},{"Ag",47},{"Cd",48},{"In",49},{"Sn",50},
	{"Sb",51},{"Te",52},{"I",53},{"Xe",54},{"Cs",55},{"Ba",56},{"La",57},
	{"Ce",58},{"Pr",59},{"Nd",60},{"Pm",61},{"Sm",62},{"Eu",63},{"Gd",64},
	{"Tb",65},{"Dy",66},{"Ho",67},{"Er",68},{"Tm",69},{"Yb",70},{"Lu",71},
	{"Hf",72},{"Ta",73},{"W",74},{"Re",75},{"Os",76},{"Ir",77},{"Pt",78},
	{"Au",79},{"Hg",80},{"Tl",81},{"Pb",82},{"Bi",83},{"Po",84},{"At",85},
	{"Rn",86},{"Fr",87},{"Ra",88},{"Ac",89},{"Th",90},{"Pa",91},{"U",92},
	{"Np",93},{"Pu",94},{"Am",95},{"Cm",96},{"Bk",97},{"Cf",98},{"Es",99},
	{"Fm",100},{"Md",101},{"No",102},{"Lr",103},{"Rf",104},{"Db",105},
	{"Sg",106},{"Bh",107},{"Hs",108},{"Mt",109},{"Ds",110},{"Rg",111},
	{"Cn",112},{"Nh",113},{"Fl",114},{"Mc",115},{"Lv",116},{"Ts",117},
	{"Og",118},
};

int MaterialRecordGenerator::atomic_number_for_(const std::string& sym) noexcept {
	for (const auto& row : k_element_Z) {
		if (sym == row.sym) return row.Z;
	}
	return 0;
}

int MaterialRecordGenerator::period_for_Z_(int Z) noexcept {
	if (Z <=  2) return 1;
	if (Z <= 10) return 2;
	if (Z <= 18) return 3;
	if (Z <= 36) return 4;
	if (Z <= 54) return 5;
	if (Z <= 86) return 6;
	return 7;
}

// ============================================================================
// material_key derivation
// Format: {elem}_ox{ox}_c{coord}_{geom}_{phase}_{hash4}
// hash4 is the low 4 hex digits of std::hash over all fields — prevents
// collisions when two records share the same axis values.
// ============================================================================

std::string MaterialRecordGenerator::make_material_key_(const AxisSample& s) {
	// Canonical base string
	std::ostringstream base;
	base << s.element
		 << "_ox" << s.oxidation_state
		 << "_c"  << s.coordination
		 << "_"   << s.geometry
		 << "_"   << s.phase;

	// Short hash for uniqueness
	std::size_t h = std::hash<std::string>{}(base.str())
				  ^ (std::hash<double>{}(s.temperature_K) << 1)
				  ^ (std::hash<double>{}(s.pressure_atm)  << 2)
				  ^ (std::hash<int>{}(s.sample_index)     << 3);

	std::ostringstream out;
	out << base.str() << "_" << std::hex << std::setw(4)
		<< std::setfill('0') << (h & 0xFFFF);
	return out.str();
}

// ============================================================================
// Constructor
// ============================================================================

MaterialRecordGenerator::MaterialRecordGenerator(const GeneratorOptions& opts)
	: opts_(opts)
{}

// ============================================================================
// from_axis_sample
// ============================================================================

UFXMaterialRecord MaterialRecordGenerator::from_axis_sample(const AxisSample& s) const {
	UFXMaterialRecord rec;

	rec.identity   = build_identity_(s);
	rec.force_field = build_force_field_(s);
	rec.provenance = build_provenance_(s, rec.force_field);

	// Record-level classification
	rec.source_class = SourceClass::Generated;

	// Timestamps
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	std::ostringstream ts;
	ts << now_ms;
	rec.created_at = ts.str();
	rec.updated_at = ts.str();

	return rec;
}

// ============================================================================
// build_identity_
// ============================================================================

IdentityBlock MaterialRecordGenerator::build_identity_(const AxisSample& s) const {
	IdentityBlock id;

	id.material_key        = make_material_key_(s);
	id.formula             = s.element;   // Phase 2: single-element formula
	id.phase               = s.phase;
	id.elements            = { s.element };
	id.atomic_numbers      = { atomic_number_for_(s.element) };
	id.oxidation_state     = static_cast<double>(s.oxidation_state);
	id.coordination_number = s.coordination;
	id.geometry_tag        = s.geometry;

	// Derive hybridization heuristic from coordination
	if (s.coordination <= 2)      id.hybridization = "sp";
	else if (s.coordination == 3) id.hybridization = "sp2";
	else if (s.coordination == 4) id.hybridization = "sp3";
	else if (s.coordination == 6) id.hybridization = "d2sp3";
	else                          id.hybridization = "unknown";

	// Local environment hash: hash of {element, coord, geometry, ox_state}
	std::ostringstream env;
	env << s.element << "|" << s.coordination << "|"
		<< s.geometry << "|" << s.oxidation_state;
	std::size_t h = std::hash<std::string>{}(env.str());
	std::ostringstream hstr;
	hstr << std::hex << h;
	id.local_environment_hash = hstr.str();

	return id;
}

// ============================================================================
// build_force_field_
// Bridge: LocalUFFReferenceProvider -> UFFAutoCreator -> ForceFieldBlock
// ============================================================================

ForceFieldBlock MaterialRecordGenerator::build_force_field_(const AxisSample& s) const {
	ForceFieldBlock ff;

	// Build UFF atom type label: element + "_" + coordination digit
	// e.g. "Fe_6", "O_2", "C_3"
	// This is approximate for Phase 2; Rev 2 will use full Rappé nomenclature.
	std::string atom_type = s.element + "_" + std::to_string(s.coordination);

	// Try published reference first
	std::optional<vsepr::uff::UFFEntry> entry;
	if (opts_.use_uff_reference) {
		auto provider = vsepr::uff::make_local_reference_provider();
		// Try exact type, then bare element
		entry = provider->lookup(atom_type);
		if (!entry.has_value()) {
			entry = provider->lookup(s.element);
		}
	}

	// Fall back to auto-creator
	if (!entry.has_value() && opts_.use_uff_autocreate) {
		auto provider = vsepr::uff::make_local_reference_provider();
		vsepr::uff::UFFAutoCreator creator(*provider);
		vsepr::uff::ChemicalContext ctx;
		ctx.element        = s.element;
		ctx.atomic_number  = atomic_number_for_(s.element);
		ctx.coordination   = s.coordination;
		ctx.geometry_tag   = s.geometry;
		ctx.oxidation_state = static_cast<double>(s.oxidation_state);
		entry = creator.create_missing_entry(atom_type, ctx);
	}

	// Map UFFEntry -> ForceFieldBlock
	if (entry.has_value()) {
		const auto& e = *entry;
		ff.atom_type           = e.atom_type;
		ff.r1                  = e.r1;
		ff.theta0              = e.theta0;
		ff.x1                  = e.x1;
		ff.D1                  = e.D1;
		ff.zeta                = e.zeta;
		ff.Z1                  = e.Z1;
		ff.torsion_barrier     = e.Vi;
		ff.inversion_barrier   = e.Uj;
		ff.local_environment_hash = "";  // filled from identity after construction
		ff.mixing_rule         = "geometric";

		switch (e.confidence) {
			case vsepr::uff::ParamConfidence::Published:
				ff.parameter_source_class = "reference";
				ff.parameter_confidence   = 0.95;
				break;
			case vsepr::uff::ParamConfidence::Derived:
				ff.parameter_source_class = "generated";
				ff.parameter_confidence   = 0.65;
				break;
			case vsepr::uff::ParamConfidence::Estimated:
				ff.parameter_source_class = "generated";
				ff.parameter_confidence   = opts_.generated_confidence;
				break;
			default:
				ff.parameter_source_class = "generated";
				ff.parameter_confidence   = 0.10;
				break;
		}
		ff.fallback_rule      = e.source_id;
		ff.validated_against  = e.source_id;
		ff.parameter_revision = "phase2_rev1";
	} else {
		// No entry at all — stub zeros, very low confidence
		ff.atom_type              = atom_type;
		ff.parameter_source_class = "generated";
		ff.parameter_confidence   = 0.05;
		ff.fallback_rule          = "no_entry";
		ff.parameter_revision     = "phase2_stub";
	}

	return ff;
}

// ============================================================================
// build_provenance_
// Always populated — hard rule.
// ============================================================================

ProvenanceBlock MaterialRecordGenerator::build_provenance_(
	const AxisSample& s,
	const ForceFieldBlock& ff) const
{
	ProvenanceBlock prov;
	prov.source_id         = "ufx_auto2_randomfill";
	prov.source_class_tag  = "generated";
	prov.source_version    = "phase2_rev1";
	prov.method_tag        = "random_axis_sample";
	prov.confidence        = ff.parameter_confidence;
	prov.uncertainty       = 1.0 - ff.parameter_confidence;
	prov.validation_status = ValidationStatus::Generated;
	prov.promotion_status  = "generated";

	// Retrieval date: Unix ms timestamp as string
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	std::ostringstream ts;
	ts << now_ms;
	prov.retrieval_date    = ts.str();
	prov.last_checked_date = ts.str();

	// Unit conversion trace: note the axes used
	std::ostringstream trace;
	trace << "T=" << s.temperature_K << "K P=" << s.pressure_atm
		  << "atm ox=" << s.oxidation_state << " coord=" << s.coordination;
	prov.unit_conversion_trace = trace.str();

	return prov;
}

} // namespace vsepr::ufx
