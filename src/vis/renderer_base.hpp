#pragma once

#include "core/math_vec3.hpp"
#include "gl_camera.hpp"
#include <memory>
#include <string>
#include <vector>

namespace vsepr {
namespace render {

/**
 * Minimal atomic geometry data for rendering
 * 
 * This is the ONLY input a renderer needs - no simulation state coupling.
 * Can be populated from:
 *   - XYZ files
 *   - PDB structures
 *   - Optimized geometries
 *   - MD snapshots
 *   - Reaction products
 */
struct AtomicGeometry {
    std::vector<int> atomic_numbers;           // Z values
    std::vector<Vec3> positions;               // Cartesian coordinates (Å)
    std::vector<std::pair<int,int>> bonds;     // Optional connectivity (atom indices)
    
    // Optional metadata for advanced rendering
    std::vector<float> occupancies;            // For B-factor/RMSD coloring
    std::vector<float> charges;                // For electrostatic coloring
    std::vector<int> residue_ids;              // For protein ribbon rendering
    std::vector<char> secondary_structure;     // 'H'=helix, 'E'=sheet, 'C'=coil
    
    // Optional periodic boundary conditions
    struct PBCBox {
        Vec3 a, b, c;  // Lattice vectors
    };
    PBCBox* box = nullptr;
    
    // Factory methods
    static AtomicGeometry from_xyz(const std::vector<int>& Z, 
                                   const std::vector<Vec3>& pos);
    
    static AtomicGeometry from_xyz_with_bonds(const std::vector<int>& Z,
                                             const std::vector<Vec3>& pos,
                                             const std::vector<std::pair<int,int>>& bonds);
};

/**
 * Chemistry type classification for renderer selection
 */
enum class ChemistryType {
    ORGANIC,      // C, H, N, O, S, P dominant (proteins, drugs, polymers)
    CLASSIC,      // Main group elements (VSEPR geometries)
    METALLIC,     // Metal-centered complexes, coordination compounds
    MIXED,        // Combination (e.g., metalloprotein)
    UNKNOWN
};

/**
 * Rendering quality tier (determines shader complexity)
 */
enum class RenderQuality {
    ULTRA,        // 5 subdivisions, PBR, AO, shadows (12,288 tri/sphere)
    HIGH,         // 4 subdivisions, Phong, soft shadows (3,072 tri/sphere)
    MEDIUM,       // 3 subdivisions, Phong, no shadows (768 tri/sphere)
    LOW,          // 2 subdivisions, flat shading (192 tri/sphere)
    MINIMAL       // Wireframe only (debugging)
};

/**
 * Color scheme for molecular rendering
 */
enum class ColorScheme {
    CPK,                    // CPK (Corey-Pauling-Koltun) atom colors
    BY_ELEMENT,             // Jmol-style element colors
    BY_RESIDUE,             // Protein residue colors
    BY_SECONDARY_STRUCTURE, // α-helix (red), β-sheet (blue), coil (green)
    BY_CHARGE,              // Electrostatic potential
    BY_TEMPERATURE,         // B-factor or RMSD
    BY_LIGAND_FIELD,        // Coordination chemistry (ligand strength)
    MONOCHROME,             // Single color
    CUSTOM                  // User-defined
};

/**
 * Render style (geometry representation)
 */
enum class RenderStyle {
    BALL_AND_STICK,         // Spheres + cylinders (classic)
    SPACE_FILLING,          // Van der Waals spheres (CPK)
    LICORICE,               // Stick bonds only (organic)
    RIBBON,                 // Secondary structure ribbon (proteins)
    CARTOON,                // Simplified cartoon (proteins)
    SURFACE,                // Molecular surface (SAS/SES)
    WIREFRAME,              // Bonds only (debugging)
    POLYHEDRA               // Coordination polyhedra (metals)
};

/**
 * Lighting model
 */
enum class LightingModel {
    PHONG,          // Blinn-Phong (ambient + diffuse + specular)
    PBR,            // Physically-based rendering (metallic/roughness)
    FLAT,           // No lighting (debugging)
    CEL_SHADED      // Cartoon-style (discrete shading levels)
};

/**
 * Base renderer interface
 * 
 * All molecular renderers (Organic, Classic, Metallic) inherit from this.
 * Provides common functionality: camera, lighting, color schemes.
 * 
 * INPUT: Pure XYZ geometry data (no simulation coupling)
 */
class MoleculeRendererBase {
public:
    virtual ~MoleculeRendererBase() = default;
    
    /**
     * Initialize OpenGL resources (must be called with active context)
     */
    virtual bool initialize() = 0;
    
