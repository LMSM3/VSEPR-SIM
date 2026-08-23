# Identity Matrix Notation Reference
## VSEPR-SIM Universal Particle and Scale-Ladder Formalism

**Classification:** Top Secret  
**WO chain:** WO-VSEPR-SIM Extreme Addendum / WO-66-N / WO-66-P  
**Version:** v5.1.4 | Branch: v5.0.0-main  
**Status:** ACTIVE — Canonical notation reference

---

## Preface

This document is the authoritative notation reference for all identity matrix
objects used in VSEPR-SIM.  It supersedes inline notation in earlier theory
papers for the structures defined here.  Every matrix, symbol, and law given
here is the form that implementation headers must match.

Sections 0–17 define the matrix catalogue.  Section 18 states SM's Second Law.
Section 19 defines the universal object triple `𝒫ᵢ`.  Section 20 gives
the compact identity matrix shapes for leptons, mesons, and baryons.

---

## 0. Scale Ladder Matrix

The simulation operates across seven named scale slots `S₀ … S₆`.
Each slot carries its own domain, mode set, length scale, propagation rule,
and residual channel.

```
S = [ Sₙ | Dₙ | Mₙ | Lₙ | Pₙ | Rₙ ]
```

| Row | Slot | Domain | Mode set | Length / propagation |
|-----|------|--------|----------|----------------------|
| 0   | S₀   | E ∈ {0,1}          | ê₁                          | discrete / binary          |
| 1   | S₁   | θ, ϕ, relation     | M₁                          | angular / relational        |
| 2   | S₂   | x, y, z, structure | M₂                          | spatial / structural        |
| 3   | S₃   | t, Δt, evolution   | M₃                          | temporal / evolution        |
| 4   | S₄   | g_μν, Φ_G          | M₄                          | curved / gravitational      |
| 5   | S₅   | w, scale-depth     | X_D                         | dark / scale-depth          |
| 6   | S₆   | ∅                  | —                           | residual / unresolved       |

Layer vectors alongside S:

```
L  = [ L₀  L₁  L₂  L₃  L₄  L₅  L_EM ]
P  = [ P₀  P₁  P₂  P₃  P₄  P₅  P₆   ]
R  = [ R₀  R₁  R₂  R₃  R₄  R₅  R₆   ]
```

`L_EM` is the electromagnetic coupling layer.  `X_D` is the dark-boson / scale-depth
mediator assigned to S₅.  `R₆` carries all unresolved residual from packing.

---

## 1. Universal Particle Identity Matrix

Every simulated particle at any scale carries:

```
Iₚ = [ id | family | gen | Q | B | Lₑ | L_μ | L_τ | J | P | C | m | τ | x | y | z | w | h ]
```

| Field | Meaning |
|-------|---------|
| `id`     | persistent deterministic identifier |
| `family` | particle family tag (quark / lepton / boson / hadron / atom / …) |
| `gen`    | generation index (1, 2, 3 or composite) |
| `Q`      | electric charge |
| `B`      | baryon number |
| `Lₑ, L_μ, L_τ` | lepton family numbers |
| `J`      | total angular momentum / spin |
| `P`      | parity |
| `C`      | charge conjugation eigenvalue |
| `m`      | rest mass |
| `τ`      | mean lifetime (−1 = stable) |
| `x,y,z`  | spatial coordinates |
| `w`      | scale-depth coordinate (S₅) |
| `h`      | persistent hash / lineage seed |

The `h` field is the bridge to the hash matrix `Hᵢ` defined in §19.

---

## 2. Quark Component Matrix

For a hadron with constituent quarks f₁, f₂, f₃:

```
		[ f₁    f₂    f₃   ]
		[ Q₁    Q₂    Q₃   ]
		[ B₁    B₂    B₃   ]
Cq  =   [ T₃,₁  T₃,₂  T₃,₃ ]
		[ T₈,₁  T₈,₂  T₈,₃ ]
		[ J₁    J₂    J₃   ]
		[ m₁    m₂    m₃   ]
		[ η₁    η₂    η₃   ]
```

**Packing rule:**

```
Iₕ = Πq(Cq)
```

