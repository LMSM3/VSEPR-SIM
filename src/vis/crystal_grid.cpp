/**
 * crystal_grid.cpp
 * ----------------
 * Implementation of crystallographic grid visualization
 */

#include "crystal_grid.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace vsepr {
namespace render {

// ============================================================================
// LatticeVectors Implementation
// ============================================================================

Vec3 LatticeVectors::to_fractional(const Vec3& cartesian) const {
    // Solve: r = A·f  =>  f = A^{-1}·r
    // Use Cramer's rule for 3x3 inverse
    
    double det = volume();
    if (std::abs(det) < 1e-10) {
        // Degenerate lattice
        return {0, 0, 0};
    }
    
    // Compute A^{-1} using cofactor method
    Vec3 f;
    
    // f_x = det([r, b, c]) / det(A)
    f.x = (cartesian.x * (b.y * c.z - b.z * c.y) +
           cartesian.y * (b.z * c.x - b.x * c.z) +
           cartesian.z * (b.x * c.y - b.y * c.x)) / det;
    
    // f_y = det([a, r, c]) / det(A)
    f.y = (a.x * (cartesian.y * c.z - cartesian.z * c.y) +
           a.y * (cartesian.z * c.x - cartesian.x * c.z) +
           a.z * (cartesian.x * c.y - cartesian.y * c.x)) / det;
    
    // f_z = det([a, b, r]) / det(A)
    f.z = (a.x * (b.y * cartesian.z - b.z * cartesian.y) +
           a.y * (b.z * cartesian.x - b.x * cartesian.z) +
           a.z * (b.x * cartesian.y - b.y * cartesian.x)) / det;
    
    return f;
}

void LatticeVectors::get_parameters(
    double& a_len, double& b_len, double& c_len,
    double& alpha, double& beta, double& gamma
) const {
    // Lengths
    a_len = std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
    b_len = std::sqrt(b.x*b.x + b.y*b.y + b.z*b.z);
    c_len = std::sqrt(c.x*c.x + c.y*c.y + c.z*c.z);
    
    // Angles (in degrees)
    auto dot = [](const Vec3& u, const Vec3& v) {
        return u.x*v.x + u.y*v.y + u.z*v.z;
    };
    
    alpha = std::acos(dot(b, c) / (b_len * c_len)) * 180.0 / M_PI;  // angle between b and c
    beta  = std::acos(dot(a, c) / (a_len * c_len)) * 180.0 / M_PI;  // angle between a and c
    gamma = std::acos(dot(a, b) / (a_len * b_len)) * 180.0 / M_PI;  // angle between a and b
}

double LatticeVectors::volume() const {
    // V = a·(b × c)
    Vec3 cross_bc = {
        b.y * c.z - b.z * c.y,
        b.z * c.x - b.x * c.z,
        b.x * c.y - b.y * c.x
    };
    return a.x * cross_bc.x + a.y * cross_bc.y + a.z * cross_bc.z;
}

LatticeVectors LatticeVectors::cubic(double a) {
    return {
        {a, 0, 0},
        {0, a, 0},
        {0, 0, a}
    };
}

LatticeVectors LatticeVectors::fcc(double a) {
    // FCC primitive vectors (conventional cell parameter a)
    return {
        {0.0,   a/2.0, a/2.0},  // a_vec
        {a/2.0, 0.0,   a/2.0},  // b_vec
        {a/2.0, a/2.0, 0.0}     // c_vec
    };
}

LatticeVectors LatticeVectors::bcc(double a) {
    // BCC primitive vectors
    return {
        {-a/2.0, a/2.0, a/2.0},  // a_vec
        { a/2.0,-a/2.0, a/2.0},  // b_vec
        { a/2.0, a/2.0,-a/2.0}   // c_vec
    };
}

// ============================================================================
// CoordinationPolyhedron Implementation
// ============================================================================

std::array<uint8_t, 3> CoordinationPolyhedron::compute_color(
    const std::vector<std::array<uint8_t, 3>>& atom_colors
) {
    if (atom_colors.empty()) {
        return {128, 128, 128};  // Gray fallback
    }
    
    // Compute mean RGB
    double r_sum = 0, g_sum = 0, b_sum = 0;
    for (const auto& color : atom_colors) {
        r_sum += color[0];
        g_sum += color[1];
        b_sum += color[2];
    }
    
    double n = static_cast<double>(atom_colors.size());
    uint8_t r_mean = static_cast<uint8_t>(r_sum / n);
    uint8_t g_mean = static_cast<uint8_t>(g_sum / n);
    uint8_t b_mean = static_cast<uint8_t>(b_sum / n);
    
    // Invert: RGB_poly = (255, 255, 255) - mean(RGB_atoms)
    // This gives high contrast and is information-dense
    return {
        static_cast<uint8_t>(255 - r_mean),
        static_cast<uint8_t>(255 - g_mean),
        static_cast<uint8_t>(255 - b_mean)
    };
}

// ============================================================================
// CrystalStructure Implementation
// ============================================================================

CrystalStructure CrystalStructure::generate_supercell(int nx, int ny, int nz) const {
    CrystalStructure supercell;
    supercell.name = name + " supercell";
    
    // Expand lattice vectors
    supercell.lattice.a = lattice.a * static_cast<double>(nx);
    supercell.lattice.b = lattice.b * static_cast<double>(ny);
    supercell.lattice.c = lattice.c * static_cast<double>(nz);
    
    // Replicate atoms
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                for (const auto& atom : atoms) {
                    CrystalAtom new_atom = atom;
                    
                    // Fractional coordinates in supercell
                    new_atom.fractional = {
                        (atom.fractional.x + ix) / nx,
                        (atom.fractional.y + iy) / ny,
                        (atom.fractional.z + iz) / nz
                    };
                    
                    // Cartesian coordinates
                    new_atom.cartesian = supercell.lattice.to_cartesian(new_atom.fractional);
                    
                    supercell.atoms.push_back(new_atom);
                }
            }
        }
    }
    
    return supercell;
}

