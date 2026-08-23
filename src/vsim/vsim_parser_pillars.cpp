// vsim_parser_pillars.cpp - WO-89 Three Metallic Pillars appliers
//
// Adds parser appliers for the new [pillar], [species.registry], [reaction.*],
// [metal_center], [descriptor.electronic], [catalysis], [thermodynamics],
// [ignition], and [audit.*] sections introduced by the Three Metallic Pillars
// work. These appliers populate the corresponding structs on VsimDocument so
// that downstream runners can implement the pillar models without falling back
// to raw_sections.

#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_value.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace vsim {

namespace {

std::string local_trim(const std::string& s) {
	const char* ws = " \t\n\r";
	size_t a = s.find_first_not_of(ws);
	if (a == std::string::npos) return {};
	size_t b = s.find_last_not_of(ws);
	return s.substr(a, b - a + 1);
}

template<typename T>
std::vector<T> parse_numeric_list(const Value& val) {
	std::vector<T> out;
	if (value_is_list(val)) {
		for (const auto& s : as_list(val)) {
			try {
				if constexpr (std::is_integral_v<T>) {
					out.push_back(static_cast<T>(std::stoll(s)));
				} else {
					out.push_back(static_cast<T>(std::stod(s)));
				}
			} catch (...) { /* skip non-numeric entries */ }
		}
	} else {
		std::string raw = to_string(val);
		std::istringstream ss(raw);
		std::string tok;
		while (std::getline(ss, tok, ',')) {
			tok = local_trim(tok);
			if (tok.empty()) continue;
			try {
				if constexpr (std::is_integral_v<T>) {
					out.push_back(static_cast<T>(std::stoll(tok)));
				} else {
					out.push_back(static_cast<T>(std::stod(tok)));
				}
			} catch (...) {}
		}
	}
	return out;
}

std::string as_str_trim(const Value& val) {
	if (value_is_string(val)) return as_string(val);
	std::string raw = to_string(val);
	raw.erase(raw.begin(), std::find_if(raw.begin(), raw.end(), [](unsigned char ch) { return !std::isspace(ch); }));
	raw.erase(std::find_if(raw.rbegin(), raw.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), raw.end());
	return raw;
}

std::vector<std::string> parse_string_list(const Value& val) {
	if (value_is_list(val)) return as_list(val);
	std::vector<std::string> out;
	std::string raw = to_string(val);
	std::istringstream ss(raw);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		tok = local_trim(tok);
		if (tok.empty()) continue;
		if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"')
			tok = tok.substr(1, tok.size() - 2);
		out.push_back(tok);
	}
	return out;
}

} // namespace

void VsimParser::apply_pillar_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto& p = doc_.pillar;
	p.present = true;
	if      (key == "id")                 p.id                = as_str_trim(val);
	else if (key == "name")               p.name              = as_str_trim(val);
	else if (key == "model")              p.model             = as_str_trim(val);
	else if (key == "spatial_authority")  p.spatial_authority = as_str_trim(val);
	else if (key == "event_authority")    p.event_authority   = as_str_trim(val);
	else if (key == "conservation_mode")  p.conservation_mode = as_str_trim(val);
}

void VsimParser::apply_species_registry_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto& r = doc_.species_registry;
	r.present = true;
	if (key == "species" || key == "list") {
		r.species = parse_string_list(val);
	}
}

void VsimParser::apply_reaction_network_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto& rn = doc_.reaction_network;
	rn.present = true;
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	if      (key == "id")                   rn.id                   = as_str_trim(val);
	else if (key == "selection")            rn.selection            = as_str_trim(val);
	else if (key == "rate_law")             rn.rate_law             = as_str_trim(val);
	else if (key == "energy_model")         rn.energy_model         = as_str_trim(val);
	else if (key == "spatial_index")        rn.spatial_index        = as_str_trim(val);
	else if (key == "collision_model")      rn.collision_model      = as_str_trim(val);
	else if (key == "three_body_support")   rn.three_body_support   = to_bool();
	else if (key == "reject_on_overlap")    rn.reject_on_overlap    = to_bool();
	else if (key == "reject_on_deficit")    rn.reject_on_deficit    = to_bool();
}