**Example — neutron charge column:**

```
qₙ = [ +2/3, −1/3, −1/3 ]ᵀ

Qₙ = 1ᵀ qₙ = 0
H_Q = qₙᵀ qₙ = 2/3
```

The internal charge activity `H_Q = 2/3 ≠ 0` while the net charge `Qₙ = 0`.
The neutron is charge-cancelled, not charge-empty.

---

## 3. Hadron Identity Matrix

```
Iₕ = [ id | type | quark_content | Q | B | S | Cₕ | T | I₃ | J | P | m | τ | color_state | Rₕ ]
```

| Field | Meaning |
|-------|---------|
| `quark_content` | packed from `Cq` via §2 |
| `S`    | strangeness |
| `Cₕ`  | charm number |
| `T`    | isospin magnitude |
| `I₃`  | isospin projection |
| `color_state` | color-singlet descriptor |
| `Rₕ`  | packing residual |

**Examples:**

```
p  = [ uud | Q=+1 | B=1 | J=1/2 ]
n  = [ udd | Q=0  | B=1 | J=1/2 ]
J/ψ = [ cc̄  | Q=0  | B=0 | J=1   ]
```

---

## 4. Lepton Identity Matrix

```
Iₗ = [ id | ℓ | Q | Lₑ | L_μ | L_τ | J | m | chirality | helicity | τ | x | y | z | w | h ]
```

**Examples:**

```
e⁻  = [ Q=−1 | Lₑ=1 | J=1/2 | mₑ ]
νₑ  = [ Q=0  | Lₑ=1 | J=1/2 | m_νₑ ]
```

**Compact 2×2 form** (see §20):

```
Iₗ = [ Q      Lf ]
	 [ J      m  ]
```

---

## 5. Boson / Mediator Identity Matrix

```
Ib = [ id | field | Q | J | m | range | coupling | source | target | Sₙ | Rb ]
```

| Field | Meaning |
|-------|---------|
| `field`    | force field tag: EM / strong / weak / dark |
| `range`    | ∞ (photon), confined (gluon), short (W/Z), scale-depth (X_D) |
| `coupling` | coupling constant or running coupling descriptor |
| `Sₙ`       | primary scale-ladder slot |
| `Rb`       | boson residual / propagator remainder |

**Examples:**

```
γ   = [ EM     | Q=0  | J=1 | m=0  | range=∞          ]
g   = [ strong | Q=0  | J=1 | color | range=confined    ]
W⁺  = [ weak   | Q=+1 | J=1 | m_W  | range=short       ]
X_D = [ dark   | Q=0  | J=? | m=?  | range=scale-depth ]
```

---

## 6. Atomistic / MD Identity Matrix

**Single atom:**

```
I_A = [ id | type | Z | m | q | x | y | z | vx | vy | vz | fx | fy | fz | mol | h ]
```

**Full MD state at time t:**

```
		 ⎡ I_{A,1}(t) ⎤
C_A(t) = ⎢ I_{A,2}(t) ⎥
		 ⎣     ⋮      ⎦
```

**Trajectory tensor:**

```
T_A = [ C_A(t₀), C_A(t₁), …, C_A(t_T) ]
```

---

## 7. Chemical Identity Matrix

```
I_C = [ mol_id | formula | G_C | B | q_eff | CN | Θ | Γ | Ω | E_C | R_C ]
```

| Field | Meaning |
|-------|---------|
| `G_C = (V,E)` | molecular graph |
| `B = [Bᵢⱼ]`  | bond matrix |
| `CNᵢ = Σⱼ≠ᵢ 1(Bᵢⱼ > τ_b)` | coordination number |
| `E_C` | chemical energy |
| `R_C` | chemical residual |

**Projection from atomistic state:**

```
I_C = P_{C←A}(C_A)
```

---

## 8. Solid / Material Identity Matrix

```
I_M = [ mat_id | phase | L | N | ρ | CN | MSD | D_eff | ε | σ | D | R_M ]
```

