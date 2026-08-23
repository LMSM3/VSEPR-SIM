# VSEPR-SIM v5.1.4 Bridge Paper
## Day 65–66 Addendum: Global State, Identity Matrices, MD Bridge, and Annihilation Event Model

| Field           | Value                                      |
|---|---|
| **Document**    | `v5114_bridge_paper.md`                    |
| **Version**     | VSEPR-SIM v5.1.13 → v5.1.4                |
| **WO chain**    | WO-66K, WO-66L, WO-66M, WO-66J            |
| **Branch**      | `v5.0.0-main`                              |
| **Status**      | Authoritative — required before 66K/L/M/J |

---

## 0. Purpose

This document is the required pre-implementation paper for work orders 66K, 66L,
66M, and 66J.  It defines the packing structures, identity vectors, event channels,
and matrix policies that those work orders implement.

These are *packing structures*, not a demand to immediately implement every
exotic physics layer.  The classical MD bridge must stop wobbling before subatomic
sampling begins.

---

## 1. Global State Vector

The full simulation state at time `t` is:

```
X(t) = [ I_p,1(t), I_p,2(t), ..., I_p,N(t) ]
```

Each particle identity vector is:

```
I_p = [ id, f, g, Q, B, L_e, L_μ, L_τ, J, P, C, m, τ, x, y, z, w, h ]
```

where:

| Symbol | Meaning                                       |
|---|---|
| `id`   | Deterministic birth hash (see §66K)           |
| `f`    | Flavor / type code                            |
| `g`    | Generation index                              |
| `Q`    | Electric charge (units of e)                  |
| `B`    | Baryon number                                 |
| `L_e, L_μ, L_τ` | Lepton family numbers                |
| `J`    | Spin proxy                                    |
| `P`    | Parity proxy                                  |
| `C`    | Charge conjugation proxy                      |
| `m`    | Rest mass (VSIM units)                        |
| `τ`    | Proper-time residual                          |
| `x,y,z` | Spatial position                            |
| `w`    | 4th coordinate (proper-time extension, S4)    |
| `h`    | Birth hash: `H(id, f, g, Q, B, L, J, m, τ, ξ_p)` |

Spatial and 4D position:

```
x_p = (x, y, z)          // classical MD position
ξ_p = (x, y, z, w)       // S4 extended position
```

Birth hash:

```
h_p = H(id, f, g, Q, B, L, J, m, τ, ξ_p)
```

Hash function `H` is FNV-1a 64-bit over quantized fields.  Position `ξ_p` is
included at birth only; it is NOT re-hashed during trajectory evolution.

---

## 2. Scale Identity Ladder

Each scale level carries a tuple:

```
S_n = [ s_n, d_n, m_n, ℓ_n, p_n, r_n ]
```

The full ladder:

| Level | Label     | Physical meaning               |
|---|---|---|
| S0    | existence | bare existence gate            |
| S1    | direction | vector / orientation           |
| S2    | relation  | color / gluon / relational     |
| S3    | structure | EM / spatial structure         |
| S4    | evolution | weak / time-ordered evolution  |
| S5    | curvature | gravitational / curved space   |
| S6    | scale depth | renorm group / dark sector   |

Compact notation:

```
S: E → d → R → Ω → t → g_μν → w
```

Particle `scale_mask` (bitmask):

```
bit 0 = S0 (existence)
bit 1 = S1 (direction)
bit 2 = S2 (color/gluon)
bit 3 = S3 (EM/spatial)
bit 4 = S4 (weak/time)
bit 5 = S5 (curvature)
bit 6 = S6 / dark sector (hypothesis only — not empirical)
```

---

## 3. Classical MD Bridge State

```
X_MD(t) = [ R(t), V(t), F(t), E(t), B(t), D(t), H(t) ]
```

| Component | Meaning                           |
|---|---|
| `R(t)`    | Position set `{r_i(t)}`           |
| `V(t)`    | Velocity set `{v_i(t)}`           |
| `F(t)`    | Force set (decomposed below)      |
| `E(t)`    | Energy traces                     |
| `B(t)`    | Bond state                        |
| `D(t)`    | Defect state                      |
| `H(t)`    | Hash trace                        |

Force decomposition:

```
F_i = F_i,bond + F_i,near + F_i,far + F_i,field + F_i,thermal
```

Equation of motion:

```
m_i * d²r_i/dt² = F_i
```

