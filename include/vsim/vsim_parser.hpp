#pragma once
/**
 * vsim_parser.hpp  -  .vsim file parser
 * ====================================
 *
 * Parses TOML-subset .vsim scripts into VsimDocument.
 *
 * Grammar:
 *   file        := (line '\n')*
 *   line        := comment | section_header | key_value | blank
 *   comment     := '#' <rest of line>
 *   section_hdr := '[' IDENTIFIER ']'
 *   key_value   := IDENTIFIER WS* '=' WS* value (WS* comment)?
 *   value       := bool_lit | int_lit | float_lit | string_lit | list_lit
 *   bool_lit    := 'true' | 'false'
 *   string_lit  := '"' <chars> '"'
 *   list_lit    := '[' value (',' value)* ']'
 *   IDENTIFIER  := [A-Za-z_][A-Za-z0-9_]*
 *
 * Known sections handled structurally:
 *   [project]             -> ProjectSection
 *   [simulation]          -> SimulationSection + MoleculeEntry sub-blocks
 *   [export]              -> ExportSection
 *   [material]            -> MaterialSection          (WO-VSIM-03B)
 *   [run]                 -> RunSection               (WO-VSIM-03B)
 *   [environment]         -> EnvironmentSection       (WO-VSIM-03B)
 *   [excite.<type>]       -> ExciteSection registry   (WO-VSIM-03B)
 *   [observe]             -> ObserveSection           (WO-VSIM-03B)
 *   [[override.particle]] -> vector<ParticleOverrideEntry> (WO-VSIM-03B)
 *   [[raw.object]]        -> vector<RawObjectEntry>   (WO-VSIM-03B)
 *   [defaults.run]        -> GoldenDefaultsSection::run
 *   [defaults.analysis]   -> GoldenDefaultsSection::analysis
 *   [test.<name>]         -> GoldenTestEntry (appended to golden_tests)
 *   [test.<name>.run]     -> GoldenTestEntry::run
 *   [test.<name>.analysis]-> GoldenTestEntry::analysis
 *   [suite]               -> SuiteSection
 *   [suite.limits]        -> SuiteSection limits
 *   [suite.smoke]         -> SuiteSection smoke subset
 *   [report]              -> GoldenReportSection
 *
 * WO-VSIM-61C/61D analysis pipeline sections (analysis-only .vsim scripts):
 *   [system]                  -> VsimDocument::pipeline_system
 *   [analysis.structure]      -> VsimDocument::pipeline_structure
 *   [analysis.sampling]       -> VsimDocument::pipeline_sampling
 *   [analysis.scale_sampling] -> VsimDocument::pipeline_scale_sampling  (WO-61D)
 *   [analysis.inference]      -> VsimDocument::pipeline_inference        (WO-61D v2)
 *   [inference]               -> VsimDocument::pipeline_inference        (v1 deprecated alias)
 *   [output]                  -> VsimDocument::pipeline_output
 *
 * Unknown sections are captured in VsimDocument::raw_sections.
 *
 * WO-56C  |  v5.0.0-beta.7  |  WO-VSIM-03B  |  beta-8
 */

#include "vsim_document.hpp"
#include <string>
#include <stdexcept>

namespace vsim {

// ============================================================================
// ParseError  -  carries line number for diagnostics
// ============================================================================

struct ParseError : std::runtime_error {
	int line_number;
	ParseError(int line, const std::string& msg)
		: std::runtime_error("vsim parse error at line " + std::to_string(line) + ": " + msg)
		, line_number(line) {}
};

// ============================================================================
// Parser
// ============================================================================

class VsimParser {
public:
	/**
	 * Parse a .vsim file from disk.
	 * Throws ParseError on structural problems.
	 * Missing optional fields are filled with defaults.
	 */
	static VsimDocument parse_file(const std::string& path);

	/**
	 * Parse .vsim content from a string (useful for tests).
	 * source_path is stored in document.source_path (may be empty).
	 */
	static VsimDocument parse_string(const std::string& content,
									 const std::string& source_path = "<string>");

private:
	VsimParser() = default;

