# The Extreme Identity Matrix Layer
## Theory, Architecture, and Implementation Guide

**WO-VSEPR-SIM Extreme Addendum**
Version: v5.1.4 | Branch: v5.0.0-main
Status: ACTIVE

---

## Part I — Theoretical Foundation

### 1.1 Why a New Layer?

Classical MD can propagate positions, velocities, and forces accurately for
atoms and molecules. It cannot, by itself, reason about *what* a particle is
at the level of conservation laws, quantum numbers, or field identity.

The identity matrix layer introduces a second data spine — running parallel
to the geometric spine (r, v, F) — that carries the answer to:

> "What intrinsic constraints govern how particle A can interact with
> particle B, and what the products of that interaction must conserve?"

This is not a demand to solve the Standard Model. It is a structured way to
gate, label, and log interactions that the classical integrator cannot
distinguish by geometry alone. Weak decay looks geometrically identical to an
elastic collision. The identity layer makes the difference observable and
verifiable.

---

### 1.2 The Two-Spine Model

Every particle at time *t* has two representations that must stay consistent:

```
Geometric spine:  r_i(t), v_i(t), F_i(t)          — classical MD
Identity spine:   I_p,i = [id, family, gen, Q, B,
							L_e, L_μ, L_τ, J, P, C,
							m, τ, x, y, z, w, h]   — this layer
```

The geometric spine is evolved by the Velocity Verlet integrator. The
identity spine is evolved by **event gates** — discrete transitions that fire
when a gate condition is satisfied and produce a new identity state for one
or more particles.

The integrator does not need to know about weak interactions. The event gate
layer sits *above* the integrator and inspects `I_p` pairs at each step to
decide whether a transition is triggered.

---

### 1.3 The Scale Identity Ladder

Every interaction belongs to a scale slot S_n. The ladder is:

```
S0 — existence        E ∈ {0,1}            (particle is present)
S1 — direction        1D vector carrier     (momentum direction)
S2 — relation         phase / angle         (entanglement, clock)
S3 — spatial structure  γ, g               (EM, strong — classical MD home)
S4 — time evolution   W±, Z0               (weak interaction home)
S5 — curvature        g_μν                 (gravity — easily added, see §3.4)
S6 — scale depth      X_D (guarded)        (dark sector — hypothesis only)
```

For the current addendum, S3 and S4 are the active zones:
- **S3**: everything in existing classical MD (Lennard-Jones, Coulomb, bonds).
- **S4**: weak transitions (beta decay, W-mediated flavor change, lepton number
  change). This is the new territory being opened.

The `scale_mask` on each particle records which slots are live. The integrator
only looks at S3 forces. The event gate layer checks S4 flags before attempting
a weak transition.

---

### 1.4 The Particle Identity Vector I_p

```
I_p = [id, family, gen, Q, B, L_e, L_μ, L_τ, J, P, C, m, τ, x, y, z, w, h]
```

The fields relevant to weak interaction testing are:

| Field | Role in weak physics |
|---|---|
| `family` | Lepton vs quark — determines which weak current applies |
| `gen` | Generation: distinguishes e/μ/τ and u/d/s/c/b/t |
| `Q` | Charge conservation check (ΔQ = 0 across event) |
| `B` | Baryon number conservation (ΔB = 0 in SM) |
| `L_e, L_μ, L_τ` | Lepton number conservation per family |
| `J` | Spin — tracks helicity flip in V-A coupling |
| `P` | Parity — weak interaction violates P |
| `τ` | Lifetime — sets the per-step decay probability |
| `h` | Hash — persistent identity across product chains |

A weak transition gate checks that ΔQ = 0, ΔB = 0, ΔL_family = 0, and that
the spin structure is consistent with V-A coupling before it fires.

---

### 1.5 The Quark Component Matrix C_q

For hadron-level weak interactions (beta decay: d → u + W⁻), the hadron is
unpacked into its quark triplet:

