# vsim_skeletons/

Native `.vsim` equivalents of the WO-83 Python demo scripts.

## Tier legend

| Tier | Meaning | Working today? |
|---|---|---|
| A | Chemistry works; `[classify]` block is commented-out stub only because the parser doesn't wire it yet (WO-84A) | YES — once parser is wired |
| B | Provider-dependent behavior; `[classify.providers]` fields don't exist yet (WO-84A–C) | PARTIAL |
| C | Known limitation repros; expected value will flip when underlying chemistry is implemented (WO-84B–D) | NO — deferred |

## Stub comment conventions

- `# STUB: [classify] — awaiting vsim_parser.cpp [classify] section (WO-84A)`
  Block exists in native VSIM language design but is not yet parsed.

- `# STUB: provider field — [classify.providers] section not yet in vsim_document.hpp`
  The provider interface exists in C++ only; no VSIM-language entry point.

- `# DEFERRED WO-84B: lone-pair provider`
  The required chemistry implementation does not exist yet.

- `# DEFERRED WO-84C: ring detection (SSSR not implemented)`
  Known placeholder; expected value is intentionally wrong until WO-84C.

- `# DEFERRED WO-84D: aromaticity (Hückel 4n+2 check not implemented)`
  Known placeholder; aromatic gate does not exist until WO-84D.

## File index

| File | WO-83 script replaced | Tier |
|---|---|---|
| `wo83d_vsepr_report.vsim` | `demo_wo83d_vsepr_report.py` | A |
| `wo83e_geometry_fallback.vsim` | `demo_wo83e_geometry_fallback.py` | A |
| `wo83f_bond_order_stub.vsim` | `demo_wo83f_bond_order_stub.py` | B |
| `wo83g_lone_pair_provider.vsim` | `demo_wo83g_lone_pair_provider.py` | B |
| `wo83h_formal_charge_stub.vsim` | `demo_wo83h_formal_charge_stub.py` | B |
| `wo83i_hybridization_hints.vsim` | `demo_wo83i_hybridization_hints.py` | A |
| `wo83j_ring_detection.vsim` | `repro_wo83j_missing_ring_detection.py` | C |
| `wo83k_aromaticity.vsim` | `repro_wo83k_missing_aromaticity.py` | C |
| `wo83l_rotatable_bond.vsim` | `demo_wo83l_rotatable_bond_stub.py` | B |
| `wo83m_family_guardrails.vsim` | `demo_wo83m_family_guardrails.py` | A |
| `wo83n_lipid_score.vsim` | `demo_wo83n_lipid_like_score.py` | A |
| `wo83o_strain_score.vsim` | `demo_wo83o_strain_score.py` | A |
| `wo83p_vsepr_export.vsim` | `demo_wo83p_vsepr_export.py` | A |
| `wo83q_organic_export.vsim` | `demo_wo83q_organic_export.py` | A |
| `wo83r_classify_pipeline.vsim` | `demo_wo83r_classify_pipeline.py` | A |
| `wo83t_determinism.vsim` | `demo_wo83t_determinism.py` | A |
| `wo83u_safe_defaults.vsim` | `demo_wo83u_safe_defaults.py` | A |
| `wo83w_known_bug_xef4.vsim` | `repro_wo83w_known_bug.py` | C |

`demo_wo83v_build_classify.sh` has no VSIM equivalent (Tier D — build system only).
