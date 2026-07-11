/**
 * energy_units.cpp  —  Energy formatting implementation
 * ======================================================
 * VSEPR-SIM 3.0.1
 */

#include "core/energy_units.hpp"

#include <sstream>
#include <iomanip>

namespace vsepr {

// ============================================================================
// Energy::format()
// ============================================================================

std::string Energy::format(EnergyUnit u, int precision) const {
    std::ostringstream oss;
    oss << std::setprecision(precision) << as(u) << " " << energy_unit_symbol(u);
    return oss.str();
}

// ============================================================================
// Energy::format_all()
// ============================================================================

std::string Energy::format_all(int precision) const {
    std::ostringstream oss;
    oss << std::setprecision(precision);
    oss << as_hartree() << " Ha  |  "
        << as_ev()      << " eV  |  "
        << as_kcalmol() << " kcal/mol  |  "
        << as_kjmol()   << " kJ/mol";
    return oss.str();
}

} // namespace vsepr