    /**
     * Core render method - XYZ data only
     * 
     * @param geom Atomic geometry (Z, positions, optional bonds)
     * @param camera Camera for view/projection
     * @param width Viewport width (pixels)
     * @param height Viewport height (pixels)
     */
    virtual void render(const AtomicGeometry& geom, 
                       const vis::Camera& camera,
                       int width, int height) = 0;
    
    /**
     * Render to texture (for offscreen rendering)
     */
    virtual void render_to_texture(const AtomicGeometry& geom,
                                   const vis::Camera& camera,
                                   unsigned int fbo,
                                   int width, int height) {
        // Default: same as screen rendering
        render(geom, camera, width, height);
    }
    
    /**
     * Get chemistry type this renderer is optimized for
     */
    virtual ChemistryType get_chemistry_type() const = 0;
    
    /**
     * Get renderer name (for UI display)
     */
    virtual std::string get_name() const = 0;
    
    // ========================================================================
    // Common Settings (all renderers support these)
    // ========================================================================
    
    virtual void set_quality(RenderQuality quality) { quality_ = quality; }
    virtual RenderQuality get_quality() const { return quality_; }
    
    virtual void set_style(RenderStyle style) { style_ = style; }
    virtual RenderStyle get_style() const { return style_; }
    
    virtual void set_color_scheme(ColorScheme scheme) { color_scheme_ = scheme; }
    virtual ColorScheme get_color_scheme() const { return color_scheme_; }
    
    virtual void set_lighting_model(LightingModel model) { lighting_model_ = model; }
    virtual LightingModel get_lighting_model() const { return lighting_model_; }
    
    virtual void set_background_color(float r, float g, float b) {
        background_[0] = r;
        background_[1] = g;
        background_[2] = b;
    }
    
    virtual void set_atom_scale(float scale) { atom_scale_ = scale; }
    virtual void set_bond_radius(float radius) { bond_radius_ = radius; }
    virtual void set_show_bonds(bool show) { show_bonds_ = show; }
    virtual void set_show_box(bool show) { show_box_ = show; }
    virtual void set_show_labels(bool show) { show_labels_ = show; }
    
    // ========================================================================
    // Utility Functions
    // ========================================================================
    
    /**
     * Detect chemistry type from molecular composition
     * 
     * Rules:
     *   - >50% C, H, N, O, S, P → ORGANIC
     *   - Transition metals (Z 21-30, 39-48, 57-80) → METALLIC
     *   - Otherwise → CLASSIC
     */
    static ChemistryType detect_chemistry_type(const AtomicGeometry& geom);
    
    /**
     * Get CPK color for element (by atomic number)
     */
    static void get_cpk_color(int Z, float* rgb);
    
    /**
     * Get Van der Waals radius (Å)
     */
    static float get_vdw_radius(int Z);
    
    /**
     * Get covalent radius (Å)
     */
    static float get_covalent_radius(int Z);
    
protected:
    // Common state
    RenderQuality quality_ = RenderQuality::HIGH;
    RenderStyle style_ = RenderStyle::BALL_AND_STICK;
    ColorScheme color_scheme_ = ColorScheme::CPK;
    LightingModel lighting_model_ = LightingModel::PHONG;
    
    float background_[3] = {0.1f, 0.1f, 0.1f};  // Dark gray
    float atom_scale_ = 0.3f;     // 30% of vdW radius
    float bond_radius_ = 0.15f;   // Å
    
    bool show_bonds_ = true;
    bool show_box_ = false;
    bool show_labels_ = false;
};

/**
 * Factory for creating appropriate renderer based on chemistry type
 */
class RendererFactory {
public:
    /**
     * Create renderer automatically based on molecule composition
     * 
     * Detects chemistry type and instantiates:
     *   - OrganicRenderer for organic molecules
     *   - ClassicRenderer for main group elements
     *   - MetallicRenderer for coordination complexes
     * 
     * @param geom Atomic geometry to analyze
     * @return Appropriate renderer (caller owns)
     */
    static std::unique_ptr<MoleculeRendererBase> create_auto(const AtomicGeometry& geom);
    
    /**
     * Create specific renderer by type
     */
    static std::unique_ptr<MoleculeRendererBase> create_organic();
    static std::unique_ptr<MoleculeRendererBase> create_classic();
    static std::unique_ptr<MoleculeRendererBase> create_metallic();
    
    /**
     * Create renderer by name (for UI selection)
     * 
     * @param name "organic", "classic", "metallic", or "ballstick" (alias for classic)
     */
    static std::unique_ptr<MoleculeRendererBase> create_by_name(const std::string& name);
};

} // namespace render
} // namespace vsepr
