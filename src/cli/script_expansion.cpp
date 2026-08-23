#include "cli/script_expansion.hpp"

#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_registry.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace vsepr::cli {
namespace {

using vsim::DistributionSection;
using vsim::MaterialSection;
using vsim::RegistryBundle;
using vsim::VsimDocument;

bool has_metric(const VsimDocument& doc, const std::string& metric) {
	return std::find(doc.observe.metrics.begin(), doc.observe.metrics.end(), metric)
		!= doc.observe.metrics.end();
}

bool formula_matches(const std::string& formula, const std::string& target) {
	std::string lhs = formula;
	std::string rhs = target;
	std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return lhs == rhs;
}

void add_candidate(ScriptExpansion& expansion, ExpansionCandidate candidate) {
	if (candidate.kind == ExpansionKind::MissingProperty)
		++expansion.missing_property_count;
	if (candidate.kind == ExpansionKind::ReportAnnotation)
		++expansion.report_annotation_count;
	if (candidate.executable)
		++expansion.executable_candidate_count;
	expansion.candidates.push_back(std::move(candidate));
}

void add_builder_record(
	ScriptExpansion& expansion,
	std::string path,
	std::string value,
	BuilderRecordOrigin origin,
	BuilderRecordStatus status,
	std::string reason
) {
	if (origin == BuilderRecordOrigin::Written)
		++expansion.written_record_count;
	else
		++expansion.inferred_record_count;
	if (status == BuilderRecordStatus::FutureRunnable)
		++expansion.future_runnable_record_count;
	if (status == BuilderRecordStatus::Executed)
		++expansion.executed_record_count;
	expansion.builder_records.push_back({
		std::move(path), std::move(value), origin, status, std::move(reason)
	});
}

std::string vector_text(const XYZVec3& value) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(6)
		<< '[' << value.x << ',' << value.y << ',' << value.z << ']';
	return out.str();
}

void add_written_material_records(
	const MaterialSection& material,
	ScriptExpansion& expansion
) {
	auto add = [&](bool declared, const char* path, const std::string& value) {
		if (declared)
			add_builder_record(expansion, path, value, BuilderRecordOrigin::Written,
				BuilderRecordStatus::PreviewOnly, "declared by [material]");
	};
	add(material.formula_declared, "material.formula", material.formula);
	add(material.prototype_declared, "material.prototype", material.prototype);
	add(material.structure_declared, "material.structure", material.structure);
	add(material.space_group_declared, "material.space_group", material.space_group);
	add(material.lattice_declared, "material.lattice", material.lattice);
	add(material.basis_declared, "material.basis", material.basis);
	add(material.cell_declared, "material.cell", material.cell);
	add(material.phase_declared, "material.phase", material.phase);
}

void add_registry_records(
	const MaterialSection& material,
	const RegistryBundle& bundle,
	ScriptExpansion& expansion
) {
	if (!material.prototype_declared && !material.resolved_prototype().empty()) {
		add_builder_record(expansion, "material.prototype", material.resolved_prototype(),
			BuilderRecordOrigin::Inferred, BuilderRecordStatus::FutureRunnable,
			"resolved from material structure intent");
	}
	if (!bundle.populated)
		return;
	if (!material.space_group_declared && !bundle.space_group.empty())
		add_builder_record(expansion, "material.space_group", bundle.space_group,
			BuilderRecordOrigin::Inferred, BuilderRecordStatus::FutureRunnable,
			"resolved by the internal material registry");
	if (!material.basis_declared && !bundle.basis.empty())
		add_builder_record(expansion, "material.basis", bundle.basis,
			BuilderRecordOrigin::Inferred, BuilderRecordStatus::FutureRunnable,
			"resolved by the internal material registry");
	if (!bundle.generator.empty())
		add_builder_record(expansion, "material.geometry_source", bundle.generator,
			BuilderRecordOrigin::Inferred, BuilderRecordStatus::FutureRunnable,
			"registry-selected internal structure generator");
}

