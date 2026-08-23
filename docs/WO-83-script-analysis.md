# WO-83 Script Analysis — Build-System Dependency and Py→VSIM Gap

**Branch:** `feature/wizard-full-module-expansion`
**Prepared by:** post-WO-83Z reflection pass

---

## 1. The "inferior build system" pattern

Every WO-83 demo/repro script — without exception — follows one identical structure:

```python
# 1. Hard-code a path to the compiled C++ binary
REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"          # ← Windows-only build artifact

def classify(formula: str) -> str:
	# 2. Generate a throw-away .vsim fixture in Python string space
	vsim = textwrap.dedent(f"""
		[project]
		name = "test"
		[material]
		formula = "{formula}"
	""")
	# 3. Write it to a temp file on disk
	with tempfile.NamedTemporaryFile(suffix=".vsim", ...) as f:
		f.write(vsim); path = f.name
	# 4. Shell out to the compiled binary
	r = subprocess.run([str(EXE), "classify", path],
					   capture_output=True, text=True)
	os.unlink(path)                           # 5. Clean up the temp file
	return r.stdout                           # 6. Return raw text for string matching

# 7. Assert on raw stdout strings
assert "vsepr_report" in out
assert "ring_count:      0" in out           # ← checks for a known-wrong placeholder!
```

This is not a VSIM script. It is a Python acceptance harness over a compiled Windows binary.

### Why this is the "inferior" path

| Problem | Detail |
|---|---|
| **Build artifact dependency** | Every script silently fails if `build/vsepr.exe` is absent, stale, or wrong-architecture. No build state is verified before running. |
| **No structured data** | All checks are `"string" in stdout`. Not typed fields, not structured results, not pipeline-composable data. |
| **Temp file churn** | 15 scripts each independently create, write, and delete a temp `.vsim` file to invoke the same CLI path. |
| **Platform coupling** | `build/vsepr.exe` is Windows UCRT64 only. Non-portable across any other environment. |
| **No feedback loop** | Python cannot feed classify results back into a VSIM formation or dynamics pipeline. The classify output dies in the harness. |
| **Stale exe risk** | If the binary is not rebuilt after a code change, the scripts pass against outdated logic. Nothing enforces freshness. |

### The native VSIM approach (target state after WO-84A)

A native VSIM classify script is a `.vsim` file with a `[classify]` section. The pipeline:
1. Reads `[material] formula` directly from the document
2. Drives the C++ classifier without Python, without temp files, without subprocess
3. Exposes structured results to `[classify.expect]` assertions, report export, and downstream pipeline stages
4. Is platform-neutral and portable

The `[classify]` section is not yet parsed by `vsim_parser.cpp`. Until WO-84A wires it, `vsepr classify <file.vsim>` only reads `[material]` and ignores the rest. The skeleton files in `scripts/vsim_skeletons/` document the intended native form for each script.

---

## 2. Script tier classification

### Tier A — Chemistry works; wrapper is cosmetic (11 scripts)

The underlying classifier produces correct output. The Python layer is pure I/O plumbing.
These become native `.vsim` classify scripts once `[classify]` is wired in `vsim_parser.cpp` (WO-84A).

| Script | Formula(s) | Asserts |
|---|---|---|
| `demo_wo83d_vsepr_report.py` | NH3 | `vsepr_report`, `trigonal_pyramidal` or `tetrahedral` |
| `demo_wo83e_geometry_fallback.py` | H2O, NH3 | `vsepr_report`, `organic_candidate` |
| `demo_wo83i_hybridization_hints.py` | CO2, BF3, CH4 | `sp` / `sp2` / `sp3` hints |
| `demo_wo83m_family_guardrails.py` | CH4, C6H12, C16H32O2 | `[inferred]`/`[default]`, `lipid_like_score:` |
| `demo_wo83n_lipid_like_score.py` | C16H34, CH4, C16H32O2, C2N4O4 | numeric ordering via regex |
| `demo_wo83o_strain_score.py` | CH4, SF6 | `strain_score:` in `[0,1]` |
| `demo_wo83p_vsepr_export.py` | CO2 | byte-identical dual run, `sites:`, `conf=`, `rms_dev=` |
| `demo_wo83q_organic_export.py` | C6H12O | `primary_family:`, `sp3_count:`, `strain_score:`, `lipid_like_score:`, `decomp_risk:` |
| `demo_wo83r_classify_pipeline.py` | CH4/NH3/CO2/SF6/C6H12O | `vsepr_report`, `organic_candidate`, `result: PASS` |
| `demo_wo83t_determinism.py` | NH3, CO2, CH4 | exact string equality across 3 runs |
| `demo_wo83u_safe_defaults.py` | C, H2, Xe | exit code 0 (no crash) |

