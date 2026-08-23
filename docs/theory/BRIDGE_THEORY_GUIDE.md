# VSIM Theory Bridge Guide
## v5114_bridge_paper.md — §0–17 Annotated Implementation Reference
<!-- VSEPR-SIM v5.0.0-main | WO-K | Day 77 -->

| Field         | Value                                                               |
|---|---|
| **Document**  | `BRIDGE_THEORY_GUIDE.md`                                            |
| **Version**   | v5.1.4 → v5.1.13 bridge                                            |
| **WO chain**  | WO-66K, WO-66L, WO-66M, WO-66J (prerequisite reading)              |
| **Source**    | `docs/theory/v5114_bridge_paper.md`                                 |
| **Status**    | Authoritative — required before implementing any WO-66K/L/M/J work |

---

This guide annotates every section of the v5114 bridge paper with:
- Current implementation status in the codebase
- Exact header / source file where each concept lives
- Active constraints and ordering rules
- Notes on what is deliberately deferred

---

# Part 1 — Global State, Identity, Scale, MD Bridge (§0–9)

---

## §0 — Purpose and Scope

**Paper statement:**
> These are packing structures, not a demand to immediately implement every
> exotic physics layer. The classical MD bridge must stop wobbling before
> subatomic sampling begins.

**Implementation note:**
This is the governing constraint for the entire WO-66 chain.  The ordering
principle — stabilise classical MD first, extend identity structure second —
must be respected in every implementation PR.

**Active constraint:** `M_ij^X = 0` (subatomic channel disabled) is **hard-coded off**
in v5.1.4.  Do not enable it until the kernel passes all QA golden tests.

**File map:**
- `docs/theory/v5114_bridge_paper.md` — source paper (authoritative)
- `include/vsim/vsim_document.hpp` — simulation schema (authoritative defaults)
- `src/vsim/vsim_parser.cpp` — key wiring for all bridge-related fields

---

## §1 — Global State Vector `X(t)`

**Definition:**

```
X(t) = [ I_p,1(t), I_p,2(t), ..., I_p,N(t) ]
```

Each particle identity vector:

```
I_p = [ id, f, g, Q, B, L_e, L_mu, L_tau, J, P, C, m, tau, x, y, z, w, h ]
```

**18 components.** Only the last 5 (`m, tau, x, y, z`) are part of the
classical MD bridge state `X_MD(t)`. The remaining 13 are identity fields
that live in the analysis/sidecar layer — never in truth state.

**Implementation status:**
- Classical position/velocity/mass: ✅ in `SimulationState` (`src/sim/sim_state.hpp`)
- Identity vector `I_p`: 🟡 partially — `particle_identity.hpp` holds the container;
  full 18-component population is WO-66K Phase 1
- `id` field (birth hash): 🟡 `identity_hash` field exists; FNV-1a seeding is WO-66K

**Birth hash rule:**

```
h_p = H(id, f, g, Q, B, L, J, m, tau, xi_p)
```

`H` is FNV-1a 64-bit over quantized fields.  The birth position `xi_p` is
included **once at birth only** — it is NOT re-hashed during trajectory evolution.
Violating this rule breaks determinism.

**Key file:** `include/vsim/bridge65/` — `atom_event.hpp`, `bridge65_config.hpp`

---

## §2 — Scale Identity Ladder `S_n`

**Definition:**

```
S_n = [ s_n, d_n, m_n, l_n, p_n, r_n ]
```

| Level | Label     | Physical meaning               | Status            |
|---|---|---|---|
| S0    | existence | bare existence gate            | ✅ implicit in particle count |
| S1    | direction | vector / orientation           | ✅ velocity/force vectors  |
| S2    | relation  | color / gluon / relational     | 🟡 `caf_channel` in MCF-CAI (WO-77) |
| S3    | structure | EM / spatial structure         | ✅ Coulomb + LJ force channels |
| S4    | evolution | weak / time-ordered evolution  | 🟡 proper-time field `w` deferred |
| S5    | curvature | gravitational / curved space   | ⬜ not yet active |
| S6    | scale depth | renorm group / dark sector   | ⬜ hypothesis only — **not empirical** |

