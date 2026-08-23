#pragma once
/**
 * default_identity_matrices.hpp
 * =============================
 * WO-VSEPR-SIM Extreme Addendum  |  Default Identity Matrices
 *
 * Provides the canonical default matrix structs for particle/state packing
 * used throughout extreme verification and persistent-state simulations.
 *
 * Sections defined here (matching bridge paper §§1–10 and addendum §§1–10):
 *
 *   1.  DefaultScaleIdentityMatrix   — S_n ladder (S0..S6)
 *   2.  DefaultParticleIdentityVector — I_p packed row
 *   3.  DefaultQuarkComponentMatrix  — C_q (3-quark packing)
 *   4.  DefaultLeptonIdentityMatrix  — I_ℓ
 *   5.  DefaultBosonIdentityMatrix   — I_b / mediator identity
 *   6.  DefaultRelationParticleMatrix — R_AB (relation / entanglement)
 *   7.  DefaultProperTimeIdentityMatrix — I_τ (clock-motion residuals)
 *   8.  DefaultDarkSectorProjectionMatrix — I_D  (w-depth projection; guarded)
 *   9.  DefaultEventAnnihilationMatrix — E_k (transition event record)
 *  10.  DefaultSolverComparisonState  — V_run (mode + residual + timing)
 *
 * Design rules:
 *   - All structs are plain aggregates with clear defaults.
 *   - X_D dark-sector fields are present but guarded via `xd_active = false`.
 *   - No virtual dispatch; no heap allocation in the structs themselves.
 *   - Identifiers match the addendum notation directly for traceability.
 *
 * v5.1.4  |  WO-VSEPR-SIM Extreme  |  v5.0.0-main
 */

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>

namespace vsepr {
namespace identity {

// ============================================================================
// 1. Default scale identity matrix  (addendum §1, bridge paper §2)
//
// S = [ S_n | D_n | M_n | L_n | P_n | R_n ]
//
// The scale ladder:
//   S0 = existence     E ∈ {0,1}          mediator: ∅
//   S1 = direction     1D vector           mediator: direction carrier
//   S2 = relation      relation/angle      mediator: phase carrier
//   S3 = spatial struct spatial structure  mediator: γ/g
//   S4 = time evol     time evolution      mediator: W±/Z0
//   S5 = curvature     curvature/gravity   mediator: g_μν
//   S6 = scale depth   w-scale depth       mediator: X_D  (guarded)
// ============================================================================

enum class ScaleSlot : uint8_t {
	S0_Existence    = 0,
	S1_Direction    = 1,
	S2_Relation     = 2,
	S3_Structure    = 3,
	S4_Evolution    = 4,
	S5_Curvature    = 5,
	S6_ScaleDepth   = 6,
	COUNT           = 7,
};

struct ScaleSlotRow {
	uint8_t     sn          {0};    // slot index 0–6
	double      d_n         {0.0};  // dimension descriptor
	double      m_n         {0.0};  // mass / energy at this scale
	double      l_n         {0.0};  // length scale
	double      p_n         {0.0};  // momentum / phase
	double      r_n         {0.0};  // curvature / relation radius
	bool        active      {false};// slot enabled in this run
	bool        guard_xd    {true}; // block X_D writes (S6 only; default on)
	std::string mediator;           // canonical mediator label
	std::string description;        // human label
};

struct DefaultScaleIdentityMatrix {
	static constexpr int N_SLOTS = 7;
	std::array<ScaleSlotRow, N_SLOTS> slots;

	DefaultScaleIdentityMatrix() {
		slots[0] = {0, 0.0, 0.0, 0.0, 0.0, 0.0, true,  true,  "",     "existence"};
		slots[1] = {1, 1.0, 0.0, 0.0, 0.0, 0.0, true,  true,  "",     "direction"};
		slots[2] = {2, 2.0, 0.0, 0.0, 0.0, 0.0, false, true,  "",     "relation"};
		slots[3] = {3, 3.0, 0.0, 0.0, 0.0, 0.0, true,  true,  "γ/g",  "spatial_structure"};
		slots[4] = {4, 4.0, 0.0, 0.0, 0.0, 0.0, false, true,  "W±/Z0","time_evolution"};
		slots[5] = {5, 5.0, 0.0, 0.0, 0.0, 0.0, false, true,  "g_μν", "curvature"};
		slots[6] = {6, 6.0, 0.0, 0.0, 0.0, 0.0, false, true,  "X_D",  "scale_depth"};
	}