void add_distribution_records(
	const DistributionSection& distribution,
	ScriptExpansion& expansion
) {
	const auto status = distribution.minimally_complete()
		? BuilderRecordStatus::FutureRunnable
		: BuilderRecordStatus::Unsupported;
	const std::string prefix = "distribution." + distribution.name + ".";
	auto add = [&](const char* field, const std::string& value) {
		if (!value.empty())
			add_builder_record(expansion, prefix + field, value,
				BuilderRecordOrigin::Written, status, "declared by the distribution card");
	};
	add("target", distribution.target);
	add("species", distribution.species);
	if (distribution.count > 0)
		add("count", std::to_string(distribution.count));
	add("placement", distribution.placement);
	add("shape", distribution.shape);
	if (distribution.has_center) add("center", vector_text(distribution.center));
	if (distribution.has_extent) add("extent", vector_text(distribution.extent));
	if (distribution.has_velocity_mean)
		add("velocity_mean", vector_text(distribution.velocity_mean));
	if (distribution.has_velocity_spread)
		add("velocity_spread", vector_text(distribution.velocity_spread));

	add_builder_record(expansion, prefix + "resolved_target", distribution.target,
		BuilderRecordOrigin::Inferred, status,
		distribution.minimally_complete()
			? "typed placement intent is complete; execution is not selected by preview"
			: "distribution is missing required typed placement intent");

	expansion.distributions.push_back({
		distribution.name,
		distribution.target,
		distribution.species,
		distribution.count,
		distribution.placement,
		distribution.shape,
		distribution.minimally_complete(),
		false,
		false
	});
}

void infer_candidates(const VsimDocument& doc, ScriptExpansion& expansion) {
	const std::string& formula = doc.material.formula;
	const std::string& run_mode = doc.run.mode;
	const bool requests_classify =
		doc.raw_sections.count("analysis.classify") > 0
		|| has_metric(doc, "vsepr_sites")
		|| has_metric(doc, "organic_candidate");

	if (!formula.empty())
		add_candidate(expansion, {ExpansionKind::MaterialVariant,
			"material_registry_resolution",
			"material formula is available for registry-backed interpretation",
			0.78, false, false});

	if (requests_classify)
		add_candidate(expansion, {ExpansionKind::ClassificationHint,
			"classify_preview_packet",
			"script requests classifier-facing metrics that can be summarized before execution",
			0.86, false, false});

	if (has_metric(doc, "bond_angles"))
		add_candidate(expansion, {ExpansionKind::MissingProperty,
			"bond_order_metadata",
			"bond-angle observations are requested, but bond-order metadata is not explicit in the flat script",
			0.64, false, false});

	if (formula_matches(formula, "CH4"))
		add_candidate(expansion, {ExpansionKind::GeometryVariant,
			"tetrahedral_strain_probe",
			"methane-like local geometry can seed a tetrahedral strain sensitivity preview",
			0.74, false, false});
	else if (formula_matches(formula, "NH3"))
		add_candidate(expansion, {ExpansionKind::GeometryVariant,
			"trigonal_pyramidal_lone_pair_probe",
			"ammonia-like connectivity can seed a lone-pair geometry preview",
			0.72, false, false});
	else if (formula_matches(formula, "H2O"))
		add_candidate(expansion, {ExpansionKind::GeometryVariant,
			"bent_lone_pair_probe",
			"water-like connectivity can seed an oxygen lone-pair geometry preview",
			0.72, false, false});
	else if (formula_matches(formula, "CO2"))
		add_candidate(expansion, {ExpansionKind::GeometryVariant,
			"linear_center_probe",
			"carbon dioxide-like connectivity can seed a linear center preview",
			0.70, false, false});

	if (doc.run.max_steps > 0 || doc.simulation.fire_max_steps > 0)
		add_candidate(expansion, {ExpansionKind::ParameterSweep,
			"step_budget_sensitivity",
			"run step limits are available for a later explicit variation plan",
			0.58, false, false});

	if (!run_mode.empty())
		add_candidate(expansion, {ExpansionKind::RuntimeAlternative,
			"direct_script_path",
			"direct run intent is preserved as the anchor path; preview does not execute it",
			0.95, false, false});

	if (doc.exports.write_report_md)
		add_candidate(expansion, {ExpansionKind::ReportAnnotation,
			"expansion_summary_block",
			"report settings can carry a non-executed expansion summary block",
			0.82, false, false});

	if (!doc.project.name.empty() && !formula.empty() && !run_mode.empty()) {
		add_candidate(expansion, {ExpansionKind::AIHookPacket,
			"script_context_packet",
			"project, material, and run records are available for a future context packet",
			0.68, false, false});
		expansion.ai_ready = true;
	}
}

} // namespace

std::string to_string(ExpansionKind kind) {
	switch (kind) {
		case ExpansionKind::MissingProperty:     return "MissingProperty";
		case ExpansionKind::GeometryVariant:     return "GeometryVariant";
		case ExpansionKind::MaterialVariant:     return "MaterialVariant";
		case ExpansionKind::ParameterSweep:      return "ParameterSweep";
		case ExpansionKind::ClassificationHint:  return "ClassificationHint";
		case ExpansionKind::RuntimeAlternative:  return "RuntimeAlternative";
		case ExpansionKind::ReportAnnotation:    return "ReportAnnotation";
		case ExpansionKind::AIHookPacket:        return "AIHookPacket";
	}
	return "Unknown";
}

