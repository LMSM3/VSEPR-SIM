#!/usr/bin/env python3
"""
Chemical Database Validator - VSEPR Discovery Pipeline
Validates generated molecules against known chemistry databases
"""

import requests
import json
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from pathlib import Path
import time

@dataclass
class MoleculeRecord:
    """Record of a molecule from database"""
    formula: str
    name: str
    cid: Optional[int] = None  # PubChem CID
    inchi: Optional[str] = None
    smiles: Optional[str] = None
    molecular_weight: Optional[float] = None
    properties: Dict = None
    
    def __post_init__(self):
        if self.properties is None:
            self.properties = {}

@dataclass
class ValidationResult:
    """Result of molecule validation"""
    is_known: bool
    confidence: float  # 0.0 to 1.0
    matches: List[MoleculeRecord]
    novel: bool  # True if potentially novel
    notes: str

class ChemicalDatabase:
    """Interface to chemical databases (PubChem, local cache)"""
    
    def __init__(self, cache_dir: str = "chemical_cache"):
        self.cache_dir = Path(cache_dir)
        self.cache_dir.mkdir(exist_ok=True)
        self.session = requests.Session()
        
        # PubChem API endpoints
        self.pubchem_base = "https://pubchem.ncbi.nlm.nih.gov/rest/pug"
        
        # Local cache
        self.cache = self._load_cache()
    
    def _load_cache(self) -> Dict:
        """Load local cache of known molecules"""
        cache_file = self.cache_dir / "molecule_cache.json"
        if cache_file.exists():
            with open(cache_file, 'r') as f:
                return json.load(f)
        return {}
    
    def _save_cache(self):
        """Save cache to disk"""
        cache_file = self.cache_dir / "molecule_cache.json"
        with open(cache_file, 'w') as f:
            json.dump(self.cache, f, indent=2)
    
    def lookup_by_formula(self, formula: str) -> List[MoleculeRecord]:
        """
        Lookup molecule by molecular formula
        Example: H2O, CH4, C6H6
        """
        # Check cache first
        cache_key = f"formula:{formula}"
        if cache_key in self.cache:
            print(f"  [CACHE] Found {formula} in local cache")
            return self._parse_cached_records(self.cache[cache_key])
        
        print(f"  [API] Querying PubChem for {formula}...")
        
        try:
            # PubChem API: search by formula
            url = f"{self.pubchem_base}/compound/formula/{formula}/JSON"
            response = self.session.get(url, timeout=10)
            
            if response.status_code == 200:
                data = response.json()
                records = self._parse_pubchem_response(data)
                
                # Cache results
                self.cache[cache_key] = data
                self._save_cache()
                
                return records
            elif response.status_code == 404:
                print(f"  [NOT FOUND] {formula} not in PubChem")
                return []
            else:
                print(f"  [ERROR] PubChem API returned {response.status_code}")
                return []
        except Exception as e:
            print(f"  [ERROR] API request failed: {e}")
            return []
    
    def _parse_pubchem_response(self, data: Dict) -> List[MoleculeRecord]:
        """Parse PubChem JSON response"""
        records = []
        
        if 'PC_Compounds' in data:
            for compound in data['PC_Compounds'][:5]:  # Limit to top 5
                cid = compound.get('id', {}).get('id', {}).get('cid')
                
                # Extract properties
                props = compound.get('props', [])
                formula = None
                name = None
                mw = None
                smiles = None
                inchi = None
                
                for prop in props:
                    label = prop.get('urn', {}).get('label', '')
                    
                    if label == 'Molecular Formula':
                        formula = prop['value']['sval']
                    elif label == 'IUPAC Name':
                        name = prop['value']['sval']
                    elif label == 'Molecular Weight':
                        mw = float(prop['value']['fval'])
                    elif label == 'SMILES':
                        smiles = prop['value']['sval']
                    elif label == 'InChI':
                        inchi = prop['value']['sval']
                
                record = MoleculeRecord(
                    formula=formula or "Unknown",
                    name=name or f"CID_{cid}",
                    cid=cid,
                    molecular_weight=mw,
                    smiles=smiles,
                    inchi=inchi
                )
                records.append(record)
        
        return records
    
    def _parse_cached_records(self, cached_data: Dict) -> List[MoleculeRecord]:
        """Parse cached PubChem data"""
        return self._parse_pubchem_response(cached_data)

