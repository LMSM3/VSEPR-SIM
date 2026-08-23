#pragma once
// mcf_cai_io.hpp  -  MCF-CAI Kernel Fork  -  .xyza and .dynx frame writers
// Phase 3a | v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE
//
// Extended .xyza output adds MCF-CAI Information-column quantities as
// additional property columns: D_chem, D_fund, projection_loss.
//
// Extended .xyza comment-line format (per frame):
//   step=<N> time=<ps> E=<eV> T=<K> Lattice="<ax> 0 0 0 <ay> 0 0 0 <az>"
//   pbc="<T/F> <T/F> <T/F>"
//   properties="symbol:charge:velocity:force:energy:D_chem:D_fund:proj_loss"

#include "mcf_cai_world.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

namespace vsim::kernel_mcf {

// ============================================================================
// .xyza writer
// ============================================================================

/// Write one McfCaiFrame to an output stream in extended .xyza format.
/// Includes D_chem, D_fund, projection_loss as extra property columns.
inline void write_xyza_frame(const McfCaiFrame& frame, std::ostream& out) {
    const auto& atoms = frame.atoms;
    out << atoms.size() << "\n";

    // Comment / metadata line
    auto fmts = [](double v, int p=6) {
        std::ostringstream s; s << std::fixed << std::setprecision(p) << v; return s.str();
    };
    const auto& box = frame.box;
    const double lx = box.lengths[0] > 0.0 ? box.lengths[0] : 0.0;
    const double ly = box.lengths[1] > 0.0 ? box.lengths[1] : 0.0;
    const double lz = box.lengths[2] > 0.0 ? box.lengths[2] : 0.0;
    out << "step=" << frame.step
        << " time="   << fmts(frame.time_ps)
        << " E="      << fmts(frame.total_energy)
        << " T="      << fmts(frame.temperature, 3)
        << " Lattice=\"" << fmts(lx,4) << " 0 0  0 " << fmts(ly,4) << " 0  0 0 " << fmts(lz,4) << "\""
        << " pbc=\"" << (box.pbc_x?"T":"F") << " " << (box.pbc_y?"T":"F") << " " << (box.pbc_z?"T":"F") << "\""
        << " properties=\"symbol:charge:velocity:force:energy:D_chem:D_fund:proj_loss\""
        << "\n";

    // Atom lines
    out << std::scientific << std::setprecision(8);
    for (const auto& a : atoms) {
        out << a.symbol
            << " " << a.position[0]  << " " << a.position[1]  << " " << a.position[2]
            << " " << a.charge
            << " " << a.velocity[0]  << " " << a.velocity[1]  << " " << a.velocity[2]
            << " " << a.force[0]     << " " << a.force[1]     << " " << a.force[2]
            << " " << a.energy
            << " " << a.dist_chem
            << " " << a.dist_fund
            << " " << a.projection_loss
            << "\n";
    }
}

/// Write a full trajectory (multiple frames) to an output stream.
inline void write_xyza_trajectory(const std::vector<McfCaiFrame>& frames, std::ostream& out) {
    for (const auto& f : frames) write_xyza_frame(f, out);
}

// ============================================================================
// .dynx bridge
// Converts a McfCaiFrame to the DynxFrame struct without pulling in
// the full dynx_writer.hpp heavy include chain.
// Actual DynxEmitter integration is done at the runner level (Phase 3b).
// ============================================================================

/// Minimal dynx-compatible particle record from a McfCaiFrame atom.
struct McfCaiDynxAtom {
    std::string symbol;
    std::array<double,3> pos;
    std::array<double,3> vel;
    std::array<double,3> force;
    double energy;
    // MCF-CAI extensions (appended as RENDER/EVENT tags in dynx)
    double dist_chem;
    double dist_fund;
    double projection_loss;
};

struct McfCaiDynxFrame {
    uint64_t step;
    double   time_ps;
    std::vector<McfCaiDynxAtom> atoms;
};

/// Convert McfCaiFrame to McfCaiDynxFrame for downstream dynx pipeline.
inline McfCaiDynxFrame to_dynx_frame(const McfCaiFrame& f) {
    McfCaiDynxFrame df;
    df.step    = f.step;
    df.time_ps = f.time_ps;
    df.atoms.reserve(f.atoms.size());
    for (const auto& a : f.atoms) {
        McfCaiDynxAtom da;
        da.symbol         = a.symbol;
        da.pos            = { a.position[0], a.position[1], a.position[2] };
        da.vel            = { a.velocity[0], a.velocity[1], a.velocity[2] };
        da.force          = { a.force[0],    a.force[1],    a.force[2] };
        da.energy         = a.energy;
        da.dist_chem      = a.dist_chem;
        da.dist_fund      = a.dist_fund;
        da.projection_loss = a.projection_loss;
        df.atoms.push_back(da);
    }
    return df;
}

/// Write McfCaiDynxFrame in text dynx v1 format to a stream.
/// Emits RENDER lines for D_chem and D_fund as colour hints.
inline void write_dynx_frame(const McfCaiDynxFrame& df, std::ostream& out) {
    out << "FRAME " << df.step << " " << std::fixed << std::setprecision(6)
        << (df.time_ps * 1000.0) << "\n";  // dynx time in fs

    out << std::scientific << std::setprecision(8);
    for (std::size_t i = 0; i < df.atoms.size(); ++i) {
        const auto& a = df.atoms[i];
        // Particle line: N symbol x y z vx vy vz energy
        out << i << " " << a.symbol
            << " " << a.pos[0]  << " " << a.pos[1]  << " " << a.pos[2]
            << " " << a.vel[0]  << " " << a.vel[1]  << " " << a.vel[2]
            << " " << a.energy  << "\n";
        // FORCE line
        out << "FORCE " << i
            << " " << a.force[0] << " " << a.force[1] << " " << a.force[2] << "\n";
        // RENDER line: encode D_chem->green, D_fund->blue, proj_loss->red
        // Clamped to [0,1]; visible always
        const float r = static_cast<float>(std::min(1.0, a.projection_loss));
        const float g = static_cast<float>(std::max(0.0, std::min(1.0, a.dist_chem)));
        const float b = static_cast<float>(std::max(0.0, std::min(1.0, a.dist_fund)));
        out << std::fixed << std::setprecision(4)
            << "RENDER " << i << " " << r << " " << g << " " << b
            << " mcf_obj 1\n";
    }
    out << "END_FRAME\n";
}

} // namespace vsim::kernel_mcf