	void parse_content(const std::string& content);

	void handle_section(const std::string& section_name, int line_no);
	void handle_key_value(const std::string& key, const std::string& raw_value, int line_no);

	void apply_project_key(const std::string& key, const Value& val, int line_no);
	void apply_seed_key(const std::string& key, const Value& val, int line_no);
	void apply_simulation_key(const std::string& key, const Value& val, int line_no);
	void apply_molecule_key(const std::string& key, const Value& val, int line_no);
	void apply_export_key(const std::string& key, const Value& val, int line_no);
	void apply_export_visual_key(const std::string& key, const Value& val, int line_no);
	void apply_export_demo_key(const std::string& key, const Value& val, int line_no);
	void apply_visual_key(const std::string& key, const Value& val, int line_no);
	void apply_visual_external_key(const std::string& key, const Value& val, int line_no);
	void apply_visual_workspace_key(const std::string& key, const Value& val, int line_no);
	void apply_room_key(const std::string& key, const Value& val, int line_no);
	void parse_show_directive(const std::string& line, int line_no);
	void parse_print_console_directive(const std::string& line, int line_no);  // WO-85A
	void apply_features_key(const std::string& key, const Value& val, int line_no);  // WO-85B
	void apply_open_key(const std::string& key, const Value& val, int line_no);
	void apply_open_advanced_key(const std::string& key, const Value& val, int line_no);
	void apply_variance_key(const std::string& key, const Value& val, int line_no);
	void apply_n_evolution_key(const std::string& key, const Value& val, int line_no);
	void apply_while_key(const std::string& key, const Value& val, int line_no);
	void apply_batch_key(const std::string& key, const Value& val, int line_no);

	// WO-57B: PBC block appliers
	void apply_cell_key(const std::string& key, const Value& val, int line_no);
	void apply_boundary_key(const std::string& key, const Value& val, int line_no);
	void apply_pbc_key(const std::string& key, const Value& val, int line_no);

	// WO-VSIM-03B: intent-based authoring appliers
	void apply_material_key(const std::string& key, const Value& val, int line_no);
	void apply_distribution_key(const std::string& key, const Value& val, int line_no);
	void apply_run_key(const std::string& key, const Value& val, int line_no);
	void apply_dense_record_key(const std::string& key, const Value& val, int line_no);  // WO-28MAR
	void apply_environment_key(const std::string& key, const Value& val, int line_no);
	void apply_chemistry_key(const std::string& key, const Value& val, int line_no);
	void apply_chemplus_key(const std::string& key, const Value& val, int line_no);  // WO-84T
	void apply_excite_key(const std::string& key, const Value& val, int line_no);
	void apply_observe_key(const std::string& key, const Value& val, int line_no);
	void apply_override_particle_key(const std::string& key, const Value& val, int line_no);
	void apply_raw_object_key(const std::string& key, const Value& val, int line_no);
	void apply_dissolution_key(const std::string& key, const Value& val, int line_no);  // WO-56D

	// WO-VSIM-04A: isomer analysis appliers
	void apply_isomer_analysis_key(const std::string& key, const Value& val, int line_no);
	void apply_isomer_generator_key(const std::string& key, const Value& val, int line_no);
	void apply_isomer_tracking_key(const std::string& key, const Value& val, int line_no);

	// WO-VSIM-61C: analysis pipeline section appliers
	void apply_pipeline_system_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_structure_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_sampling_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_inference_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_output_key(const std::string& key, const Value& val, int line_no);

	// WO-VSIM-61D: scale sampling applier
	void apply_pipeline_scale_sampling_key(const std::string& key, const Value& val, int line_no);

	// WO-VSIM-62A: empirical verification appliers
	void apply_pipeline_verify_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_structure_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_rdf_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_msd_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_mass_key(const std::string& key, const Value& val, int line_no);
	// WO-VSIM-LAMMPS-VERIFY-01: extended verify sub-sections
	void apply_pipeline_verify_compare_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_thresholds_key(const std::string& key, const Value& val, int line_no);
	void apply_pipeline_verify_outputs_key(const std::string& key, const Value& val, int line_no);

