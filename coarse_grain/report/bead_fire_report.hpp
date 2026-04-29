#pragma once
/**
 * bead_fire_report.hpp — Automated Outcome Reporting for Bead FIRE Minimization
 *
 * Generates structured Markdown reports documenting:
 *   - Convergence outcome and statistics
 *   - Energy decomposition (steric, electrostatic, dispersion)
 *   - Force evolution (RMS and max)
 *   - FIRE algorithm parameters (α, dt)
 *   - Full step-by-step history (if recorded)
 *
 * Anti-black-box: every minimization step, every force component, every
 * energy contribution is explicitly reported for scientific inspection.
 *
 * Intended for:
 *   - Research documentation
 *   - Computational workflow reporting
 *   - Minimization diagnostics
 *   - Publication-quality output
 *
 * Reference:
 *   - coarse_grain/models/bead_fire.hpp (data structures)
 *   - atomistic/report/report_md.cpp (report pattern)
 *   - .github/copilot-instructions.md §2, §9 (reporting mission)
 */

#include "coarse_grain/models/bead_fire.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace coarse_grain {

/**
 * Generate a Markdown report for a BeadFIRE minimization outcome.
 *
 * @param result   BeadFIREResult containing convergence data and history
 * @param n_beads  Number of beads in the system
 * @param params   FIRE parameters used for minimization
 * @return Markdown-formatted report string
 */