void VsimParser::apply_reaction_channel_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };

	if (doc_.reaction_network.channels.empty())
		doc_.reaction_network.channels.emplace_back();
	auto& c = doc_.reaction_network.channels.back();

	if      (key == "id")                    c.id                    = as_str_trim(val);
	else if (key == "stage")                 c.stage                 = as_str_trim(val);
	else if (key == "from_state")            c.from_state            = as_str_trim(val);
	else if (key == "to_state")              c.to_state              = as_str_trim(val);
	else if (key == "reactants")             c.reactants             = parse_string_list(val);
	else if (key == "products")              c.products              = parse_string_list(val);
	else if (key == "third_body")            c.third_body            = as_str_trim(val);
	else if (key == "binding_mode")          c.binding_mode          = as_str_trim(val);
	else if (key == "neighbor_rule")         c.neighbor_rule         = as_str_trim(val);
	else if (key == "geometry_from")         c.geometry_from         = as_str_trim(val);
	else if (key == "geometry_to")           c.geometry_to           = as_str_trim(val);
	else if (key == "pre_exponential")       c.pre_exponential       = n();
	else if (key == "barrier_eV")            c.barrier_eV            = n();
	else if (key == "energy_delta_eV")       c.energy_delta_eV       = n();
	else if (key == "temperature_power")     c.temperature_power     = n();
	else if (key == "site_radius_ang")       c.site_radius_ang       = n();
	else if (key == "coordination_delta")    c.coordination_delta    = i();
	else if (key == "electron_delta_proxy")  c.electron_delta_proxy  = i();
	else if (key == "oxidation_state_delta") c.oxidation_state_delta = i();
	else if (key == "max_depth_layers")      c.max_depth_layers      = i();
	else if (key == "increments_turnover")   c.increments_turnover   = to_bool();
	else if (key == "enabled")               c.enabled               = to_bool();
}

void VsimParser::apply_reaction_passivation_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto& p = doc_.reaction_passivation;
	p.present = true;
	if      (key == "metric")                p.metric                = as_str_trim(val);
	else if (key == "model")                 p.model                 = as_str_trim(val);
	else if (key == "alpha_per_layer")       p.alpha_per_layer       = n();
	else if (key == "minimum_accessibility") p.minimum_accessibility = n();
	else if (key == "affects_channels")      p.affects_channels      = parse_string_list(val);
	else if (key == "enabled")               p.present               = value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true");
}

void VsimParser::apply_reaction_stage_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto i = [&](){ return static_cast<int>(numeric(val)); };

	if (doc_.reaction_stages.empty())
		doc_.reaction_stages.emplace_back();
	auto& st = doc_.reaction_stages.back();

	if      (key == "id")                 st.id                 = as_str_trim(val);
	else if (key == "start_step")         st.start_step         = i();
	else if (key == "end_step")           st.end_step           = i();
	else if (key == "environment_T_K")    st.environment_T_K    = n();
	else if (key == "pressure_proxy_GPa") st.pressure_proxy_GPa = n();
	else if (key == "active_reservoirs")  st.active_reservoirs  = parse_string_list(val);
	else if (key == "remove_unbound")     st.remove_unbound     = parse_string_list(val);
}

void VsimParser::apply_metal_center_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& m = doc_.metal_center;
	m.present = true;
	if      (key == "element")                  m.element                  = as_str_trim(val);
	else if (key == "formal_charge_initial")    m.formal_charge_initial    = i();
	else if (key == "oxidation_state_initial")  m.oxidation_state_initial  = i();
	else if (key == "electron_count_initial")   m.electron_count_initial   = i();
	else if (key == "coordination_limit")       m.coordination_limit       = i();
	else if (key == "reference_geometry")       m.reference_geometry       = as_str_trim(val);
	else if (key == "active_geometry_set")      m.active_geometry_set      = parse_string_list(val);
	else if (key == "state_equivalence")        m.state_equivalence        = as_str_trim(val);
	else if (key == "track_ligand_field")       m.track_ligand_field       = to_bool();
	else if (key == "track_steric_load")        m.track_steric_load        = to_bool();
	else if (key == "track_donation_proxy")     m.track_donation_proxy     = to_bool();
	else if (key == "track_backbonding_proxy")  m.track_backbonding_proxy  = to_bool();
}

void VsimParser::apply_descriptor_electronic_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& d = doc_.descriptor_electronic;
	d.present = true;
	if      (key == "model")                    d.model                    = as_str_trim(val);
	else if (key == "calibration")              d.calibration              = as_str_trim(val);
	else if (key == "formal_charge")            d.formal_charge            = to_bool();
	else if (key == "oxidation_state")          d.oxidation_state          = to_bool();
	else if (key == "electron_count_proxy")     d.electron_count_proxy     = to_bool();
	else if (key == "ligand_field_proxy_eV")    d.ligand_field_proxy_eV    = to_bool();
	else if (key == "donation_proxy")           d.donation_proxy           = to_bool();
	else if (key == "backbonding_proxy")        d.backbonding_proxy        = to_bool();
	else if (key == "bond_order_proxy")         d.bond_order_proxy         = to_bool();
	else if (key == "publish_as_wavefunction")  d.publish_as_wavefunction  = to_bool();
	else if (key == "enabled")                  d.present                  = to_bool();
}