	// WO-75A: IKK end-tag enrichment  [analysis.ikk_end_tag]
	void apply_ikk_end_tag_key(const std::string& key, const Value& val, int line_no);

	// WO-75B Phase 1: IKK I-vector  [analysis.ivec]
	void apply_ivec_key(const std::string& key, const Value& val, int line_no);

	// WO-76 Steps 3+5: MCF-CAI object state grid  [object.<layer>.<basis>]
	// apply_mcf_cai_key() is called with the current active layer+basis stored
	// in current_mcf_layer_ / current_mcf_basis_ set by handle_section().
	void apply_mcf_cai_key(const std::string& key, const Value& val, int line_no);
	// WO-VSIM-COLLISION-AUDIT-01 / WO-VSIM-COLLIDER-MAP-01: collision section appliers
	void apply_collision_key(const std::string& key, const Value& val, int line_no);
	void apply_collision_case_key(const std::string& key, const Value& val, int line_no);
	void apply_collision_reference_key(const std::string& key, const Value& val, int line_no);
	void apply_collision_audit_key(const std::string& key, const Value& val, int line_no);
	void apply_collision_outputs_key(const std::string& key, const Value& val, int line_no);

	// Golden suite section appliers
	void apply_golden_run_config(GoldenTestRunConfig& cfg,
								 const std::string& key, const Value& val);
	void apply_golden_analysis(GoldenTestAnalysis& ana,
							   const std::string& key, const Value& val);
	void apply_defaults_run_key(const std::string& key, const Value& val);
	void apply_defaults_analysis_key(const std::string& key, const Value& val);
	void apply_test_key(const std::string& key, const Value& val);
	void apply_test_run_key(const std::string& key, const Value& val);
	void apply_test_analysis_key(const std::string& key, const Value& val);
	void apply_suite_key(const std::string& key, const Value& val);
	void apply_suite_limits_key(const std::string& key, const Value& val);
	void apply_suite_smoke_key(const std::string& key, const Value& val);
	void apply_report_key(const std::string& key, const Value& val);

	// WO-VSEPR-SIM Extreme Addendum
	void apply_identity_matrices_key(const std::string& key, const Value& val);
	void apply_verification_key(const std::string& key, const Value& val);
	void apply_verification_modes_key(const std::string& key, const Value& val);
	void apply_extras_togglescale_key(const std::string& key, const Value& val);

	// WO-66N  Constructor objects + batching + upper-block references
	void apply_objects_constructor_line(const std::string& lhs, const std::string& rhs, int line_no);
	void apply_objects_batch_key(const std::string& key, const Value& val, int line_no);

	// WO-66O  Organic/peptide scale diagnostics
	void apply_diagnostics_organic_key(const std::string& key, const Value& val, int line_no);

	// WO-66P  Crystal/PBC functional constructor (also maps legacy [cell]/[pbc] keys)
	void apply_crystal_constructor_key(const std::string& key, const Value& val, int line_no);

	// WO-66Q  Non-molecular objects (geometry/surface/source/sink/ambient)
	void apply_nm_geometry_key(const std::string& key, const Value& val, int line_no);
	void apply_nm_surface_key(const std::string& key, const Value& val, int line_no);
	void apply_nm_source_key(const std::string& key, const Value& val, int line_no);
	void apply_nm_sink_key(const std::string& key, const Value& val, int line_no);
	void apply_nm_ambient_key(const std::string& key, const Value& val, int line_no);

	// WO-67N/67O  Bridge constructors (DEMBridge / FEABridge)
	void apply_bridge_constructor_line(const std::string& lhs, const std::string& rhs, int line_no);

	// WO-72C  .dynx session-archive emission control
	void apply_dynx_key(const std::string& key, const Value& val, int line_no);

	// WO-72U/72L/72S  smart loop and selection sections
	void apply_until_key (const std::string& key, const Value& val, int line_no);
	void apply_loop_key  (const std::string& key, const Value& val, int line_no);
	void apply_select_key(const std::string& key, const Value& val, int line_no);