| Symbol | Meaning |
|--------|---------|
| `L`    | lattice / cell descriptor |
| `N`    | neighbor graph |
| `D`    | defect descriptor |
| `MSD`  | mean-square displacement |
| `D_eff`| effective diffusivity |

**Projection:**

```
I_M = P_{M←A}(C_A)
```

---

## 9. True-Location / Fuzz Identity Matrix

For particle i with m candidate locations:

```
		 ⎡ cᵢ₁(t) ⎤
Cᵢ(t) = ⎢ cᵢ₂(t) ⎥
		 ⎣   ⋮    ⎦
```

**True identity-location (barycentric collapse):**

```
ξᵢ★(t) = αᵢᵀ Cᵢ(t) / (αᵢᵀ 1)
```

**Observed position:**

```
rᵢᵒᵇˢ(t) = P₃D(ξᵢ★)
```

**Fuzz-state matrix:**

```
I_{fuzz,i} = [ idᵢ | ξᵢ★ | rᵢᵒᵇˢ | r_{f,i} | ℓ_S | Φ_{i,S} | state ]

Φ_{i,S} = ℓ_S / r_{f,i}
```

`Φ_{i,S} ≪ 1` → point-like at this scale.  
`Φ_{i,S} ~ 1` → fuzz-zone extends to scale boundary.

---

## 10. Relation-Particle Identity Matrix

```
R_AB = [ Δx | Δy | Δz | Δt | T_{AB,1} | T_{AB,2} | ⋯ | T_{AB,m} ]
```

**Canonical (rest) form:**

```
R_AB = [ 0, 0, 0, 0 | T_AB ]
```

**Pair decomposition:**

```
I_AB  →  I_A + I_B + R_AB
```

**Measurement projections:**

```
O_A = P_a(I_A, R_AB)
O_B = P_b(I_B, R_AB)
```

---

## 11. Proper-Time Identity Matrix

```
I_τ = [ x̂ | p̂ | τ̂ | H_c | H_m | ⟨τ⟩ | R_τ ]

τ̂ = τ(x̂, p̂)
R_τ = τ̂ − ⟨τ̂⟩
```

**Clock-motion relation:**

```
I_cm  →  I_c + I_m + R_cm
R_cm = [ 0, 0, 0, 0 | T_cm ]
```

---

## 12. Dark Projection Identity Matrix

```
I_D = [ x | y | z | w | ρ | K_G(w) | K_EM(w) | X_D | R_D ]
```

**Gravitational and visible density projections:**

```
ρ_grav(x,y,z;t) = ∫ ρ(x,y,z,w;t) K_G(w) dw
ρ_vis(x,y,z;t)  = ∫ ρ(x,y,z,w;t) K_EM(w) dw
```

**Dark residual:**

```
ρ_dark = ρ_grav − ρ_vis
	   = ∫ ρ(x,y,z,w;t) [ K_G(w) − K_EM(w) ] dw
```

The dark density is not a separate substance; it is the unprojected residual
between gravitational and electromagnetic kernel integrals over the
scale-depth coordinate `w`.

---

## 13. Annihilation / Transition Event Matrix

```
E_k = [ event_id | t_k | parent A | C_out | Π_out | products | ε_cons | R_k ]
```

**General transition:**

```
I_parent  --[A]--> C_out  --[Π]-->  I_products
```

**Example — B_c⁺ weak decay:**

```
B_c⁺  --[A_weak]-->  cc̄ ud̄  --[Π]-->  J/ψ + π⁺
```

---

## 14. Detector Projection / Reconstruction Matrix

**Forward detector projection:**

```
P_det : X  →  y_det
```

**Detector reconstruction:**

```
R_det : y_det  →  X̂
```

**Detector identity matrix:**

```
I_det = [ event_id | X_true | P_det | y_det | R_det | X̂ | R_det ]
```

**Residual:**

```
R_det = X_true − R_det( P_det(X_true) )
```

---

## 15. Solver Comparison Identity Matrix

```
V_run = [ mode | E | ψ | I | R | Λ | ε_E | ε_R | ε_λ | ε_cons | T_runtime ]
```

**Mode set:**

```
M = { G_dyn, G_norm, G+SchEq+Eigen, SchEq }
```