void VsimParser::apply_catalysis_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& c = doc_.catalysis;
	c.present = true;
	if      (key == "active_center_count")      c.active_center_count      = i();
	else if (key == "turnover_event")           c.turnover_event           = as_str_trim(val);
	else if (key == "product_species")          c.product_species          = as_str_trim(val);
	else if (key == "recovered_state")          c.recovered_state          = as_str_trim(val);
	else if (key == "state_equivalence_fields") c.state_equivalence_fields = parse_string_list(val);
	else if (key == "exact_coordinate_return")  c.exact_coordinate_return  = to_bool();
	else if (key == "stall_window_steps")       c.stall_window_steps       = i();
}

void VsimParser::apply_thermodynamics_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& t = doc_.thermodynamics;
	t.present = true;
	if      (key == "pressure_model")           t.pressure_model           = as_str_trim(val);
	else if (key == "temperature_model")        t.temperature_model        = as_str_trim(val);
	else if (key == "heat_capacity_model")      t.heat_capacity_model      = as_str_trim(val);
	else if (key == "reaction_heat_coupling")   t.reaction_heat_coupling   = n();
	else if (key == "radiative_loss_enabled")   t.radiative_loss_enabled   = to_bool();
	else if (key == "wall_loss_enabled")        t.wall_loss_enabled        = to_bool();
	else if (key == "minimum_temperature_K")    t.minimum_temperature_K    = n();
	else if (key == "maximum_temperature_K")    t.maximum_temperature_K    = n();
}

void VsimParser::apply_ignition_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& ig = doc_.ignition;
	ig.present = true;
	if      (key == "metric")                   ig.metric                  = as_str_trim(val);
	else if (key == "baseline_window_steps")    ig.baseline_window_steps   = i();
	else if (key == "threshold_delta_K")        ig.threshold_delta_K       = n();
	else if (key == "secondary_metric")         ig.secondary_metric        = as_str_trim(val);
	else if (key == "secondary_threshold")      ig.secondary_threshold     = n();
	else if (key == "record_ignition_delay")    ig.record_ignition_delay   = to_bool();
}

void VsimParser::apply_audit_conservation_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto n = [&](){ return numeric(val); };
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& a = doc_.audit_conservation;
	a.present = true;
	if      (key == "elements")               a.elements               = parse_string_list(val);
	else if (key == "charge")                 a.charge                 = to_bool();
	else if (key == "energy")                 a.energy                 = as_str_trim(val);
	else if (key == "strict")                 a.strict                 = to_bool();
	else if (key == "check_every_n_steps")    a.check_every_n_steps    = i();
	else if (key == "relative_tolerance")     a.relative_tolerance     = n();
	else if (key == "absolute_tolerance")     a.absolute_tolerance     = n();
	else if (key == "reject_invalid_event")   a.reject_invalid_event   = to_bool();
}

void VsimParser::apply_audit_state_rules_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto i = [&](){ return static_cast<int>(numeric(val)); };
	auto to_bool = [&](){ return value_is_bool(val) ? as_bool(val) : (as_str_trim(val) == "true"); };
	auto& r = doc_.audit_state_rules;
	r.present = true;
	if      (key == "coordination_min")             r.coordination_min             = i();
	else if (key == "coordination_max")             r.coordination_max             = i();
	else if (key == "allowed_oxidation_states")     r.allowed_oxidation_states     = parse_numeric_list<int>(val);
	else if (key == "allowed_formal_charges")       r.allowed_formal_charges       = parse_numeric_list<int>(val);
	else if (key == "allowed_geometry_classes")     r.allowed_geometry_classes     = parse_string_list(val);
	else if (key == "reject_impossible_transition") r.reject_impossible_transition = to_bool();
}

void VsimParser::apply_audit_acceptance_key(const std::string& key, const Value& val, int /*line_no*/)
{
	auto& a = doc_.audit_acceptance;
	a.present = true;
	if (key == "requirements" || key == "rules") {
		a.requirements = parse_string_list(val);
	}
}

} // namespace vsim