```
C_q = [ f₁  Q₁  B₁  T₃₁  T₈₁  J₁  m₁  η₁ ]
	  [ f₂  Q₂  B₂  T₃₂  T₈₂  J₂  m₂  η₂ ]
	  [ f₃  Q₃  B₃  T₃₃  T₈₃  J₃  m₃  η₃ ]
```

The weak transition fires on a single quark row (d→u), then the hadron
identity is repacked. This is a proxy for the hadronic matrix element —
not QFT, but sufficient to test that:
1. The flavor change is correctly logged.
2. Charge and baryon number are conserved by construction.
3. The W⁻ boson product carries the correct quantum numbers.
4. The lepton pair (e⁻, ν̄_e) produced by the W⁻ decay has correct L_e.

---

### 1.6 The Boson Identity Matrix I_b and Weak Mediators

The W± and Z0 bosons are first-class objects in this layer:

```
W⁺: field=Weak, Q=+1, J=1, m=80.379 GeV, range≈0.002 fm proxy, scale_n=S4
W⁻: field=Weak, Q=-1, J=1, m=80.379 GeV, range≈0.002 fm proxy, scale_n=S4
Z0: field=Weak, Q=0,  J=1, m=91.187 GeV, range≈0.002 fm proxy, scale_n=S4
```

In reduced-particle units the mass and range are scaled to the simulation's
unit system. The key constraint is not the exact mass but the quantum numbers:
the W⁻ produced in β⁻ decay must carry Q = −1 and decay immediately to
(e⁻, ν̄_e) with correct lepton numbers.

The boson lives for exactly one event step as a virtual carrier, then decays
to its products. Its lifetime `τ` is set to `dt` (one timestep) so it never
propagates as a real particle in the geometric spine.

---

### 1.7 Proper-Time Residuals and the Clock-Motion Split

The proper-time identity matrix I_τ is used to test time-dilation residuals
in multi-body weak decay chains:

```
I_τ = [x̂, p̂, τ̂, H_c, H_m, ⟨τ̂⟩, R_τ]
R_τ = τ̂ − ⟨τ̂⟩
```

For a muon (lifetime ~2.2 μs in its rest frame), the observed decay time
in the lab frame is dilated by γ = E/m. The proper-time matrix tracks the
residual between the expected dilated lifetime and the observed one. A
nonzero R_τ that exceeds the tolerance threshold flags a time-dilation
accounting error — useful for testing that the decay probability is being
applied in the correct reference frame.

---

### 1.8 The Event/Annihilation Matrix E_k

Every weak transition is logged as an E_k event:

```
E_k = [event_id, t_k, parent_A, parent_B, type,
	   C_out, Π_out, products, ε_E, ε_p, R_k]
```

The `type` field distinguishes:
- `Decay` — single-parent weak decay (n → p + e⁻ + ν̄_e)
- `Annihilation` — pair annihilation (e⁻ + e⁺ → 2γ)
- `Collision` — W-mediated scattering (ν_e + e⁻ → ν_e + e⁻)

The conservation residuals ε_E (energy) and ε_p (momentum) are computed
after each event and logged. A sweep over many events produces a residual
distribution — the test passes if the distribution is consistent with
numerical precision, not with a physical violation.

---

### 1.9 The Solver Comparison State V_run

The extreme suite runs the same scenario under four solver modes and
compares residuals:

```
V_run = [mode, E, ψ, I, R, Λ, ε_E, ε_R, ε_λ, ε_cons, T_runtime]

Modes:
  G_dyn          — dynamic gluon field (full S3 force)
  G_norm         — normalized gluon (static color proxy)
  G + SchEq + Eigen — coupled field + wavefunction + eigenvalue
  SchEq only     — Schrödinger equation, no gluon dynamics
```

The comparison `C = Compare(G_dyn, G_norm, G+SchEq+Eigen, SchEq)` checks
whether the weak decay rate is stable across solver modes. If a decay fires
in one mode and not another for the same seed and initial conditions, the
gate logic has a solver dependency — a bug, not physics.

---

## Part II — Architecture