class MoleculeValidator:
    """Validates generated molecules against known chemistry"""
    
    def __init__(self, database: ChemicalDatabase):
        self.db = database
    
    def validate_molecule(self, formula: str, energy: float, stability: str) -> ValidationResult:
        """
        Validate a molecule from simulation
        
        Args:
            formula: Molecular formula (e.g., "H2O")
            energy: Final energy from simulation (kcal/mol)
            stability: Stability assessment ("stable", "unstable", "metastable")
        
        Returns:
            ValidationResult with database matches and novelty assessment
        """
        print(f"\n=== Validating Molecule: {formula} ===")
        print(f"  Energy: {energy:.2f} kcal/mol")
        print(f"  Stability: {stability}")
        
        # Lookup in database
        matches = self.db.lookup_by_formula(formula)
        
        # Determine if known or novel
        is_known = len(matches) > 0
        confidence = 1.0 if is_known else 0.0
        
        # Assess novelty
        novel = False
        notes = ""
        
        if not is_known:
            if stability == "stable":
                novel = True
                notes = "Potentially novel stable molecule! No database match found."
            else:
                notes = "Unknown molecule, but unstable in simulation."
        else:
            # Check if properties match known chemistry
            known_stable = any(m.name.lower() not in ["unstable", "reactive"] for m in matches)
            
            if stability == "stable" and known_stable:
                notes = f"Matches known stable molecule: {matches[0].name}"
            elif stability == "unstable" and known_stable:
                notes = f"Simulation predicts unstable, but {matches[0].name} is known stable (check parameters!)"
                confidence = 0.5
            else:
                notes = f"Matches database: {matches[0].name}"
        
        return ValidationResult(
            is_known=is_known,
            confidence=confidence,
            matches=matches,
            novel=novel,
            notes=notes
        )

def main():
    """Example usage"""
    print("═══════════════════════════════════════════════════════════")
    print("  CHEMICAL DATABASE VALIDATOR")
    print("═══════════════════════════════════════════════════════════")
    
    # Initialize database
    db = ChemicalDatabase(cache_dir="out/chemical_cache")
    validator = MoleculeValidator(db)
    
    # Test cases
    test_molecules = [
        ("H2O", -237.2, "stable"),      # Water (known)
        ("CH4", -17.9, "stable"),       # Methane (known)
        ("C60", -1200.0, "stable"),     # Buckminsterfullerene (known)
        ("Ar3", -0.71, "stable"),       # Argon trimer (novel?)
        ("H3", -50.0, "unstable"),      # Trihydrogen (unstable)
    ]
    
    results = []
    for formula, energy, stability in test_molecules:
        result = validator.validate_molecule(formula, energy, stability)
        results.append((formula, result))
        
        print(f"\n  Result:")
        print(f"    Known: {result.is_known}")
        print(f"    Confidence: {result.confidence:.1%}")
        print(f"    Novel: {result.novel}")
        print(f"    Notes: {result.notes}")
        
        if result.matches:
            print(f"    Matches:")
            for match in result.matches[:3]:
                print(f"      - {match.name} (CID: {match.cid})")
        
        time.sleep(1)  # Rate limiting
    
    # Summary
    print("\n═══════════════════════════════════════════════════════════")
    print("  SUMMARY")
    print("═══════════════════════════════════════════════════════════")
    
    known_count = sum(1 for _, r in results if r.is_known)
    novel_count = sum(1 for _, r in results if r.novel)
    
    print(f"\n  Molecules tested: {len(results)}")
    print(f"  Known molecules:  {known_count}")
    print(f"  Novel candidates: {novel_count}")
    
    if novel_count > 0:
        print(f"\n  🎉 NOVEL MOLECULES DISCOVERED:")
        for formula, result in results:
            if result.novel:
                print(f"    - {formula}: {result.notes}")

if __name__ == "__main__":
    main()