**Velocity Verlet integrator** (canonical for v5.1.4):

```
r_i^(n+1) = r_i^n + v_i^n Δt + ½ a_i^n Δt²
v_i^(n+1) = v_i^n + ½ (a_i^n + a_i^(n+1)) Δt
```

---

## 4. Matrix-Force Policy

Per-pair interaction policy matrix:

```
M_ij = [ m_bond, m_near, m_far, m_em, m_thermal, m_flux, m_gate, m_flags ]_ij
```

Force sum:

```
F_i = Σ_{j≠i}  M_ij · K(r_ij, v_ij, q_i, q_j, s_i, s_j)
```

Expanded channel form:

```
F_i = Σ_{j≠i} (
	M_ij^LJ  F_ij^LJ    +   Lennard-Jones
	M_ij^C   F_ij^C     +   Coulomb
	M_ij^B   F_ij^B     +   Bond
	M_ij^T   F_ij^T     +   Thermal
	M_ij^X   F_ij^X         Subatomic (disabled in v5.1.4)
)
```

**v5.1.4 policy:**  `M_ij^X = 0`  — subatomic sampling disabled.

---

## 5. Per-Atom Event-Energy State

```
A_i^n = [ id_i, t_n, K_i^n, U_i^n, E_i^n, ΔE_i^n, |F_i^n|, C_i^n, B_i^n, R_i^n ]
```

Energy:

```
K_i^n = ½ m_i ||v_i^n||²
E_i^n = K_i^n + U_i^n
ΔE_i^n = E_i^n − E_i^(n−1)
```

Gates (Heaviside `H(x) = 1 if x > 0`):

**Collision gate:**
```
C_ij^n = H(r_c − r_ij^n) · H(E_ij^rel − E_c)
```

**Bond creation:**
```
B_ij,+^n = H(r_b − r_ij^n) · H(Δt_ij^contact − Δt_b) · H(χ_ij − χ_b)
```

**Bond breaking:**
```
B_ij,-^n = H(r_ij^n − λ_b r_0,ij) · H(E_ij^stretch − E_b)
```

**Force spike:**
```
Φ_i^n = H(|F_i^n| − μ_F − k_F σ_F)
```

**Energy anomaly:**
```
Ψ_i^n = H(|ΔE_i^n| − ΔE_max)
```

---

## 6. Event Limiter

Event rate:

```
ρ_E = N_E / Δt_wall
```

Limiter activation:

```
L = H(ρ_E − ρ_max)
```

Console event set:

```
E_print = E                                 if L = 0  (all events)
E_print = Sample(E, p_s) ∪ E_critical       if L = 1  (rate-limited)
```

Critical set (never suppressed):

```
E_critical = { B_-, Ψ, Φ, C_high }
```

**Artifact preservation rule:**

```
E_jsonl    = E          (full archive — never sampled)
E_console ⊆ E_jsonl    (console is a subset)
```

Sample the console.  Never sample the science.

---

## 7. Formation–Defect Bridge

Formation operator:

```
F_k : T → I_k + D_k
```

Defect set `D_k = {d_1, d_2, ..., d_m}`.

Per-defect state:

```
d_a = [ id_a, type_a, x_a, t_a, s_a, χ_a, P_a, H_a ]
```

Defect severity:

```
s_a = w_r R_a + w_E E_a + w_σ σ_a + w_τ τ_a
```

Detection gates:

| Gate              | Condition                                              |
|---|---|
| Vacancy           | `H(ρ_0 − ρ_local) · H(c_0 − c_local)`               |
| Interstitial      | `H(ρ_local − ρ_0) · H(E_local − E_0)`               |
| Miscoordination   | `H(|z_i − z_i*| − δ_z)`                              |
| Void              | `H(V_empty − V_c) · H(τ_void − τ_c)`                 |

Defect-to-property map:

```
P_mat = Π_P(T, D)
P_mat = [ ρ, D_eff, k_eff, E_eff, G_eff, K_eff, φ, κ, η_f ]
```

---

## 8. Quark Component Matrix (66L)

```
C_q = [ f_a  Q_a  B_a  T3_a  T8_a  J_a  m_a  η_a ]   (rows: a=1,2,3)
```

Hadron packing:

```
I_h = Π_q(C_q)
```

Charge projection:

```
Q_h = Σ_{a=1}^{3} Q_a
```