**Particle `scale_mask` (bitmask):**

```
bit 0 = S0  bit 1 = S1  bit 2 = S2  bit 3 = S3
bit 4 = S4  bit 5 = S5  bit 6 = S6 (hypothesis only)
```

**Key constraint:** S6 (`dark sector`) must never enter the empirical data layer.
`I_X^D ∈ H_hypothesis` and `I_X^D ∉ H_empirical`.

**Key file:** `include/vsim/identity/particle_identity.hpp` — `scale_mask` field

---

## §3 — Classical MD Bridge State `X_MD(t)`

**Definition:**

```
X_MD(t) = [ R(t), V(t), F(t), E(t), B(t), D(t), H(t) ]
```

| Component | Meaning               | File                                   |
|---|---|---|
| `R(t)`    | Position set          | `sim_state.hpp` `positions`            |
| `V(t)`    | Velocity set          | `sim_state.hpp` `velocities`           |
| `F(t)`    | Force set             | `sim_state.hpp` `forces`               |
| `E(t)`    | Energy traces         | `sim_state.hpp` `energies`             |
| `B(t)`    | Bond state            | `sim_state.hpp` bond lists             |
| `D(t)`    | Defect state          | `identity_sidecar.hpp` defect records  |
| `H(t)`    | Hash trace            | `identity_sidecar.hpp` hash log        |

**Force decomposition:**

```
F_i = F_i,bond + F_i,near + F_i,far + F_i,field + F_i,thermal
```

**Canonical integrator (v5.1.4):** Velocity Verlet

```
r_i^(n+1) = r_i^n + v_i^n * dt + 1/2 * a_i^n * dt^2
v_i^(n+1) = v_i^n + 1/2 * (a_i^n + a_i^(n+1)) * dt
```

This is implemented in `mcf_cai_integrator.hpp` → `VelocityVerletIntegrator`.
Any alternate integrator (FIRE, BAOAB) must not be promoted to the default
path until verified against the golden test suite.

---

## §4 — Matrix-Force Policy `M_ij`

**Definition:**

```
M_ij = [ m_bond, m_near, m_far, m_em, m_thermal, m_flux, m_gate, m_flags ]_ij
```

**Force sum:**

```
F_i = sum_{j≠i} M_ij · K(r_ij, v_ij, q_i, q_j, s_i, s_j)
```

**Expanded channel form:**

```
F_i = sum_{j≠i} (
	M_ij^LJ  F_ij^LJ      Lennard-Jones
	M_ij^C   F_ij^C       Coulomb
	M_ij^B   F_ij^B       Bond
	M_ij^T   F_ij^T       Thermal
	M_ij^X   F_ij^X       Subatomic  <-- DISABLED in v5.1.4
)
```

**v5.1.4 policy: `M_ij^X = 0`** — the subatomic channel is structurally present
in the matrix but contributes zero force.  This gate is enforced at the force
evaluation level.  It is NOT a runtime flag — do not add a toggle for it until
the classical MD regime is stable.

**Key file:** `include/vsim/kernel_mcf/mcf_cai_force.hpp` (WO-77 Phase 2)

---

## §5 — Per-Atom Event-Energy State `A_i^n`

**Definition:**

```
A_i^n = [ id_i, t_n, K_i^n, U_i^n, E_i^n, dE_i^n, |F_i^n|, C_i^n, B_i^n, R_i^n ]
```

**Energy:**

```
K_i^n = 1/2 m_i |v_i^n|^2
E_i^n = K_i^n + U_i^n
dE_i^n = E_i^n - E_i^(n-1)
```

**Detection gates** (Heaviside `H(x) = 1 if x > 0`):