std::string to_string(BuilderRecordOrigin origin) {
	return origin == BuilderRecordOrigin::Written ? "written" : "inferred";
}

std::string to_string(BuilderRecordStatus status) {
	switch (status) {
		case BuilderRecordStatus::PreviewOnly:    return "preview_only";
		case BuilderRecordStatus::FutureRunnable: return "future_runnable";
		case BuilderRecordStatus::Executed:       return "executed";
		case BuilderRecordStatus::Unsupported:    return "unsupported";
	}
	return "unsupported";
}

ScriptExpansion expand_script_file(const std::filesystem::path& script_path) {
	const VsimDocument doc = vsim::VsimParser::parse_file(script_path.string());
	const auto validation = doc.validate();
	if (!validation.ok) {
		std::ostringstream message;
		message << "Cannot expand invalid VSIM document";
		for (const auto& error : validation.errors)
			message << "\n  - " << error;
		throw std::runtime_error(message.str());
	}

	ScriptExpansion expansion;
	expansion.source_name = script_path.filename().string();
	expansion.canonical_parser = true;

	add_written_material_records(doc.material, expansion);
	std::ostringstream registry_log;
	const RegistryBundle bundle = vsim::RegistryResolver::resolve(doc.material, registry_log);
	add_registry_records(doc.material, bundle, expansion);
	for (const auto& [name, distribution] : doc.distributions) {
		(void)name;
		add_distribution_records(distribution, expansion);
	}
	infer_candidates(doc, expansion);
	return expansion;
}

std::string format_script_expansion(const ScriptExpansion& expansion) {
	std::ostringstream out;
	out << "Script expansion preview\n\n";
	out << "source: " << expansion.source_name << "\n";
	out << "parse_status: ok\n";
	out << "canonical_parser: " << (expansion.canonical_parser ? "true" : "false") << "\n";
	out << "direct_execution: not run\n";
	out << "builder_records: " << expansion.builder_records.size() << "\n";
	out << "written_records: " << expansion.written_record_count << "\n";
	out << "inferred_records: " << expansion.inferred_record_count << "\n";
	out << "future_runnable_records: " << expansion.future_runnable_record_count << "\n";
	out << "executed_records: " << expansion.executed_record_count << "\n";
	out << "distribution_count: " << expansion.distributions.size() << "\n";
	out << "expansion_candidates: " << expansion.candidates.size() << "\n";
	out << "missing_properties: " << expansion.missing_property_count << "\n";
	out << "executable_candidates: " << expansion.executable_candidate_count << "\n";
	out << "report_annotations: " << expansion.report_annotation_count << "\n";
	out << "ai_ready: " << (expansion.ai_ready ? "true" : "false") << "\n";

	for (std::size_t i = 0; i < expansion.builder_records.size(); ++i) {
		const auto& record = expansion.builder_records[i];
		out << "\n[builder " << i << "]\n";
		out << "    path: " << record.path << "\n";
		out << "    value: " << record.value << "\n";
		out << "    origin: " << to_string(record.origin) << "\n";
		out << "    status: " << to_string(record.status) << "\n";
		out << "    reason: " << record.reason << "\n";
	}

	for (const auto& distribution : expansion.distributions) {
		out << "\n[distribution " << distribution.name << "]\n";
		out << "    target: " << distribution.target << "\n";
		out << "    species: " << distribution.species << "\n";
		out << "    count: " << distribution.count << "\n";
		out << "    placement: " << distribution.placement << "\n";
		out << "    shape: " << distribution.shape << "\n";
		out << "    complete: " << (distribution.complete ? "true" : "false") << "\n";
		out << "    executable_now: " << (distribution.executable ? "true" : "false") << "\n";
		out << "    executed: " << (distribution.executed ? "true" : "false") << "\n";
	}

	for (std::size_t i = 0; i < expansion.candidates.size(); ++i) {
		const auto& candidate = expansion.candidates[i];
		out << "\n[candidate " << i << "] " << to_string(candidate.kind) << "\n";
		out << "    label: " << candidate.label << "\n";
		out << "    reason: " << candidate.reason << "\n";
		out << "    confidence: " << std::fixed << std::setprecision(2)
			<< candidate.confidence << "\n";
		out << "    executable: " << (candidate.executable ? "true" : "false") << "\n";
		out << "    selected: " << (candidate.selected ? "true" : "false") << "\n";
	}

	out << "\nselected_execution: none\n";
	out << "result: PASS\n";
	return out.str();
}

int run_script_expansion_preview(const std::filesystem::path& script_path) {
	const ScriptExpansion expansion = expand_script_file(script_path);
	std::cout << format_script_expansion(expansion);
	return 0;
}

} // namespace vsepr::cli
