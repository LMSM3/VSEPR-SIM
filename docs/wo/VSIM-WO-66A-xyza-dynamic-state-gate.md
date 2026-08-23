# VSIM-WO-66A — .xyza Dynamic State Gate

| Field | Value |
|---|---|
| WO ID | VSIM-WO-66A |
| Title | .xyza Dynamic State Gate |
| Status | **COMPLETE** |
| Branch | v5.0.0-main |
| Precedes | VSIM-WO-66B (Formation Engine Revival) |
| Test group | Group 48 — `XyzaDynamicStateGroup48` |

---

## Objective

Establish a production-ready dynamic-state abstraction for `.xyza` multi-frame
trajectories that is:

- Backed by concrete (non-optional) struct fields for velocity, force, and charge
- Compatible with the existing low-level `xyz_unified.hpp` / `xyz_reader.hpp` / `xyz_writer.hpp` stack
- Bridgeable to the formation engine pre-state layer (WO-66B handoff)
- Covered by regression tests that gate every scope item below

---

## Scope checklist

| # | Item | Status |
|---|---|---|
| 1 | Audit current `.xyza` reader | ✅ Done — `xyz_reader.hpp` `parse_frame_xyza()` is spec-compliant and zero-fills |
| 2 | Audit current `.xyza` writer | ✅ Done — `xyz_writer.hpp` `write_atom_xyza()` emits correct column layout |
| 3 | Confirm fixed 11-column atom-line support | ✅ `G48-01` — round-trip verified at field precision |
| 4 | Confirm missing Q/V/F trailing values default to zero | ✅ `G48-02` — zero-fill confirmed for files without `properties=` |
| 5 | Confirm Properties declaration compatibility | ✅ `G48-03` — per-frame `properties=` tag round-trips correctly |
| 6 | Confirm charge model passthrough | ✅ `G48-04` — `charge_model="partial"` / `"formal"` survives round-trip |
| 7 | Confirm velocity and force arrays enter runtime state | ✅ `G48-05` — `XyzaAtomRecord.velocity_A_per_fs` / `force_eV_per_A` verified |
| 8 | Confirm live report can expose Q/V/F summaries | ✅ `G48-06` — `summarise()` / `print_qvf_summary()` helpers tested |
| 9 | Confirm archive writer can preserve .xyza dynamic frames | ✅ `G48-07` — `XyzaDynamicWriter` streaming 4-frame round-trip |
| 10 | Confirm formation engine can consume dynamic pre-state | ✅ `G48-08/09` — `to_xyz_frame()` / `from_xyz_frame()` bridge tested |

---

## Deliverables

| File | Purpose |
|---|---|
| `include/vsim/io/xyza_dynamic.hpp` | `XyzaAtomRecord`, `XyzaFrame`, read/write API, bridge, report helpers |
| `src/io/xyza_dynamic.cpp` | Translation unit anchor + unit-constant documentation |
| `tests/test_xyza_dynamic_state.cpp` | 9 regression test groups (Group 48) |
| `examples/xyza/h2o_dynamic.xyza` | 4-frame NVT H₂O trajectory (partial charges, velocities, forces, per-atom energy) |
| `examples/xyza/charged_collision_seed.xyza` | Na⁺/Na⁺/Cl⁻/Na⁺/Cl⁻ collision seed (formal charges, convergent velocities) |

---

## Data model

### XyzaAtomRecord

```cpp
struct XyzaAtomRecord {
	std::string species;          // Element symbol

	Vec3   position_A;            // Å
	double charge_e    = 0.0;     // elementary e

	Vec3   velocity_A_per_fs;     // Å/fs
	Vec3   force_eV_per_A;        // eV/Å  (converted from kcal/(mol·Å) on read)
	double energy_eV   = 0.0;     // eV    (converted from kcal/mol on read)
};
```

### XyzaFrame

```cpp
struct XyzaFrame {
	uint64_t step         = 0;
	double   time_fs      = 0.0;
	double   energy_eV    = 0.0;
	double   temperature_K= 0.0;

	std::string label;
	ChargeModel charge_model = ChargeModel::Unknown;

	bool   has_box = false;
	double lx, ly, lz;
	bool   pbc[3];

	bool has_charge, has_velocity, has_force, has_energy_col;
	std::vector<XyzaAtomRecord> atoms;
};
```

