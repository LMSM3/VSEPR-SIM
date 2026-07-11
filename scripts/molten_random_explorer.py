"""
molten_random_explorer.py -- Random-reseed ML-style molten salt explorer
========================================================================

Instead of sweeping a fixed list, this explorer:

  1. Randomly samples a subset of salts each generation
  2. Randomly samples operating conditions (T_center, T_wall, geometry)
  3. Runs thermal_iterate on each sample
  4. Scores each run by convergence quality (RMS) and thermal performance
  5. Keeps a running "best-of" ledger across generations
  6. Reseeds the next generation's parameter ranges based on what worked

The loop runs for N_GENERATIONS, each with SAMPLES_PER_GEN random trials.
After each generation the parameter distributions tighten around the
best-performing region (simple exploitation) while retaining a mutation
fraction (exploration).

This is the "simple ML style system" -- no gradient descent, no
backprop, just: sample -> evaluate -> rank -> reseed -> repeat.
The value is that it finds operating corners that a fixed grid sweep
would miss, and it does it without pretending to be smarter than it is.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations
import sys, pathlib, json, time, random, math, copy
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Optional, Tuple

# ── imports ────────────────────────────────────────────────────────────────
ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import importlib.util

def _load(name: str, path: pathlib.Path):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod  = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

mt = _load("pykernel.molten_thermal", ROOT / "pykernel" / "molten_thermal.py")

MATERIALS = mt.MATERIALS
MISSING   = mt.MISSING

OUT = ROOT / "out" / "reports" / "molten_random"
OUT.mkdir(parents=True, exist_ok=True)

# ── full candidate pools ───────────────────────────────────────────────────
ALL_SALTS = [f for f, m in MATERIALS.items()
             if m.phase == "liquid" and m.category in ("fuel", "coolant")
             and f not in ("Na_liq", "Pb_liq", "H2O_liq")]

ALL_METALS = [f for f, m in MATERIALS.items()
              if m.phase == "solid"]


# ═══════════════════════════════════════════════════════════════════════════
# Parameter space
# ═══════════════════════════════════════════════════════════════════════════

@dataclass
class ParamRange:
    """Defines a uniform range for one continuous parameter."""
    lo: float
    hi: float

    def sample(self, rng: random.Random) -> float:
        return rng.uniform(self.lo, self.hi)

    def shrink_toward(self, center: float, factor: float = 0.7) -> "ParamRange":
        """Tighten range toward center by factor, keeping at least 20% width."""
        half = (self.hi - self.lo) / 2.0
        new_half = max(half * factor, (self.hi - self.lo) * 0.1)
        lo = max(self.lo, center - new_half)
        hi = min(self.hi, center + new_half)
        return ParamRange(lo, hi)


@dataclass
class SearchSpace:
    """Full parameter search space for one generation."""
    T_center: ParamRange = field(default_factory=lambda: ParamRange(600.0, 1300.0))
    T_wall:   ParamRange = field(default_factory=lambda: ParamRange(400.0, 900.0))
    r_inner:  ParamRange = field(default_factory=lambda: ParamRange(0.005, 0.02))
    r_outer:  ParamRange = field(default_factory=lambda: ParamRange(0.02, 0.08))
    N_grid:   ParamRange = field(default_factory=lambda: ParamRange(8, 64))

    def sample(self, rng: random.Random) -> dict:
        T_c = self.T_center.sample(rng)
        T_w = self.T_wall.sample(rng)
        # enforce T_center > T_wall + 50
        if T_c <= T_w + 50:
            T_c = T_w + 50 + rng.uniform(0, 100)
        r_i = self.r_inner.sample(rng)
        r_o = self.r_outer.sample(rng)
        if r_o <= r_i + 0.005:
            r_o = r_i + 0.005 + rng.uniform(0, 0.01)
        N = int(self.N_grid.sample(rng))
        N = max(4, min(N, 128))
        # round N to nearest even
        N = N + (N % 2)
        return {
            "T_center_K": round(T_c, 1),
            "T_wall_K": round(T_w, 1),
            "r_inner_m": round(r_i, 5),
            "r_outer_m": round(r_o, 5),
            "N": N,
        }


# ═══════════════════════════════════════════════════════════════════════════
# Trial result
# ═══════════════════════════════════════════════════════════════════════════

@dataclass
class TrialResult:
    generation: int
    trial_id: int
    seed: int
    formula: str
    material_type: str          # "salt" or "metal"
    T_center_K: float
    T_wall_K: float
    r_inner_m: float
    r_outer_m: float
    N: int
    n_steps: int
    rms_final: float
    converged: bool
    T_mid_K: float
    T_max_K: float
    T_min_K: float
    score: float                # composite fitness score (lower = better)
    elapsed_s: float
    k_W_mK: float = 0.0
    rho_kgm3: float = 0.0
    Cp_JkgK: float = 0.0

    def to_dict(self) -> dict:
        return asdict(self)


# ═══════════════════════════════════════════════════════════════════════════
# Scoring
# ═══════════════════════════════════════════════════════════════════════════

def score_trial(r: dict, params: dict) -> float:
    """
    Composite score: lower is better.

    Components:
      - RMS convergence quality (want low)
      - Temperature gradient utilization (want T_mid between T_wall and T_center)
      - Grid efficiency (penalize excessive N for marginal gain)
    """
    rms = r["rms_final"]

    # RMS term: log scale, clamped
    rms_score = max(math.log10(max(rms, 1e-15)) + 15, 0.0)  # 0 at rms=1e-15, ~15 at rms=1

    # Temperature utilization: how well does T_mid sit in the physical range?
    T_mid = r["T_profile"][params["N"] // 2] if r["T_profile"] else params["T_center_K"]
    T_c = params["T_center_K"]
    T_w = params["T_wall_K"]
    T_range = max(T_c - T_w, 1.0)
    # Penalize if T_mid is outside [T_w, T_c] (volumetric heating can push it above)
    if T_mid > T_c:
        overshoot = (T_mid - T_c) / T_range
        temp_penalty = overshoot * 5.0
    else:
        temp_penalty = 0.0

    # Grid penalty: mild preference for lower N at same quality
    grid_penalty = params["N"] / 200.0

    return round(rms_score + temp_penalty + grid_penalty, 6)


# ═══════════════════════════════════════════════════════════════════════════
# Property lookup
# ═══════════════════════════════════════════════════════════════════════════

def _get_props(formula: str, T_avg: float) -> dict:
    m = MATERIALS[formula]
    k  = m.get("thermal_conductivity", T_avg)
    rho = m.get("density", T_avg)
    cp = m.get("Cp", T_avg)
    if k   == MISSING: k   = 1.0
    if rho == MISSING: rho = m.density_kgm3
    if cp  == MISSING: cp  = 1500.0
    return {"k": k, "rho": rho, "Cp": cp}


# ═══════════════════════════════════════════════════════════════════════════
# Single trial
# ═══════════════════════════════════════════════════════════════════════════

def run_trial(formula: str, mat_type: str, params: dict,
              gen: int, trial_id: int, seed: int) -> TrialResult:
    """Run one thermal_iterate trial and score it."""
    n_steps = 1500 + params["N"] * 40
    t0 = time.perf_counter()

    r = mt.thermal_iterate(
        formula, params["N"], n_steps=n_steps,
        T_wall=params["T_wall_K"], T_center=params["T_center_K"],
        r_inner=params["r_inner_m"], r_outer=params["r_outer_m"],
    )

    elapsed = time.perf_counter() - t0
    converged = r["rms_final"] < 1e-2
    sc = score_trial(r, params)

    T_mid = r["T_profile"][params["N"] // 2] if r["T_profile"] else params["T_center_K"]
    props = _get_props(formula, (params["T_center_K"] + params["T_wall_K"]) / 2.0)

    return TrialResult(
        generation=gen, trial_id=trial_id, seed=seed,
        formula=formula, material_type=mat_type,
        T_center_K=params["T_center_K"], T_wall_K=params["T_wall_K"],
        r_inner_m=params["r_inner_m"], r_outer_m=params["r_outer_m"],
        N=params["N"], n_steps=n_steps,
        rms_final=r["rms_final"], converged=converged,
        T_mid_K=T_mid, T_max_K=r["T_max"], T_min_K=r["T_min"],
        score=sc, elapsed_s=round(elapsed, 3),
        k_W_mK=props["k"], rho_kgm3=props["rho"], Cp_JkgK=props["Cp"],
    )


# ═══════════════════════════════════════════════════════════════════════════
# Generation runner
# ═══════════════════════════════════════════════════════════════════════════

def run_generation(gen: int, space: SearchSpace, rng: random.Random,
                   pool: List[str], pool_type: str,
                   samples: int, best_so_far: List[TrialResult]
                   ) -> Tuple[List[TrialResult], SearchSpace]:
    """
    Run one generation of random trials.

    Returns (results, updated_search_space).
    """
    results: List[TrialResult] = []

    # pick a random subset of materials (at least 2, at most all)
    n_mats = rng.randint(2, min(len(pool), max(3, len(pool) // 2 + 1)))
    chosen = rng.sample(pool, n_mats)

    print(f"\n  Gen {gen}: {n_mats} materials from {pool_type} pool: {chosen}")
    print(f"  Search space: T_c=[{space.T_center.lo:.0f},{space.T_center.hi:.0f}] "
          f"T_w=[{space.T_wall.lo:.0f},{space.T_wall.hi:.0f}] "
          f"r_o=[{space.r_outer.lo:.4f},{space.r_outer.hi:.4f}] "
          f"N=[{int(space.N_grid.lo)},{int(space.N_grid.hi)}]")

    trial_id = 0
    for formula in chosen:
        # each material gets samples/n_mats trials (at least 1)
        n_trials = max(1, samples // n_mats)
        for _ in range(n_trials):
            seed = rng.randint(0, 2**31)
            params = space.sample(rng)
            tr = run_trial(formula, pool_type, params, gen, trial_id, seed)
            results.append(tr)

            tag = "OK" if tr.converged else "!!"
            print(f"    #{trial_id:3d}  {formula:20s}  T_c={tr.T_center_K:.0f} T_w={tr.T_wall_K:.0f}  "
                  f"N={tr.N:3d}  rms={tr.rms_final:.3e}  score={tr.score:.3f}  [{tag}]")
            trial_id += 1

    # ── reseed: shrink search space toward best results ────────────────────
    all_results = best_so_far + results
    all_results.sort(key=lambda t: t.score)
    top_k = all_results[:max(3, len(all_results) // 4)]

    if top_k:
        avg_Tc = sum(t.T_center_K for t in top_k) / len(top_k)
        avg_Tw = sum(t.T_wall_K for t in top_k) / len(top_k)
        avg_ro = sum(t.r_outer_m for t in top_k) / len(top_k)
        avg_N  = sum(t.N for t in top_k) / len(top_k)

        # 70% exploitation, 30% exploration (keep some width)
        new_space = SearchSpace(
            T_center=space.T_center.shrink_toward(avg_Tc, 0.7),
            T_wall=space.T_wall.shrink_toward(avg_Tw, 0.7),
            r_inner=space.r_inner,  # keep inner radius range stable
            r_outer=space.r_outer.shrink_toward(avg_ro, 0.7),
            N_grid=space.N_grid.shrink_toward(avg_N, 0.7),
        )

        print(f"\n  Reseed: top-{len(top_k)} avg -> T_c={avg_Tc:.0f} T_w={avg_Tw:.0f} "
              f"r_o={avg_ro:.4f} N={avg_N:.0f}")
    else:
        new_space = space

    return results, new_space


# ═══════════════════════════════════════════════════════════════════════════
# Mutation: swap material pool
# ═══════════════════════════════════════════════════════════════════════════

def maybe_swap_pool(gen: int, rng: random.Random,
                    current_pool: List[str], current_type: str
                    ) -> Tuple[List[str], str]:
    """
    Every few generations, randomly decide whether to switch
    from salts to metals (or vice versa) for diversity.
    """
    if gen > 0 and gen % 3 == 0 and rng.random() < 0.4:
        if current_type == "salt" and ALL_METALS:
            print(f"\n  *** POOL SWAP: salt -> metal (gen {gen}) ***")
            return ALL_METALS[:], "metal"
        elif current_type == "metal":
            print(f"\n  *** POOL SWAP: metal -> salt (gen {gen}) ***")
            return ALL_SALTS[:], "salt"
    return current_pool, current_type


# ═══════════════════════════════════════════════════════════════════════════
# Main explorer
# ═══════════════════════════════════════════════════════════════════════════

def explore(n_generations: int = 8, samples_per_gen: int = 12,
            master_seed: int = 42) -> dict:
    """
    Run the full random-reseed exploration loop.

    Args:
        n_generations:  how many generations to run
        samples_per_gen: random trials per generation
        master_seed: reproducibility seed
    """
    rng = random.Random(master_seed)
    space = SearchSpace()
    pool = ALL_SALTS[:]
    pool_type = "salt"
    best_so_far: List[TrialResult] = []
    all_trials: List[TrialResult] = []
    gen_summaries: List[dict] = []

    t0 = time.perf_counter()

    print("=" * 72)
    print("  MOLTEN RANDOM EXPLORER -- ML-style reseed loop")
    print(f"  Generations: {n_generations}  Samples/gen: {samples_per_gen}")
    print(f"  Master seed: {master_seed}")
    print(f"  Salt pool:  {ALL_SALTS}")
    print(f"  Metal pool: {ALL_METALS}")
    print("=" * 72)

    for gen in range(n_generations):
        pool, pool_type = maybe_swap_pool(gen, rng, pool, pool_type)
        results, space = run_generation(
            gen, space, rng, pool, pool_type,
            samples_per_gen, best_so_far)

        all_trials.extend(results)
        best_so_far = sorted(best_so_far + results, key=lambda t: t.score)
        # keep top 20 across all generations
        best_so_far = best_so_far[:20]

        gen_best = min(results, key=lambda t: t.score)
        gen_worst = max(results, key=lambda t: t.score)
        n_conv = sum(1 for t in results if t.converged)

        gs = {
            "generation": gen,
            "pool_type": pool_type,
            "n_trials": len(results),
            "n_converged": n_conv,
            "best_score": gen_best.score,
            "best_formula": gen_best.formula,
            "best_rms": gen_best.rms_final,
            "worst_score": gen_worst.score,
            "global_best_score": best_so_far[0].score if best_so_far else None,
            "global_best_formula": best_so_far[0].formula if best_so_far else None,
            "space_T_center": [space.T_center.lo, space.T_center.hi],
            "space_T_wall": [space.T_wall.lo, space.T_wall.hi],
            "space_r_outer": [space.r_outer.lo, space.r_outer.hi],
            "space_N": [space.N_grid.lo, space.N_grid.hi],
        }
        gen_summaries.append(gs)

        print(f"\n  Gen {gen} summary: {n_conv}/{len(results)} converged  "
              f"best={gen_best.score:.3f} ({gen_best.formula})  "
              f"global_best={best_so_far[0].score:.3f} ({best_so_far[0].formula})")

    elapsed = time.perf_counter() - t0

    # ── write outputs ──────────────────────────────────────────────────────
    import csv as _csv

    # all trials
    all_dicts = [t.to_dict() for t in all_trials]
    (OUT / "all_trials.json").write_text(json.dumps(all_dicts, indent=2), encoding="utf-8")
    if all_dicts:
        cols = list(all_dicts[0].keys())
        with open(OUT / "all_trials.csv", "w", newline="", encoding="utf-8") as fh:
            w = _csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(all_dicts)

    # best-of ledger
    best_dicts = [t.to_dict() for t in best_so_far]
    (OUT / "best_of_ledger.json").write_text(json.dumps(best_dicts, indent=2), encoding="utf-8")

    # generation summaries
    (OUT / "generation_summaries.json").write_text(
        json.dumps(gen_summaries, indent=2), encoding="utf-8")

    # ── final report ───────────────────────────────────────────────────────
    print()
    print("=" * 72)
    print("  EXPLORATION COMPLETE")
    print(f"  Total trials: {len(all_trials)}")
    print(f"  Elapsed: {elapsed:.1f} s")
    print()
    print("  TOP 10 ACROSS ALL GENERATIONS:")
    for i, t in enumerate(best_so_far[:10]):
        tag = "OK" if t.converged else "!!"
        print(f"    {i+1:2d}. gen={t.generation} {t.formula:20s} "
              f"T_c={t.T_center_K:.0f} T_w={t.T_wall_K:.0f} "
              f"N={t.N:3d} rms={t.rms_final:.3e} score={t.score:.3f} [{tag}]")

    print()
    print("  SEARCH SPACE EVOLUTION:")
    for gs in gen_summaries:
        g = gs["generation"]
        tc = gs["space_T_center"]
        tw = gs["space_T_wall"]
        ro = gs["space_r_outer"]
        print(f"    Gen {g}: T_c=[{tc[0]:.0f},{tc[1]:.0f}] "
              f"T_w=[{tw[0]:.0f},{tw[1]:.0f}] "
              f"r_o=[{ro[0]:.4f},{ro[1]:.4f}] "
              f"best={gs['best_score']:.3f} ({gs['best_formula']})")

    print()
    print(f"  Output: {OUT}")
    print("=" * 72)

    return {
        "n_generations": n_generations,
        "total_trials": len(all_trials),
        "elapsed_s": round(elapsed, 2),
        "master_seed": master_seed,
        "global_best": best_so_far[0].to_dict() if best_so_far else None,
        "generation_summaries": gen_summaries,
    }


# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    explore()
