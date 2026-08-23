// WO-72V — property table implementation
#include "vsim/cache/property_table.hpp"

namespace vsim::cache {

void PropertyTable::add(const PropertyRow& row) {
	table_[row.formula] = row;
}

std::optional<PropertyRow> PropertyTable::find(const std::string& formula) const {
	auto it = table_.find(formula);
	if (it == table_.end()) return std::nullopt;
	return it->second;
}

void PropertyTable::load_builtins() {
	//                   formula  rho    k_th   Cp     sigma_el  Eg_eV  Tm     Tb
	add({"NaCl",  2.165,  6.5,   0.864,  1e-10,   8.5,  1074.0, 1738.0});
	add({"Si",    2.330, 149.0,  0.705,   4e-4,   1.12,  1687.0, 3538.0});
	add({"Fe",    7.874,  80.4,  0.449,  1e7,    -1.0,   1811.0, 3134.0});
	add({"Al",    2.700, 237.0,  0.897,  3.8e7,  -1.0,    933.0, 2743.0});
	add({"Cu",    8.960, 401.0,  0.385,  6.0e7,  -1.0,   1358.0, 2835.0});
	add({"MgO",   3.580,  45.0,  0.879,  1e-11,   7.8,  3125.0, 3873.0});
	add({"TiO2",  4.230,   4.8,  0.690,  1e-12,   3.0,  2116.0, 3245.0});
	add({"C",     3.515, 900.0,  0.502,  1e-14,   5.5,  3800.0, 4300.0});
	add({"SiO2",  2.200,   1.4,  0.703,  1e-18,   9.0,  1986.0, 2503.0});
	add({"Al2O3", 3.987,  30.0,  0.880,  1e-13,   8.8,  2345.0, 3250.0});
}

} // namespace vsim::cache