### 2.1 Data Flow

```
[.vsim script]
	  |
	  v
[VsimParser]
  identity_matrices section  →  DefaultIdentityMatrixSet (active flags)
  verification section       →  VerificationSection (mode flags)
  extras.togglescale section →  ExtrasToggleScaleSection (slot control)
	  |
	  v
[Simulation kernel — geometric spine]
  r_i(t), v_i(t), F_i(t)   evolved by Velocity Verlet
	  |
	  v  (parallel, every step)
[Identity event gate layer]
  For each particle pair (i,j):
	1. Read I_p,i and I_p,j from identity spine
	2. Check decay gate for single-particle events (τ_i, step)
	3. Check interaction gate for pair events (Q, L, B, flavor)
	4. If gate fires → build E_k event record
	5. Update identity spine (new products, new I_p rows)
	6. Log E_k to JSONL and [ANN]/[DECAY]/[PP] console stream
	  |
	  v
[Verification layer]
  Compare(V_run_0, V_run_1, V_run_2, V_run_3)
  Check ε_E, ε_p, ε_cons against thresholds
  Write solver_comparison.tsv and verification_report.md
```

### 2.2 Header Dependencies

```
include/identity/
  default_identity_matrices.hpp   — all 10 matrix structs
  particle_identity.hpp           — ParticleIdentity (birth hash, quantum numbers)
  particle_identity_seeder.hpp    — deterministic birth-hash seeding, are_anti_partners()

include/vsim/bridge65/
  atom_event.hpp                  — classical MD atom event record
  atom_event_logger.hpp           — append-only logger, event limiter
  annihilation_event.hpp          — AnnihilationEvent, check_annihilation_gate()

include/vsim/extras/
  togglescale.hpp                 — ToggleScaleOperator, scale-slot control

include/vsim/bundle/
  x_bundle.hpp                    — .x suite execution container
```

### 2.3 The Weak Interaction Gate (Design)

The weak interaction gate is the S4 analogue of the annihilation gate defined
in the bridge paper. It is not yet implemented in source — this section defines
the interface contract so that implementation is unambiguous.

```
Gate signature:
  WeakTransitionResult check_weak_gate(
	  const ParticleIdentity& pi,  // candidate particle
	  double step_dt,              // current timestep
	  uint64_t step,               // current step number
	  uint64_t rng_state           // deterministic RNG state
  ) noexcept;

Gate logic:
  1. Decay probability: p_decay = 1 − exp(−dt / τ_i)
	 where τ_i = pi.tau (lifetime in simulation time units)
  2. RNG draw: r = rng(rng_state, pi.id, step)
  3. Fire if: r < p_decay  AND  pi.scale_mask has S4 bit set
  4. Product selection: use pi.family and pi.gen to look up the
	 decay channel table (e.g. n → p + e⁻ + ν̄_e for neutron)
  5. Build E_k event record with type=Decay
  6. Conservation check: ΔQ=0, ΔB=0, ΔL_family=0
  7. If conservation check fails → log error, do not fire

Key constraint: the RNG draw uses (pi.id, step) as a seed extension
so that the same particle decays at the same step in every replay
with the same run seed. Determinism is mandatory.
```

### 2.4 Gravity Integration Note

Adding true Newtonian gravity to the existing force pipeline is deliberately
straightforward — it requires only one new force term at scale slot S5:

```
F_i,grav = Σ_{j≠i}  G · m_i · m_j / r_ij²  · r̂_ij
```

In the matrix-force policy (bridge paper §4):

```
F_i = Σ_{j≠i} ( M_ij^LJ · F_ij^LJ
			   + M_ij^C  · F_ij^C
			   + M_ij^B  · F_ij^B
			   + M_ij^T  · F_ij^T
			   + M_ij^G  · F_ij^G     ← new: gravitational term
			   + M_ij^X  · F_ij^X )   (remains 0 in v5.1.4)
```

