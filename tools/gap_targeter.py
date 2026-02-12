#!/usr/bin/env python3
"""
Gap Targeting Loop
Identifies sparse cells in parameter space and schedules runs to improve coverage.
"""

import sys
import json
import numpy as np
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Tuple
import subprocess

@dataclass
class Cell:
    """Cell in parameter space grid."""
    scale: int
    temp: float
    density: float
    
    # Coverage metrics
    n_runs: int = 0
    n_success: int = 0
    n_failures: int = 0
    
    # Mismatch metrics
    mean_energy: float = 0.0
    std_energy: float = 0.0
    max_mismatch: float = 0.0  # vs neighbors
    
    def success_rate(self) -> float:
        return self.n_success / self.n_runs if self.n_runs > 0 else 0.0
    
    def is_sparse(self, threshold: int = 10) -> bool:
        return self.n_runs < threshold
    
    def is_high_mismatch(self, threshold: float = 0.5) -> bool:
        return self.max_mismatch > threshold


class GapTargeter:
    """Coverage-based run scheduler."""
    
    def __init__(self, output_dir: Path):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Define grid
        self.scale_bins = [2, 5, 10, 20, 50, 100]  # atom count
        self.temp_bins = np.linspace(50, 500, 10)   # K
        self.density_bins = np.linspace(0.001, 0.1, 10)  # g/cm³
        
        # Cell grid
        self.cells: Dict[Tuple[int, int, int], Cell] = {}
        self._initialize_grid()
    
    def _initialize_grid(self):
        """Create empty grid."""
        for i, scale in enumerate(self.scale_bins):
            for j, temp in enumerate(self.temp_bins):
                for k, density in enumerate(self.density_bins):
                    key = (i, j, k)
                    self.cells[key] = Cell(scale=scale, temp=temp, density=density)
    
    def _get_cell_key(self, scale: int, temp: float, density: float) -> Tuple[int, int, int]:
        """Map parameters to grid cell."""
        
        # Find closest bin
        i = np.argmin([abs(s - scale) for s in self.scale_bins])
        j = np.argmin([abs(t - temp) for t in self.temp_bins])
        k = np.argmin([abs(d - density) for d in self.density_bins])
        
        return (i, j, k)
    
    def record_run(self, scale: int, temp: float, density: float,
                   success: bool, energy: float):
        """Record run result in grid."""
        
        key = self._get_cell_key(scale, temp, density)
        cell = self.cells[key]
        
        cell.n_runs += 1
        if success:
            cell.n_success += 1
        else:
            cell.n_failures += 1
        
        # Update energy stats
        if success:
            n = cell.n_success
            old_mean = cell.mean_energy
            cell.mean_energy = old_mean + (energy - old_mean) / n
            cell.std_energy = np.sqrt((cell.std_energy**2 * (n-1) + (energy - old_mean) * (energy - cell.mean_energy)) / n)
    
    def compute_mismatches(self):
        """Compute mismatch between neighboring cells."""
        
        for (i, j, k), cell in self.cells.items():
            if cell.n_success < 2:
                continue
            
            # Check neighbors
            neighbors = [
                (i-1, j, k), (i+1, j, k),
                (i, j-1, k), (i, j+1, k),
                (i, j, k-1), (i, j, k+1),
            ]
            
            mismatches = []
            for neighbor_key in neighbors:
                if neighbor_key in self.cells:
                    neighbor = self.cells[neighbor_key]
                    if neighbor.n_success >= 2:
                        # Energy mismatch
                        delta = abs(cell.mean_energy - neighbor.mean_energy)
                        combined_std = np.sqrt(cell.std_energy**2 + neighbor.std_energy**2)
                        if combined_std > 0:
                            mismatch = delta / combined_std
                            mismatches.append(mismatch)
            
            if mismatches:
                cell.max_mismatch = max(mismatches)
    
    def identify_gaps(self, sparse_threshold: int = 10,
                     mismatch_threshold: float = 0.5) -> List[Cell]:
        """Identify cells that need more runs."""
        
        self.compute_mismatches()
        
        gaps = []
        for cell in self.cells.values():
            if cell.is_sparse(sparse_threshold) or cell.is_high_mismatch(mismatch_threshold):
                gaps.append(cell)
        
        # Sort by priority (sparse + high mismatch first)
        gaps.sort(key=lambda c: (c.n_runs, -c.max_mismatch))
        
        return gaps
    
    def schedule_runs(self, n_runs: int) -> List[Dict]:
        """Generate run schedule to fill gaps."""
        
        gaps = self.identify_gaps()
        
        if not gaps:
            print("No gaps detected - coverage is complete!")
            return []
        
        print(f"Identified {len(gaps)} gap cells")
        print(f"Scheduling {n_runs} runs to improve coverage...")
        
        schedule = []
        for i in range(n_runs):
            # Round-robin through gaps
            cell = gaps[i % len(gaps)]
            
            run_config = {
                "scale": cell.scale,
                "temp": cell.temp,
                "density": cell.density,
                "formula": self._generate_formula(cell.scale),
                "seed": i + 1000000,  # Unique seed
            }
            
            schedule.append(run_config)
        
        return schedule
    
    def _generate_formula(self, scale: int) -> str:
        """Generate formula for given scale."""
        
        # Simple heuristic: use molecules that scale well
        if scale <= 5:
            return "H2O"
        elif scale <= 20:
            return "CH4"
        elif scale <= 50:
            return "C6H6"
        else:
            return "C10H22"  # Decane
    
    def execute_schedule(self, schedule: List[Dict]):
        """Execute scheduled runs."""
        
        for i, config in enumerate(schedule):
            print(f"  Run {i+1}/{len(schedule)}: scale={config['scale']}, T={config['temp']:.1f}K, ρ={config['density']:.4f}")
            
            cmd = [
                "./build/meso-sim",
                "--formula", config["formula"],
                "--seed", str(config["seed"]),
                "--temp", str(config["temp"]),
                "--density", str(config["density"]),
                "--steps", "1000",
                "--output", f"/tmp/gap_fill_{config['seed']}.xyz"
            ]
            
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
                
                # Parse energy (dummy for now)
                energy = 0.0
                if "Final energy:" in result.stdout:
                    energy = float(result.stdout.split("Final energy:")[1].split()[0])
                
                success = result.returncode == 0
                
                self.record_run(
                    scale=config["scale"],
                    temp=config["temp"],
                    density=config["density"],
                    success=success,
                    energy=energy
                )
                
            except Exception as e:
                print(f"    Failed: {e}")
                self.record_run(
                    scale=config["scale"],
                    temp=config["temp"],
                    density=config["density"],
                    success=False,
                    energy=0.0
                )
    
    def save_coverage_report(self):
        """Save coverage analysis."""
        
        report = {
            "grid_size": len(self.cells),
            "total_runs": sum(c.n_runs for c in self.cells.values()),
            "cells_with_data": sum(1 for c in self.cells.values() if c.n_runs > 0),
            "sparse_cells": sum(1 for c in self.cells.values() if c.is_sparse()),
            "high_mismatch_cells": sum(1 for c in self.cells.values() if c.is_high_mismatch()),
            "coverage_percentage": sum(1 for c in self.cells.values() if c.n_runs > 0) / len(self.cells) * 100,
            "cells": []
        }
        
        for (i, j, k), cell in self.cells.items():
            if cell.n_runs > 0:
                report["cells"].append({
                    "indices": [i, j, k],
                    "scale": cell.scale,
                    "temp": cell.temp,
                    "density": cell.density,
                    "n_runs": cell.n_runs,
                    "success_rate": cell.success_rate(),
                    "mean_energy": cell.mean_energy,
                    "max_mismatch": cell.max_mismatch,
                })
        
        report_file = self.output_dir / "coverage_report.json"
        with open(report_file, "w") as f:
            json.dump(report, f, indent=2)
        
        print(f"\n✓ Coverage report saved: {report_file}")
        
        # Print summary
        print("\n" + "="*80)
        print("COVERAGE ANALYSIS")
        print("="*80)
        print(f"Grid size:            {report['grid_size']} cells")
        print(f"Cells with data:      {report['cells_with_data']} ({report['coverage_percentage']:.1f}%)")
        print(f"Sparse cells (<10):   {report['sparse_cells']}")
        print(f"High mismatch (>0.5): {report['high_mismatch_cells']}")
        print(f"Total runs:           {report['total_runs']}")
        print("="*80)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Gap targeting loop")
    parser.add_argument("--output", type=Path, default="gap_analysis", help="Output directory")
    parser.add_argument("--fill", type=int, default=1000, help="Number of runs to fill gaps")
    parser.add_argument("--load", type=Path, help="Load existing coverage data")
    
    args = parser.parse_args()
    
    targeter = GapTargeter(args.output)
    
    # Load existing data if provided
    if args.load and args.load.exists():
        print(f"Loading coverage data from {args.load}")
        with open(args.load) as f:
            data = json.load(f)
            for cell_data in data["cells"]:
                i, j, k = cell_data["indices"]
                cell = targeter.cells[(i, j, k)]
                cell.n_runs = cell_data["n_runs"]
                cell.n_success = int(cell_data["n_runs"] * cell_data["success_rate"])
                cell.n_failures = cell.n_runs - cell.n_success
                cell.mean_energy = cell_data["mean_energy"]
                cell.max_mismatch = cell_data.get("max_mismatch", 0.0)
    
    # Identify gaps and schedule runs
    schedule = targeter.schedule_runs(args.fill)
    
    if schedule:
        targeter.execute_schedule(schedule)
    
    # Save report
    targeter.save_coverage_report()


if __name__ == "__main__":
    main()
