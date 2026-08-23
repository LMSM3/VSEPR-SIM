#pragma once
// mcf_cai_sidecar.hpp  -  MCF-CAI Kernel Fork  -  Information column update
// Doctrine: reads Carrier+Action cells only. Never writes to Carrier or Action.
// Maps onto IdentitySidecarRecord doctrine from identity_sidecar.hpp.
// v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE

#include "mcf_cai_object.hpp"
#include "mcf_cai_world.hpp"
#include <algorithm>
#include <cmath>

namespace vsim::kernel_mcf {

// ============================================================================
// McfCaiSidecar  -  updates all three Information cells from Carrier/Action data
// ============================================================================

class McfCaiSidecar {
public:
    // Update Information column for every object in the world.
    // Called by McfCaiWorld::update_information_column().
    static void update(McfCaiWorld& world, double dt_ps_elapsed) {
        for (auto& obj : world) {
            update_macro_info(obj, dt_ps_elapsed);
            update_chem_info(obj);
            update_fund_info(obj);
        }
    }

    // Update a single object.
    static void update_object(McfCaiObject& obj, double dt_ps_elapsed) {
        update_macro_info(obj, dt_ps_elapsed);
        update_chem_info(obj);
        update_fund_info(obj);
    }

private:
    // -- 𝓜_I : Macro Information ----------------------------------------
    static void update_macro_info(McfCaiObject& obj, double dt_ps_elapsed) {
        MacroInfo& mi = obj.macro_i;
        // Accumulate age
        mi.formation_age += dt_ps_elapsed;
        // Defect memory decays with deformation pressure
        const double deform = obj.macro_a.deformation;
        if (deform > 0.0)
            mi.defect_memory = std::max(0.0, mi.defect_memory - 0.001 * deform);
        // Coarse-grain information loss: proportional to grain count variance
        if (obj.macro_c.grain_count > 1)
            mi.coarse_grain_loss = 1.0 - 1.0 / static_cast<double>(obj.macro_c.grain_count);
        else
            mi.coarse_grain_loss = 0.0;
    }

    // -- 𝓒_I : Chemical Information ----------------------------------------
    static void update_chem_info(McfCaiObject& obj) {
        ChemInfo& ci = obj.chem_i;
        const ChemAction& ca = obj.chem_a;
        // Accumulate reaction entropy loss proportional to events
        ci.reaction_event_count += ca.bonds_formed + ca.bonds_broken;
        if (ca.bonds_formed + ca.bonds_broken > 0)
            ci.reaction_entropy_loss += 0.01 * static_cast<double>(ca.bonds_formed + ca.bonds_broken);
        // Chemical distinguishability D_chem decays with reaction events
        // D_chem -> 0 as distinguishability is lost through reactions
        if (ci.reaction_event_count > 0)
            ci.dist_chem = std::exp(-0.001 * static_cast<double>(ci.reaction_event_count));
        // Bond entropy: Shannon over bond types (simplified: normalised by max bonds)
        const double n_bonds = static_cast<double>(obj.chem_c.bonds.size());
        ci.bond_entropy = (n_bonds > 0.0) ? std::log(n_bonds + 1.0) : 0.0;
    }

    // -- 𝓕_I : Fundamental Information ----------------------------------------
    // Maps to IKK IV: |Psi^hid| = ||(I - Pi_{a<-b})|Psi_a>||
    static void update_fund_info(McfCaiObject& obj) {
        FundInfo& fi = obj.fund_i;
        const FundCarrier& fc = obj.fund_c;
        const FundAction& fa = obj.fund_a;
        // Projection loss: magnitude of colour-force channel residual
        const float cr = fc.caf_channel[0];
        const float cg = fc.caf_channel[1];
        const float cb = fc.caf_channel[2];
        const double caf_mag = std::sqrt(static_cast<double>(cr*cr + cg*cg + cb*cb));
        // Residual = deviation from fully resolved (caf_mag == 0 means no sub-atomic content)
        fi.projection_loss = caf_mag;
        // Hidden W-coordinate: driven by decay and annihilation flags
        if (fa.decay_enabled)       fi.hidden_W += 0.001;
        if (fa.annihilation_flag)   fi.hidden_W += 1.0;
        // Fundamental distinguishability D_fund
        // Decays with projection loss and hidden W accumulation
        fi.dist_fund = std::exp(-(fi.projection_loss + fi.hidden_W));
        fi.dist_fund = std::max(0.0, std::min(1.0, fi.dist_fund));
        // Identity residual: ||I_true - I_top|| proxy
        fi.identity_residual = fi.projection_loss + fi.entropy_loss;
        // Entropy loss accumulates with each annihilation event
        if (fa.annihilation_flag)
            fi.entropy_loss += 1.0;
    }
};

// ============================================================================
// World-level sidecar aggregate summary (for reporting / end-tag)
// ============================================================================

struct McfCaiSidecarSummary {
    double mean_dist_chem     = 1.0;
    double mean_dist_fund     = 1.0;
    double total_entropy_loss = 0.0;
    double max_projection_loss = 0.0;
    double mean_defect_memory = 0.0;
    double mean_coarse_loss   = 0.0;
    std::size_t n_objects     = 0;
};

inline McfCaiSidecarSummary compute_sidecar_summary(const McfCaiWorld& world) {
    McfCaiSidecarSummary s;
    s.n_objects = world.size();
    if (s.n_objects == 0) return s;
    for (const auto& obj : world) {
        s.mean_dist_chem      += obj.chem_i.dist_chem;
        s.mean_dist_fund      += obj.fund_i.dist_fund;
        s.total_entropy_loss  += obj.fund_i.entropy_loss;
        s.max_projection_loss  = std::max(s.max_projection_loss, obj.fund_i.projection_loss);
        s.mean_defect_memory  += obj.macro_i.defect_memory;
        s.mean_coarse_loss    += obj.macro_i.coarse_grain_loss;
    }
    const double inv_n = 1.0 / static_cast<double>(s.n_objects);
    s.mean_dist_chem     *= inv_n;
    s.mean_dist_fund     *= inv_n;
    s.mean_defect_memory *= inv_n;
    s.mean_coarse_loss   *= inv_n;
    return s;
}

} // namespace vsim::kernel_mcf
