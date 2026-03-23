#pragma once
/**
 * cg_split_view.hpp — Split-Screen Atomistic ↔ Coarse-Grained Inspector
 *
 * Left pane:   Atomistic ball-and-stick (atom-resolved)
 * Right pane:  Coarse-grained bead spheres with topology overlay
 *
 * Interactive cross-highlighting:
 *   - Click/hover bead → highlight parent atoms on left
 *   - Click/hover atom → highlight owning bead on right
 *
 * Anti-black-box design: every mapping decision is visually traceable.
 * Deterministic: same input always produces same visual output.
 *
 * Features:
 *   - Split-screen rendering via GL viewports
 *   - Cross-pane selection highlighting
 *   - Bead-type coloring
 *   - COM↔COG projection toggle
 *   - Centroid-to-bead arrows (optional)
 *   - Topology overlay on right pane
 *   - Side panel: mapping residual metrics, conservation report
 *   - Click bead → atom indices and labels listed
 *   - Click atom → assigned bead type and rule id shown
 *
 * Usage (from app code):
 *   CGSplitView view;
 *   view.set_data(atomistic_geom, bead_system, mapping_scheme);
 *   // in render loop:
 *   view.update(mouse_x, mouse_y, width, height);
 *   view.render(width, height);
 */

#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/mapping/mapping_rule.hpp"
#include "vis/renderer_base.hpp"
#include <imgui.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace coarse_grain {
namespace vis {

// ============================================================================
// Selection State
// ============================================================================

/**
 * Selection source — which pane originated the selection.
 */
enum class SelectionSource {
    NONE,
    ATOMISTIC_PANE,     // User clicked/hovered an atom
    BEAD_PANE           // User clicked/hovered a bead
};

/**
 * Cross-pane selection — tracks what is currently highlighted.
 */
struct CrossSelection {
    SelectionSource source = SelectionSource::NONE;
    int             selected_atom_index = -1;    // Set when source == ATOMISTIC_PANE
    int             selected_bead_index = -1;    // Set when source == BEAD_PANE

    // Derived highlights (populated by resolve())
    std::vector<uint32_t> highlighted_atoms;     // Atoms to highlight (left pane)
    std::vector<uint32_t> highlighted_beads;     // Beads to highlight (right pane)

    void clear() {
        source = SelectionSource::NONE;
        selected_atom_index = -1;
        selected_bead_index = -1;
        highlighted_atoms.clear();
        highlighted_beads.clear();
    }
};

// ============================================================================
// Bead Color Palette
// ============================================================================

struct BeadColor {
    float r, g, b;
};

/**
 * Default bead-type color palette (distinguishable, colorblind-aware).
 */
inline BeadColor default_bead_color(uint32_t type_id) {
    // 10-color palette (Wong 2011, adapted)
    static const BeadColor palette[] = {
        {0.00f, 0.45f, 0.70f},  // blue
        {0.90f, 0.62f, 0.00f},  // orange
        {0.00f, 0.62f, 0.45f},  // teal
        {0.80f, 0.47f, 0.65f},  // pink
        {0.94f, 0.89f, 0.26f},  // yellow
        {0.34f, 0.71f, 0.91f},  // sky blue
        {0.84f, 0.37f, 0.00f},  // vermillion
        {0.50f, 0.50f, 0.50f},  // grey
        {0.60f, 0.80f, 0.20f},  // lime
        {0.40f, 0.20f, 0.60f},  // purple
    };
    return palette[type_id % 10];
}

// ============================================================================
// CGSplitView
// ============================================================================

class CGSplitView {
public:
    CGSplitView() = default;

    // ========================================================================
    // Data Binding
    // ========================================================================

    /**
     * Set the data for both panes.
     *
     * @param atomistic_geom   AtomicGeometry for left pane (from existing pipeline)
     * @param system           BeadSystem produced by AtomToBeadMapper
     * @param scheme           MappingScheme used (for rule inspection)
     * @param conservation     Conservation report (for side panel)
     */
    void set_data(const vsepr::render::AtomicGeometry& atomistic_geom,
                  const BeadSystem& system,
                  const MappingScheme& scheme,
                  const ConservationReport& conservation)
    {
        atomistic_geom_ = atomistic_geom;
        bead_system_ = system;
        scheme_ = scheme;
        conservation_ = conservation;
        selection_.clear();
        build_atom_to_bead_lut();
        build_bead_geom();
    }