	// Apply active slot mask from a bitmask (bit i = S_i active)
	void apply_mask(uint8_t mask) {
		for (int i = 0; i < N_SLOTS; ++i)
			slots[i].active = (mask >> i) & 1;
	}

	// Guard X_D: prevents writes to slot S6 until empirically measured
	void set_guard_xd(bool g) { slots[6].guard_xd = g; }

	bool is_active(ScaleSlot s) const {
		return slots[static_cast<uint8_t>(s)].active;
	}

	bool is_xd_guarded() const { return slots[6].guard_xd; }
};

// ============================================================================
// 2. Default particle identity vector  (addendum §2, bridge paper §1)
//
// I_p = [id, family, gen, Q, B, L_e, L_μ, L_τ, J, P, C, m, τ, x, y, z, w, h]
// ============================================================================

enum class ParticleFamily : uint8_t {
	Unknown   = 0,
	Quark     = 1,
	Lepton    = 2,
	Boson     = 3,
	Hadron    = 4,
	Atom      = 5,
	Molecule  = 6,
};

struct DefaultParticleIdentityVector {
	int64_t       id         {-1};       // persistent identity key
	ParticleFamily family    {ParticleFamily::Unknown};
	int8_t        gen        {0};        // generation or class (1,2,3 for SM)
	double        Q          {0.0};      // electric charge
	int8_t        B          {0};        // baryon number
	int8_t        L_e        {0};        // lepton number: electron family
	int8_t        L_mu       {0};        // lepton number: muon family
	int8_t        L_tau      {0};        // lepton number: tau family
	double        J          {0.0};      // spin / angular identity (half-integer ok)
	int8_t        P          {0};        // parity: +1, -1, 0=undefined
	uint8_t       C          {0};        // color/color-neutral descriptor
	double        m          {0.0};      // mass / effective mass
	double        tau        {0.0};      // lifetime (0 = stable)
	double        x          {0.0};      // projected spatial coord
	double        y          {0.0};
	double        z          {0.0};
	double        w          {0.0};      // hidden scale-depth coordinate
	uint64_t      h          {0};        // persistent hash / lineage seed

	// I_p row is the default row in X(t) persistent-state simulations
	// X(t) = [ I_p,1 | I_p,2 | ... | I_p,N ]
};

// ============================================================================
// 3. Default quark component matrix  (addendum §3, bridge paper §8)
//
// C_q = [ f | Q | B | T3 | T8 | J | m | η ]  (3 rows, one per quark)
//
// Packed hadron identity:  I_h = Π_q(C_q)
// ============================================================================

struct QuarkRow {
	uint8_t flavor   {0};     // 0=u, 1=d, 2=s, 3=c, 4=b, 5=t
	double  Q        {0.0};   // electric charge (e.g. +2/3, -1/3)
	int8_t  B        {0};     // baryon number contribution (+1/3 → store as +1 per quark)
	double  T3       {0.0};   // isospin third component
	double  T8       {0.0};   // SU(3) T8 color generator projection
	double  J        {0.5};   // spin
	double  m        {0.0};   // rest mass
	double  eta      {0.0};   // intrinsic CP phase / rapidity proxy
};

struct DefaultQuarkComponentMatrix {
	std::array<QuarkRow, 3> rows;  // triplet: q1, q2, q3

	// Construct a neutron-proxy (u, d, d)
	static DefaultQuarkComponentMatrix neutron_proxy() {
		DefaultQuarkComponentMatrix c;
		c.rows[0] = {0, +2.0/3.0, +1, +0.5,  0.0, 0.5, 2.2,    0.0}; // u
		c.rows[1] = {1, -1.0/3.0, +1, -0.5,  0.0, 0.5, 4.7,    0.0}; // d
		c.rows[2] = {1, -1.0/3.0, +1, -0.5,  0.0, 0.5, 4.7,    0.0}; // d
		return c;
	}