`M_ij^G` is the S5 gate matrix. To enable gravity:
1. Set `S5_active = true` in `DefaultScaleIdentityMatrix`.
2. Set `scale_mask |= (1 << 5)` on all massive particles.
3. Add the Newtonian force accumulator to the inner loop.
4. No new identity fields are needed — `m` already exists in `I_p`.

General Relativistic corrections (g_μν deformation of the force law) use
the `DefaultDarkSectorProjectionMatrix` K_G kernel but are not required
for a Newtonian gravity test.

---

## Part III — Implementation: A Simple Weak Interaction Test

This section walks through a complete, minimal weak interaction test:
neutron beta decay in a box of two particles. The purpose is to verify
that the identity gate fires correctly, products have correct quantum numbers,
and conservation residuals are within tolerance.

### 3.1 The .vsim Script

Save as `scripts/weak/neutron_beta_decay_minimal.vsim`:

```toml
[meta]
name    = "neutron_beta_decay_minimal"
version = "5.1.4"
purpose = "Minimal weak gate test: n -> p + e- + nu_e_bar"

[system]
mode  = "weak_decay_test"
space = "3d"
units = "reduced_particle"
seed  = 4201

# ---- Particle definitions ----

[particles.neutron]
count          = 1
family         = "hadron"
gen            = 1
mass           = 939.565         # MeV/c² proxy in reduced units
charge         = 0.0
baryon_number  = 1
lepton_e       = 0
spin           = 0.5
lifetime       = 880.2           # seconds in reduced time units
scale_mask     = [0, 1, 3, 4]   # S0+S1+S3+S4 active
position       = [0.0, 0.0, 0.0]
velocity       = [0.01, 0.0, 0.0]

[particles.proton_observer]
count          = 1
family         = "hadron"
gen            = 1
mass           = 938.272
charge         = 1.0
baryon_number  = 1
lepton_e       = 0
spin           = 0.5
lifetime       = 0.0             # stable
scale_mask     = [0, 1, 3]      # S0+S1+S3 only (no weak decay)
position       = [5.0, 0.0, 0.0]
velocity       = [-0.01, 0.0, 0.0]

# ---- Identity matrices ----

[identity_matrices]
default_particle  = true
quark_component   = true
lepton_identity   = true
boson_identity    = true
event_annihilation = true
relation_particle  = false
proper_time        = false
dark_projection    = false

# ---- Weak interaction ----

[weak]
enabled           = true
mediator          = "W_boson_virtual"
channels          = ["neutron_beta_minus"]
conserve_charge   = true
conserve_baryon   = true
conserve_lepton   = true
log_residuals     = true

# ---- Scale toggle ----

[extras.togglescale]
enabled      = true
default_mode = "pin"
slots        = [0, 1, 3, 4]     # S0 S1 S3 S4 — EM + weak active
guard_xd     = true             # X_D writes blocked

# ---- Verification ----

[verification]
run_all          = true
compare          = false         # single mode for this test
convergence      = true
persistent_state = true

[verification.modes]
gluon_dynamic    = false
gluon_normal     = false
scheq_only       = true          # simplest solver mode for weak test

# ---- Run ----

[run]
steps           = 2000
dt              = 0.001
sample_interval = 1

# ---- Outputs ----

[outputs]
trajectory     = "neutron_decay.xyzf"
events_jsonl   = "neutron_decay_events.jsonl"
live_log       = "neutron_decay_live.log"
summary        = "neutron_decay_summary.md"

[observe]
particle_particle = true
decay             = true
weak_transition   = true
```

---

### 3.2 What the Runtime Must Do (Implementation Contract)

When the parser sees `mode = "weak_decay_test"` and `[weak] enabled = true`,
the runtime must:

**Step A — Build identity spines**

For each particle in `[particles.*]`:
1. Call `ParticleIdentitySeeder::spawn_and_build(...)` with the parsed
   quantum numbers.
2. Store the resulting `ParticleIdentity` in the per-particle identity spine.
3. Set `scale_mask` from the `scale_mask` array in the config.

**Step B — Main loop, per step**

