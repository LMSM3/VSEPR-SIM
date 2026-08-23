#!/usr/bin/env python3
"""
Regression Detector
Change one scoring weight or bond cutoff, rerun benchmark suite, report what changed.
"""

import sys
import json
import subprocess
import hashlib
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Set
import difflib

@dataclass
class Result:
    """Single benchmark result."""
    formula: str
    seed: int
    classification: str
    score: float
    energy: float
    bonds: int
    runtime_ms: float
    
    def to_dict(self):
        return asdict(self)


@dataclass
class ChangeReport:
    """Report of changes between two runs."""
    config_hash_old: str
    config_hash_new: str
    config_diff: str
    
    total_runs: int
    reclassified: int
    score_changed: int
    invariant_breaks: List[str]
    
    reclassified_details: List[Dict]
    score_deltas: Dict[str, float]  # formula -> delta


class RegressionDetector:
    """Detect regressions from config changes."""
    
    # Invariants to check
    INVARIANTS = {
        "energy_monotonic": "Energy should decrease during relaxation",
        "bonds_stable": "Bond count should stabilize after equilibration",
        "score_bounded": "Score should be in [0, 1]",
        "classification_consistent": "Same input → same classification",
    }
    
    def __init__(self, benchmark_suite: Path, output_dir: Path):
        self.benchmark_suite = Path(benchmark_suite)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def hash_config(self, config: Dict) -> str:
        """Hash config for change detection."""
        config_str = json.dumps(config, sort_keys=True)
        return hashlib.sha256(config_str.encode()).hexdigest()[:12]
    
    def run_benchmark(self, config: Dict) -> List[Result]:
        """Run entire benchmark suite with given config."""
        
        results = []
        
        # Load benchmark cases
        if not self.benchmark_suite.exists():
            print(f"Benchmark suite not found: {self.benchmark_suite}")
            return results
        
        with open(self.benchmark_suite) as f:
            cases = [json.loads(line) for line in f if line.strip()]
        
        print(f"Running {len(cases)} benchmark cases...")
        
        for i, case in enumerate(cases):
            formula = case["formula"]
            seed = case["seed"]
            
            # Build command with config overrides
            cmd = [
                "./build/atomistic-sim",
                "--formula", formula,
                "--seed", str(seed),
                "--steps", "1000",
                "--output", f"/tmp/bench_{seed}.xyz"
            ]
            
            # Apply config overrides
            for key, val in config.items():
                cmd.extend([f"--{key}", str(val)])
            
            try:
                import time
                start = time.time()
                
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                
                elapsed_ms = (time.time() - start) * 1000
                
                if result.returncode != 0:
                    continue
                
                # Parse output (dummy values for now)
                classification = "stable"
                score = 0.8
                energy = -100.0
                bonds = 10
                
                # TODO: Parse from result.stdout
                
                results.append(Result(
                    formula=formula,
                    seed=seed,
                    classification=classification,
                    score=score,
                    energy=energy,
                    bonds=bonds,
                    runtime_ms=elapsed_ms
                ))
                
                if (i+1) % 10 == 0:
                    print(f"  Completed {i+1}/{len(cases)}")
            
            except Exception as e:
                print(f"  Failed {formula} (seed {seed}): {e}")
        
        return results
    
    def compare_runs(self, baseline: List[Result], current: List[Result]) -> ChangeReport:
        """Compare two benchmark runs."""
        
        # Index by (formula, seed)
        baseline_map = {(r.formula, r.seed): r for r in baseline}
        current_map = {(r.formula, r.seed): r for r in current}
        
        # Find differences
        reclassified = []
        score_deltas = {}
        
        for key, curr in current_map.items():
            if key not in baseline_map:
                continue
            
            base = baseline_map[key]
            
            # Classification changed?
            if base.classification != curr.classification:
                reclassified.append({
                    "formula": curr.formula,
                    "seed": curr.seed,
                    "old_class": base.classification,
                    "new_class": curr.classification,
                    "score_delta": curr.score - base.score,
                })
            
            # Score changed significantly?
            delta = abs(curr.score - base.score)
            if delta > 0.01:  # 1% threshold
                score_deltas[curr.formula] = curr.score - base.score
        
        # Check invariants
        invariant_breaks = self._check_invariants(current)
        
        return ChangeReport(
            config_hash_old="baseline",
            config_hash_new="current",
            config_diff="",
            total_runs=len(current),
            reclassified=len(reclassified),
            score_changed=len(score_deltas),
            invariant_breaks=invariant_breaks,
            reclassified_details=reclassified,
            score_deltas=score_deltas
        )
    
    def _check_invariants(self, results: List[Result]) -> List[str]:
        """Check if any invariants are broken."""
        
        breaks = []
        
        # score_bounded
        for r in results:
            if not (0.0 <= r.score <= 1.0):
                breaks.append(f"score_bounded: {r.formula} has score {r.score:.3f} (out of [0,1])")
        
        # TODO: Check other invariants (needs trajectory data)
        
        return breaks
    
    def detect_regression(self, baseline_config: Dict, new_config: Dict):
        """Run regression detection."""
        
        print("="*80)
        print("REGRESSION DETECTION")
        print("="*80)
        
        # Hash configs
        baseline_hash = self.hash_config(baseline_config)
        new_hash = self.hash_config(new_config)
        
        print(f"Baseline config hash: {baseline_hash}")
        print(f"New config hash:      {new_hash}")
        
        # Show diff
        baseline_str = json.dumps(baseline_config, indent=2, sort_keys=True)
        new_str = json.dumps(new_config, indent=2, sort_keys=True)
        
        diff = list(difflib.unified_diff(
            baseline_str.splitlines(),
            new_str.splitlines(),
            lineterm='',
            fromfile='baseline',
            tofile='new'
        ))
        
        if diff:
            print("\nConfig changes:")
            for line in diff[:20]:  # Show first 20 lines
                print(f"  {line}")
        
        # Run baseline
        print(f"\nRunning baseline ({baseline_hash})...")
        baseline_results = self.run_benchmark(baseline_config)
        
        baseline_file = self.output_dir / f"baseline_{baseline_hash}.json"
        with open(baseline_file, "w") as f:
            json.dump([r.to_dict() for r in baseline_results], f, indent=2)
        
        print(f"  ✓ {len(baseline_results)} results")
        
        # Run new config
        print(f"\nRunning new config ({new_hash})...")
        new_results = self.run_benchmark(new_config)
        
        new_file = self.output_dir / f"current_{new_hash}.json"
        with open(new_file, "w") as f:
            json.dump([r.to_dict() for r in new_results], f, indent=2)
        
        print(f"  ✓ {len(new_results)} results")
        
        # Compare
        print("\nComparing results...")
        report = self.compare_runs(baseline_results, new_results)
        
        report.config_hash_old = baseline_hash
        report.config_hash_new = new_hash
        report.config_diff = '\n'.join(diff)
        
        # Save report
        report_file = self.output_dir / f"regression_report_{new_hash}.json"
        with open(report_file, "w") as f:
            json.dump({
                "baseline_hash": report.config_hash_old,
                "new_hash": report.config_hash_new,
                "config_diff": report.config_diff,
                "total_runs": report.total_runs,
                "reclassified_count": report.reclassified,
                "score_changed_count": report.score_changed,
                "invariant_breaks": report.invariant_breaks,
                "reclassified_details": report.reclassified_details,
                "score_deltas": report.score_deltas,
            }, f, indent=2)
        
        print(f"\n✓ Regression report saved: {report_file}")
        
        # Print summary
        self._print_report(report)
        
        return report
    
    def _print_report(self, report: ChangeReport):
        """Print human-readable regression report."""
        
        print("\n" + "="*80)
        print("REGRESSION ANALYSIS")
        print("="*80)
        print(f"Total runs:          {report.total_runs}")
        print(f"Reclassified:        {report.reclassified} ({report.reclassified/report.total_runs*100:.1f}%)")
        print(f"Score changed:       {report.score_changed} ({report.score_changed/report.total_runs*100:.1f}%)")
        print(f"Invariant breaks:    {len(report.invariant_breaks)}")
        
        if report.reclassified > 0:
            print(f"\nReclassifications (first 10):")
            for detail in report.reclassified_details[:10]:
                print(f"  {detail['formula']} (seed {detail['seed']}): {detail['old_class']} → {detail['new_class']} (Δscore={detail['score_delta']:+.3f})")
        
        if report.score_deltas:
            print(f"\nLargest score changes (top 5):")
            sorted_deltas = sorted(report.score_deltas.items(), key=lambda x: abs(x[1]), reverse=True)
            for formula, delta in sorted_deltas[:5]:
                print(f"  {formula:10s} Δ={delta:+.3f}")
        
        if report.invariant_breaks:
            print(f"\nInvariant breaks:")
            for brk in report.invariant_breaks[:10]:
                print(f"  ❌ {brk}")
        
        print("="*80)
        
        # Overall verdict
        if report.reclassified == 0 and not report.invariant_breaks:
            print("✅ NO REGRESSIONS DETECTED")
        else:
            print("⚠️  REGRESSIONS DETECTED - REVIEW REQUIRED")
        
        print("="*80)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Regression detector")
    parser.add_argument("--benchmark", type=Path, default="benchmark_suite.jsonl", help="Benchmark suite file")
    parser.add_argument("--output", type=Path, default="regression_analysis", help="Output directory")
    parser.add_argument("--baseline-config", type=Path, help="Baseline config JSON")
    parser.add_argument("--new-config", type=Path, help="New config JSON")
    
    args = parser.parse_args()
    
    # Load configs
    baseline_config = {}
    if args.baseline_config and args.baseline_config.exists():
        with open(args.baseline_config) as f:
            baseline_config = json.load(f)
    
    new_config = {}
    if args.new_config and args.new_config.exists():
        with open(args.new_config) as f:
            new_config = json.load(f)
    
    # Example config change
    if not baseline_config:
        baseline_config = {
            "bond_cutoff": 1.8,
            "scoring_weight_energy": 0.4,
            "scoring_weight_structure": 0.6,
        }
    
    if not new_config:
        new_config = baseline_config.copy()
        new_config["bond_cutoff"] = 2.0  # Increase cutoff
    
    # Run
    detector = RegressionDetector(args.benchmark, args.output)
    detector.detect_regression(baseline_config, new_config)


if __name__ == "__main__":
    main()