	// Construct a proton-proxy (u, u, d)
	static DefaultQuarkComponentMatrix proton_proxy() {
		DefaultQuarkComponentMatrix c;
		c.rows[0] = {0, +2.0/3.0, +1, +0.5,  0.0, 0.5, 2.2,    0.0}; // u
		c.rows[1] = {0, +2.0/3.0, +1, +0.5,  0.0, 0.5, 2.2,    0.0}; // u
		c.rows[2] = {1, -1.0/3.0, +1, -0.5,  0.0, 0.5, 4.7,    0.0}; // d
		return c;
	}

	// Charge projection: Q_h = Σ Q_a
	double charge_projection() const {
		return rows[0].Q + rows[1].Q + rows[2].Q;
	}

	// Hidden charge activity: H_Q = q^T q = Σ Q_a^2
	double hidden_charge_activity() const {
		return rows[0].Q*rows[0].Q + rows[1].Q*rows[1].Q + rows[2].Q*rows[2].Q;
	}

	// Hidden-active test: Q_h == 0 but H_Q != 0 (neutron-like)
	bool is_hidden_active(double tol = 1e-9) const {
		return std::abs(charge_projection()) < tol && hidden_charge_activity() > tol;
	}
};

// ============================================================================
// 4. Default lepton identity matrix  (addendum §4, bridge paper §9)
//
// I_ℓ = [id, ℓ, Q, L_e, L_μ, L_τ, J, m, chirality, τ, x, y, z, w]
// ============================================================================

enum class LeptonType : uint8_t {
	Unknown  = 0,
	Electron = 1,
	Muon     = 2,
	Tau      = 3,
	NuE      = 4,
	NuMu     = 5,
	NuTau    = 6,
};

enum class Chirality : int8_t {
	Undefined = 0,
	Left      = -1,
	Right     = +1,
};

struct DefaultLeptonIdentityMatrix {
	int64_t    id          {-1};
	LeptonType lepton_type {LeptonType::Unknown};
	double     Q           {0.0};
	int8_t     L_e         {0};
	int8_t     L_mu        {0};
	int8_t     L_tau       {0};
	double     J           {0.5};
	double     m           {0.0};
	Chirality  chirality   {Chirality::Undefined};
	double     tau         {0.0};    // lifetime (0 = stable for practical purposes)
	double     x           {0.0};
	double     y           {0.0};
	double     z           {0.0};
	double     w           {0.0};

	// Factories for standard SM leptons
	static DefaultLeptonIdentityMatrix electron(int64_t id = -1, double m_e = 0.511e-3) {
		DefaultLeptonIdentityMatrix p;
		p.id = id; p.lepton_type = LeptonType::Electron;
		p.Q = -1.0; p.L_e = 1; p.J = 0.5; p.m = m_e;
		p.chirality = Chirality::Left; p.tau = 0.0; // stable
		return p;
	}

	static DefaultLeptonIdentityMatrix positron(int64_t id = -1, double m_e = 0.511e-3) {
		DefaultLeptonIdentityMatrix p;
		p.id = id; p.lepton_type = LeptonType::Electron;
		p.Q = +1.0; p.L_e = -1; p.J = 0.5; p.m = m_e;
		p.chirality = Chirality::Right; p.tau = 0.0;
		return p;
	}

