// mcf_cai_world.cpp  -  MCF-CAI Kernel Fork  -  World implementation
// v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE

#include "../../include/vsim/kernel_mcf/mcf_cai_world.hpp"
#include "../../include/vsim/kernel_mcf/mcf_cai_sidecar.hpp"

#include <cmath>
#include <stdexcept>

namespace vsim::kernel_mcf {

// ---------------------------------------------------------------------------
// add
// ---------------------------------------------------------------------------
ObjectId McfCaiWorld::add(McfCaiObject obj) {
    if (obj.id == kNullObjectId) obj.id = next_id();
    const uint32_t idx = static_cast<uint32_t>(objects_.size());
    id_index_[obj.id] = idx;
    objects_.push_back(std::move(obj));
    return objects_.back().id;
}

// ---------------------------------------------------------------------------
// find_by_id / find_by_name
// ---------------------------------------------------------------------------
McfCaiObject* McfCaiWorld::find_by_id(ObjectId id) {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) return nullptr;
    return &objects_[it->second];
}
const McfCaiObject* McfCaiWorld::find_by_id(ObjectId id) const {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) return nullptr;
    return &objects_[it->second];
}
McfCaiObject* McfCaiWorld::find_by_name(const std::string& name) {
    for (auto& obj : objects_)
        if (obj.name == name) return &obj;
    return nullptr;
}
const McfCaiObject* McfCaiWorld::find_by_name(const std::string& name) const {
    for (const auto& obj : objects_)
        if (obj.name == name) return &obj;
    return nullptr;
}

// ---------------------------------------------------------------------------
// PBC helpers
// ---------------------------------------------------------------------------
void McfCaiWorld::wrap_positions() {
    for (auto& obj : objects_) {
        auto& p = obj.pos();
        if (box_.pbc_x && box_.lengths[0] > 0.0) {
            p[0] -= box_.lengths[0] * std::floor(p[0] / box_.lengths[0]);
        }
        if (box_.pbc_y && box_.lengths[1] > 0.0) {
            p[1] -= box_.lengths[1] * std::floor(p[1] / box_.lengths[1]);
        }
        if (box_.pbc_z && box_.lengths[2] > 0.0) {
            p[2] -= box_.lengths[2] * std::floor(p[2] / box_.lengths[2]);
        }
    }
}

Vec3d McfCaiWorld::min_image(const Vec3d& a, const Vec3d& b) const noexcept {
    Vec3d dr = { a[0]-b[0], a[1]-b[1], a[2]-b[2] };
    for (int d = 0; d < 3; ++d) {
        const bool pbc = (d==0) ? box_.pbc_x : (d==1) ? box_.pbc_y : box_.pbc_z;
        if (pbc && box_.lengths[d] > 0.0) {
            const double L = box_.lengths[d];
            dr[d] -= L * std::round(dr[d] / L);
        }
    }
    return dr;
}

// ---------------------------------------------------------------------------
// SoA extract / commit
// ---------------------------------------------------------------------------
McfCaiSoA McfCaiWorld::extract_soa() const {
    McfCaiSoA soa;
    const std::size_t N = objects_.size();
    soa.ids.reserve(N);
    soa.px.reserve(N); soa.py.reserve(N); soa.pz.reserve(N);
    soa.vx.reserve(N); soa.vy.reserve(N); soa.vz.reserve(N);
    soa.fx.reserve(N); soa.fy.reserve(N); soa.fz.reserve(N);
    soa.fx_p.reserve(N); soa.fy_p.reserve(N); soa.fz_p.reserve(N);
    soa.mass.reserve(N); soa.charge.reserve(N);
    for (const auto& o : objects_) {
        soa.ids.push_back(o.id);
        soa.px.push_back(o.pos()[0]); soa.py.push_back(o.pos()[1]); soa.pz.push_back(o.pos()[2]);
        soa.vx.push_back(o.velocity[0]); soa.vy.push_back(o.velocity[1]); soa.vz.push_back(o.velocity[2]);
        soa.fx.push_back(o.force[0]); soa.fy.push_back(o.force[1]); soa.fz.push_back(o.force[2]);
        soa.fx_p.push_back(o.force_prev[0]); soa.fy_p.push_back(o.force_prev[1]); soa.fz_p.push_back(o.force_prev[2]);
        soa.mass.push_back(o.mass());
        soa.charge.push_back(o.charge());
    }
    return soa;
}

void McfCaiWorld::commit_soa(const McfCaiSoA& soa) {
    const std::size_t N = soa.size();
    if (N != objects_.size())
        throw std::runtime_error("McfCaiWorld::commit_soa: size mismatch");
    for (std::size_t i = 0; i < N; ++i) {
        auto& o = objects_[i];
        o.pos() = { soa.px[i], soa.py[i], soa.pz[i] };
        o.velocity = { soa.vx[i], soa.vy[i], soa.vz[i] };
        o.force_prev = o.force;
        o.force = { soa.fx[i], soa.fy[i], soa.fz[i] };
    }
}

