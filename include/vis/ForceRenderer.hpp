// ForceRenderer.hpp - Force vector visualization (arrows, decomposition)
// Integrates with existing scalable_renderer.cpp

#pragma once
#include "data/Forces.hpp"
#include "vis/renderer_base.hpp"
#include <GL/glew.h>
#include <vector>

namespace vsepr::vis {

// ════════════════════════════════════════════════════════════════════════════
// Force visualization modes
// ════════════════════════════════════════════════════════════════════════════

enum class ForceMode {
    PrimaryOnly,      // One arrow per atom (largest contributor)
    AllContributors,  // All pairwise forces
    Decomposed,       // Separate LJ, Coulomb, bonded
    InteractionPairs  // Lines between atom pairs
};

enum class ForceColorScheme {
    Magnitude,        // Blue (weak) → Yellow (medium) → Red (strong)
    Type,             // Green (LJ), Red (Coulomb), Blue (bonded)
    Sign              // Red (repulsion), Blue (attraction)
};

// ════════════════════════════════════════════════════════════════════════════
// Force arrow geometry
// ════════════════════════════════════════════════════════════════════════════

struct ForceArrow {
    data::Vec3 origin;        // Start position (atom)
    data::Vec3 direction;     // Unit vector
    float magnitude;          // Force magnitude
    std::string source;       // Source atom ID (for tooltip)
    
    // Visual properties
    float length;             // Arrow length (scaled)
    float thickness;          // Arrow shaft radius
    std::array<float, 3> color; // RGB
};

// ════════════════════════════════════════════════════════════════════════════
// ForceRenderer: Renders force vectors as 3D arrows
// ════════════════════════════════════════════════════════════════════════════

class ForceRenderer : public RendererBase {
public:
    explicit ForceRenderer() : mode_(ForceMode::PrimaryOnly),
                               color_scheme_(ForceColorScheme::Magnitude),
                               scale_factor_(1.0f),
                               log_scale_(true),
                               cutoff_magnitude_(0.1f) {}
    
    // ─── Load forces ───
    void load_forces(const data::Forces& forces);
    void clear_forces();
    
    // ─── Rendering ───
    void render(const glm::mat4& view, const glm::mat4& projection) override;
    void render_arrows(const std::vector<ForceArrow>& arrows,
                       const glm::mat4& view,
                       const glm::mat4& projection);
    
    // ─── Configuration ───
    void set_mode(ForceMode mode) { mode_ = mode; rebuild_arrows(); }
    void set_color_scheme(ForceColorScheme scheme) { color_scheme_ = scheme; rebuild_arrows(); }
    void set_scale_factor(float scale) { scale_factor_ = scale; rebuild_arrows(); }
    void set_log_scale(bool enable) { log_scale_ = enable; rebuild_arrows(); }
    void set_cutoff(float cutoff) { cutoff_magnitude_ = cutoff; rebuild_arrows(); }
    
    ForceMode mode() const { return mode_; }
    ForceColorScheme color_scheme() const { return color_scheme_; }
    float scale_factor() const { return scale_factor_; }
    bool log_scale() const { return log_scale_; }
    float cutoff() const { return cutoff_magnitude_; }
    
    // ─── UI Controls (ImGui) ───
    void render_ui_controls();
    
private:
    void rebuild_arrows();
    void build_primary_arrows();
    void build_all_contributors_arrows();
    void build_decomposed_arrows();
    void build_interaction_pairs();
    
    ForceArrow create_arrow(const data::Vec3& origin,
                           const data::Vec3& direction,
                           float magnitude,
                           const std::string& source);
    
    std::array<float, 3> compute_color(float magnitude, const std::string& type);
    float compute_arrow_length(float magnitude);
    float compute_arrow_thickness(float magnitude);
    
    // Arrow geometry generation
    void generate_arrow_mesh(const ForceArrow& arrow,
                            std::vector<float>& vertices,
                            std::vector<unsigned int>& indices);
    
    data::Forces forces_;
    std::vector<ForceArrow> arrows_;
    
    ForceMode mode_;
    ForceColorScheme color_scheme_;
    float scale_factor_;
    bool log_scale_;
    float cutoff_magnitude_;
    
    // OpenGL resources
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint shader_program_ = 0;
    size_t index_count_ = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// Helper functions
// ════════════════════════════════════════════════════════════════════════════

// Generate cylinder mesh for arrow shaft
void generate_cylinder(const data::Vec3& start, const data::Vec3& end,
                      float radius, int segments,
                      std::vector<float>& vertices,
                      std::vector<unsigned int>& indices);

// Generate cone mesh for arrow head
void generate_cone(const data::Vec3& base, const data::Vec3& tip,
                  float radius, int segments,
                  std::vector<float>& vertices,
                  std::vector<unsigned int>& indices);

// Color map: magnitude → RGB
std::array<float, 3> magnitude_to_color(float magnitude, float max_magnitude);

// Color map: type → RGB
std::array<float, 3> type_to_color(const std::string& type);

} // namespace vsepr::vis
