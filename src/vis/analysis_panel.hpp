#pragma once

#include "renderer_base.hpp"
#include "picking.hpp"
#include "ui_theme.hpp"
#include <imgui.h>
#include <string>
#include <vector>

namespace vsepr {
namespace render {

/**
 * Analysis panel with hover tooltips
 * 
 * Shows detailed information when hovering over atoms/bonds:
 * - **Atoms**: Element name, symbol, Z, mass, electronegativity, radius, neighbors
 * - **Bonds**: Length, type, atoms involved
 * 
 * Windows 11 style UI with rich formatting.
 */
class AnalysisPanel {
public:
    AnalysisPanel();
    
    /**
     * Update panel (call each frame)
     * 
     * @param geom Molecular geometry
     * @param mouse_x Mouse X coordinate
     * @param mouse_y Mouse Y coordinate
     * @param screen_width Viewport width
     * @param screen_height Viewport height
     * @param view_matrix Camera view matrix
     * @param proj_matrix Camera projection matrix
     */
    void update(const AtomicGeometry& geom,
               float mouse_x, float mouse_y,
               int screen_width, int screen_height,
               const float* view_matrix,
               const float* proj_matrix);
    
    /**
     * Render ImGui panel
     */
    void render();
    
    /**
     * Enable/disable tooltips
     */
    void set_tooltips_enabled(bool enabled) { tooltips_enabled_ = enabled; }
    bool are_tooltips_enabled() const { return tooltips_enabled_; }
    
    /**
     * Set atom scale (for picking)
     */
    void set_atom_scale(float scale) { picker_.set_atom_scale(scale); }
    
    /**
     * Set bond radius (for picking)
     */
    void set_bond_radius(float radius) { picker_.set_bond_radius(radius); }
    
private:
    MoleculePicker picker_;
    
    bool tooltips_enabled_ = true;
    
    // Current hover state
    std::optional<AtomPick> current_atom_;
    std::optional<BondPick> current_bond_;
    bool atom_is_closer_ = false;
    
    // Cached geometry (for neighbor detection)
    const AtomicGeometry* cached_geom_ = nullptr;
    
    // Render functions
    void render_atom_tooltip(const AtomicGeometry& geom, const AtomPick& pick);
    void render_bond_tooltip(const AtomicGeometry& geom, const BondPick& pick);
    
    // Utility functions
    std::string get_element_name(int Z) const;
    std::string get_element_symbol(int Z) const;
    float get_element_mass(int Z) const;
    float get_electronegativity(int Z) const;
    int count_neighbors(const AtomicGeometry& geom, int atom_index) const;
    std::vector<int> get_bonded_atoms(const AtomicGeometry& geom, int atom_index) const;
};

} // namespace render
} // namespace vsepr