inline std::string bead_fire_report_md(const BeadFIREResult& result,
                                        int n_beads,
                                        const BeadFIREParams& params)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(6);

    // ========================================================================
    // Header
    // ========================================================================
    o << "# Bead FIRE Minimization Report\n\n";
    o << "**Generated:** Lattice-Level FIRE (LL-FIRE) minimization outcome\n\n";
    o << "---\n\n";

    // ========================================================================
    // Convergence Outcome
    // ========================================================================
    o << "## Convergence Outcome\n\n";

    if (result.converged) {
        o << "**Status:** ✓ Converged\n\n";
    } else {
        o << "**Status:** ✗ Did not converge\n\n";
    }

    o << "| Metric | Value |\n";
    o << "|--------|-------|\n";
    o << "| Steps taken | " << result.steps_taken << " / " << params.max_steps << " |\n";
    o << "| N<sub>beads</sub> | " << n_beads << " |\n";
    o << "| U<sub>final</sub> | " << std::setprecision(8) << result.U_final << " kcal/mol |\n";
    o << "| F<sub>RMS</sub> | " << std::setprecision(4) << result.Frms_final;
    if (result.converged && result.Frms_final < params.epsF) {
        o << " ✓ (< " << params.epsF << ") |\n";
    } else {
        o << " (threshold: " << params.epsF << ") |\n";
    }
    o << "| F<sub>max</sub> | " << result.Fmax_final << " kcal/mol·Å |\n";
    o << "| α<sub>final</sub> | " << std::setprecision(6) << result.alpha_final << " |\n";
    o << "| Δt<sub>final</sub> | " << result.dt_final << " fs |\n";
    o << "\n";

    // ========================================================================
    // Energy Decomposition
    // ========================================================================
    o << "## Energy Decomposition\n\n";
    o << "Final per-channel energy contributions:\n\n";
    o << "| Channel | Energy (kcal/mol) | Fraction |\n";
    o << "|---------|-------------------|----------|\n";

    double total = result.U_final;
    if (std::abs(total) > 1e-12) {
        o << "| Steric | " << std::setprecision(6) << result.U_steric 
          << " | " << std::setprecision(2) << (100.0 * result.U_steric / total) << "% |\n";
        o << "| Electrostatic | " << std::setprecision(6) << result.U_electrostatic 
          << " | " << std::setprecision(2) << (100.0 * result.U_electrostatic / total) << "% |\n";
        o << "| Dispersion | " << std::setprecision(6) << result.U_dispersion 
          << " | " << std::setprecision(2) << (100.0 * result.U_dispersion / total) << "% |\n";
    } else {
        o << "| Steric | " << result.U_steric << " | — |\n";
        o << "| Electrostatic | " << result.U_electrostatic << " | — |\n";
        o << "| Dispersion | " << result.U_dispersion << " | — |\n";
    }
    o << "| **Total** | **" << std::setprecision(6) << total << "** | **100%** |\n";
    o << "\n";

    // ========================================================================
    // FIRE Algorithm Parameters
    // ========================================================================
    o << "## FIRE Algorithm Parameters\n\n";
    o << "| Parameter | Value | Description |\n";
    o << "|-----------|-------|-------------|\n";
    o << "| dt<sub>init</sub> | " << params.dt << " fs | Initial timestep |\n";
    o << "| dt<sub>max</sub> | " << params.dt_max << " fs | Maximum timestep |\n";
    o << "| α<sub>init</sub> | " << params.alpha << " | Velocity mixing |\n";
    o << "| f<sub>inc</sub> | " << params.finc << " | dt increase factor |\n";
    o << "| f<sub>dec</sub> | " << params.fdec << " | dt decrease factor |\n";
    o << "| f<sub>α</sub> | " << params.falpha << " | α decrease factor |\n";
    o << "| n<sub>min</sub> | " << params.nmin << " | Steps before dt increase |\n";
    o << "| ε<sub>F</sub> | " << std::setprecision(4) << params.epsF << " kcal/mol·Å | Force convergence |\n";
    o << "| ε<sub>U</sub> | " << std::setprecision(2) << std::scientific << params.epsU 
      << std::fixed << " kcal/mol | Energy convergence |\n";
    o << "| δ<sub>FD</sub> | " << std::setprecision(2) << std::scientific << params.fd_delta 
      << std::fixed << " Å | Finite difference step |\n";
    o << "\n";

    // ========================================================================
    // Convergence History (if recorded)
    // ========================================================================
    if (!result.history.empty()) {
        o << "## Convergence History\n\n";
        o << "Full step-by-step trajectory:\n\n";
        o << "| Step | U<sub>total</sub> | U<sub>steric</sub> | U<sub>elec</sub> | U<sub>disp</sub> | F<sub>RMS</sub> | F<sub>max</sub> | α | Δt | ΔU/N<sub>bead</sub> |\n";
        o << "|------|-------------------|-------------------|------------------|------------------|-----------------|-----------------|---|----|-----------------|\n";

        // Sample history for report (show first 10, last 10, and every 10th in between)
        int n_steps = static_cast<int>(result.history.size());
        std::vector<int> indices_to_show;

        // First 10
        for (int i = 0; i < std::min(10, n_steps); ++i)
            indices_to_show.push_back(i);

        // Every 10th in the middle
        if (n_steps > 20) {
            for (int i = 10; i < n_steps - 10; i += 10)
                indices_to_show.push_back(i);
        }

        // Last 10
        if (n_steps > 10) {
            for (int i = std::max(10, n_steps - 10); i < n_steps; ++i) {
                if (std::find(indices_to_show.begin(), indices_to_show.end(), i) == indices_to_show.end())
                    indices_to_show.push_back(i);
            }
        }

        for (int idx : indices_to_show) {
            const auto& step = result.history[idx];
            o << "| " << step.step;
            o << " | " << std::setprecision(4) << step.U_total;
            o << " | " << std::setprecision(4) << step.U_steric;
            o << " | " << std::setprecision(4) << step.U_electrostatic;
            o << " | " << std::setprecision(4) << step.U_dispersion;
            o << " | " << std::setprecision(4) << step.Frms;
            o << " | " << std::setprecision(4) << step.Fmax;
            o << " | " << std::setprecision(4) << step.alpha;
            o << " | " << std::setprecision(3) << step.dt;
            o << " | " << std::setprecision(2) << std::scientific << step.dU_per_bead << std::fixed;
            o << " |\n";
        }

        if (n_steps > 30) {
            o << "\n*Showing sampled steps. Full history contains " << n_steps << " steps.*\n";
        }
        o << "\n";

        // ====================================================================
        // Convergence Trajectory Summary
        // ====================================================================
        o << "### Convergence Statistics\n\n";

        if (n_steps >= 2) {
            double U_initial = result.history[0].U_total;
            double U_final = result.history[n_steps-1].U_total;
            double dU_total = U_final - U_initial;
            double dU_per_bead_total = dU_total / n_beads;

            o << "| Metric | Value |\n";
            o << "|--------|-------|\n";
            o << "| ΔU<sub>total</sub> | " << std::setprecision(6) << dU_total << " kcal/mol |\n";
            o << "| ΔU / N<sub>bead</sub> | " << std::setprecision(6) << dU_per_bead_total << " kcal/mol |\n";
            o << "| U<sub>initial</sub> | " << std::setprecision(6) << U_initial << " kcal/mol |\n";
            o << "| U<sub>final</sub> | " << std::setprecision(6) << U_final << " kcal/mol |\n";

            // Find max force in trajectory
            double F_max_traj = 0.0;
            for (const auto& step : result.history) {
                F_max_traj = std::max(F_max_traj, step.Fmax);
            }
            o << "| F<sub>max,trajectory</sub> | " << std::setprecision(4) << F_max_traj << " kcal/mol·Å |\n";
            o << "\n";
        }
    } else {
        o << "## Convergence History\n\n";
        o << "*History recording was disabled for this minimization.*\n\n";
    }

    // ========================================================================
    // Mathematical Model
    // ========================================================================
    o << "## Mathematical Model\n\n";
    o << "FIRE (Fast Inertial Relaxation Engine) velocity-Verlet integration:\n\n";
    o << "$$\n";
    o << "\\mathbf{F}_i = -\\nabla_{\\mathbf{r}_i} U_{\\text{total}}(\\{\\mathbf{r}\\}, \\{\\mathbf{q}\\})\n";
    o << "$$\n\n";
    o << "$$\n";
    o << "\\mathbf{r}_{i,t+1} = \\mathbf{r}_{i,t} + \\Delta t \\, \\mathbf{v}_{i,t}\n";
    o << "$$\n\n";
    o << "$$\n";
    o << "\\mathbf{v}_{i,t+1} = (1-\\alpha)\\mathbf{v}_{i,t} + \\alpha\\,\\frac{\\mathbf{F}_{i,t}}{\\|\\mathbf{F}_t\\|}\\,\\|\\mathbf{v}_{i,t}\\|\n";
    o << "$$\n\n";
    o << "**Power criterion:**\n";
    o << "$$\n";
    o << "P = \\sum_i \\mathbf{v}_i \\cdot \\mathbf{F}_i\n";
    o << "$$\n\n";
    o << "**Convergence criteria:**\n";
    o << "$$\n";
    o << "\\|\\mathbf{F}\\|_{\\text{RMS}} < \\varepsilon_F \n";
    o << "\\quad \\lor \\quad \n";
    o << "\\frac{|U_t - U_{t-1}|}{N_{\\text{bead}}} < \\varepsilon_U\n";
    o << "$$\n\n";

    // ========================================================================
    // Force Evaluation Method
    // ========================================================================
    o << "## Force Evaluation\n\n";
    o << "Forces computed via central finite difference:\n\n";
    o << "$$\n";
    o << "F_i^\\alpha = -\\frac{U(\\mathbf{r}_i + \\delta\\hat{\\mathbf{e}}_\\alpha) - ";
    o << "U(\\mathbf{r}_i - \\delta\\hat{\\mathbf{e}}_\\alpha)}{2\\delta}\n";
    o << "$$\n\n";
    o << "where δ = " << std::setprecision(2) << std::scientific << params.fd_delta 
      << std::fixed << " Å.\n\n";
    o << "Energy chain:\n";
    o << "- `BeadSystem` → `InteractionEngine::interaction_energy()`\n";
    o << "- Per-channel spherical harmonic rotation (Wigner D-matrices)\n";
    o << "- Per-(ℓ,m) radial kernel evaluation\n";
    o << "- Environment coupling modulation (η-responsive)\n";
    o << "\n";

    // ========================================================================
    // References
    // ========================================================================
    o << "## References\n\n";
    o << "- **Algorithm:** Bitzek et al., *Phys. Rev. Lett.* **97**, 170201 (2006)\n";
    o << "- **Implementation:** `coarse_grain/models/bead_fire.hpp`\n";
    o << "- **Force model:** `coarse_grain/models/interaction_engine.hpp`\n";
    o << "- **Theory:** `section_anisotropic_beads.tex` §4-5\n";
    o << "\n---\n\n";
    o << "*Report generated by VSEPR-SIM Lattice-Level FIRE minimizer*\n";

    return o.str();
}

/**
 * Write a BeadFIRE outcome report to a Markdown file.
 *
 * @param path      Output file path (e.g., "bead_fire_report.md")
 * @param result    BeadFIREResult with convergence data and history
 * @param n_beads   Number of beads in the minimized system
 * @param params    FIRE parameters used
 * @return true on success, false on file write failure
 */
inline bool write_bead_fire_report(const std::string& path,
                                    const BeadFIREResult& result,
                                    int n_beads,
                                    const BeadFIREParams& params)
{
    std::string content = bead_fire_report_md(result, n_beads, params);

    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << content;
    return out.good();
}

/**
 * Generate a compact single-line convergence summary for console output.
 *
 * @param result   BeadFIREResult
 * @return Single-line summary string
 */
inline std::string bead_fire_summary(const BeadFIREResult& result)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(4);

    if (result.converged) {
        o << "Converged";
    } else {
        o << "Not converged";
    }

    o << " | Steps: " << result.steps_taken;
    o << " | U: " << std::setprecision(6) << result.U_final << " kcal/mol";
    o << " | F_RMS: " << std::setprecision(4) << result.Frms_final << " kcal/mol·Å";

    return o.str();
}

} // namespace coarse_grain
