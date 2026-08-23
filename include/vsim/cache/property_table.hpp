#pragma once
// WO-72V — density / thermal / conductivity property table
// Tabulated physical properties per material formula.
#include <string>
#include <unordered_map>
#include <optional>

namespace vsim::cache {

struct PropertyRow {
	std::string formula;
	double density_gcc        = 0.0; // g/cm³  (0 = unknown)
	double thermal_cond_W_mK  = 0.0; // W/(m·K)
	double heat_cap_J_gK      = 0.0; // J/(g·K)
	double electrical_cond_Sm = 0.0; // S/m     (0 = unknown)
	double bandgap_eV         = -1.0; // -1 = metal / not applicable
	double melting_K          = 0.0;
	double boiling_K          = 0.0;
};

class PropertyTable {
public:
	void add(const PropertyRow& row);
	std::optional<PropertyRow> find(const std::string& formula) const;
	std::size_t size() const { return table_.size(); }
	void load_builtins();

private:
	std::unordered_map<std::string, PropertyRow> table_;
};

} // namespace vsim::cache