**Comparison operator:**

```
C = Compare( G_dyn, G_norm, G+SchEq+Eigen, SchEq )
```

---

## 16. Persistent Simulation State Matrix

```
S_k = [ t_k | X_k | E_k | R_k | H_k | Λ_k | L_k ]
```

| Symbol | Meaning |
|--------|---------|
| `X_k`  | active identity-state matrix at step k |
| `E_k`  | event matrix at step k |
| `R_k`  | residual matrix |
| `H_k`  | hidden column activity vector |
| `Λ_k`  | diagonalised mode intensities |
| `L_k`  | lineage graph |

**State update:**

```
S_{k+1} = U(S_k, E_k)
```

---

## 17. Master Packing / Residual Identity

**Universal recursive packing rule:**

```
I_{n+1} = Πₙ(Cₙ)
Cₙ      = Aₙ(I_{n+1})
Rₙ      = Cₙ − Aₙ(Πₙ(Cₙ))
Ĉₙ      = Cₙ + Rₙ
```

**Hidden-active channel rule:**

```
If  Iⱼ = 0  and  Hⱼ > 0  →  channel is active but hidden
```

Packing is many-to-one; exact reconstruction fails when projection destroys
uniqueness.  The residual `Rₙ` carries what the higher-scale identity cannot
represent.

---

## 18. SM's Second Law — Scale-Coalescence Identity Law

### Plain Statement

When three or more particles exist within the relevant bond or interactability
length of their current scale, their combined state **may generate a new
higher-scale identity**.  This new identity exists at the weighted barycentric
location of the lower-scale particles and carries its own packed state,
residual structure, and projection rules.

This is not matrix compression.  It is a **state-count change**:

```
N_identity  →  N_identity + 1
```

A new identity object begins to exist.

### Formal Statement

Let `m` lower-scale identities exist:

```
{ I₁⁽ⁿ⁾, I₂⁽ⁿ⁾, …, Iₘ⁽ⁿ⁾ },   m ≥ 3
```

with pair distances satisfying the bond-length condition for enough connected
pairs to form a bound cluster:

```
dᵢⱼ⁽ⁿ⁾ ≤ ℓ_bond⁽ⁿ⁾
```

Then a new higher-scale identity is generated:

```
I★⁽ⁿ⁺¹⁾ = Πₙ(Cₙ)
```

where:

```
	 ⎡ I₁⁽ⁿ⁾ ⎤
Cₙ = ⎢ I₂⁽ⁿ⁾ ⎥
	 ⎣   ⋮   ⎦
```

The minimum coalescence condition is `m ≥ 3`.

### Coalescence Gate

```
G_coal⁽ⁿ⁾ = 1[m ≥ 3] · 1[d̄⁽ⁿ⁾ ≤ ℓ_bond⁽ⁿ⁾] · 1[Bₙ(Cₙ) ≥ τ_B]
```

When `G_coal⁽ⁿ⁾ = 1`:

```
I★⁽ⁿ⁺¹⁾ = Πₙ(Cₙ)      and     Exist(I★⁽ⁿ⁺¹⁾) = 1
```

### Barycentric Location of the New Identity

**3D location:**

```
r★⁽ⁿ⁺¹⁾ = Σᵢ αᵢ rᵢ⁽ⁿ⁾ / Σᵢ αᵢ
```

**Full identity-space location:**

```
ξ★⁽ⁿ⁺¹⁾ = Σᵢ αᵢ ξᵢ⁽ⁿ⁾ / Σᵢ αᵢ
```

The weights `αᵢ` may represent mass, energy, coupling strength, binding weight,
charge contribution, or any other scale-appropriate packing weight.

**Observed location:**

```
r★ᵒᵇˢ = P₃D(ξ★⁽ⁿ⁺¹⁾)
```

### Why Fuzziness Can Disappear

At the lower scale the group may have internal spread `r_f⁽ⁿ⁾ > 0`.
When packed into a higher-scale identity that spread becomes **internal structure**:

```
lower-scale spread  →  higher-scale internal residual
```

The higher-scale object becomes point-like when:

```
Φ★,S⁽ⁿ⁺¹⁾ = ℓ_S⁽ⁿ⁺¹⁾ / r_{f,★}⁽ⁿ⁺¹⁾  ≪  1
```

```
At lower scale:    Φ_S  ~  1   →  fuzz zone
At packed scale:   Φ_{S+1}  ≪  1   →  new identity is point-defined
```

### State-Field Quota Change

Before coalescence:   `Q_n = m`  (m lower-scale objects)  
After coalescence:    `Q_n = m`  (still intact at lower scale)  
					  `Q_{n+1} = Q_{n+1} + 1`  (new higher-scale object added)

The total multi-scale identity inventory changes:

```
Q_total  →  Q_total + 1
```

The lower-scale particles do **not** necessarily vanish.  They become
components of a higher-scale identity:

```
{ Iᵢ⁽ⁿ⁾ }ᵢ₌₁ᵐ  ⇒  ( { Iᵢ⁽ⁿ⁾ }ᵢ₌₁ᵐ , I★⁽ⁿ⁺¹⁾ )
```

### Coalescence Residual

```
R_coal⁽ⁿ⁾ = Cₙ − Aₙ(I★⁽ⁿ⁺¹⁾)
```

Interpretation: `R_coal` = internal structure hidden by new identity formation.

### Scale Examples

| Lower-scale group | Coalesced identity | Location | Fuzz behaviour |
|---|---|---|---|
| Three quarks | baryon | quark barycenter | color/charge fuzz → internal hadron structure |
| Nucleons + electrons | atom | atomic COM | subatomic fuzz → atom-scale identity |
| Three or more atoms | molecule / fragment | molecular centroid | atomic spread → bond geometry |
| Many atoms | grain / defect / phase | local material barycenter | atomic disorder → material residual |
| Many masses | gravitational system | mass barycenter | object spread → orbital/field identity |
| Visible mass + hidden residual | dark-scale identity | projected mass center | EM fuzz → gravitational residual |

### Paper-Ready Statement

**Scale-Coalescence Identity Law.**  When three or more lower-scale identities
exist within the relevant bond or interaction length of their scale, and their
collective binding score exceeds the coalescence threshold, a new higher-scale
identity may be defined.  This identity is located at the weighted barycenter
of the lower-scale components and carries its own packed state, projection
residual, and scale-specific observability.  The lower-scale identities are not
destroyed; instead, they become components of a newly active higher-scale
object.  In this transition, lower-scale fuzziness may collapse into
higher-scale point-definition because the previous spread becomes internal
residual structure.

> **Summary:** identity birth through scale coalescence adds complexity and
> additional information — a new particle now exists at a higher scale.

---

## 19. Universal Object Triple and Hash Matrix

### 19.1 Universal Object Definition

Every simulated identity-bearing object is a triple:

```
𝒫ᵢ = ( Iᵢ, Hᵢ, Ωw )
```

| Component | Meaning |
|-----------|---------|
| `Iᵢ`  | generic identity matrix (what it **is**) |
| `Hᵢ`  | 2×2 hash matrix (which **instance** it is) |
| `Ωw`  | world seed (which **universe path** it follows) |

Extended form with residual and true location:

```
𝒫ᵢ = ( Iᵢ, Hᵢ, Rᵢ, ξᵢ★, 𝒥ᵢ )
```

where `𝒥ᵢ` (script J) is the **recoverable information / distinguishability**
of the object — the measure of how much of its lower-scale structure can be
reconstructed from its higher-scale packed representation.

### 19.2 Deterministic State Path

```
γᵢ(t) = F( Iᵢ, Hᵢ, Ωw, t )
```

The simulated path is not invented at runtime.  It is generated from identity,
hash, world seed, and evolution rules.  Same input universe → same particle
identity → same path.

### 19.3 Hash Matrix

Every object carries a 2×2 deterministic hash matrix:

```
Hᵢ = Hash₂ₓ₂( Ωw, idᵢ, birthᵢ, parentᵢ, Iᵢ )

	 ⎡ h₀₀  h₀₁ ⎤
Hᵢ = ⎢           ⎥
	 ⎣ h₁₀  h₁₁ ⎦

h_{ab} = Hash( Ωw ∥ idᵢ ∥ birthᵢ ∥ parentᵢ ∥ a ∥ b )
```