| Gate             | Expression                                            |
|---|---|
| Collision        | `C_ij^n = H(r_c - r_ij^n) · H(E_ij^rel - E_c)`      |
| Bond creation    | `B_ij,+^n = H(r_b - r_ij^n) · H(dt_contact - dt_b) · H(chi_ij - chi_b)` |
| Bond breaking    | `B_ij,-^n = H(r_ij^n - lambda_b r_0,ij) · H(E_stretch - E_b)` |
| Force spike      | `Phi_i^n = H(|F_i^n| - mu_F - k_F sigma_F)`          |
| Energy anomaly   | `Psi_i^n = H(|dE_i^n| - dE_max)`                     |

**Key file:** `include/vsim/bridge65/atom_event.hpp` — `AtomEvent` struct

---

## §6 — Event Limiter

**Rate:**

```
rho_E = N_E / dt_wall
```

**Limiter activation:** `L = H(rho_E - rho_max)`

**Output policy:**

```
E_print = E                             if L = 0  (all events)
E_print = Sample(E, p_s) union E_crit  if L = 1  (rate-limited)
```

**Critical set (never suppressed):**

```
E_critical = { B_-, Psi, Phi, C_high }
```

**Artifact preservation rule:**

```
E_jsonl    = E           (full archive — never sampled)
E_console  ⊆ E_jsonl    (console is a subset)
```

> **"Sample the console.  Never sample the science."**

**Key file:** `include/vsim/bridge65/event_limiter.hpp`

**Implementation status:** ✅ `EventLimiter` struct and rate-gate logic are present.
The critical-set filter is wired; the JSONL sink bypasses the limiter unconditionally.

---

## §7 — Formation–Defect Bridge

**Formation operator:**

```
F_k : T → I_k + D_k
```

Template `T` generates a target identity `I_k` plus a defect set `D_k = {d_1, ..., d_m}`.

**Per-defect state:**

```
d_a = [ id_a, type_a, x_a, t_a, s_a, chi_a, P_a, H_a ]
```

**Defect severity (weighted sum):**

```
s_a = w_r R_a + w_E E_a + w_sigma sigma_a + w_tau tau_a
```

**Detection gates:**

| Gate              | Condition                                    |
|---|---|
| Vacancy           | `H(rho_0 - rho_local) · H(c_0 - c_local)`   |
| Interstitial      | `H(rho_local - rho_0) · H(E_local - E_0)`   |
| Miscoordination   | `H(|z_i - z_i*| - delta_z)`                 |
| Void              | `H(V_empty - V_c) · H(tau_void - tau_c)`    |

**Defect-to-property map:**

```
P_mat = Pi_P(T, D)
P_mat = [ rho, D_eff, k_eff, E_eff, G_eff, K_eff, phi, kappa, eta_f ]
```

**Key files:**
- `include/vsim/identity/identity_sidecar.hpp` — defect records
- `[analysis.inference]` in `.vsim` scripts — activates `I_op` property map

---

## §8 — Quark Component Matrix `C_q` (66L)

**Definition:**

```
C_q = [ f_a  Q_a  B_a  T3_a  T8_a  J_a  m_a  eta_a ]   (rows: a=1,2,3)
```

**Hadron packing:**

```
I_h = Pi_q(C_q)
```

**Charge projection:**

```
Q_h = sum_{a=1}^{3} Q_a
```

**Hidden charge activity:**

```
H_Q = q^T q
```

**Neutron example (udd):**

```
q_n = [ +2/3, -1/3, -1/3 ]
Q_n = 0
H_Q = (2/3)^2 + (1/3)^2 + (1/3)^2 = 2/3
```

`Q_n = 0` but `H_Q = 2/3 ≠ 0` — this is the hidden-charge activity test.
A neutron is electrically neutral but internally charged.

**Implementation status:** ⬜ WO-66L — not yet implemented.
**Prerequisite:** WO-66K (birth-hash seeder) must be complete before quark matrix
constructors are added.  See §17 ordering constraint.

**Planned file:** `include/vsim/identity/quark_component_matrix.hpp`

