"""
test_calibration.py -- Verification tests for calibration_shell + formation.
Direct-import safe (bypasses pykernel.__init__ VisPy gate).
"""
import sys, pathlib, importlib.util

# Direct-load modules
_root = pathlib.Path(__file__).resolve().parent.parent

def _load(name, filename):
    p = _root / filename
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

gas  = _load('pykernel.gas', 'gas.py')
form = _load('pykernel.formation', 'formation.py')
cal  = _load('pykernel.calibration_shell', 'calibration_shell.py')

passed = 0
failed = 0

def check(ok, name):
    global passed, failed
    if ok:
        print(f"  [PASS] {name}")
        passed += 1
    else:
        print(f"  [FAIL] {name}")
        failed += 1

# ── Formation module ──────────────────────────────────────────────────

print("\n--- Formation ---")
check(form.HeatingRegime.classify(800) == form.HeatingRegime.MOLTEN_SALT, "regime: 800C -> SALT")
check(form.HeatingRegime.classify(1200) == form.HeatingRegime.HELIUM_GAS, "regime: 1200C -> HELIUM")
check(form.HeatingRegime.classify(1400) == form.HeatingRegime.RADIATION, "regime: 1400C -> RADIATION")

check(form.LoopPosition.classify(700) == form.LoopPosition.PRIMARY, "loop: 700C -> PRIMARY")
check(form.LoopPosition.classify(850) == form.LoopPosition.SECONDARY, "loop: 850C -> SECONDARY")
check(form.LoopPosition.classify(1100) == form.LoopPosition.TERTIARY, "loop: 1100C -> TERTIARY")
check(form.LoopPosition.classify(1300) == form.LoopPosition.BEYOND, "loop: 1300C -> BEYOND")

ref = form.reference_dataset()
check(len(ref) == 12, "reference dataset has 12 elements")
check(ref[0].symbol == "Au", "first element is Au")
check(ref[6].symbol == "Fe", "element 6 is Fe")
check(ref[0].is_scoreable(), "Au is scoreable")

# Formation record
fr = form.FormationRecord(symbol="Ti", name="Titanium", structure=form.LatticeClass.HCP, n_beads=64, steps=62)
check(fr.is_scoreable(), "Ti record is scoreable")
check(fr.populated_fields() > 3, "Ti has populated fields")

# Degradation
dt = form.DegradationTracker(component_id="test_pipe", T_operating_K=1073.15)
delta = dt.advance_time(3600.0)
check(delta['creep_strain'] > 0, "creep accumulates over 1hr")
check(delta['corrosion_um'] > 0, "corrosion accumulates over 1hr")
check(delta['diffusion_um'] > 0, "diffusion accumulates over 1hr")
check(dt.severity_score() > 0, "severity > 0 after 1hr")
check(dt.severity_score() <= 1.0, "severity <= 1.0")

cyc = dt.advance_cycle()
check('crack_growth_m' in cyc, "cycle returns crack_growth_m")

# Regime transition
rt = form.RegimeTransition()
check(rt.update(800) is None, "no transition at 800C")
new = rt.update(1200)
check(new == form.HeatingRegime.HELIUM_GAS, "transition to HELIUM at 1200C")
check(len(rt.history) == 1, "one transition recorded")

# ── Calibration Shell ─────────────────────────────────────────────────

print("\n--- Calibration Shell ---")
cfg = cal.RunConfig()
check(cfg.field_count == 30, "RunConfig has 30 fields")
check(len(cfg.validate()) == 0, "default config validates OK")

bad = cal.RunConfig(pipe_length_m=-1)
check(len(bad.validate()) > 0, "negative pipe_length fails validation")

# Presets
salt = cal.CalibrationShell.preset_salt_loop()
check(salt.T_peak_C == 900.0, "salt preset T_peak=900")
check(salt.gas_species == "N2", "salt preset uses N2")

he = cal.CalibrationShell.preset_helium_htgr()
check(he.T_peak_C == 1200.0, "helium preset T_peak=1200")
check(he.gas_species == "He", "helium preset uses He")

rad = cal.CalibrationShell.preset_radiation()
check(rad.T_peak_C == 1500.0, "radiation preset T_peak=1500")

# Initialize session
sess = cal.CalibrationShell.initialize(cfg)
check(sess.step_count == 0, "session starts at step 0")
check(sess.regime is not None, "regime assigned")
check(sess.gas_pipe is not None, "gas_pipe initialized")
check(sess.formation is not None, "formation record loaded")
check(sess.degradation is not None, "degradation tracker initialized")

# Run 10 steps
recs = sess.step(10)
check(len(recs) == 10, "10 step records returned")
check(recs[0]['T_C'] >= 600.0, "first step T >= 600C")
check(recs[-1]['T_C'] > recs[0]['T_C'], "temperature increases over steps")
check(recs[-1]['dP_Pa'] > 0, "pressure drop is positive")
check(recs[-1]['Re'] > 0, "Reynolds number is positive")
check(sess.degradation.creep.strain > 0, "creep accumulated during run")

# Summary
s = sess.summary()
check("SimSession" in s, "summary contains SimSession")
check("severity" in s, "summary contains severity")

# Quick run
qr = cal.CalibrationShell.quick_run(cal.CalibrationShell.preset_salt_loop())
check(qr.step_count == 1000, "quick_run completes all 1000 steps")
check(len(qr.history) == 1000, "full history recorded")

# Regime transition during run
cross_cfg = cal.RunConfig(T_peak_C=1400.0, max_steps=100)
cross_sess = cal.CalibrationShell.quick_run(cross_cfg)
check(len(cross_sess.regime_watch.history) > 0, "regime transitions occur in cross-regime run")
check(cross_sess.regime_watch.current_regime == form.HeatingRegime.RADIATION,
      "ends in RADIATION regime at 1400C peak")

print(f"\n{'='*50}")
print(f"RESULTS: {passed} passed, {failed} failed, {passed+failed} total")
if failed == 0:
    print("ALL CALIBRATION TESTS PASSED")
else:
    print(f"{failed} TESTS FAILED")