std::vector<CoordinationPolyhedron> CrystalStructure::find_coordination_polyhedra(
    double cutoff_angstrom
) const {
    std::vector<CoordinationPolyhedron> polyhedra;
    
    double cutoff_sq = cutoff_angstrom * cutoff_angstrom;
    
    // For each atom, find neighbors within cutoff
    for (size_t i = 0; i < atoms.size(); ++i) {
        CoordinationPolyhedron poly;
        poly.central_atom_idx = i;
        
        const Vec3& r_i = atoms[i].cartesian;
        
        // Find all neighbors
        for (size_t j = 0; j < atoms.size(); ++j) {
            if (i == j) continue;
            
            const Vec3& r_j = atoms[j].cartesian;
            Vec3 dr = {r_j.x - r_i.x, r_j.y - r_i.y, r_j.z - r_i.z};
            double dist_sq = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            
            if (dist_sq < cutoff_sq) {
                poly.neighbor_indices.push_back(j);
            }
        }
        
        // Only add if has neighbors
        if (!poly.neighbor_indices.empty()) {
            // Collect neighbor colors for polyhedron coloring
            std::vector<std::array<uint8_t, 3>> neighbor_colors;
            for (size_t idx : poly.neighbor_indices) {
                neighbor_colors.push_back(atoms[idx].color_rgb);
            }
            
            // Compute inverted mean color
            poly.color_rgb = CoordinationPolyhedron::compute_color(neighbor_colors);
            
            polyhedra.push_back(poly);
        }
    }
    
    return polyhedra;
}

void CrystalStructure::wrap_fractional(Vec3& f) {
    // Wrap to [0, 1)
    f.x = f.x - std::floor(f.x);
    f.y = f.y - std::floor(f.y);
    f.z = f.z - std::floor(f.z);
}

// ============================================================================
// CrystalGridRenderer Implementation
// ============================================================================

CrystalGridRenderer::CrystalGridRenderer()
    : nx_(1), ny_(1), nz_(1)
    , show_polyhedra_(true)
    , show_cell_edges_(true)
    , polyhedron_opacity_(0.5f)
    , cutoff_(3.5)
{
}

void CrystalGridRenderer::set_structure(const CrystalStructure& structure) {
    base_structure_ = structure;
    
    // Generate supercell
    expanded_structure_ = base_structure_.generate_supercell(nx_, ny_, nz_);
    
    // Find coordination polyhedra
    if (show_polyhedra_) {
        polyhedra_ = expanded_structure_.find_coordination_polyhedra(cutoff_);
    }
}

void CrystalGridRenderer::render() {
    // Render order (painter's algorithm):
    // 1. Cell edges (wireframe)
    // 2. Coordination polyhedra (translucent)
    // 3. Bonds (cylinders)
    // 4. Atoms (spheres)
    
    if (show_cell_edges_) {
        render_cell_edges();
    }
    
    if (show_polyhedra_) {
        render_polyhedra();
    }
    
    render_bonds();
    render_atoms();
}

void CrystalGridRenderer::render_atoms() {
    // TODO: Integrate with existing sphere renderer
    // For now, placeholder
}

void CrystalGridRenderer::render_bonds() {
    // TODO: Integrate with existing cylinder renderer
}

void CrystalGridRenderer::render_polyhedra() {
    // TODO: Render translucent polyhedra with inverted colors
    // Use OpenGL alpha blending
}

void CrystalGridRenderer::render_cell_edges() {
    // TODO: Render wireframe cube using line primitives
    // Use cyan color like reference image
}

// ============================================================================
// Predefined Crystal Structures
// ============================================================================

