#pragma once
/**
 * named_regions.hpp
 *
 * Electrohydrodynamic Simulation — Stage 2: Domain Extraction
 *
 * Named-region registry for mapping CAD body names to solver boundary
 * condition zones.  Every surface that needs a BC gets a unique tag.
 */

#include <string>
#include <vector>
#include <unordered_map>

namespace vsepr {
namespace ehd {
namespace domain {

// ============================================================================
// Surface Tag
// ============================================================================

enum class SurfaceRole {
    WALL_NOSLIP,
    WALL_INSULATING,
    ELECTRODE_SURFACE,
    INLET,
    OUTLET,
    SYMMETRY,
    PERIODIC
};

struct SurfaceTag {
    int         id;
    std::string name;       // e.g. "wall_inner", "electrode_pos_surface"
    SurfaceRole role;
    std::string parent_body;

    // Dirichlet value (voltage, velocity component, etc.)
    double dirichlet_value = 0.0;
    bool   has_dirichlet   = false;
};

// ============================================================================
// Named Region Registry
// ============================================================================

class NamedRegionRegistry {
public:
    int add_surface(const std::string& name, SurfaceRole role,
                    const std::string& parent_body) {
        int id = static_cast<int>(tags_.size()) + 1;
        tags_.push_back({id, name, role, parent_body, 0.0, false});
        index_[name] = tags_.size() - 1;
        return id;
    }

    void set_dirichlet(const std::string& name, double value) {
        auto it = index_.find(name);
        if (it != index_.end()) {
            tags_[it->second].dirichlet_value = value;
            tags_[it->second].has_dirichlet   = true;
        }
    }

    const SurfaceTag* find(const std::string& name) const {
        auto it = index_.find(name);
        if (it != index_.end()) return &tags_[it->second];
        return nullptr;
    }

    const std::vector<SurfaceTag>& all() const { return tags_; }

    std::vector<const SurfaceTag*> by_role(SurfaceRole role) const {
        std::vector<const SurfaceTag*> result;
        for (const auto& t : tags_) {
            if (t.role == role) result.push_back(&t);
        }
        return result;
    }

    size_t size() const { return tags_.size(); }

private:
    std::vector<SurfaceTag>                     tags_;
    std::unordered_map<std::string, size_t>     index_;
};

// ============================================================================
// Default Region Setup
// ============================================================================

/**
 * Build default named regions for a helical EHD reactor.
 */
inline NamedRegionRegistry build_default_regions(const EHDParameters& p) {
    NamedRegionRegistry reg;

    reg.add_surface("inlet",                 SurfaceRole::INLET,             "fluid_domain");
    reg.add_surface("outlet",                SurfaceRole::OUTLET,            "fluid_domain");
    reg.add_surface("wall_inner",            SurfaceRole::WALL_NOSLIP,       "tube_wall");
    reg.add_surface("wall_outer_insulating", SurfaceRole::WALL_INSULATING,   "tube_wall");

    reg.add_surface("electrode_pos_surface", SurfaceRole::ELECTRODE_SURFACE, "electrode_pos");
    reg.set_dirichlet("electrode_pos_surface", p.voltage_pos);

    reg.add_surface("electrode_neg_surface", SurfaceRole::ELECTRODE_SURFACE, "electrode_neg");
    reg.set_dirichlet("electrode_neg_surface", p.voltage_neg);

    return reg;
}

} // namespace domain
} // namespace ehd
} // namespace vsepr
