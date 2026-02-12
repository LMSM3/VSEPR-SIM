#!/usr/bin/env python3
"""
Autonomous Failure Classifier
Runs 10,000 jobs, categorizes every failure into buckets with minimal repro seeds.
"""

import sys
import json
import subprocess
import re
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict
from enum import Enum
import hashlib

class FailureCategory(Enum):
    NUMERICAL = "numerical"      # dt too big, overflow, NaN
    PHYSICS = "physics"          # clash explosion, unphysical bond graph
    OOD = "out_of_domain"        # composition/scale outside training
    CONVERGENCE = "convergence"  # optimizer failed to converge
    TIMEOUT = "timeout"          # exceeded time limit
    UNKNOWN = "unknown"          # unclassified

@dataclass
class Failure:
    category: FailureCategory
    reason: str
    seed: int
    formula: str
    params: Dict[str, float]
    error_message: str
    minimal_repro: Optional[str] = None
    
    def to_dict(self):
        d = asdict(self)
        d['category'] = self.category.value
        return d

class FailureClassifier:
    """Autonomous failure categorization with minimal repro generation."""
    
    # Pattern matchers for failure categories
    PATTERNS = {
        FailureCategory.NUMERICAL: [
            r"(?i)nan|inf|overflow|underflow",
            r"(?i)timestep.*too.*large",
            r"(?i)numerical.*unstable",
            r"(?i)division.*by.*zero",
            r"(?i)sqrt.*negative",
        ],
        FailureCategory.PHYSICS: [
            r"(?i)clash|explosion|overlap",
            r"(?i)unphysical.*bond",
            r"(?i)energy.*diverged",
            r"(?i)force.*too.*large",
            r"(?i)invalid.*geometry",
        ],
        FailureCategory.OOD: [
            r"(?i)unknown.*element",
            r"(?i)unsupported.*composition",
            r"(?i)atom.*count.*exceeded",
            r"(?i)too.*many.*atoms",
            r"(?i)invalid.*formula",
        ],
        FailureCategory.CONVERGENCE: [
            r"(?i)failed.*converge",
            r"(?i)max.*iterations",
            r"(?i)optimizer.*failed",
            r"(?i)gradient.*exploded",
        ],
        FailureCategory.TIMEOUT: [
            r"(?i)timeout|time.*limit|killed",
            r"(?i)walltime.*exceeded",
        ],
    }
    
    def __init__(self, output_dir: Path):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        self.failures_by_category = {cat: [] for cat in FailureCategory}
        self.total_runs = 0
        self.total_failures = 0
    
    def classify(self, error_message: str, stderr: str, returncode: int) -> FailureCategory:
        """Classify failure based on error patterns."""
        
        # Timeout is special (returncode)
        if returncode in [-9, 124, 137]:  # SIGKILL, timeout, SIGKILL
            return FailureCategory.TIMEOUT
        
        # Combine error sources
        full_error = f"{error_message}\n{stderr}"
        
        # Match patterns
        for category, patterns in self.PATTERNS.items():
            for pattern in patterns:
                if re.search(pattern, full_error):
                    return category
        
        return FailureCategory.UNKNOWN
    
    def extract_reason(self, error_message: str, category: FailureCategory) -> str:
        """Extract concise failure reason."""
        
        # Try to find first line with error
        lines = error_message.split('\n')
        for line in lines:
            if any(kw in line.lower() for kw in ['error', 'failed', 'exception', 'abort']):
                return line.strip()[:200]
        
        # Fallback: first non-empty line
        for line in lines:
            if line.strip():
                return line.strip()[:200]
        
        return f"{category.value} (no specific reason extracted)"
    
    def generate_minimal_repro(self, seed: int, formula: str, params: Dict) -> str:
        """Generate minimal reproduction command."""
        
        cmd_parts = [
            f"./build/meso-sim",
            f"--formula {formula}",
            f"--seed {seed}",
        ]
        
        for key, val in params.items():
            cmd_parts.append(f"--{key} {val}")
        
        return " ".join(cmd_parts)
    
    def record_failure(self, seed: int, formula: str, params: Dict[str, float],
                      error_message: str, stderr: str, returncode: int):
        """Record and classify a failure."""
        
        self.total_failures += 1
        
        # Classify
        category = self.classify(error_message, stderr, returncode)
        reason = self.extract_reason(error_message, category)
        minimal_repro = self.generate_minimal_repro(seed, formula, params)
        
        # Create failure object
        failure = Failure(
            category=category,
            reason=reason,
            seed=seed,
            formula=formula,
            params=params,
            error_message=error_message[:500],  # Truncate
            minimal_repro=minimal_repro
        )
        
        # Store
        self.failures_by_category[category].append(failure)
        
        # Write to category-specific log
        category_file = self.output_dir / f"failures_{category.value}.jsonl"
        with open(category_file, "a") as f:
            f.write(json.dumps(failure.to_dict()) + "\n")
        
        return failure
    
    def run_batch(self, n_jobs: int, formula_pool: List[str],
                  param_ranges: Dict[str, tuple]):
        """Run N jobs and classify all failures."""
        
        import random
        
        print(f"Running {n_jobs} jobs with autonomous failure classification...")
        
        for i in range(n_jobs):
            self.total_runs += 1
            
            # Random config
            seed = random.randint(1, 1000000)
            formula = random.choice(formula_pool)
            params = {k: random.uniform(*v) for k, v in param_ranges.items()}
            
            # Run
            cmd = [
                "./build/meso-sim",
                "--formula", formula,
                "--seed", str(seed),
                "--steps", str(int(params.get('steps', 1000))),
                "--temp", str(params.get('temp', 300.0)),
                "--output", f"/tmp/run_{seed}.xyz"
            ]
            
            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=60
                )
                
                if result.returncode != 0:
                    self.record_failure(
                        seed=seed,
                        formula=formula,
                        params=params,
                        error_message=result.stdout,
                        stderr=result.stderr,
                        returncode=result.returncode
                    )
                
            except subprocess.TimeoutExpired as e:
                self.record_failure(
                    seed=seed,
                    formula=formula,
                    params=params,
                    error_message="Timeout",
                    stderr=str(e),
                    returncode=124
                )
            
            # Progress
            if (i+1) % 100 == 0:
                print(f"  Completed {i+1}/{n_jobs} runs ({self.total_failures} failures)")
    
    def generate_report(self) -> Dict:
        """Generate final failure analysis report."""
        
        report = {
            "total_runs": self.total_runs,
            "total_failures": self.total_failures,
            "success_rate": (self.total_runs - self.total_failures) / self.total_runs if self.total_runs > 0 else 0.0,
            "failures_by_category": {},
            "top_reasons": {},
            "minimal_repros": {}
        }
        
        for category, failures in self.failures_by_category.items():
            count = len(failures)
            if count == 0:
                continue
            
            report["failures_by_category"][category.value] = {
                "count": count,
                "percentage": count / self.total_failures * 100 if self.total_failures > 0 else 0.0
            }
            
            # Top reasons
            reason_counts = {}
            for f in failures:
                reason = f.reason[:100]  # Group by first 100 chars
                reason_counts[reason] = reason_counts.get(reason, 0) + 1
            
            top_reasons = sorted(reason_counts.items(), key=lambda x: x[1], reverse=True)[:5]
            report["top_reasons"][category.value] = top_reasons
            
            # Sample minimal repros
            report["minimal_repros"][category.value] = [
                f.minimal_repro for f in failures[:3]
            ]
        
        return report
    
    def save_report(self):
        """Save comprehensive failure analysis."""
        
        report = self.generate_report()
        
        report_file = self.output_dir / "failure_analysis.json"
        with open(report_file, "w") as f:
            json.dump(report, f, indent=2)
        
        print(f"\n✓ Failure analysis saved: {report_file}")
        
        # Print summary
        print("\n" + "="*80)
        print("FAILURE ANALYSIS SUMMARY")
        print("="*80)
        print(f"Total runs:     {report['total_runs']}")
        print(f"Total failures: {report['total_failures']}")
        print(f"Success rate:   {report['success_rate']*100:.1f}%")
        print("\nFailures by category:")
        for cat, data in report["failures_by_category"].items():
            print(f"  {cat:20s} {data['count']:5d} ({data['percentage']:5.1f}%)")
        
        print("\nTop failure reasons:")
        for cat, reasons in report["top_reasons"].items():
            print(f"\n  {cat}:")
            for reason, count in reasons[:3]:
                print(f"    [{count:3d}] {reason[:70]}")
        
        print("\nMinimal repros (first 3 per category):")
        for cat, repros in report["minimal_repros"].items():
            print(f"\n  {cat}:")
            for repro in repros:
                print(f"    $ {repro}")
        
        print("="*80)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Autonomous failure classifier")
    parser.add_argument("--jobs", type=int, default=10000, help="Number of jobs to run")
    parser.add_argument("--output", type=Path, default="failure_analysis", help="Output directory")
    parser.add_argument("--formulas", type=str, default="H2O,CH4,NH3,CO2,C2H6", help="Comma-separated formula pool")
    
    args = parser.parse_args()
    
    # Setup
    classifier = FailureClassifier(args.output)
    formula_pool = args.formulas.split(',')
    
    param_ranges = {
        "steps": (100, 10000),
        "temp": (50.0, 500.0),
        "dt": (0.1, 2.0),
    }
    
    # Run
    classifier.run_batch(args.jobs, formula_pool, param_ranges)
    
    # Report
    classifier.save_report()


if __name__ == "__main__":
    main()