Uses: branching, perturbation, decay sampling, tie-breaking, lineage,
deterministic noise.

**Important distinction:**

```
hash uniqueness ≠ physical proof
```

The hash makes the simulation instance reproducible.  It does not assert
experimental truth.

### 19.4 Branch Path Equation

Let possible branches be `B = { b₁, b₂, …, bₘ }` with weights `wⱼ`.

```
uᵢ  = Uniform( Hᵢ, Ωw, t )          # deterministic draw
b★  = Select( uᵢ, { wⱼ } )          # branch selection
```

Determinism guarantee:

```
Ωw, Hᵢ, t  ⇒  b★   (same every run)
```

Constraint:

```
if decay allowed  →  hash selects branch
					  (hash never invents a physically forbidden branch)
```

### 19.5 Pairing with SM's Second Law

When `G_coal⁽ⁿ⁾ = 1`, a new higher-scale identity is born.
Assign it a new hash matrix derived from its parents:

```
H★ = Hash₂ₓ₂( Ωw, H₁, H₂, …, Hₘ, I★ )
```

Identity birth is therefore deterministic:

```
{ Iᵢ, Hᵢ }ᵢ₌₁ᵐ  ⇒  ( I★, H★ )
```

Emergence is not vague.  It has a reproducible identity seed.

### 19.6 Persistent Object Record

```cpp
struct IdentityObject {
	uint64_t             id;
	Matrix               identity;     // 2x2 (lepton/meson) or 3x3 (baryon)
	Matrix               hash2x2;      // always 2x2 — deterministic instance
	uint64_t             world_seed;
	uint64_t             parent_id;
	std::vector<uint64_t> child_ids;
	double               birth_time;
	double               death_time;
	bool                 alive;
	Vec3                 position;
	Vec3                 velocity;
	double               residual_norm;
	double               fuzz_ratio;
};
```

The `hash2x2` is always 2×2 regardless of the identity matrix shape.

---

## 20. Compact Identity Matrix Shapes

### 20.1 Lepton — 2×2

```
Iₗ = [ Q      Lf ]
	 [ J      m  ]
```

**Electron:**

```
I_{e⁻} = [ −1     Lₑ ]
		  [ 1/2    mₑ ]
```

**Electron neutrino:**

```
I_{νₑ} = [ 0      Lₑ ]
		  [ 1/2   m_νₑ]
```

**Full unique object:**

```
𝒫_{e⁻} = ( I_{e⁻},  H_{e⁻},  Ωw )
```

### 20.2 Meson — 2×2

A meson is a quark–antiquark pair `qq̄`, which naturally occupies a 2×2 matrix:

```
I_M = [ q        Γ_bind ]
	  [ q̄        η_M    ]
```

| Entry | Meaning |
|-------|---------|
| `q`        | quark identity |
| `q̄`        | antiquark identity |
| `Γ_bind`   | binding / channel descriptor |
| `η_M`      | meson state: spin / parity / excitation / mass shell |

**Charged pion (π⁺ = ud̄):**

```
I_{π⁺} = [ u    Γ_{qq̄} ]
		  [ d̄   η_{π⁺}  ]

Q_{π⁺} = Q_u + Q_{d̄} = 2/3 + 1/3 = +1
```

**J/ψ (cc̄):**

```
I_{J/ψ} = [ c    Γ_{cc̄} ]
		   [ c̄   η_{J/ψ} ]

Q_{J/ψ} = 2/3 − 2/3 = 0
H_Q = (2/3)² + (−2/3)² = 8/9 ≠ 0
```

The J/ψ is charge-cancelled but internally active.

### 20.3 Baryon — 3×3

A baryon is a three-quark state `qqq`, naturally occupying a 3×3 matrix:

```
I_B = [ q₁    q₂    q₃  ]
	  [ Q₁    Q₂    Q₃  ]
	  [ C₁    C₂    C₃  ]
```