	static DefaultLeptonIdentityMatrix electron_neutrino(int64_t id = -1, double m_nu = 0.0) {
		DefaultLeptonIdentityMatrix p;
		p.id = id; p.lepton_type = LeptonType::NuE;
		p.Q = 0.0; p.L_e = 1; p.J = 0.5; p.m = m_nu;
		p.chirality = Chirality::Left; p.tau = 0.0;
		return p;
	}
};

// ============================================================================
// 5. Default boson / mediator identity matrix  (addendum §5, bridge paper §10)
//
// I_b = [id, field, Q, J, m, range, coupling, source, target, S_n]
// ============================================================================

enum class BosonField : uint8_t {
	EM     = 0,   // photon
	Strong = 1,   // gluon
	Weak   = 2,   // W±, Z0
	Higgs  = 3,   // Higgs (scalar)
	Dark   = 4,   // X_D placeholder  — guard_xd must be off to activate
	Gravity= 5,   // graviton (reserved)
};

struct DefaultBosonIdentityMatrix {
	int64_t    id        {-1};
	BosonField field     {BosonField::EM};
	double     Q         {0.0};     // electric charge
	double     J         {1.0};     // spin (1 for vectors, 2 for graviton, 0 for Higgs)
	double     m         {0.0};     // mass (GeV/c²; 0 for massless)
	double     range     {0.0};     // 0 = infinite; >0 = finite in sim units
	double     coupling  {0.0};     // effective coupling constant
	int64_t    source_id {-1};      // source particle id (-1 = any)
	int64_t    target_id {-1};      // target particle id (-1 = any)
	uint8_t    scale_n   {3};       // active scale slot S_n (3=spatial structure default)

	// Guard: X_D bosons may not be created until explicitly unguarded
	bool       xd_active {false};   // always false unless dark sector enabled

	// Factories
	static DefaultBosonIdentityMatrix photon() {
		DefaultBosonIdentityMatrix b;
		b.field = BosonField::EM; b.Q = 0; b.J = 1; b.m = 0;
		b.range = 0; b.coupling = 1.0/137.0; b.scale_n = 3;
		return b;
	}

	static DefaultBosonIdentityMatrix gluon() {
		DefaultBosonIdentityMatrix b;
		b.field = BosonField::Strong; b.Q = 0; b.J = 1; b.m = 0;
		b.range = 1.0; // confinement radius proxy in sim units
		b.scale_n = 3;
		return b;
	}

	static DefaultBosonIdentityMatrix W_plus() {
		DefaultBosonIdentityMatrix b;
		b.field = BosonField::Weak; b.Q = +1; b.J = 1; b.m = 80.379;
		b.range = 0.002; // ~0.002 fm in sim units
		b.scale_n = 4;
		return b;
	}

	// X_D placeholder: never activate without empirical measurement
	static DefaultBosonIdentityMatrix dark_mediator_placeholder() {
		DefaultBosonIdentityMatrix b;
		b.field = BosonField::Dark; b.Q = 0; b.J = 0; b.m = 0;
		b.range = 0; b.coupling = 0; b.scale_n = 6;
		b.xd_active = false; // guard enforced
		return b;
	}
};

// ============================================================================
// 6. Default relation-particle matrix  (addendum §6, bridge paper §11)
//
// R_AB = [Δx, Δy, Δz, Δt | T_AB,1, T_AB,2, …, T_AB,m]
//
// Canonical relation-particle form: R_AB = [0,0,0,0 | T_AB] with T_AB ≠ 0.
// Δx = Δy = Δz = Δt = 0 but T_AB ≠ 0 — real without being a signal.
// ============================================================================

struct DefaultRelationParticleMatrix {
	int64_t id_A        {-1};   // particle A
	int64_t id_B        {-1};   // particle B

	// Spatiotemporal separation (0 for canonical relation-particle)
	double  delta_x     {0.0};
	double  delta_y     {0.0};
	double  delta_z     {0.0};
	double  delta_t     {0.0};

	// Non-spatiotemporal relation tensors T_AB,k (up to 8 components by default)
	std::vector<double> T_AB;

	// True if this is a canonical relation-particle (no spacetime separation)
	bool is_canonical_relation() const {
		constexpr double tol = 1e-12;
		return std::abs(delta_x) < tol && std::abs(delta_y) < tol &&
			   std::abs(delta_z) < tol && std::abs(delta_t) < tol &&
			   !T_AB.empty();
	}

	// Construct a canonical clock-motion relation placeholder
	static DefaultRelationParticleMatrix canonical(int64_t a, int64_t b,
												   double T_cm = 1.0) {
		DefaultRelationParticleMatrix r;
		r.id_A = a; r.id_B = b;
		r.delta_x = r.delta_y = r.delta_z = r.delta_t = 0.0;
		r.T_AB.push_back(T_cm);
		return r;
	}
};

// ============================================================================
// 7. Default proper-time identity matrix  (addendum §7, bridge paper §12)
//
// I_τ = [ x̂, p̂, τ̂, H_c, H_m, ⟨τ⟩, R_τ ]
// R_τ = τ̂ − ⟨τ̂⟩
// Clock-motion split: I_cm → I_c + I_m + R_cm
// ============================================================================

struct DefaultProperTimeIdentityMatrix {
	// Canonical position/momentum operators (expectation values or eigenvalues)
	double  x_hat       {0.0};   // ⟨x̂⟩
	double  p_hat       {0.0};   // ⟨p̂⟩