Hidden charge activity:

```
H_Q = q^T q
```

**Neutron example** (`udd`):

```
q_n = [ +2/3, -1/3, -1/3 ]
Q_n = 0
H_Q = (2/3)² + (1/3)² + (1/3)² = 2/3
```

So `Q_n = 0` but `H_Q = 2/3 ≠ 0`.  This is the hidden-charge activity test.

---

## 9. Lepton Identity Matrix (66L)

```
I_ℓ = [ id, ℓ, Q, L_e, L_μ, L_τ, J, m, χ, τ, x, y, z, w ]
```

**Electron:**

```
I_e- = [ id, e, -1, 1, 0, 0, ½, m_e, χ, τ_e, x, y, z, w ]
```

**Electron neutrino:**

```
I_νe = [ id, νe, 0, 1, 0, 0, ½, m_νe, χ, τ_νe, x, y, z, w ]
```

---

## 10. Boson Identity Matrix (66L)

```
I_b = [ id, field, Q, J, m, R, α, source, target, S_n ]
```

| Particle    | field   | Q  | J | m      | R          | α          | S_n |
|---|---|---|---|---|---|---|---|
| Photon      | EM      | 0  | 1 | 0      | ∞          | α_EM       | S3  |
| Gluon       | strong  | 0  | 1 | 0      | R_conf     | α_s        | S3  |
| W+ boson    | weak    | +1 | 1 | m_W    | R_W        | α_W        | S4  |
| Dark X (hyp)| D       | 0  | J_D | m_D | R_D        | α_D        | S6  |

**Constraint on dark mediator:**

```
I_X^D ∈ H_hypothesis
I_X^D ∉ H_empirical
```

No speculative physics leaks into the empirical data layer.

---

## 11. Relation-Particle Matrix

```
R_AB = [ Δx, Δy, Δz, Δt | T_AB,1, T_AB,2, ..., T_AB,m ]
```

Canonical non-spatiotemporal relation (entanglement proxy):

```
R_AB = [ 0, 0, 0, 0 | T_AB ]
Δx = Δy = Δz = Δt = 0,   T_AB ≠ 0
```

Thus `R_AB ≠ 0` without defining `v_signal = Δx/Δt`.

---

## 12. Proper-Time Identity Matrix

```
I_τ = [ x̂, p̂, τ̂, H_c, H_m, ⟨τ̂⟩, R_τ ]
R_τ = τ̂ − ⟨τ̂⟩
```

Clock-motion split:

```
I_cm → I_c + I_m + R_cm
R_cm = [ 0, 0, 0, 0 | T_cm ]
```

---

## 13. Annihilation Event Model (66J)

### 13.1 Event record

```
A_i + B_j → Σ_k P_k + ΔE + ΔI + ΔS
```

Input particle state:

```
A_i = [ id_i, x_i, v_i, m_i, q_i, s_i, H_i, Φ_i ]
B_j = [ id_j, x_j, v_j, m_j, q_j, s_j, H_j, Φ_j ]
```

Event record:

```
E_ann = [ id_i, id_j, t, x_c, E_in, E_out, P_products, ΔS, ΔI, flags ]
```

Collision center:

```
x_c = (m_i x_i + m_j x_j) / (m_i + m_j)
```

Incoming kinetic energy:

```
E_in = ½ m_i |v_i|² + ½ m_j |v_j|² + U_ij
```

### 13.2 Annihilation trigger gate

```
annihilate(i,j) = 1   if  r_ij < r_c  AND  C_id(i,j) = 1  AND  E_rel > E_c
				= 0   otherwise
```

where:

```
r_ij  = |x_i − x_j|
E_rel = ½ μ |v_i − v_j|²
μ     = (m_i m_j) / (m_i + m_j)
```

### 13.3 Identity compatibility gate `C_id(i,j)`

| Pair                          | Gate   |
|---|---|
| electron + positron           | allowed |
| proton + antiproton           | allowed |
| neutron + antineutron         | allowed |
| alpha + anti-alpha            | allowed |
| arbitrary bead + bead         | blocked (unless explicitly marked) |
| chemistry atom + normal atom  | blocked |
| molecule + molecule           | blocked (unless destructive reaction mode) |

### 13.4 Event channel separation