```
for step in 0..run.steps:
	// 1. Geometric integration (unchanged)
	verlet_integrate(r, v, F, dt)

	// 2. Weak decay gate (new)
	for each particle pi with S4 bit set in scale_mask:
		result = check_weak_gate(pi, dt, step, run_seed)
		if result.fired:
			// a. Select decay channel (neutron → proton + W_virtual)
			channel = get_decay_channel(pi.family, pi.gen, pi.charge)
			// b. Produce W boson (virtual, lifetime = dt)
			W = make_W_boson(channel.delta_charge, step)
			// c. Decay W → (e⁻, ν̄_e)
			products = decay_W(W, pi.position, pi.velocity)
			// d. Replace neutron identity with proton identity
			pi_new = make_proton_from_neutron(pi, step)
			// e. Add electron and antineutrino to identity spine
			add_particle(products.electron, step)
			add_particle(products.antineutrino, step)
			// f. Build E_k event record
			E_k = build_decay_event(step, time, pi, pi_new, products)
			// g. Check conservation
			check_conservation(E_k)   // asserts ΔQ=0, ΔB=0, ΔL_e=0
			// h. Log
			logger.log(E_k)
			print("[DECAY] " + E_k.decay_line())
```

**Step C — Conservation check**

```
ΔQ  = Q_proton + Q_electron - Q_neutron
	= (+1)    + (-1)        - (0)
	= 0  ✓

ΔB  = B_proton - B_neutron
	= (+1)     - (+1)
	= 0  ✓

ΔL_e = L_e(electron) + L_e(antineutrino) - L_e(neutron)
	 = (+1)           + (-1)              - (0)
	 = 0  ✓
```

If any of these is nonzero the event is rejected and logged as a
`conservation_violation` in the JSONL stream.

**Step D — Expected live output**

```
[DECAY] step=1347 t=1.347 id=neutron_001 mode=beta_minus
		product_p=proton_001 product_e=e_001 product_nu=nubar_e_001
		dE=0.000782 GeV  dQ=0  dB=0  dL_e=0  residualE=2.1e-9

[PP]    step=1347 t=1.347 idA=neutron_001 idB=proton_observer_001
		r=3.472 Erel=0.0041 gate=none action=NONE
```

---

### 3.3 Expected Verification Output

After 2000 steps the summary should include:

```markdown
# Neutron Beta Decay Minimal — Verification Report

## Run parameters
seed        : 4201
steps       : 2000
dt          : 0.001
solver_mode : scheq_only

## Decay events
total_decay_events   : 1         (probabilistic; may be 0 for short run)
conservation_passed  : 1
conservation_failed  : 0

## Residuals (if event fired)
epsilon_E  : < 1e-6   (energy)
epsilon_p  : < 1e-6   (momentum)
epsilon_Q  : 0.000    (charge)
epsilon_B  : 0.000    (baryon)
epsilon_L  : 0.000    (lepton)

## Scale slots active
S0 (existence)       : YES
S1 (direction)       : YES
S3 (spatial struct)  : YES
S4 (time evolution)  : YES — weak gate active
S6 (scale depth/X_D) : GUARDED

## Result
PASS — conservation satisfied, residuals within tolerance
```

---

### 3.4 Extending to True Gravity (One Step)

To add Newtonian gravity to the same test, add to the `.vsim` script:

```toml
[extras.togglescale]
slots = [0, 1, 3, 4, 5]         # add S5 = curvature/gravity

[gravity]
enabled = true
G       = 6.674e-11              # Newton's constant in sim units
```

And in the runtime inner loop, after the weak gate:

```cpp
// Gravity force accumulation (S5 slot)
if (scale.is_active(ScaleSlot::S5_Curvature)) {
	for each pair (i, j):
		double r2  = distance_squared(r_i, r_j);
		double mag = cfg.G * pi.mass * pj.mass / r2;
		F_i += mag * unit_vector(r_j - r_i);
		F_j -= mag * unit_vector(r_j - r_i);
}
```