namespace crystals {

CrystalStructure aluminum_fcc() {
    CrystalStructure Al;
    Al.name = "Al FCC";
    Al.space_group_number = 225;
    Al.space_group_symbol = "Fm-3m";
    
    // Lattice parameter
    double a = 4.05;  // Å
    Al.lattice = LatticeVectors::cubic(a);  // Use conventional cell for simplicity
    
    // Atoms (conventional FCC cell has 4 atoms)
    // Fractional coordinates:
    //   (0, 0, 0), (0.5, 0.5, 0), (0.5, 0, 0.5), (0, 0.5, 0.5)
    
    auto add_al_atom = [&](double fx, double fy, double fz) {
        CrystalAtom atom;
        atom.atomic_number = 13;  // Al
        atom.fractional = {fx, fy, fz};
        atom.cartesian = Al.lattice.to_cartesian(atom.fractional);
        atom.color_rgb = {192, 192, 192};  // Silver
        atom.radius = 1.43f;  // Aluminum atomic radius
        Al.atoms.push_back(atom);
    };
    
    add_al_atom(0.0, 0.0, 0.0);
    add_al_atom(0.5, 0.5, 0.0);
    add_al_atom(0.5, 0.0, 0.5);
    add_al_atom(0.0, 0.5, 0.5);
    
    return Al;
}

CrystalStructure iron_bcc() {
    CrystalStructure Fe;
    Fe.name = "Fe BCC";
    Fe.space_group_number = 229;
    Fe.space_group_symbol = "Im-3m";
    
    double a = 2.87;  // Å
    Fe.lattice = LatticeVectors::cubic(a);
    
    // BCC: 2 atoms at (0,0,0) and (0.5,0.5,0.5)
    auto add_fe_atom = [&](double fx, double fy, double fz) {
        CrystalAtom atom;
        atom.atomic_number = 26;  // Fe
        atom.fractional = {fx, fy, fz};
        atom.cartesian = Fe.lattice.to_cartesian(atom.fractional);
        atom.color_rgb = {224, 102, 51};  // Iron rust color
        atom.radius = 1.26f;
        Fe.atoms.push_back(atom);
    };
    
    add_fe_atom(0.0, 0.0, 0.0);
    add_fe_atom(0.5, 0.5, 0.5);
    
    return Fe;
}

CrystalStructure sodium_chloride() {
    CrystalStructure NaCl;
    NaCl.name = "NaCl";
    NaCl.space_group_number = 225;
    NaCl.space_group_symbol = "Fm-3m";
    
    double a = 5.64;  // Å
    NaCl.lattice = LatticeVectors::cubic(a);
    
    // Na atoms (FCC sublattice)
    auto add_na = [&](double fx, double fy, double fz) {
        CrystalAtom atom;
        atom.atomic_number = 11;  // Na
        atom.fractional = {fx, fy, fz};
        atom.cartesian = NaCl.lattice.to_cartesian(atom.fractional);
        atom.color_rgb = {171, 92, 242};  // Purple (flame test color)
        atom.radius = 1.02f;
        NaCl.atoms.push_back(atom);
    };
    
    // Cl atoms (offset FCC sublattice)
    auto add_cl = [&](double fx, double fy, double fz) {
        CrystalAtom atom;
        atom.atomic_number = 17;  // Cl
        atom.fractional = {fx, fy, fz};
        atom.cartesian = NaCl.lattice.to_cartesian(atom.fractional);
        atom.color_rgb = {31, 240, 31};  // Green (chlorine gas)
        atom.radius = 1.81f;
        NaCl.atoms.push_back(atom);
    };
    
    // Na sublattice
    add_na(0.0, 0.0, 0.0);
    add_na(0.5, 0.5, 0.0);
    add_na(0.5, 0.0, 0.5);
    add_na(0.0, 0.5, 0.5);
    
    // Cl sublattice (shifted by 0.5, 0, 0)
    add_cl(0.5, 0.0, 0.0);
    add_cl(0.0, 0.5, 0.0);
    add_cl(0.0, 0.0, 0.5);
    add_cl(0.5, 0.5, 0.5);
    
    return NaCl;
}

CrystalStructure silicon_diamond() {
    CrystalStructure Si;
    Si.name = "Si";
    Si.space_group_number = 227;
    Si.space_group_symbol = "Fd-3m";
    
    double a = 5.43;  // Å
    Si.lattice = LatticeVectors::cubic(a);
    
    // Diamond structure: 8 atoms per conventional cell
    auto add_si = [&](double fx, double fy, double fz) {
        CrystalAtom atom;
        atom.atomic_number = 14;  // Si
        atom.fractional = {fx, fy, fz};
        atom.cartesian = Si.lattice.to_cartesian(atom.fractional);
        atom.color_rgb = {61, 123, 196};  // Blue-gray
        atom.radius = 1.17f;
        Si.atoms.push_back(atom);
    };
    
    // FCC sublattice
    add_si(0.0, 0.0, 0.0);
    add_si(0.5, 0.5, 0.0);
    add_si(0.5, 0.0, 0.5);
    add_si(0.0, 0.5, 0.5);
    
    // Shifted FCC sublattice (diamond structure offset)
    add_si(0.25, 0.25, 0.25);
    add_si(0.75, 0.75, 0.25);
    add_si(0.75, 0.25, 0.75);
    add_si(0.25, 0.75, 0.75);
    
    return Si;
}

} // namespace crystals

} // namespace render
} // namespace vsepr