| Channel            | Purpose                                      |
|---|---|
| `ParticleParticleEvent` | Classical collision / interaction       |
| `ChemistryBondEvent`    | Bond formation / breaking               |
| `ChemistryNonBondEvent` | Non-bonded pair force events            |
| `DecayEvent`            | Single-particle decay chains            |
| `AnnihilationEvent`     | Anti-pair annihilation (this WO)        |
| `GlueFieldEvent`        | Glue-field disturbance logging          |
| `EmissionEvent`         | Product emission from any event         |

Annihilation is NOT stored as a bond break, NOT as chemistry, NOT as
"particle disappeared."  It is a first-class event channel.

### 13.5 Live print format

```
[PP]    step=184 t=0.184 idA=e_001 idB=pos_001 r=0.018 Erel=0.382 gate=id+dist+energy action=ANNIHILATE
[CHEM]  step=184 t=0.184 idA=C_004 idB=O_002 r=1.24 type=nonbond force=repulsive action=NONE
[DECAY] step=522 t=0.522 id=Pu_239_001 mode=alpha product=He4_001 daughter=U235_001 dE=...
[ANN]   step=184 t=0.184 pair=e_001+pos_001 xc=(0.01,0.00,0.00) Ein=... Eout=... products=gamma_001,gamma_002 residualE=...
```

---

## 14. Annihilation Test Ladder (66J)

| Test | Purpose                                                |
|---|---|
| ANN-01 | Identity gate — only valid anti-pairs annihilate   |
| ANN-02 | Distance gate — `r_ij < r_c`                       |
| ANN-03 | Energy gate — `E_rel > E_c`                        |
| ANN-04 | Product record creation                             |
| ANN-05 | Energy / momentum residual logging                  |
| ANN-06 | Live print integration ([ANN] stream)               |
| ANN-07 | Glue-field disturbance response                     |
| ANN-08 | Eigen trend capture                                 |
| ANN-09 | Batch sweep                                         |
| ANN-10 | Heavy proxy annihilation (alpha + anti-alpha)       |

---

## 15. Output Tables

### Event table
```
step, time, event_type, id_i, id_j, r_ij, E_rel, E_in, E_out, dE,
p_residual, products, flags
```

### Particle lifecycle table
```
id, birth_step, death_step, parent_ids, child_ids, event_origin, state_final
```

### Field disturbance table
```
step, time, field_type, source_event, local_energy_before, local_energy_after,
delta_field, relaxation_time
```

### Eigen training table
```
case_id, input_vector, event_class, E_residual, p_residual,
product_count, stability_score, trend_label
```

---

## 16. Benchmark Set (66J)

| Bench | Pair                    | Primary metric                              |
|---|---|---|
| B1    | e⁻ + e⁺ → 2γ           | energy residual, product symmetry            |
| B2    | p + p̄ → π⁺π⁻π⁰ proxy  | multiplicity, momentum spread, field response|
| B3    | n + n̄ → mesonic proxy  | neutral-pair stability, no Coulomb bias      |
| B4    | α + ᾱ → multi-product  | energy scaling, product count, visual clarity|

---

## 17. Implementation Order

```
WO-66K  →  Birth-hash seeder (particle_identity_seeder.hpp)
WO-66L  →  Quark / lepton / boson matrix constructors
WO-66M  →  .x bundle manifest descriptor (x_bundle.hpp)
WO-66J  →  Annihilation event channel (annihilation_event.hpp)
```

Do not begin annihilation testing before the identity seeder is in place.
The identity seeder is the foundation for `C_id(i,j)`.

---

## 18. Relationship to Existing Modules

| This paper's concept       | Existing file                              |
|---|---|
| `I_p` identity vector      | `include/identity/particle_identity.hpp`   |
| `h_p` birth hash           | `identity_hash` field (to be extended)     |
| `A_i` event-energy state   | `include/vsim/bridge65/atom_event.hpp`     |
| Event limiter `L`          | `include/vsim/bridge65/event_limiter.hpp`  |
| Force matrix `M_ij`        | `data/bridge65.force_matrix.json`          |
| `D_k` defect state         | formation engine (WO-66A)                  |
| `C_q` quark matrix         | `include/identity/identity_matrix.hpp`     |
| `.x` bundle                | `include/vsim/bundle/x_bundle.hpp` (new)   |
| Annihilation event         | `include/vsim/bridge65/annihilation_event.hpp` (new) |

---

*End of v5.1.4 Bridge Paper.  Required reading before any 66K/L/M/J implementation.*