	// Proper-time eigenvalue / expectation
	double  tau_hat     {0.0};   // τ̂ ≡ τ(x̂, p̂)
	double  tau_mean    {0.0};   // ⟨τ̂⟩ over ensemble

	// Clock and motion Hamiltonians
	double  H_c         {0.0};   // clock Hamiltonian
	double  H_m         {0.0};   // motion Hamiltonian

	// Proper-time residual
	double  R_tau() const { return tau_hat - tau_mean; }

	// Clock-motion relation  R_cm = [0,0,0,0 | T_cm]
	DefaultRelationParticleMatrix R_cm;

	// Split into clock and motion components (addendum §7)
	struct ClockMotionSplit {
		double I_c_tau {0.0};   // clock part
		double I_m_tau {0.0};   // motion part
		DefaultRelationParticleMatrix R_cm;
	};

	ClockMotionSplit split(int64_t id_clock = -1, int64_t id_motion = -1) const {
		ClockMotionSplit s;
		s.I_c_tau = H_c;
		s.I_m_tau = H_m;
		s.R_cm = DefaultRelationParticleMatrix::canonical(id_clock, id_motion, R_tau());
		return s;
	}
};

// ============================================================================
// 8. Default dark-sector projection matrix  (addendum §8)
//
// I_D = [x, y, z, w, ρ, K_G(w), K_EM(w), X_D, R_D]
//
// ρ_dark = ρ_grav - ρ_vis
//        = ∫ ρ(x,y,z,w;t) [K_G(w) − K_EM(w)] dw
//
// POLICY: xd_active = false by default. Not enabled for classical MD.
// ============================================================================

struct DefaultDarkSectorProjectionMatrix {
	double  x         {0.0};
	double  y         {0.0};
	double  z         {0.0};
	double  w         {0.0};   // hidden scale-depth coordinate

	double  rho_total {0.0};   // ρ(x,y,z,w;t) at this point
	double  K_G       {0.0};   // gravitational projection kernel K_G(w)
	double  K_EM      {0.0};   // EM projection kernel K_EM(w)

	// Derived projections
	double rho_grav() const { return rho_total * K_G; }
	double rho_vis()  const { return rho_total * K_EM; }
	double rho_dark() const { return rho_grav() - rho_vis(); }

	// X_D placeholder (guard enforced — do not activate without measurement)
	double  X_D       {0.0};   // dark mediator field amplitude
	double  R_D       {0.0};   // dark relation residual

	bool    xd_active {false}; // guard: blocks X_D writes until empirically measured
};

// ============================================================================
// 9. Default event / annihilation matrix  (addendum §9, bridge paper §13)
//
// E_k = [event_id, t_k, parent_A, C_out, Π_out, products, ε_cons, R_k]
//
// General transition:  I_parent --A--> C_out --Π--> I_products
// ============================================================================

enum class EventTransitionType : uint8_t {
	Unknown       = 0,
	Decay         = 1,
	Annihilation  = 2,
	Collision     = 3,
	Emission      = 4,
	Capture       = 5,
	PhaseChange   = 6,
	// Detector projection types (CERN-style inverse packing)
	DetectorProj  = 7,   // P_det: X → y_det
	DetectorRecon = 8,   // R_det: y_det → X̃  (must stay separate from P_det)
};

struct EventProductRow {
	std::string label;
	double      px      {0.0};
	double      py      {0.0};
	double      pz      {0.0};
	double      energy  {0.0};
	double      mass    {0.0};
	int         charge  {0};
};

struct DefaultEventAnnihilationMatrix {
	uint64_t              event_id   {0};
	double                t_k        {0.0};   // event time
	int64_t               parent_A   {-1};    // primary parent id
	int64_t               parent_B   {-1};    // secondary parent id (for pair events)
	EventTransitionType   type       {EventTransitionType::Unknown};