	// WO-OUTPUT-PHASE2  physical probe sections
	void apply_thermal_probe_key(const std::string& key, const Value& val, int line_no);
	void apply_field_probe_key  (const std::string& key, const Value& val, int line_no);

	// WO-75D  biological object layer
	void apply_bio_key(const std::string& key, const Value& val, int line_no);

	// WO-NL0A  continuous random-materials discovery
	void apply_discovery_key(const std::string& key, const Value& val, int line_no);

	// WO-OUTPUT-PHASE2  batch expand / axis subsections
	void apply_batch_expand_key(const std::string& key, const Value& val, int line_no);
	void apply_batch_axis_key  (const std::string& key, const Value& val, int line_no);

	// Surface Analysis Examples — [[object.surface]] parser
	void apply_surface_key(const std::string& key, const Value& val, int line_no);

	// WO-89 PILLARS appliers
	void apply_pillar_key(const std::string& key, const Value& val, int line_no);
	void apply_species_registry_key(const std::string& key, const Value& val, int line_no);
	void apply_reaction_network_key(const std::string& key, const Value& val, int line_no);
	void apply_reaction_channel_key(const std::string& key, const Value& val, int line_no);
	void apply_reaction_passivation_key(const std::string& key, const Value& val, int line_no);
	void apply_reaction_stage_key(const std::string& key, const Value& val, int line_no);
	void apply_metal_center_key(const std::string& key, const Value& val, int line_no);
	void apply_descriptor_electronic_key(const std::string& key, const Value& val, int line_no);
	void apply_catalysis_key(const std::string& key, const Value& val, int line_no);
	void apply_thermodynamics_key(const std::string& key, const Value& val, int line_no);
	void apply_ignition_key(const std::string& key, const Value& val, int line_no);
	void apply_audit_conservation_key(const std::string& key, const Value& val, int line_no);
	void apply_audit_state_rules_key(const std::string& key, const Value& val, int line_no);
	void apply_audit_acceptance_key(const std::string& key, const Value& val, int line_no);



	Value parse_value(std::string raw, int line_no);
	std::string strip_comment(const std::string& line);
	std::string trim(const std::string& s);

	VsimDocument  doc_;
	std::string   current_section_;
	bool          in_molecule_block_ = false;  // inside [[simulation.molecule]] sub-block
	bool          in_double_bracket_ = false;  // true when section header was [[ ]] not [ ]

	// WO-VSIM-03B parse state
	std::string   current_excite_type_;        // active [excite.<type>] subtype (empty = none)
	std::string   current_distribution_name_;  // active [distribution.<name>] card
	bool          in_override_particle_ = false; // inside [[override.particle]] block
	bool          in_raw_object_        = false; // inside [[raw.object]] block

	// Golden suite parse state
	std::string   current_test_name_;          // active [test.<name>] key (empty = none)
	bool          in_test_run_block_      = false;
	bool          in_test_analysis_block_ = false;
	bool          in_suite_limits_        = false;
	bool          in_suite_smoke_         = false;

	// WO-66N/66Q parse state
	std::string   current_nm_object_type_;  // "geometry"|"surface"|"source"|"sink"|"ambient"
	std::string   current_nm_object_path_;  // current nm-object path being assembled
	std::string   current_batch_base_;      // active [objects.batch] base path

	// WO-76 Step 5 parse state  — active MCF-CAI section target
	int  current_mcf_layer_ { -1 };  // McfLayer int, -1 = not in an mcf-cai section
	int  current_mcf_basis_ { -1 };  // CaiBasis int, -1 = not in an mcf-cai section

	// Surface Analysis Examples — [[object.surface]] parse state
	bool in_surface_block_ { false };  // inside [[object.surface]] block

	// WO-89 PILLARS parse state
	bool in_reaction_channel_ { false };  // inside [[reaction.channel]] block
	bool in_reaction_stage_   { false };  // inside [[reaction.stage]] block
};

} // namespace vsim