// ---------------------------------------------------------------------------
// recompute_stats
// ---------------------------------------------------------------------------
void McfCaiWorld::recompute_stats() {
    const std::size_t N = objects_.size();
    double ke = 0.0, rms2 = 0.0, maxF2 = 0.0;
    // kB in eV/K
    constexpr double kB = 8.617333e-5;
    // mass unit: amu. velocity unit: A/ps.
    // KE = 0.5 * m[amu] * v[A/ps]^2 * unit_factor
    // 1 amu * (1 A/ps)^2 = 1.66054e-27 * 1e-20 / 1.602e-19 eV ~ 0.010364 eV
    constexpr double mv2_to_eV = 0.010364269;
    for (const auto& o : objects_) {
        const double v2 = o.velocity[0]*o.velocity[0]
                        + o.velocity[1]*o.velocity[1]
                        + o.velocity[2]*o.velocity[2];
        ke += 0.5 * o.mass() * v2 * mv2_to_eV;
        for (int d = 0; d < 3; ++d)
            rms2 += o.force[d] * o.force[d];
        const double f2 = o.force[0]*o.force[0] + o.force[1]*o.force[1] + o.force[2]*o.force[2];
        if (f2 > maxF2) maxF2 = f2;
    }
    stats_.kinetic_energy   = ke;
    stats_.temperature      = (N > 0 && ke > 0.0) ? (2.0 * ke) / (3.0 * static_cast<double>(N) * kB) : 0.0;
    stats_.rms_force        = (N > 0) ? std::sqrt(rms2 / (3.0 * static_cast<double>(N))) : 0.0;
    stats_.max_force        = std::sqrt(maxF2);
    stats_.total_energy     = stats_.kinetic_energy + stats_.potential_energy;
    stats_.converged        = stats_.rms_force < params_.tol_rms_force
                           && stats_.max_force < params_.tol_max_force;
    stats_.time_ps         += params_.dt_ps;
    ++stats_.step;
}

// ---------------------------------------------------------------------------
// update_information_column  (delegates to McfCaiSidecar)
// ---------------------------------------------------------------------------
void McfCaiWorld::update_information_column() {
    McfCaiSidecar::update(*this, params_.dt_ps * static_cast<double>(params_.sidecar_update_every));
    // Refresh aggregate sidecar stats
    double sd_c = 0.0, sd_f = 0.0, sel = 0.0;
    for (const auto& o : objects_) {
        sd_c += o.chem_i.dist_chem;
        sd_f += o.fund_i.dist_fund;
        sel  += o.fund_i.entropy_loss;
    }
    const double inv_n = objects_.empty() ? 0.0 : 1.0 / static_cast<double>(objects_.size());
    stats_.mean_dist_chem    = sd_c * inv_n;
    stats_.mean_dist_fund    = sd_f * inv_n;
    stats_.total_entropy_loss = sel;
}

// ---------------------------------------------------------------------------
// step_forward / advance  (integrator is external; basic fallback here)
// ---------------------------------------------------------------------------
void McfCaiWorld::step_forward() {
    // The world itself does not own an integrator in v1.0.
    // Callers are expected to drive the loop via an IMcfCaiIntegrator.
    // This fallback simply advances time and calls sidecar if due.
    stats_.time_ps += params_.dt_ps;
    ++stats_.step;
    if (params_.sidecar_update_every > 0 &&
        stats_.step % static_cast<uint64_t>(params_.sidecar_update_every) == 0) {
        update_information_column();
    }
}

void McfCaiWorld::advance(uint64_t n_steps) {
    for (uint64_t i = 0; i < n_steps; ++i) step_forward();
}

// ---------------------------------------------------------------------------
// snapshot
// ---------------------------------------------------------------------------
McfCaiFrame McfCaiWorld::snapshot() const {
    McfCaiFrame f;
    f.step         = stats_.step;
    f.time_ps      = stats_.time_ps;
    f.total_energy = stats_.total_energy;
    f.temperature  = stats_.temperature;
    f.box          = box_;
    f.atoms.reserve(objects_.size());
    for (const auto& o : objects_) {
        McfCaiFrame::AtomRecord r;
        r.id             = o.id;
        r.symbol         = o.chem_c.symbol;
        r.position       = o.pos();
        r.velocity       = o.velocity;
        r.force          = o.force;
        r.charge         = o.charge();
        r.energy         = 0.0; // per-atom PE not tracked in v1.0
        r.dist_chem      = o.chem_i.dist_chem;
        r.dist_fund      = o.fund_i.dist_fund;
        r.projection_loss = o.fund_i.projection_loss;
        f.atoms.push_back(r);
    }
    return f;
}

} // namespace vsim::kernel_mcf