---

## §9 — Lepton Identity Matrix `I_l` (66L)

**Definition:**

```
I_l = [ id, l, Q, L_e, L_mu, L_tau, J, m, chi, tau, x, y, z, w ]
```

**Electron:**

```
I_e- = [ id, e, -1, 1, 0, 0, 1/2, m_e, chi, tau_e, x, y, z, w ]
```

**Electron neutrino:**

```
I_ve = [ id, ve, 0, 1, 0, 0, 1/2, m_ve, chi, tau_ve, x, y, z, w ]
```

**Implementation status:** ⬜ WO-66L — not yet implemented.
The lepton matrix shares the identity-column layout with `I_p` (§1).
All lepton flavours must be constructed by the same factory pattern.

**Planned file:** `include/vsim/identity/lepton_identity_matrix.hpp`

---

# Part 2 — Bosons, Relations, Events, Annihilation (§10–17)

---

## §10 — Boson Identity Matrix `I_b` (66L)

**Definition:**

```
I_b = [ id, field, Q, J, m, R, alpha, source, target, S_n ]
```

| Particle   | field  | Q  | J | m   | R      | alpha   | S_n |
|---|---|---|---|---|---|---|---|
| Photon     | EM     | 0  | 1 | 0   | inf    | alpha_EM | S3 |
| Gluon      | strong | 0  | 1 | 0   | R_conf | alpha_s  | S3 |
| W+ boson   | weak   | +1 | 1 | m_W | R_W    | alpha_W  | S4 |
| Dark X(hyp)| D      | 0  | J_D| m_D| R_D   | alpha_D  | S6 |

**Dark mediator constraint:**

```
I_X^D ∈ H_hypothesis
I_X^D ∉ H_empirical
```

No speculative physics leaks into the empirical data layer.  `S6` particles are
tagged hypothesis-only in their `scale_mask` and may not influence force
evaluation or event records in the empirical layer.

**Implementation status:** ⬜ WO-66L — not yet implemented.

---

## §11 — Relation-Particle Matrix `R_AB`

**Definition:**

```
R_AB = [ dx, dy, dz, dt | T_AB,1, T_AB,2, ..., T_AB,m ]
```

**Canonical non-spatiotemporal relation (entanglement proxy):**

```
R_AB = [ 0, 0, 0, 0 | T_AB ]
dx = dy = dz = dt = 0,   T_AB ≠ 0
```

This encodes `R_AB ≠ 0` without defining a signal velocity `v = dx/dt`.
The relation is non-local and does not violate causality constraints because
the spatiotemporal components are identically zero.

**Implementation status:** ⬜ Not yet implemented.
This is a future identity-sidecar overlay, not a force channel.

---

## §12 — Proper-Time Identity Matrix `I_tau`

**Definition:**

```
I_tau = [ x_hat, p_hat, tau_hat, H_c, H_m, <tau_hat>, R_tau ]
R_tau = tau_hat - <tau_hat>
```

**Clock-motion split:**

```
I_cm → I_c + I_m + R_cm
R_cm = [ 0, 0, 0, 0 | T_cm ]
```

The clock component `I_c` and motion component `I_m` are separated by the
relation matrix `R_cm` which has zero spatiotemporal extent.  Proper-time
residual `R_tau` measures deviation from mean proper time across the ensemble.

**Implementation status:** ⬜ Deferred post-WO-66L.  The `w` coordinate field
in `I_p` is the S4 hook for this; it is reserved but unpopulated in v5.1.4.

---

## §13 — Annihilation Event Model (66J)

### 13.1 — Event Record

```
A_i + B_j → sum_k P_k + dE + dI + dS
```

Input particle state:

```
A_i = [ id_i, x_i, v_i, m_i, q_i, s_i, H_i, Phi_i ]
B_j = [ id_j, x_j, v_j, m_j, q_j, s_j, H_j, Phi_j ]
```

Event record:

```
E_ann = [ id_i, id_j, t, x_c, E_in, E_out, P_products, dS, dI, flags ]
```