    // ========================================================================
    // Per-Frame Update
    // ========================================================================

    /**
     * Process mouse input and update selection state.
     *
     * @param mouse_x   Screen-space mouse X
     * @param mouse_y   Screen-space mouse Y
     * @param clicked    True if mouse was clicked this frame
     * @param width      Full window width
     * @param height     Full window height
     */
    void update(float mouse_x, float mouse_y, bool clicked,
                int width, int height)
    {
        int half_w = width / 2;

        // Determine which pane the mouse is in
        bool in_left  = (mouse_x < half_w);
        bool in_right = (mouse_x >= half_w);

        if (clicked) {
            if (in_left) {
                // Ray-cast against atomistic atoms
                int atom_idx = pick_atom(mouse_x, mouse_y, half_w, height);
                if (atom_idx >= 0) {
                    selection_.source = SelectionSource::ATOMISTIC_PANE;
                    selection_.selected_atom_index = atom_idx;
                    selection_.selected_bead_index = -1;
                    resolve_selection();
                }
            } else if (in_right) {
                // Ray-cast against beads
                int bead_idx = pick_bead(mouse_x - half_w, mouse_y, half_w, height);
                if (bead_idx >= 0) {
                    selection_.source = SelectionSource::BEAD_PANE;
                    selection_.selected_bead_index = bead_idx;
                    selection_.selected_atom_index = -1;
                    resolve_selection();
                }
            }
        }
    }

    // ========================================================================
    // Rendering
    // ========================================================================

    /**
     * Render the split-screen view via ImGui.
     *
     * This renders the inspection panels and overlays.
     * Actual 3D rendering is done by the caller using get_atomistic_geom()
     * and get_bead_geom() with existing renderers.
     *
     * @param width   Full window width
     * @param height  Full window height
     */
    void render_ui(int width, int height)
    {
        render_side_panel(width, height);
        render_pane_labels(width, height);
        render_selection_info(width, height);
    }

    // ========================================================================
    // Accessors for 3D Rendering
    // ========================================================================

    /**
     * Get atomistic geometry (for left pane renderer).
     * Highlight colors are baked into occupancies for selected atoms.
     */
    vsepr::render::AtomicGeometry get_atomistic_geom_with_highlights() const {
        auto geom = atomistic_geom_;

        // Set occupancies to encode highlight state
        geom.occupancies.resize(geom.positions.size(), 0.0f);
        for (uint32_t idx : selection_.highlighted_atoms) {
            if (idx < geom.occupancies.size())
                geom.occupancies[idx] = 1.0f;
        }
        return geom;
    }

    /**
     * Get bead geometry as AtomicGeometry (for right pane renderer).
     *
     * Beads are represented as large spheres. The atomic_number field
     * is repurposed as bead type id (for color lookup).
     */
    const vsepr::render::AtomicGeometry& get_bead_geom() const {
        return bead_geom_;
    }

    // ========================================================================
    // Options
    // ========================================================================

    void set_show_topology(bool show)   { show_topology_ = show; }
    void set_show_arrows(bool show)     { show_arrows_ = show; }
    void set_color_by_type(bool enable) { color_by_type_ = enable; }
    void set_projection_mode(ProjectionMode mode) {
        if (mode != active_projection_) {
            active_projection_ = mode;
            build_bead_geom();
        }
    }

    bool show_topology() const   { return show_topology_; }
    bool show_arrows() const     { return show_arrows_; }
    bool color_by_type() const   { return color_by_type_; }
    ProjectionMode projection_mode() const { return active_projection_; }

    const CrossSelection& selection() const { return selection_; }

    void clear_selection() { selection_.clear(); }

private:
    // ========================================================================
    // Data
    // ========================================================================