**Blocked on:** `vsim_parser.cpp` + `cmd_classify.hpp` `[classify]` section wiring (WO-84A).

### Tier B — Provider-stub; C++ interface not exposed to VSIM (4 scripts)

The Python script itself explicitly notes that the provider interface is C++ only. These verify null-provider fallback behavior. Even after WO-84A, they require additional provider wiring (WO-84A–C) before VSIM can express the behavior natively.

| Script | Missing VSIM capability | WO gate |
|---|---|---|
| `demo_wo83f_bond_order_stub.py` | `BondOrderProvider` has no VSIM field; CO2 classifies correctly by accident (AX2 = linear), but d8 metals require real bond order to distinguish square_planar vs. tetrahedral | WO-84A, WO-84D |
| `demo_wo83g_lone_pair_provider.py` | `LonePairProvider` null path works for NH3/H2O via element inference, but the provider interface itself has no VSIM-language entry point | WO-84A, WO-84B |
| `demo_wo83h_formal_charge_stub.py` | `FormalChargeProvider` has no VSIM field; NH4/SO4/PO4 are tested only for no-crash, not correct charge assignment | WO-84A |
| `demo_wo83l_rotatable_bond_stub.py` | Rotatable bond count excludes ring bonds (ring-gated), but ring detection is deferred to WO-84C; the count is therefore wrong for cyclic molecules | WO-84A, WO-84C |

**Cannot be replicated Py→VSIM even after WO-84A without provider extension fields.**

### Tier C — Known deferred / placeholder (3 scripts)

These are regression guards for known limitations. They do not test correct behavior; they assert that the known-wrong placeholder value is stable. A future WO-84 implementation will flip the expected value.

| Script | What is deferred | Asserts wrong value intentionally | WO gate |
|---|---|---|---|
| `repro_wo83j_missing_ring_detection.py` | SSSR ring detection | `ring_count: 0` for C6H6 (correct is 1) | WO-84C |
| `repro_wo83k_missing_aromaticity.py` | Hückel 4n+2 aromaticity | no `aromatic` tag for C6H6 (correct is aromatic) | WO-84D |
| `repro_wo83w_known_bug.py` | Xe lone-pair inference (Z=54 not in element table) | `square_planar` absent for XeF4 | WO-84B |

**VSIM native equivalent:** a `[classify.expect_limitation]` gate (concept not yet in the language).
**Cannot be replicated correctly in VSIM** until the underlying chemistry is implemented.

### Tier D — No VSIM equivalent (1 script)

| Script | Nature |
|---|---|
| `demo_wo83v_build_classify.sh` | Pure CMake/Ninja build script; runs `cmake --preset release`, `ninja`, then executes built test binaries. VSIM has no concept of "trigger a C++ build". This is a CI/shell artifact. |

---

## 3. Py→VSIM capability gap matrix

```
						 vsepr classify  [classify]  [classify.providers]  ring_count  aromatic
						 (CLI exists)    (parsed)     (provider fields)     (correct)   (correct)
─────────────────────────────────────────────────────────────────────────────────────────────────
Tier A: geometry/score     YES             NO           —                     —           —
Tier B: provider stubs     YES             NO           NO                    —           —
Tier C: deferred           YES             NO           NO                    NO          NO
Tier D: build script       n/a             n/a          n/a                   n/a         n/a
─────────────────────────────────────────────────────────────────────────────────────────────────
After WO-84A              YES             YES           YES                   —           —
After WO-84A+B            YES             YES           YES (lone pair)       —           —
After WO-84A+B+C          YES             YES           YES + ring            YES         —
After WO-84A+B+C+D        YES             YES           YES + all             YES         YES
```

---

## 4. Skeleton files

Native VSIM equivalents (with stub comments) for every WO-83 script are in
`scripts/vsim_skeletons/`. Each file:
- Contains the `[project]` and `[material]` blocks that work today
- Contains a commented-out `[classify]` block that documents the intended native form
- Marks deferred provider fields with `# STUB:` or `# DEFERRED WO-84x:`

See `scripts/vsim_skeletons/README.md` for the tier legend.

---

## 5. Recommended WO-84A work to close Tier A gap

1. Add `[classify]` to `include/vsim/vsim_document.hpp` (`ClassifySection` struct)
2. Wire `apply_classify_key()` in `src/vsim/vsim_parser.cpp`
3. Pipe `vsim_document.classify` into `cmd_classify` in `apps/vsepr.cpp`
4. Add `expect_contains`, `expect_geometry`, `export_format` fields to `ClassifySection`
5. Convert all Tier A scripts to native `.vsim` classify scripts — delete the Python wrappers

After step 5, the 11 Tier A Python scripts become dead code.

---

*This document is part of the WO-83Z reflection pass. Update it when WO-84A closes Tier A.*