Collision center:

```
x_c = (m_i x_i + m_j x_j) / (m_i + m_j)
```

Incoming energy:

```
E_in = 1/2 m_i |v_i|^2 + 1/2 m_j |v_j|^2 + U_ij
```

### 13.2 — Annihilation Trigger Gate

```
annihilate(i,j) = 1   if  r_ij < r_c  AND  C_id(i,j) = 1  AND  E_rel > E_c
				= 0   otherwise
```

Triple gate: **distance** AND **identity compatibility** AND **relative energy**.
All three must be true.

### 13.3 — Identity Compatibility Gate `C_id(i,j)`

| Pair                        | Gate    |
|---|---|
| electron + positron         | allowed |
| proton + antiproton         | allowed |
| neutron + antineutron       | allowed |
| alpha + anti-alpha          | allowed |
| arbitrary bead + bead       | blocked |
| chemistry atom + normal atom| blocked |
| molecule + molecule         | blocked (unless destructive reaction mode) |

The identity gate is the primary safety barrier.  Without WO-66K (birth-hash
seeder), `C_id(i,j)` cannot be evaluated.  **Do not implement 66J before 66K.**

### 13.4 — Event Channel Separation

Annihilation is a **first-class event channel** — not a bond break, not a
chemistry reaction, not "particle disappeared":

| Channel                | Purpose                              |
|---|---|
| `ParticleParticleEvent`| Classical collision / interaction    |
| `ChemistryBondEvent`   | Bond formation / breaking            |
| `ChemistryNonBondEvent`| Non-bonded pair force events         |
| `DecayEvent`           | Single-particle decay chains         |
| `AnnihilationEvent`    | Anti-pair annihilation (WO-66J)      |
| `GlueFieldEvent`       | Glue-field disturbance logging       |
| `EmissionEvent`        | Product emission from any event      |

**Key file:** `include/vsim/bridge65/annihilation_event.hpp` (9 309 B, exists on disk)

**Implementation status:** 🟡 Header exists; kernel integration is WO-66J (Phase 1+).

---

## §14 — Annihilation Test Ladder (66J)

| Test   | Purpose                                          |
|---|---|
| ANN-01 | Identity gate — only valid anti-pairs annihilate |
| ANN-02 | Distance gate — `r_ij < r_c`                     |
| ANN-03 | Energy gate — `E_rel > E_c`                      |
| ANN-04 | Product record creation                          |
| ANN-05 | Energy / momentum residual logging               |
| ANN-06 | Live print integration (`[ANN]` stream)          |
| ANN-07 | Glue-field disturbance response                  |
| ANN-08 | Eigen trend capture                              |
| ANN-09 | Batch sweep                                      |
| ANN-10 | Heavy proxy annihilation (alpha + anti-alpha)    |

**Minimum viable sequence:** ANN-01, ANN-02, ANN-03, ANN-04, ANN-05 must all
pass before ANN-06 (live print) is wired.  ANN-07 through ANN-10 require
the MCF-CAI force evaluator (WO-77 Phase 2).

---

## §15 — Output Tables

**Event table (per annihilation event):**

```
step, time, event_type, id_i, id_j, r_ij, E_rel, E_in, E_out, dE,
p_residual, products, flags
```

**Particle lifecycle table:**

```
id, birth_step, death_step, parent_ids, child_ids, event_origin, state_final
```

**Field disturbance table:**

```
step, time, field_type, source_event, local_energy_before, local_energy_after,
delta_field, relaxation_time
```

**Eigen training table (for AI/ML training data):**

```
case_id, input_vector, event_class, E_residual, p_residual,
product_count, stability_score, trend_label
```

All four tables are mandatory for WO-66J test ladder completion.
They feed the `write_events_json` export and are never sub-sampled.

---

## §16 — Benchmark Set (66J)