    vsepr::render::AtomicGeometry atomistic_geom_;
    vsepr::render::AtomicGeometry bead_geom_;      // Beads as AtomicGeometry
    BeadSystem        bead_system_;
    MappingScheme     scheme_;
    ConservationReport conservation_;
    CrossSelection    selection_;

    // Atom→bead reverse lookup
    std::vector<int>  atom_to_bead_;  // atom_to_bead_[atom_index] = bead_index

    // Options
    bool           show_topology_   = true;
    bool           show_arrows_     = false;
    bool           color_by_type_   = true;
    ProjectionMode active_projection_ = ProjectionMode::CENTER_OF_MASS;

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    void build_atom_to_bead_lut() {
        atom_to_bead_.assign(bead_system_.source_atom_count, -1);
        for (uint32_t bi = 0; bi < bead_system_.num_beads(); ++bi) {
            for (uint32_t ai : bead_system_.beads[bi].parent_atom_indices)
                if (ai < atom_to_bead_.size())
                    atom_to_bead_[ai] = static_cast<int>(bi);
        }
    }

    void build_bead_geom() {
        bead_geom_ = {};
        bead_geom_.atomic_numbers.reserve(bead_system_.num_beads());
        bead_geom_.positions.reserve(bead_system_.num_beads());
        bead_geom_.charges.reserve(bead_system_.num_beads());
        bead_geom_.occupancies.reserve(bead_system_.num_beads());

        for (const auto& b : bead_system_.beads) {
            bead_geom_.atomic_numbers.push_back(static_cast<int>(b.type_id + 1));

            vsepr::Vec3 pos;
            if (active_projection_ == ProjectionMode::CENTER_OF_MASS) {
                pos = {b.com_position.x, b.com_position.y, b.com_position.z};
            } else {
                pos = {b.cog_position.x, b.cog_position.y, b.cog_position.z};
            }
            bead_geom_.positions.push_back(pos);
            bead_geom_.charges.push_back(static_cast<float>(b.charge));
            bead_geom_.occupancies.push_back(0.0f);
        }

        // Topology bonds
        if (show_topology_) {
            for (const auto& [a, b] : bead_system_.bonds) {
                bead_geom_.bonds.emplace_back(static_cast<int>(a), static_cast<int>(b));
            }
        }
    }

    /**
     * Resolve cross-pane selection highlights.
     */
    void resolve_selection() {
        selection_.highlighted_atoms.clear();
        selection_.highlighted_beads.clear();

        if (selection_.source == SelectionSource::BEAD_PANE &&
            selection_.selected_bead_index >= 0 &&
            selection_.selected_bead_index < static_cast<int>(bead_system_.num_beads()))
        {
            // Bead selected → highlight its parent atoms
            const auto& bead = bead_system_.beads[selection_.selected_bead_index];
            selection_.highlighted_atoms = bead.parent_atom_indices;
            selection_.highlighted_beads = {static_cast<uint32_t>(selection_.selected_bead_index)};
        }
        else if (selection_.source == SelectionSource::ATOMISTIC_PANE &&
                 selection_.selected_atom_index >= 0 &&
                 selection_.selected_atom_index < static_cast<int>(atom_to_bead_.size()))
        {
            // Atom selected → highlight owning bead and all its siblings
            int bead_idx = atom_to_bead_[selection_.selected_atom_index];
            if (bead_idx >= 0 && bead_idx < static_cast<int>(bead_system_.num_beads())) {
                selection_.selected_bead_index = bead_idx;
                selection_.highlighted_beads = {static_cast<uint32_t>(bead_idx)};
                selection_.highlighted_atoms = bead_system_.beads[bead_idx].parent_atom_indices;
            }
        }

        // Update bead geom occupancies for highlights
        for (size_t i = 0; i < bead_geom_.occupancies.size(); ++i)
            bead_geom_.occupancies[i] = 0.0f;
        for (uint32_t bi : selection_.highlighted_beads) {
            if (bi < bead_geom_.occupancies.size())
                bead_geom_.occupancies[bi] = 1.0f;
        }
    }

