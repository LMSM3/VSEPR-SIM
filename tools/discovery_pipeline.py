#!/usr/bin/env python3
"""
VSEPR Discovery Pipeline - Full Workflow
Generate → Test → Validate → Catalog
"""

import subprocess
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
import sys
import os
import uuid
import shutil

# Add tools directory to path
sys.path.insert(0, str(Path(__file__).parent))
from chemical_validator import ChemicalDatabase, MoleculeValidator, ValidationResult
from scoring import score_molecule

@dataclass
class RunCard:
    """Immutable card for a single simulation run"""
    run_id: str
    title: str
    formula: str
    domain: str  # @molecule, @gas, @bulk, @crystal
    size: int  # Number of atoms
    model: str  # LJ, LJ+Coulomb, etc.
    score: float  # Priority score (0-100)
    health: str  # converged, bounded, exploded, invalid
    timestamp: str
    metrics: Dict
    validation: Dict
    paths: Dict
    tags: List[str]
    generation_method: str

@dataclass
class DiscoveryRecord:
    """Record of a molecule discovery"""
    formula: str
    generation_method: str  # "random", "discovery", "targeted"
    energy: float  # kcal/mol
    max_force: float  # kcal/(mol·Å)
    converged: bool
    stable: bool
    database_validated: bool
    is_novel: bool
    confidence: float
    notes: str
    xyz_file: str
    timestamp: str