---

## Column layout

```
sym  x  y  z  [q  [vx vy vz  [fx fy fz  [e]]]]
 1   2  3  4   5   6  7  8    9 10 11   12
```

Columns 5–12 are optional and driven by the per-frame `properties="..."` declaration.
Missing trailing columns are zero-filled by the reader.

---

## Unit system

| Quantity | On-disk unit | In-memory unit (XyzaAtomRecord) |
|---|---|---|
| position | Å | Å |
| charge | e | e |
| velocity | Å/fs | Å/fs |
| force | kcal/(mol·Å) | eV/Å |
| energy | kcal/mol | eV |
| time | fs | fs |
| temperature | K | K |

Conversion constants in `xyza_dynamic.hpp`:

```cpp
constexpr double kcal_to_eV        = 0.043364104;
constexpr double kcal_mol_A_to_eV_A = 0.043364104;
constexpr double eV_to_kcal        = 23.060541945;
constexpr double eV_A_to_kcal_mol_A = 23.060541945;
```

---

## API surface

### Read

```cpp
XyzaDynamicReadResult read_xyza_dynamic(const std::string& path,
										 ZeroFillPolicy zfp = ZeroFillPolicy::Warn);
```

### Write (batch)

```cpp
bool write_xyza_dynamic(const std::string& path,
						 const std::vector<XyzaFrame>& frames,
						 int coord_prec = 6, int prop_prec = 6);
```

### Write (streaming)

```cpp
XyzaDynamicWriter w("trajectory.xyza");
w.append(frame);   // call per simulation step
```

### Bridge (formation engine handoff — WO-66B)

```cpp
XYZFrame  to_xyz_frame  (const XyzaFrame& f);  // eV → kcal/mol
XyzaFrame from_xyz_frame(const XYZFrame&  f);  // kcal/mol → eV
```

### Live report helpers

```cpp
XyzaFrameSummary s = summarise(frame, idx);
print_qvf_summary(std::cout, {s});
```

---

## Example files

### `examples/xyza/h2o_dynamic.xyza`

4-frame NVT water trajectory at 300 K.  Each frame carries:

- `charge_model="partial"` (Gasteiger-style O = −0.834 e, H = +0.417 e)
- Thermal velocities at 300 K (Å/fs scale)
- Forces from a tip3p-like potential (kcal/(mol·Å) on disk → eV/Å in memory)
- Per-atom energy column

### `examples/xyza/charged_collision_seed.xyza`

Single-frame seed for a binary Na⁺/Na⁺/Cl⁻ head-on collision study.  Two
Na⁺ ions carry equal-and-opposite x-velocities aimed at a central Cl⁻, with
two additional spectator ions off-axis.  Use as a formation-engine pre-state
input for WO-66B collision trajectory generation.

---

## WO-66B handoff

WO-66B (Formation Engine Revival) consumes `XyzaFrame` via `from_xyz_frame()`
to initialise a formation run from a pre-computed dynamic state.  The bridge
function is tested in G48-09 and is the integration seam.

---

## Audit findings

| Subject | Finding | Action |
|---|---|---|
| `xyz_reader.hpp` `parse_frame_xyza` | Per-frame independent `properties=` parsing is spec-correct | No change needed |
| `xyz_reader.hpp` zero-fill | Missing trailing columns emit `COLUMN_ZERO_FILL` warning and default to zero | Matches WO-66A scope item 4 |
| `xyz_writer.hpp` `build_properties_decl` | Reconstructs `properties=` from frame flags, not a cached global | Correct |
| `xyz_writer.hpp` column order | charge → velocity(3) → force(3) → energy — fixed and matches spec | Correct |
| `xyz_unified.hpp` `AtomRecord` | Optional fields (`std::optional<double> q` etc.) remain appropriate for the low-level layer; WO-66A adds a concrete-field layer above | Design confirmed |

---

*Generated by VSEPR-SIM automated WO workflow — v5.0.0-main*