	// C_out: output configuration descriptor (packed as bitmask or string)
	uint32_t              C_out      {0};

	// Π_out: product momentum tensor (summed 3-vector)
	double  pi_out_x     {0.0};
	double  pi_out_y     {0.0};
	double  pi_out_z     {0.0};

	// Products
	std::vector<EventProductRow> products;

	// Conservation residuals
	double  epsilon_E    {0.0};   // |E_out − E_expected| / E_expected
	double  epsilon_p    {0.0};   // |p_out − p_in|

	// Event residual R_k (general state residual after transition)
	double  R_k          {0.0};

	// Detector projection/reconstruction (stored separately — never merged)
	bool    is_detector_proj  {false};
	bool    is_detector_recon {false};
};

// ============================================================================
// 10. Default solver comparison state  (addendum §10)
//
// V_run = [mode, E, ψ, I, R, Λ, ε_E, ε_R, ε_λ, ε_cons, T_runtime]
//
// Modes: M = {G_dyn, G_norm, G+SchEq+Eigen, SchEq}
// ============================================================================

enum class SolverMode : uint8_t {
	GluonDynamic   = 0,   // G_dyn  — dynamic gluon field
	GluonNormal    = 1,   // G_norm — normalized gluon
	GluonSchEqEigen= 2,   // G + SchEq + Eigen  — full coupled
	SchEqOnly      = 3,   // SchEq  — Schrödinger equation only
};

inline const char* solver_mode_name(SolverMode m) noexcept {
	switch (m) {
		case SolverMode::GluonDynamic:    return "gluon_dynamic";
		case SolverMode::GluonNormal:     return "gluon_normal";
		case SolverMode::GluonSchEqEigen: return "gluon_scheq_eigen";
		case SolverMode::SchEqOnly:       return "scheq_only";
	}
	return "unknown";
}

struct DefaultSolverComparisonState {
	SolverMode  mode        {SolverMode::GluonDynamic};

	// State descriptors
	double      E           {0.0};   // energy eigenvalue or total energy
	double      psi         {0.0};   // wavefunction norm / amplitude proxy
	double      I_state     {0.0};   // identity state descriptor
	double      R_state     {0.0};   // residual state
	double      Lambda      {0.0};   // eigenvalue / scale parameter

	// Convergence residuals
	double      epsilon_E   {0.0};   // energy residual
	double      epsilon_R   {0.0};   // position residual
	double      epsilon_lam {0.0};   // eigenvalue residual
	double      epsilon_cons{0.0};   // conservation residual

	// Timing
	double      T_runtime_s {0.0};   // wall time in seconds

	// Comparison result (populated by Compare())
	bool        converged   {false};
	std::string note;                // free-text annotation
};

// ============================================================================
// Aggregate: all default identity matrices in one place
// ============================================================================

struct DefaultIdentityMatrixSet {
	DefaultScaleIdentityMatrix     scale;          // §1  S matrix
	DefaultParticleIdentityVector  particle;       // §2  I_p
	DefaultQuarkComponentMatrix    quark;          // §3  C_q
	DefaultLeptonIdentityMatrix    lepton;         // §4  I_ℓ
	DefaultBosonIdentityMatrix     boson;          // §5  I_b
	DefaultRelationParticleMatrix  relation;       // §6  R_AB
	DefaultProperTimeIdentityMatrix proper_time;   // §7  I_τ
	DefaultDarkSectorProjectionMatrix dark;        // §8  I_D
	DefaultEventAnnihilationMatrix event;          // §9  E_k
	DefaultSolverComparisonState   solver;         // §10 V_run

	// Active flags mirroring [identity_matrices] config section
	bool use_particle    {true};
	bool use_quark       {true};
	bool use_lepton      {true};
	bool use_boson       {true};
	bool use_relation    {false};
	bool use_proper_time {false};
	bool use_dark        {false};   // requires xd_active override
	bool use_event       {true};
};

} // namespace identity
} // namespace vsepr