    /**
     * Simplified atom picking (screen-space distance).
     * Returns atom index or -1.
     */
    int pick_atom(float mx, float my, int vp_w, int vp_h) const {
        (void)vp_h;
        // Simple nearest-in-screen-space pick (z-sorting omitted for clarity)
        float best_dist = 30.0f;  // pixel threshold
        int best_idx = -1;

        for (size_t i = 0; i < atomistic_geom_.positions.size(); ++i) {
            // Approximate screen projection (perspective divide not needed for inspection)
            float sx = (atomistic_geom_.positions[i].x * 20.0f + vp_w * 0.5f);
            float sy = (-atomistic_geom_.positions[i].y * 20.0f + vp_h * 0.5f);
            float dx = mx - sx;
            float dy = my - sy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < best_dist) {
                best_dist = d;
                best_idx = static_cast<int>(i);
            }
        }
        return best_idx;
    }

    /**
     * Simplified bead picking (screen-space distance).
     * Returns bead index or -1.
     */
    int pick_bead(float mx, float my, int vp_w, int vp_h) const {
        (void)vp_h;
        float best_dist = 40.0f;  // beads are larger → bigger threshold
        int best_idx = -1;

        for (size_t i = 0; i < bead_geom_.positions.size(); ++i) {
            float sx = (bead_geom_.positions[i].x * 20.0f + vp_w * 0.5f);
            float sy = (-bead_geom_.positions[i].y * 20.0f + vp_h * 0.5f);
            float dx = mx - sx;
            float dy = my - sy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < best_dist) {
                best_dist = d;
                best_idx = static_cast<int>(i);
            }
        }
        return best_idx;
    }

    // ========================================================================
    // ImGui Panels
    // ========================================================================

    void render_pane_labels(int width, int /*height*/) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 5.0f));
        ImGui::SetNextWindowSize(ImVec2(200.0f, 0.0f));
        ImGui::Begin("##LeftLabel", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "ATOMISTIC");
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu atoms)", atomistic_geom_.positions.size());
        ImGui::End();

        float half_w = width * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(half_w + 10.0f, 5.0f));
        ImGui::SetNextWindowSize(ImVec2(200.0f, 0.0f));
        ImGui::Begin("##RightLabel", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "COARSE-GRAINED");
        ImGui::SameLine();
        ImGui::TextDisabled("(%u beads)", bead_system_.num_beads());
        ImGui::End();
    }

    void render_side_panel(int width, int height) {
        float panel_w = 320.0f;
        ImGui::SetNextWindowPos(ImVec2(width - panel_w - 10.0f, 30.0f));
        ImGui::SetNextWindowSize(ImVec2(panel_w, height - 60.0f));

        ImGui::Begin("Mapping Inspector", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        // --- Projection Toggle ---
        if (ImGui::CollapsingHeader("Projection", ImGuiTreeNodeFlags_DefaultOpen)) {
            int proj = (active_projection_ == ProjectionMode::CENTER_OF_MASS) ? 0 : 1;
            if (ImGui::RadioButton("Center of Mass (COM)", &proj, 0)) {
                set_projection_mode(ProjectionMode::CENTER_OF_MASS);
            }
            if (ImGui::RadioButton("Center of Geometry (COG)", &proj, 1)) {
                set_projection_mode(ProjectionMode::CENTER_OF_GEOMETRY);
            }
        }

        // --- Display Options ---
        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Topology overlay", &show_topology_);
            ImGui::Checkbox("Centroid arrows", &show_arrows_);
            ImGui::Checkbox("Color by bead type", &color_by_type_);
        }

        // --- Conservation Report ---
        if (ImGui::CollapsingHeader("Conservation", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImVec4 pass_color(0.3f, 0.9f, 0.3f, 1.0f);
            ImVec4 fail_color(0.9f, 0.3f, 0.3f, 1.0f);

            ImGui::Text("Mass:");
            ImGui::SameLine();
            ImGui::TextColored(conservation_.mass_conserved ? pass_color : fail_color,
                               conservation_.mass_conserved ? "PASS" : "FAIL");
            ImGui::Text("  Atomistic: %.6f amu", conservation_.atomistic_total_mass);
            ImGui::Text("  CG:        %.6f amu", conservation_.coarse_grain_total_mass);
            ImGui::Text("  Error:     %.2e amu", conservation_.mass_error);

            ImGui::Separator();

            ImGui::Text("Charge:");
            ImGui::SameLine();
            ImGui::TextColored(conservation_.charge_conserved ? pass_color : fail_color,
                               conservation_.charge_conserved ? "PASS" : "FAIL");
            ImGui::Text("  Atomistic: %.6f e", conservation_.atomistic_total_charge);
            ImGui::Text("  CG:        %.6f e", conservation_.coarse_grain_total_charge);
            ImGui::Text("  Error:     %.2e e", conservation_.charge_error);
        }

        // --- Mapping Diagnostics ---
        if (ImGui::CollapsingHeader("Residual Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto diag = bead_system_.diagnostics();
            ImGui::Text("Mean residual: %.4f A", diag.mean_residual);
            ImGui::Text("Max residual:  %.4f A", diag.max_residual);
            ImGui::Text("Atoms mapped:  %u / %u", diag.n_atoms_mapped, bead_system_.source_atom_count);
            ImGui::Text("Beads:         %u", diag.n_beads);

            bool sane = bead_system_.sane();
            ImGui::Text("sane():");
            ImGui::SameLine();
            ImGui::TextColored(sane ? ImVec4(0.3f,0.9f,0.3f,1) : ImVec4(0.9f,0.3f,0.3f,1),
                               sane ? "PASS" : "FAIL");
        }

        // --- Scheme Info ---
        if (ImGui::CollapsingHeader("Mapping Scheme")) {
            ImGui::Text("Name: %s", scheme_.name.c_str());
            ImGui::Text("Rules: %u", scheme_.num_rules());
            ImGui::Text("Reduction: %u : %u",
                        bead_system_.source_atom_count, bead_system_.num_beads());
            if (bead_system_.num_beads() > 0) {
                ImGui::Text("Ratio: %.1f : 1",
                            static_cast<double>(bead_system_.source_atom_count) / bead_system_.num_beads());
            }
        }

        ImGui::End();
    }

    void render_selection_info(int width, int height) {
        if (selection_.source == SelectionSource::NONE)
            return;

        float panel_w = 280.0f;
        ImGui::SetNextWindowPos(ImVec2(10.0f, height - 200.0f));
        ImGui::SetNextWindowSize(ImVec2(panel_w, 190.0f));

        ImGui::Begin("Selection Detail", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        if (selection_.source == SelectionSource::BEAD_PANE &&
            selection_.selected_bead_index >= 0 &&
            selection_.selected_bead_index < static_cast<int>(bead_system_.num_beads()))
        {
            // ---- Bead was clicked ----
            const auto& bead = bead_system_.beads[selection_.selected_bead_index];
            ImGui::TextColored(ImVec4(1,0.8f,0.4f,1), "Bead %d", selection_.selected_bead_index);
            ImGui::Separator();

            // Bead type
            if (bead.type_id < bead_system_.bead_types.size()) {
                ImGui::Text("Type: %s (id=%u)",
                            bead_system_.bead_types[bead.type_id].name.c_str(),
                            bead.type_id);
            } else {
                ImGui::Text("Type ID: %u", bead.type_id);
            }

            ImGui::Text("Rule ID: %u", bead.mapping_rule_id);
            ImGui::Text("Mass:    %.4f amu", bead.mass);
            ImGui::Text("Charge:  %.4f e", bead.charge);
            ImGui::Text("Residual: %.4f A", bead.mapping_residual);

            // Anisotropic surface data (if available)
            if (bead.has_surface_data()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 1.0f, 1.0f), "Surface Descriptor");
                ImGui::Text("Asphericity:  %.4f", bead.asphericity());
                ImGui::Text("Aniso ratio:  %.4f", bead.anisotropy_ratio());
                ImGui::Text("Iso component: %.4f", bead.surface->isotropic_component());
                auto bp = bead.surface->band_power();
                ImGui::Text("Band power (l=0..4):");
                ImGui::Text("  %.3f  %.3f  %.3f  %.3f  %.3f",
                            bp[0], bp[1], bp[2], bp[3], bp[4]);
                int dom = bead.surface->dominant_band();
                if (dom >= 0) ImGui::Text("Dominant band: l=%d", dom);
                ImGui::Text("Samples: %d, Probe: %.2f A",
                            bead.surface->n_samples, bead.surface->probe_radius);
            }

            // Orientation data (if available)
            if (bead.has_orientation) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "Orientation");
                ImGui::Text("Normal: (%.4f, %.4f, %.4f)",
                            bead.orientation.normal.x,
                            bead.orientation.normal.y,
                            bead.orientation.normal.z);
            }

            // Parent atoms
            ImGui::Separator();
            ImGui::Text("Parent atoms (%zu):", bead.parent_atom_indices.size());
            std::string atom_list;
            for (size_t i = 0; i < bead.parent_atom_indices.size(); ++i) {
                if (i > 0) atom_list += ", ";
                uint32_t ai = bead.parent_atom_indices[i];
                atom_list += std::to_string(ai);
                // Add element label if available
                if (ai < atomistic_geom_.atomic_numbers.size()) {
                    int z = atomistic_geom_.atomic_numbers[ai];
                    atom_list += "(" + element_symbol(z) + ")";
                }
                if (atom_list.size() > 120) {
                    atom_list += "...";
                    break;
                }
            }
            ImGui::TextWrapped("%s", atom_list.c_str());
        }
        else if (selection_.source == SelectionSource::ATOMISTIC_PANE &&
                 selection_.selected_atom_index >= 0)
        {
            // ---- Atom was clicked ----
            int ai = selection_.selected_atom_index;
            int z = (ai < static_cast<int>(atomistic_geom_.atomic_numbers.size()))
                    ? atomistic_geom_.atomic_numbers[ai] : 0;

            ImGui::TextColored(ImVec4(0.7f,0.9f,1,1), "Atom %d (%s, Z=%d)",
                               ai, element_symbol(z).c_str(), z);
            ImGui::Separator();

            int bead_idx = (ai < static_cast<int>(atom_to_bead_.size()))
                           ? atom_to_bead_[ai] : -1;
            if (bead_idx >= 0 && bead_idx < static_cast<int>(bead_system_.num_beads())) {
                const auto& bead = bead_system_.beads[bead_idx];
                ImGui::Text("Assigned to bead: %d", bead_idx);

                if (bead.type_id < bead_system_.bead_types.size()) {
                    ImGui::Text("Bead type: %s",
                                bead_system_.bead_types[bead.type_id].name.c_str());
                }

                ImGui::Text("Rule ID: %u", bead.mapping_rule_id);

                // Find rule label
                for (const auto& rule : scheme_.rules) {
                    if (rule.rule_id == bead.mapping_rule_id) {
                        ImGui::Text("Rule label: %s", rule.label.c_str());
                        break;
                    }
                }

                ImGui::Text("Bead mass:    %.4f amu", bead.mass);
                ImGui::Text("Bead charge:  %.4f e", bead.charge);
                ImGui::Text("Group size:   %zu atoms", bead.parent_atom_indices.size());
            } else {
                ImGui::TextColored(ImVec4(0.9f,0.3f,0.3f,1), "UNMAPPED");
            }
        }

        ImGui::End();
    }

    /**
     * Element symbol from Z (minimal lookup for display).
     */
    static std::string element_symbol(int Z) {
        static const char* syms[] = {
            "?", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
            "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
            "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
            "Ga", "Ge", "As", "Se", "Br", "Kr"
        };
        if (Z >= 0 && Z < static_cast<int>(sizeof(syms) / sizeof(syms[0])))
            return syms[Z];
        return "Z" + std::to_string(Z);
    }
};

} // namespace vis
} // namespace coarse_grain