| Column | Meaning |
|--------|---------|
| `qᵢ`  | quark flavor / constituent identity |
| `Qᵢ`  | electric charge contribution |
| `Cᵢ`  | color / binding channel |

**Proton (uud):**

```
I_p = [ u      u      d  ]
	  [ +2/3  +2/3  −1/3 ]
	  [ r      g      b  ]

Q_p = 2/3 + 2/3 − 1/3 = +1
```

**Neutron (udd):**

```
I_n = [ u      d      d  ]
	  [ +2/3  −1/3  −1/3 ]
	  [ r      g      b  ]

Q_n = 2/3 − 1/3 − 1/3 = 0
H_Q(n) = (2/3)² + (1/3)² + (1/3)² = 2/3
```

The neutron is charge-cancelled with `H_Q = 2/3`.  The matrix says more than
the net charge does.

### 20.4 Hash Matrix — Always 2×2

The hash matrix is always 2×2 regardless of particle type:

```
Hᵢ = [ h₀₀  h₀₁ ]      (lepton, meson, and baryon all use this form)
	 [ h₁₀  h₁₁ ]
```

This keeps the instance-identity interface uniform across all object types.

---

## 21. Information / Distinguishability Symbol

The recoverable information / distinguishability of a particle object is
written `𝒥ᵢ` (script J).

| Symbol | Interpretation |
|--------|----------------|
| `𝒥ᵢ`  | recoverable information / distinguishability |
| `Qᵢ`  | state-field quota (count of active identities at a given scale) |

These are distinct:

- `𝒥ᵢ` measures how much of the lower-scale structure survives projection
  into the higher-scale representation — it is a scalar attached to the
  object.
- `Q_n` is a count of identity objects currently active at scale n — it is
  a property of the simulation inventory, not of a single particle.

When SM's Second Law fires: `Q_{n+1} → Q_{n+1} + 1` and the new object
carries its own `𝒥★` derived from the coalescence residual `R_coal`.

---

## 22. Symbol Glossary

| Symbol | Meaning | First defined |
|--------|---------|---------------|
| `Sₙ` | scale-ladder slot n | §0 |
| `Iₚ` | universal particle identity matrix | §1 |
| `Cq` | quark component matrix | §2 |
| `Iₕ` | hadron identity matrix | §3 |
| `Iₗ` | lepton identity matrix | §4 |
| `Ib` | boson identity matrix | §5 |
| `I_A` | atomistic identity matrix | §6 |
| `I_C` | chemical identity matrix | §7 |
| `I_M` | material identity matrix | §8 |
| `I_fuzz` | fuzz-state matrix | §9 |
| `R_AB` | relation-particle matrix | §10 |
| `I_τ` | proper-time matrix | §11 |
| `I_D` | dark projection matrix | §12 |
| `E_k` | event matrix | §13 |
| `I_det` | detector projection matrix | §14 |
| `V_run` | solver comparison matrix | §15 |
| `S_k` | persistent simulation state | §16 |
| `Πₙ` | scale-n packing operator | §17 |
| `Aₙ` | scale-n reconstruction operator | §17 |
| `Rₙ` | packing residual | §17 |
| `G_coal⁽ⁿ⁾` | coalescence gate | §18 |
| `𝒫ᵢ` | universal object triple | §19 |
| `Hᵢ` | 2×2 hash matrix | §19 |
| `Ωw` | world seed (256-bit) | §19 |
| `γᵢ(t)` | deterministic state path | §19 |
| `𝒥ᵢ` | recoverable information / distinguishability | §21 |
| `Qₙ` | state-field quota at scale n | §21 |
| `X_D` | dark-boson / scale-depth mediator | §0, §5 |
| `ξᵢ★` | true identity-location | §9 |
| `Φ_{i,S}` | fuzz ratio at scale S | §9 |
| `ℓ_bond⁽ⁿ⁾` | bond / interaction length at scale n | §18 |
| `τ_B` | coalescence binding threshold | §18 |
| `H_Q` | internal charge activity scalar | §2, §20 |

---

*End of Identity Matrix Notation Reference.*  
*Required reading before any WO-66-N / WO-66-P implementation.*