No new identity fields, no new event channels, no new conservation checks.
Gravity is a force addition at S5; the identity layer is unchanged.
The only verification addition is confirming that the total mechanical energy
(kinetic + gravitational potential) is conserved across steps:

```
E_total = Σ_i (½ m_i |v_i|²) − Σ_{i<j} G m_i m_j / r_ij

ε_grav = |E_total(t) − E_total(0)| / |E_total(0)|  < threshold
```

This is the same conservation residual pattern already used for the weak
decay test — the framework handles both without modification.

---

## Appendix A — Quantum Number Conservation Table

| Interaction | ΔQ | ΔB | ΔL_e | ΔL_μ | ΔL_τ | Parity |
|---|---|---|---|---|---|---|
| EM (S3) | 0 | 0 | 0 | 0 | 0 | conserved |
| Strong (S3) | 0 | 0 | 0 | 0 | 0 | conserved |
| Weak (S4) | 0 | 0 | 0 | 0 | 0 | **violated** |
| Gravity (S5) | 0 | 0 | 0 | 0 | 0 | conserved |
| Annihilation | 0 | 0 | 0 | 0 | 0 | conserved |
| X_D (S6) | — | — | — | — | — | **hypothesis; guarded** |

The weak interaction conserves all additive quantum numbers but violates
parity (P) and CP (to a smaller degree). The `P` field in `I_p` tracks
the parity state of each particle. A weak event that flips P without
setting the correct V-A helicity structure should produce a
`parity_violation_unexpected` flag in the event log — indicating a bug in
the gate logic, not actual BSM physics.

---

## Appendix B — File Index

| File | Purpose |
|---|---|
| `include/identity/default_identity_matrices.hpp` | All 10 default matrix structs |
| `include/identity/particle_identity.hpp` | ParticleIdentity with birth_hash, scale_mask |
| `include/identity/particle_identity_seeder.hpp` | Deterministic birth-hash seeding |
| `include/vsim/bridge65/annihilation_event.hpp` | Annihilation gate and event record |
| `include/vsim/extras/togglescale.hpp` | Scale-slot toggle operator |
| `include/vsim/bundle/x_bundle.hpp` | .x suite bundle manifest |
| `include/vsim/vsim_document.hpp` | VsimDocument with identity_matrices / verification / extras sections |
| `src/vsim/vsim_parser.cpp` | Parser appliers for new sections |
| `docs/theory/v5114_bridge_paper.md` | Bridge paper: global state, MD spine, force policy |
| `docs/theory/bridge65_pre_subatomic_policy.md` | Classical MD vs subatomic separation doctrine |
| `scripts/weak/neutron_beta_decay_minimal.vsim` | Example script (§3.1) |

---

---

## Appendix C — Kernel Audit: Dual-Seed Assignment and 256-bit Seeds

### C.1 The Problem: One Seed Is Not Enough

Classical MD uses a single RNG seed to produce a reproducible run. Every
particle in the run shares that context implicitly. When the simulation
introduces weak interactions, bonding uncertainty resolution, or process
evolution equations, two distinct kinds of randomness become relevant:

1. **Instance randomness** — "which particle decays at which step?" This is
   a function of the particle's own birth context (type, position, index).
   Reproducibility means the same particle, in the same run, decays at the
   same step. This is `run_seed`.

2. **World randomness** — "what is the shared environmental constant that
   governs the uncertainty floor for atomic bonding in this simulation
   space?" This is a property of the world, not of any individual particle.
   Every particle must be born with knowledge of this constant baked into
   its identity hash. This is `world_seed`.

If only one of the two seeds is provided, the system operates in
**single-seed mode** (fully backward-compatible). If both are provided,
the system operates in **dual-seed mode** — the particle birth hash is
computed in two FNV-1a passes. This is the Kernel Audit requirement.

### C.2 The Dual-Seed Birth Hash Protocol

