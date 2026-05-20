"""
VSEPR-SIM v5.1.13 — Python Verification Suite
Pre-electron / high-accuracy classical MD freeze checkpoint.
Runs as a standalone script; exits 0 on pass, 1 on any failure.
"""
import sys, json, pathlib, importlib, math, time
# ensure repo root is on sys.path so `pykernel` resolves regardless of cwd
_root = pathlib.Path(__file__).resolve().parents[2]
if str(_root) not in sys.path:
    sys.path.insert(0, str(_root))

PASS = "[PASS]"
FAIL = "[FAIL]"
errors = []

def ok(msg):  print(f"  {PASS}  {msg}")
def fail(msg): errors.append(msg); print(f"  {FAIL}  {msg}")

print()
print("=" * 64)
print("  VSEPR-SIM v5.1.13  Python Verification Suite")
print("  Pre-electron | High-accuracy classical MD baseline")
print("=" * 64)
print()

# ── 1. Pillar imports ──────────────────────────────────────────
print("[ 1 ] Pillar imports")
try:
    from pykernel.pillars import empirical_chem
    ok("pykernel.pillars.empirical_chem imported")
except Exception as e:
    fail(f"empirical_chem import failed: {e}")

try:
    from pykernel.pillars import steam_tables
    ok("pykernel.pillars.steam_tables imported")
except Exception as e:
    fail(f"steam_tables import failed: {e}")

# ── 2. Empirical JSON integrity ────────────────────────────────
print()
print("[ 2 ] Empirical JSON — schema & coverage")
try:
    data = json.loads(pathlib.Path("data/elements.empirical.json").read_text(encoding="utf-8"))
    assert data["empirical_schema"] == 1,  "schema version != 1"
    assert data["version"] == "v5.1.3",    "version field mismatch"
    els  = data["elements"]
    assert len(els) == 118,                f"expected 118 elements, got {len(els)}"
    ok(f"schema=1  version={data['version']}  elements={len(els)}")
except Exception as e:
    fail(str(e))

# ── 3. Field coverage ─────────────────────────────────────────
print()
print("[ 3 ] Field coverage thresholds  (min ≥ 80 % unless noted)")
FIELDS = [
    ("atomic_weight",          80),
    ("en_pauling",             60),
    ("ionization_energy_1_eV", 80),
    ("covalent_radius_pm",     80),
    ("polarizability_au",      70),
    ("melting_point_K",        60),
    ("electron_config",        70),
]
els_map = {e["Z"]: e for e in data["elements"]}
for field, threshold in FIELDS:
    covered = sum(1 for e in data["elements"]
                  if e.get(field) not in (None, 0.0, "", []))
    pct = covered / 118 * 100
    msg = f"{field:<32} {covered:>3}/118  ({pct:5.1f}%)"
    if pct >= threshold:
        ok(msg)
    else:
        fail(f"{msg}  < threshold {threshold}%")

# ── 4. Electron affinity unit sanity ─────────────────────────
print()
print("[ 4 ] Electron affinity unit sanity")
# Known reference values (eV, NIST)
EA_REF = {
    1:  (+0.754,  0.01),   # H
    2:  (-0.50,   0.10),   # He  (endothermic, ~−0.50 eV)
    7:  (-0.07,   0.05),   # N   (endothermic)
    10: (-1.20,   0.20),   # Ne  (endothermic)
    17: (+3.617,  0.01),   # Cl  (highest stable EA)
    92: (+0.528,  0.05),   # U
}
for z, (ref, tol) in EA_REF.items():
    e   = els_map[z]
    val = e["electron_affinity_eV"]
    sym = e["symbol"]
    diff = abs(val - ref)
    msg = f"Z={z:3} {sym:<3}  EA={val:+8.4f} eV  (ref {ref:+.3f}, tol {tol})"
    if diff <= tol:
        ok(msg)
    else:
        fail(f"{msg}  ERROR diff={diff:.4f}")

# ── 5. Ionization energy sanity ───────────────────────────────
print()
print("[ 5 ] Ionization energy sanity (IE₁, eV)")
IE_REF = {
    1:  (13.598, 0.05),  # H
    2:  (24.587, 0.05),  # He
    10: (21.565, 0.05),  # Ne
    18: (15.760, 0.10),  # Ar
    17: (12.968, 0.05),  # Cl
    26: ( 7.902, 0.10),  # Fe
}
for z, (ref, tol) in IE_REF.items():
    e   = els_map[z]
    val = e["ionization_energy_1_eV"]
    sym = e["symbol"]
    diff = abs(val - ref)
    msg = f"Z={z:3} {sym:<3}  IE₁={val:7.4f} eV  (ref {ref:.3f}, tol {tol})"
    if diff <= tol:
        ok(msg)
    else:
        fail(f"{msg}  ERROR diff={diff:.4f}")

# ── 6. Source-tag audit (every field must have _sources) ──────
print()
print("[ 6 ] Source-tag audit — spot sample (Z=1,6,26,79,92)")
SPOT = [1, 6, 26, 79, 92]
for z in SPOT:
    e    = els_map[z]
    srcs = e.get("_sources", {})
    missing = [k for k in ("atomic_weight","en_pauling","ionization_energy_1_eV",
                            "electron_affinity_eV","covalent_radius_pm")
               if k not in srcs]
    if not missing:
        ok(f"Z={z:3} {e['symbol']:<3}  all source tags present")
    else:
        fail(f"Z={z:3} {e['symbol']:<3}  missing source tags: {missing}")

# ── 7. get_element_by_symbol API ─────────────────────────────
print()
print("[ 7 ] API: get_element_by_symbol")
for sym in ("H","Fe","Au","Og"):
    try:
        e = empirical_chem.get_element_by_symbol(sym)
        ok(f"{sym:<3} → Z={e.Z}  {e.name}")
    except Exception as exc:
        fail(f"{sym}: {exc}")

# ── 8. Audit report exists and has content ───────────────────
print()
print("[ 8 ] Audit report")
rpt = pathlib.Path("docs/audit/chem_audit_empirical.md")
if rpt.exists() and rpt.stat().st_size > 1000:
    ok(f"chem_audit_empirical.md  {rpt.stat().st_size} bytes")
else:
    fail(f"chem_audit_empirical.md missing or too small ({rpt.stat().st_size if rpt.exists() else 0} bytes)")

# ── Summary ───────────────────────────────────────────────────
print()
print("=" * 64)
if not errors:
    print(f"  RESULT: ALL CHECKS PASSED — v5.1.13 freeze verified clean")
    print(f"          Pre-electron | High-accuracy classical MD baseline")
else:
    print(f"  RESULT: {len(errors)} FAILURE(S)")
    for err in errors:
        print(f"          * {err}")
print("=" * 64)
print()

sys.exit(0 if not errors else 1)