class DiscoveryPipeline:
    """Full discovery pipeline: Generate → Test → Validate → Catalog"""

    def __init__(self, output_dir: str = "out/discoveries", catalog_dir: str = "out/catalog"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Card catalog directory
        self.catalog_dir = Path(catalog_dir)
        self.catalog_dir.mkdir(parents=True, exist_ok=True)

        # Initialize validator
        self.db = ChemicalDatabase(cache_dir=str(self.output_dir / "chemical_cache"))
        self.validator = MoleculeValidator(self.db)

        # Catalog files
        self.catalog_file = self.output_dir / "discovery_catalog.json"
        self.cards_index = self.catalog_dir / "cards_index.json"

        self.catalog = self._load_catalog()
        self.cards = self._load_cards_index()
    
    def _load_catalog(self) -> List[Dict]:
        """Load existing catalog"""
        if self.catalog_file.exists():
            with open(self.catalog_file, 'r') as f:
                return json.load(f)
        return []

    def _save_catalog(self):
        """Save catalog to disk"""
        with open(self.catalog_file, 'w') as f:
            json.dump(self.catalog, f, indent=2)

    def _load_cards_index(self) -> List[Dict]:
        """Load cards index"""
        if self.cards_index.exists():
            with open(self.cards_index, 'r') as f:
                return json.load(f)
        return []

    def _save_cards_index(self):
        """Save cards index"""
        with open(self.cards_index, 'w') as f:
            json.dump(self.cards, f, indent=2)
    
    def parse_xyz_file(self, xyz_file: str) -> Tuple[str, int, Dict[str, int]]:
        """
        Parse XYZ file to extract formula and element counts

        Returns:
            (formula, num_atoms, element_counts)
        """
        with open(xyz_file, 'r') as f:
            lines = f.readlines()

        num_atoms = int(lines[0].strip())

        # Count elements
        element_counts = {}
        for line in lines[2:2+num_atoms]:
            parts = line.split()
            if len(parts) >= 4:
                element = parts[0]
                element_counts[element] = element_counts.get(element, 0) + 1

        # Build formula (sorted by element)
        formula_parts = []
        for element in sorted(element_counts.keys()):
            count = element_counts[element]
            if count == 1:
                formula_parts.append(element)
            else:
                formula_parts.append(f"{element}{count}")

        formula = ''.join(formula_parts)
        return formula, num_atoms, element_counts
    
    def parse_simulation_output(self, output_file: str) -> Dict:
        """
        Parse simulation output to extract energy, forces, convergence
        
        Expected format (from meso-sim or qa_golden_tests):
          Final energy: -123.45 kcal/mol
          Max force: 0.0012 kcal/(mol·Å)
          Converged: yes
        """
        data = {
            'energy': 0.0,
            'max_force': 0.0,
            'converged': False,
            'stable': False
        }
        
        if not Path(output_file).exists():
            return data
        
        with open(output_file, 'r') as f:
            for line in f:
                if 'energy' in line.lower():
                    try:
                        # Extract number from line like "Final energy: -123.45 kcal/mol"
                        parts = line.split(':')
                        if len(parts) >= 2:
                            energy_str = parts[1].split()[0]
                            data['energy'] = float(energy_str)
                    except:
                        pass
                
                if 'force' in line.lower() and 'max' in line.lower():
                    try:
                        parts = line.split(':')
                        if len(parts) >= 2:
                            force_str = parts[1].split()[0]
                            data['max_force'] = float(force_str)
                    except:
                        pass
                
                if 'converged' in line.lower():
                    data['converged'] = 'yes' in line.lower() or 'true' in line.lower()
                
                if 'stable' in line.lower():
                    data['stable'] = 'yes' in line.lower() or 'true' in line.lower()
        
        # Infer stability from energy and force
        if data['max_force'] < 0.01 and data['energy'] < 0:
            data['stable'] = True
        
        return data
    
    def run_discovery_workflow(self, xyz_file: str, method: str = "random") -> DiscoveryRecord:
        """
        Run full discovery workflow on a molecule
        
        Steps:
          1. Parse XYZ file to get formula
          2. Parse simulation results
          3. Validate against database
          4. Create discovery record
          5. Add to catalog
        
        Args:
            xyz_file: Path to XYZ file
            method: How molecule was generated
        
        Returns:
            DiscoveryRecord
        """
        print(f"\n═══════════════════════════════════════════════════════════")
        print(f"  DISCOVERY WORKFLOW: {Path(xyz_file).name}")
        print(f"═══════════════════════════════════════════════════════════")
        
        # Step 1: Parse XYZ
        print("\nStep 1: Parsing XYZ file...")
        formula, num_atoms, element_counts = self.parse_xyz_file(xyz_file)
        print(f"  Formula: {formula}")
        print(f"  Atoms: {num_atoms}")
        print(f"  Composition: {element_counts}")
        
        # Step 2: Parse simulation results (if available)
        print("\nStep 2: Parsing simulation results...")
        output_file = str(Path(xyz_file).with_suffix('.out'))
        sim_data = self.parse_simulation_output(output_file)
        
        print(f"  Energy: {sim_data['energy']:.2f} kcal/mol")
        print(f"  Max Force: {sim_data['max_force']:.4f} kcal/(mol·Å)")
        print(f"  Converged: {sim_data['converged']}")
        print(f"  Stable: {sim_data['stable']}")
        
        # Step 3: Database validation
        print("\nStep 3: Database validation...")
        stability_str = "stable" if sim_data['stable'] else "unstable"
        validation = self.validator.validate_molecule(
            formula, 
            sim_data['energy'], 
            stability_str
        )
        
        print(f"  Known: {validation.is_known}")
        print(f"  Novel: {validation.novel}")
        print(f"  Confidence: {validation.confidence:.1%}")
        print(f"  {validation.notes}")
        
        # Step 4: Create record
        import datetime
        record = DiscoveryRecord(
            formula=formula,
            generation_method=method,
            energy=sim_data['energy'],
            max_force=sim_data['max_force'],
            converged=sim_data['converged'],
            stable=sim_data['stable'],
            database_validated=True,
            is_novel=validation.novel,
            confidence=validation.confidence,
            notes=validation.notes,
            xyz_file=str(Path(xyz_file).absolute()),
            timestamp=datetime.datetime.now().isoformat()
        )
        
        # Step 5: Add to catalog
        self.catalog.append(asdict(record))
        self._save_catalog()

        print(f"\n✓ Added to catalog: {self.catalog_file}")

        # Step 6: Generate run card
        print("\nStep 4: Generating run card...")
        card = self._generate_run_card(xyz_file, record, sim_data, validation, element_counts)
        print(f"  ✓ Card created: {card.run_id}")

        return record

    def _calculate_priority_score(self, record: DiscoveryRecord, sim_data: Dict, 
                                  element_counts: Dict[str, int]) -> Tuple[float, Dict]:
        """
        Calculate priority score using explicit weighting system

        Returns:
            (priority_score, breakdown_dict)
        """
        # Determine health status
        health = self._determine_health(record, sim_data)

        # Infer if has long-range interactions (Coulomb)
        has_long_range = record.model in ["LJ+Coulomb", "UFF"]

        # Compute total charge (assume neutral for now, TODO: extract from sim)
        total_charge = 0.0

        # Use scoring module
        N = sum(element_counts.values())
        priority, breakdown = score_molecule(
            N=N,
            element_counts=element_counts,
            total_charge=total_charge,
            health=health,
            converged=record.converged,
            bounded=record.stable,
            has_long_range=has_long_range
        )

        return priority, breakdown

    def _infer_domain(self, formula: str, num_atoms: int) -> str:
        """Infer simulation domain from formula and size"""
        if num_atoms == 1:
            return "@molecule"
        elif num_atoms <= 10:
            if formula.startswith("Ar") or formula.startswith("He"):
                return "@cluster"
            else:
                return "@molecule"
        elif num_atoms <= 100:
            return "@gas"
        else:
            return "@bulk"

    def _determine_health(self, record: DiscoveryRecord, sim_data: Dict) -> str:
        """Determine health status from simulation data"""
        if record.converged and record.stable and record.max_force < 0.01:
            return "converged"
        elif record.energy < 0 and record.max_force < 0.1:
            return "bounded"
        elif abs(record.energy) > 1e6 or record.max_force > 10.0:
            return "exploded"
        else:
            return "invalid"

    def _generate_run_card(self, xyz_file: str, record: DiscoveryRecord, 
                          sim_data: Dict, validation: ValidationResult,
                          element_counts: Dict[str, int]) -> RunCard:
        """
        Generate an immutable run card

        Creates directory structure:
          out/catalog/<run_id>/
            structure.xyz
            summary.json
            metrics.json
            log.txt
        """
        # Generate unique run ID
        run_id = f"run_{record.timestamp.replace(':', '').replace('-', '').replace('.', '_')[:17]}_{uuid.uuid4().hex[:4]}"

        # Create run directory
        run_dir = self.catalog_dir / run_id
        run_dir.mkdir(exist_ok=True)

        # Copy structure file
        dest_xyz = run_dir / "structure.xyz"
        shutil.copy(xyz_file, dest_xyz)

        # Calculate priority score with breakdown
        score, score_breakdown = self._calculate_priority_score(record, sim_data, element_counts)

        # Infer domain
        domain = self._infer_domain(record.formula, len(record.formula))

        # Determine health
        health = self._determine_health(record, sim_data)

        # Build metrics dict
        metrics = {
            "energy_per_atom": record.energy / max(1, len(record.formula)),
            "max_force": record.max_force,
            "temperature_drift": 0.0,  # TODO: Extract from sim if available
            "charge_neutrality_error": 0.0,
            "iterations": 0  # TODO: Extract from sim
        }

        # Build validation dict
        validation_dict = {
            "is_known": validation.is_known,
            "is_novel": validation.novel,
            "confidence": validation.confidence,
            "database_matches": [
                {
                    "name": m.name,
                    "cid": m.cid,
                    "similarity": 1.0
                } for m in validation.matches[:3]
            ]
        }

        # Build paths dict
        paths = {
            "structure_xyz": "structure.xyz",
            "summary_json": "summary.json",
            "metrics_json": "metrics.json",
            "log_txt": "log.txt"
        }

        # Generate tags
        tags = []
        if record.stable:
            tags.append("stable")
        if record.is_novel:
            tags.append("novel")
        if score > 80:
            tags.append("high_priority")
        if health == "converged":
            tags.append("converged")

        # Create card
        card = RunCard(
            run_id=run_id,
            title=f"{record.formula}@{domain.replace('@', '')}",
            formula=record.formula,
            domain=domain,
            size=len(record.formula),  # Approximate
            model="LJ+Coulomb",  # TODO: Detect from simulation
            score=score,
            health=health,
            timestamp=record.timestamp,
            metrics=metrics,
            validation=validation_dict,
            paths=paths,
            tags=tags,
            generation_method=record.generation_method
        )

        # Save card as summary.json (with score_breakdown)
        summary_file = run_dir / "summary.json"
        card_dict = asdict(card)
        card_dict["score_breakdown"] = score_breakdown  # Add breakdown
        with open(summary_file, 'w') as f:
            json.dump(card_dict, f, indent=2)

        # Save detailed metrics (also include breakdown)
        metrics_file = run_dir / "metrics.json"
        metrics_with_breakdown = {**metrics, "score_breakdown": score_breakdown}
        with open(metrics_file, 'w') as f:
            json.dump(metrics_with_breakdown, f, indent=2)

        # Add to cards index (with breakdown)
        card_with_breakdown = asdict(card)
        card_with_breakdown["score_breakdown"] = score_breakdown
        self.cards.append(card_with_breakdown)
        self._save_cards_index()

        return card
    
    def generate_report(self) -> str:
        """Generate human-readable discovery report"""
        report = []
        report.append("═══════════════════════════════════════════════════════════")
        report.append("  VSEPR DISCOVERY CATALOG")
        report.append("═══════════════════════════════════════════════════════════")
        report.append(f"\nTotal discoveries: {len(self.catalog)}")
        
        # Statistics
        known_count = sum(1 for r in self.catalog if not r['is_novel'])
        novel_count = sum(1 for r in self.catalog if r['is_novel'])
        stable_count = sum(1 for r in self.catalog if r['stable'])
        
        report.append(f"Known molecules:   {known_count}")
        report.append(f"Novel candidates:  {novel_count}")
        report.append(f"Stable molecules:  {stable_count}")
        
        # List novel discoveries
        if novel_count > 0:
            report.append("\n🎉 NOVEL DISCOVERIES:")
            for record in self.catalog:
                if record['is_novel']:
                    report.append(f"\n  {record['formula']}")
                    report.append(f"    Energy: {record['energy']:.2f} kcal/mol")
                    report.append(f"    Method: {record['generation_method']}")
                    report.append(f"    Notes: {record['notes']}")
        
        # List known molecules
        if known_count > 0:
            report.append("\n✓ KNOWN MOLECULES:")
            for record in self.catalog:
                if not record['is_novel'] and record['stable']:
                    report.append(f"  {record['formula']}: {record['notes']}")
        
        return '\n'.join(report)

def main():
    """Run discovery pipeline on test molecules"""
    pipeline = DiscoveryPipeline()
    
    # Test with existing XYZ files
    test_files = [
        "test_water.xyz",
        "test_methane.xyz",
        "test_ar3.xyz"
    ]
    
    for xyz_file in test_files:
        if Path(xyz_file).exists():
            record = pipeline.run_discovery_workflow(xyz_file, method="test")
    
    # Generate report
    print("\n" + pipeline.generate_report())
    
    # Save report
    report_file = pipeline.output_dir / "discovery_report.txt"
    with open(report_file, 'w') as f:
        f.write(pipeline.generate_report())
    
    print(f"\n✓ Report saved: {report_file}")

if __name__ == "__main__":
    main()