```
Pass 1 — instance context (run_seed)
  h = FNV1a_64(run_seed || birth_index || type_code || charge ||
               mass || spin || lepton_family || baryon_number ||
               isospin_proxy || snap(x) || snap(y) || snap(z) || snap(w))

Pass 2 — world context (world_seed)  [only if world_seed.is_set()]
  h = FNV1a_64_continue(h, world_seed[255:0])

birth_hash = h
```

The result is a 64-bit value that encodes both the particle's own identity
and the world's environmental constant. Two runs with the same `run_seed`
but different `world_seed` values produce completely different birth hashes
for every particle — the world has changed, so all identities change.

### C.3 Using world_seed in Process Evolution Equations

Consider an atomic bonding model where the bond-formation probability
includes a deterministic uncertainty term:

```
P_bond(i,j,t) = P_classical(r_ij, E_ij) * Ω(h_i, h_j, W)

where:
  P_classical  = classical bonding probability (Lennard-Jones / VSEPR)
  h_i, h_j     = birth hashes of the two candidate particles
  W            = world_seed.low64()  (64-bit projection of world constant)
  Ω(h_i,h_j,W) = deterministic uncertainty modulation:
                 XOR-mix of h_i, h_j, W, then normalize to [1-δ, 1+δ]
                 where δ is the uncertainty amplitude parameter
```

The key property: `Ω` is fully deterministic for any fixed {h_i, h_j, W}.
Two identical particles (same quantum numbers, same birth position) in two
different world configurations produce different `Ω` values because `W`
differs. This is the correct model for environment-dependent bonding — the
world's seed is the proxy for "the local quantum vacuum state" in which
the bond forms.

### C.4 Seed Values — Supported Formats and Maximum Value

Seed values support up to 256 bits in all VSIM seed fields:

| Format | Example | Description |
|---|---|---|
| Decimal integer | `4201` | uint64; placed in w[0] |
| Hex (0x prefix) | `"0xdeadbeef"` | up to 64 hex chars |
| Hex (no prefix) | `"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"` | exactly 1..64 hex chars |
| Base64 | `"//////////////////////////////////////////8="` | 44-char padded base64 |
| Binary string | `"1111...1111"` | up to 256 chars of '0'/'1', MSB first |

**Canonical maximum value (2²⁵⁶ − 1):**
```
Binary  : 1111111111111111111111111111111111111111111111111111111111111111
          1111111111111111111111111111111111111111111111111111111111111111
          1111111111111111111111111111111111111111111111111111111111111111
          1111111111111111111111111111111111111111111111111111111111111111

Hex     : ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff

Base64  : //////////////////////////////////////////8=
```

All three represent the same 256-bit integer. The `seed256_max()` function
in `include/vsim/seed256.hpp` returns this value.

### C.5 .vsim Script Examples

**Single-seed mode (backward compatible):**
```toml
[seed]
foundation = 4201
```

**Dual-seed mode with decimal world_seed:**
```toml
[seed]
foundation = 4201
world_seed = 99887766
```

**Dual-seed mode with 256-bit world_seed (hex):**
```toml
[seed]
foundation = 4201
world_seed = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
```

**Dual-seed mode with base64 world_seed:**
```toml
[seed]
foundation = 4201
world_seed = "//////////////////////////////////////////8="
```

**Dual-seed mode with binary world_seed:**
```toml
[seed]
foundation = 4201
world_seed = "1000000000000000000000000000000000000000000000000000000000000001"
```

### C.6 Runtime Acceptance Contract

```
If [seed] world_seed is absent or zero:
    seeder = ParticleIdentitySeeder(foundation)
    // single-seed mode; assignment accepted once (Pass 1 only)

If [seed] world_seed is nonzero:
    seeder = ParticleIdentitySeeder(foundation, world_seed)
    // dual-seed mode; assignment accepted twice
    // Pass 1: particle instance context
    // Pass 2: world constant mixed in
    // Both passes must complete for birth_hash to be considered valid

The seeder.dual_seed_mode() accessor returns true in dual-seed mode.
Callers can log the mode in the run header for audit traceability.
```

---

*End of Appendix C.*