| Bench | Pair                      | Primary metric                               |
|---|---|---|
| B1    | e- + e+ → 2γ             | energy residual, product symmetry             |
| B2    | p + p-bar → pi+pi-pi0 proxy | multiplicity, momentum spread, field response|
| B3    | n + n-bar → mesonic proxy | neutral-pair stability, no Coulomb bias      |
| B4    | alpha + anti-alpha → multi-product | energy scaling, product count, visual clarity|

**NaCl 4×4×4 is NOT a benchmark target for annihilation.**  NaCl ions are
chemistry atoms — the `C_id` gate blocks them.  Use dedicated anti-particle
`.vsim` scripts for 66J benchmarks.

---

## §17 — Implementation Order (Non-Negotiable)

```
WO-66K  →  Birth-hash seeder  (particle_identity_seeder.hpp)
WO-66L  →  Quark / lepton / boson matrix constructors
WO-66M  →  .x bundle manifest descriptor (x_bundle.hpp)
WO-66J  →  Annihilation event channel (annihilation_event.hpp integration)
```

**Ordering constraint:**

> Do not begin annihilation testing (66J) before the identity seeder (66K) is in place.
> The identity seeder is the foundation for `C_id(i,j)`.

**Current status summary:**

| WO    | Status | Blocker                         |
|---|---|---|
| 66K   | 🟡 Ready | Phase 1 implementation pending  |
| 66L   | ⬜ Blocked | Awaits 66K Phase 1 complete     |
| 66M   | ⬜ Blocked | Awaits 66L complete             |
| 66J   | 🟡 Header exists | Awaits 66K complete for `C_id` |
| WO-77 (MCF-CAI) | 🟢 Phase 1+2 active | Phase 2 force evaluator in progress |

---

## §18 — Relationship to Existing Files

| Bridge paper concept          | Existing file                                        |
|---|---|
| `I_p` identity vector         | `include/vsim/identity/particle_identity.hpp`        |
| `h_p` birth hash              | `identity_hash` field (WO-66K extends this)          |
| `A_i^n` event-energy state    | `include/vsim/bridge65/atom_event.hpp`               |
| `EventLimiter`                | `include/vsim/bridge65/event_limiter.hpp`            |
| `AnnihilationEvent`           | `include/vsim/bridge65/annihilation_event.hpp`       |
| `M_ij` force matrix           | `include/vsim/kernel_mcf/mcf_cai_force.hpp`          |
| `X_MD(t)` bridge state        | `src/sim/sim_state.hpp` + `mcf_cai_world.hpp`        |
| Defect records `D(t)`         | `include/vsim/vsim/identity_sidecar.hpp`             |
| `.x` bundle manifest          | `include/vsim/bridge65/x_bundle.hpp`                 |
| Quark matrix `C_q`            | Planned: `quark_component_matrix.hpp` (WO-66L)       |
| Lepton matrix `I_l`           | Planned: `lepton_identity_matrix.hpp` (WO-66L)       |
| Boson matrix `I_b`            | Planned: `boson_identity_matrix.hpp` (WO-66L)        |

---

## Quick Status Dashboard

```
Classical MD kernel .............. STABLE  (Velocity Verlet, Ewald, PBC)
Identity vector I_p .............. PARTIAL  (container exists, 66K populates)
Scale ladder S_n .................. PARTIAL  (S0-S3 wired; S4-S6 deferred)
Force matrix M_ij ................ PARTIAL  (LJ+Coulomb+Bond+Thermal wired; M^X = 0)
Event-energy state A_i^n ......... OK  (atom_event.hpp)
Event limiter ..................... OK  (event_limiter.hpp)
Formation-defect bridge .......... PARTIAL  (sidecar; property map in I_op)
Quark/lepton/boson matrices ...... BLOCKED on 66K
Relation-particle R_AB ........... DEFERRED
Proper-time I_tau ................. DEFERRED
Annihilation model ............... HEADER only  (66J awaits 66K for C_id)
WO-77 MCF-CAI fork ............... PHASE 1 done, Phase 2 in progress
```
