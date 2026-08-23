#pragma once
/**
 * include/vsim/analysis/identity_sidecar.hpp
 * ============================================
 * 72-floating-11  |  V5.2.0
 *
 * Output-format cryptic sidecar — identity / entropy / information overlay.
 *
 * DOCTRINE (non-negotiable):
 *   This is an ANALYSIS OVERLAY and REPORT SIDECAR only.
 *   These fields must NEVER be written back into truth-state (.xyz / .xyzFull).
 *   They are derived quantities, not measured quantities.
 *
 * Pipeline position:
 *   state -> trajectory -> analysis -> IdentitySidecarRecord -> sidecar file
 *
 * Sidecar output formats (by convention):
 *   <run_id>.identity.json      — per-run summary record + I-vector series
 *   <run_id>.entropy.tsv        — per-frame entropy time series
 *   <run_id>.infoflow.jsonl     — per-event information flow stream
 *
 * V5.2.0 additions (WO-75B Phase 1 + WO-76 Step 4):
 *   ivec_mean        — Ī_f frame-mean I-vector (ℝ⁵, axes x/y/z/t/w)
 *   ivec_var_diag    — diag(Var_f) per-axis variance of I-vectors
 *   ivec_delta       — ΔĪ_f = Ī_f - Ī_{f-1} (zero for frame 0)
 *   ivec_delta_valid — true when particle count matches previous frame
 *   ivec_particle_count — N_f used to compute Ī_f
 *   formation_age_fs — MCF-CAI M_I: age of this formation record (fs)
 *   defect_memory    — MCF-CAI M_I: [0,1] residual defect information
 *   reaction_entropy_loss — MCF-CAI C_I: bond/reaction entropy loss (nats)
 *   d_chem           — MCF-CAI C_I: [0,1] chemical distinguishability
 *   formation_route  — MCF-CAI C_I: formation pathway label
 *   hidden_W         — MCF-CAI F_I: H = c_j^T W c_j hidden-coord intensity
 *   (projection_loss and entropy_loss_proxy already present; confirmed as F_I)
 *
 * SM's Laws connection:
 *   - SM's Law:  all interactions generate entropy loss (information loss).
 *   - SM's Second Law: when 3+ particles coexist within bond length, a higher-scale
 *     identity forms at barycentric position. Lower identities persist.
 *     => coalescence events produce lineage records + projection loss.
 *
 * 72-floating-11  |  V5.2.0  |  v5.0.0-main
 */

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vsim {
namespace analysis {

// ============================================================================
// IdentitySidecarRecord  —  per-frame or per-run summary
// ============================================================================

struct IdentitySidecarRecord {
	// ---- provenance --------------------------------------------------------
	std::string  run_id;           // source run identifier
	std::string  source_vsim;      // .vsim script path
	int64_t      frame_index  {-1};// -1 = run-level summary
	double       time_fs      {0.0};

	// ---- information / entropy fields -------------------------------------
	double  dataloss            {0.0};  // [0,1] fraction lost vs ideal
	double  dataentropy         {0.0};  // nats; Shannon proxy
	double  recoverable_info    {0.0};  // [0,1] recoverable fraction
	double  projection_loss     {0.0};  // info lost on scale projection
	double  hidden_channel      {0.0};  // unmodelled interaction proxy
	double  coalescence_residual{0.0};  // energy/info from scale crossing
	double  identity_residual   {0.0};  // deviation from identity prototype
	double  entropy_loss_proxy  {0.0};  // dS/dt proxy from displacement field
	double  state_confidence    {1.0};  // [0,1] classification posterior

	// ---- IKK I-vector frame statistics (WO-75B Phase 1) --------------------
	// Ī_f — frame-mean identity vector (ℝ⁵: x=existence, y=EM, z=spatial,
	//         t=temporal, w=internal). See ikk_identity_vector.hpp.
	// Phase 1: derived from scalar sidecar fields (N_f = 1, var = 0).
	std::array<float, 5> ivec_mean        {{ 0.f, 0.f, 0.f, 0.f, 0.f }};  // Ī_f
	std::array<float, 5> ivec_var_diag    {{ 0.f, 0.f, 0.f, 0.f, 0.f }};  // diag(Var_f)
	std::array<float, 5> ivec_delta       {{ 0.f, 0.f, 0.f, 0.f, 0.f }};  // ΔĪ_f
	bool                 ivec_delta_valid { false }; // false for frame 0
	int32_t              ivec_particle_count { 1 };  // N_f (Phase 1 = 1)

	// ---- MCF-CAI Information column (WO-76 Step 4) -------------------------
	// DOCTRINE: all fields below are sidecar-only. Never force inputs.
	//
	// M_I — Macro Information
	double  formation_age_fs       {0.0};  // age of formation record (fs)
	double  defect_memory          {0.0};  // [0,1] residual defect information
	//
	// C_I — Chemical Information
	double  reaction_entropy_loss  {0.0};  // bond/reaction entropy loss (nats)
	double  d_chem                 {0.0};  // [0,1] chemical distinguishability
	std::string formation_route;           // e.g. "precipitation", "CVD"
	//
	// F_I — Fundamental Information
	// (projection_loss and entropy_loss_proxy already exist above; they are
	//  confirmed as F_I mapping.  hidden_W is an addition.)
	double  hidden_W               {0.0};  // H = c_j^T W c_j hidden intensity

	// ---- lineage -----------------------------------------------------------
	std::string  lineage_id;            // stable cross-scale identity tag
	int          scale_level    {0};    // 0=atomistic, 1=bead, 2=macro, ...
	std::vector<std::string> parent_lineage_ids;   // lower-scale parents
	std::vector<std::string> child_lineage_ids;    // higher-scale children

	// ---- coalescence events (SM's Second Law) ------------------------------
	struct CoalescenceEvent {
		double       time_fs;
		int          particle_count;   // number of merging particles
		double       barycentric_x;
		double       barycentric_y;
		double       barycentric_z;
		double       residual_energy;
		double       projection_loss;
		std::string  new_lineage_id;
	};
	std::vector<CoalescenceEvent> coalescence_events;

	// ---- helpers -----------------------------------------------------------
	bool is_run_summary()   const noexcept { return frame_index < 0; }
	bool is_frame_record()  const noexcept { return frame_index >= 0; }
	bool has_coalescence()  const noexcept { return !coalescence_events.empty(); }
};

// ============================================================================
// IdentitySidecarSeries  —  ordered collection of per-frame records
// ============================================================================

struct IdentitySidecarSeries {
	std::string  run_id;
	std::string  source_vsim;
	std::vector<IdentitySidecarRecord> frames;
	IdentitySidecarRecord              run_summary;  // aggregate

	std::size_t frame_count() const noexcept { return frames.size(); }

	// Append a per-frame record.
	void push_frame(IdentitySidecarRecord r) {
		r.run_id = run_id;
		frames.push_back(std::move(r));
	}

	// Build a naive run-level aggregate (averages of scalar fields).
	void build_summary() {
		run_summary = {};
		run_summary.run_id      = run_id;
		run_summary.source_vsim = source_vsim;
		run_summary.frame_index = -1;
		if (frames.empty()) return;
		for (const auto& f : frames) {
			run_summary.dataloss             += f.dataloss;
			run_summary.dataentropy          += f.dataentropy;
			run_summary.recoverable_info     += f.recoverable_info;
			run_summary.projection_loss      += f.projection_loss;
			run_summary.hidden_channel       += f.hidden_channel;
			run_summary.coalescence_residual += f.coalescence_residual;
			run_summary.identity_residual    += f.identity_residual;
			run_summary.entropy_loss_proxy   += f.entropy_loss_proxy;
			run_summary.state_confidence     += f.state_confidence;
			// MCF-CAI Information column
			run_summary.formation_age_fs      += f.formation_age_fs;
			run_summary.defect_memory         += f.defect_memory;
			run_summary.reaction_entropy_loss += f.reaction_entropy_loss;
			run_summary.d_chem                += f.d_chem;
			run_summary.hidden_W              += f.hidden_W;
			// I-vector mean accumulation
			for (int a = 0; a < 5; ++a) {
				run_summary.ivec_mean[a]     += f.ivec_mean[a];
				run_summary.ivec_var_diag[a] += f.ivec_var_diag[a];
			}
		}
		const double n = static_cast<double>(frames.size());
		run_summary.dataloss             /= n;
		run_summary.dataentropy          /= n;
		run_summary.recoverable_info     /= n;
		run_summary.projection_loss      /= n;
		run_summary.hidden_channel       /= n;
		run_summary.coalescence_residual /= n;
		run_summary.identity_residual    /= n;
		run_summary.entropy_loss_proxy   /= n;
		run_summary.state_confidence     /= n;
		run_summary.formation_age_fs      /= n;
		run_summary.defect_memory         /= n;
		run_summary.reaction_entropy_loss /= n;
		run_summary.d_chem                /= n;
		run_summary.hidden_W              /= n;
		const float fn = static_cast<float>(n);
		for (int a = 0; a < 5; ++a) {
			run_summary.ivec_mean[a]     /= fn;
			run_summary.ivec_var_diag[a] /= fn;
		}
	}
};

} // namespace analysis
} // namespace vsim
