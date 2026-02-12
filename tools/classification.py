#!/usr/bin/env python3
"""
Molecular Classification Module - Pattern-Based Detection
Detects special molecular classes and applies scoring bonuses
"""

from typing import Dict, List, Set, Tuple
from dataclasses import dataclass

@dataclass
class Classification:
    """Molecular classification result"""
    label: str
    confidence: float  # 0.0 to 1.0
    bonus: float  # Scoring bonus
    reason: str  # Why this classification was applied

class MolecularClassifier:
    """
    Detects molecular classes from composition
    
    Classifications (priority order):
    1. Organometallic (+1.0 bonus) - HIGHEST IMPORTANCE
    2. Perfluorinated (+0.8) - 6+ fluorines
    3. Semiconductor (+0.7)
    4. Metallic superalloy (+0.6)
    5. Organic super acid (+0.5)
    6. Organic super base (+0.5)
    7. Explosive (+0.3) - flagged for caution
    """
    
    def __init__(self):
        # Metal elements
        self.metals = {
            'Li', 'Be', 'Na', 'Mg', 'Al', 'K', 'Ca', 'Sc', 'Ti', 'V', 'Cr',
            'Mn', 'Fe', 'Co', 'Ni', 'Cu', 'Zn', 'Ga', 'Rb', 'Sr', 'Y', 'Zr',
            'Nb', 'Mo', 'Tc', 'Ru', 'Rh', 'Pd', 'Ag', 'Cd', 'In', 'Sn', 'Sb',
            'Cs', 'Ba', 'La', 'Hf', 'Ta', 'W', 'Re', 'Os', 'Ir', 'Pt', 'Au',
            'Hg', 'Tl', 'Pb', 'Bi', 'Fr', 'Ra', 'Ac'
        }
        
        # Transition metals (subset)
        self.transition_metals = {
            'Sc', 'Ti', 'V', 'Cr', 'Mn', 'Fe', 'Co', 'Ni', 'Cu', 'Zn',
            'Y', 'Zr', 'Nb', 'Mo', 'Tc', 'Ru', 'Rh', 'Pd', 'Ag', 'Cd',
            'La', 'Hf', 'Ta', 'W', 'Re', 'Os', 'Ir', 'Pt', 'Au', 'Hg'
        }
        
        # Organic elements
        self.organic_elements = {'C', 'H', 'O', 'N', 'S', 'P'}
        
        # Semiconductor elements
        self.semiconductor_elements = {'Si', 'Ge', 'Ga', 'As', 'In', 'Sb', 'Te'}
        
        # Superalloy metals
        self.superalloy_metals = {
            'Ni', 'Co', 'Cr', 'W', 'Mo', 'Re', 'Ta', 'Nb', 'Ti', 'Al'
        }
        
        # Acid-forming elements
        self.acid_elements = {'S', 'P', 'N', 'Cl', 'Br', 'I'}
        
        # Base-forming elements
        self.base_elements = {'N'}
        
        # Explosive indicators
        self.explosive_indicators = {'N', 'O', 'Cl'}
    
    def classify(self, element_counts: Dict[str, int]) -> List[Classification]:
        """
        Classify molecule based on composition
        
        Returns:
            List of classifications (sorted by bonus, descending)
        """
        classifications = []
        elements = set(element_counts.keys())
        
        # 1. ORGANOMETALLIC (HIGHEST PRIORITY +1.0)
        organo = self._detect_organometallic(elements, element_counts)
        if organo:
            classifications.append(organo)
        
        # 2. PERFLUORINATED (+0.8)
        perfluoro = self._detect_perfluorinated(element_counts)
        if perfluoro:
            classifications.append(perfluoro)
        
        # 3. SEMICONDUCTOR (+0.7)
        semi = self._detect_semiconductor(elements, element_counts)
        if semi:
            classifications.append(semi)
        
        # 4. METALLIC SUPERALLOY (+0.6)
        superalloy = self._detect_superalloy(elements, element_counts)
        if superalloy:
            classifications.append(superalloy)
        
        # 5. ORGANIC SUPER ACID (+0.5)
        acid = self._detect_super_acid(elements, element_counts)
        if acid:
            classifications.append(acid)
        
        # 6. ORGANIC SUPER BASE (+0.5)
        base = self._detect_super_base(elements, element_counts)
        if base:
            classifications.append(base)
        
        # 7. EXPLOSIVE (+0.3) - flagged for caution
        explosive = self._detect_explosive(elements, element_counts)
        if explosive:
            classifications.append(explosive)
        
        # Sort by bonus (descending)
        classifications.sort(key=lambda c: c.bonus, reverse=True)
        
        return classifications
    
    def _detect_organometallic(self, elements: Set[str], 
                              counts: Dict[str, int]) -> Classification:
        """
        Organometallic: Contains both organic elements (C, H) and metals
        
        Examples:
        - Grignard reagents (RMgX)
        - Ferrocene (C₁₀H₁₀Fe)
        - Organolithium (RLi)
        """
        has_carbon = 'C' in elements
        has_hydrogen = 'H' in elements
        has_metal = bool(elements & self.metals)
        
        if has_carbon and has_metal:
            metal_list = sorted(elements & self.metals)
            return Classification(
                label="organometallic",
                confidence=1.0,
                bonus=1.0,
                reason=f"Contains C + metals ({', '.join(metal_list)})"
            )
        return None
    
    def _detect_perfluorinated(self, counts: Dict[str, int]) -> Classification:
        """
        Perfluorinated: 6 or more fluorine atoms
        
        Examples:
        - Teflon (CF₂)ₙ
        - Perfluorooctanoic acid (C₈HF₁₅O₂)
        """
        F_count = counts.get('F', 0)
        
        if F_count >= 6:
            return Classification(
                label="perfluorinated",
                confidence=1.0,
                bonus=0.8,
                reason=f"Contains {F_count} fluorine atoms"
            )
        return None
    
    def _detect_semiconductor(self, elements: Set[str], 
                             counts: Dict[str, int]) -> Classification:
        """
        Semiconductor: Contains Si, Ge, or III-V/II-VI compounds
        
        Examples:
        - GaAs (Gallium Arsenide)
        - InP (Indium Phosphide)
        - SiGe alloys
        """
        semi_present = elements & self.semiconductor_elements
        
        if semi_present:
            # Check for compound semiconductors (III-V, II-VI)
            if ('Ga' in elements and 'As' in elements) or \
               ('In' in elements and 'P' in elements) or \
               ('Cd' in elements and 'Te' in elements):
                return Classification(
                    label="semiconductor",
                    confidence=1.0,
                    bonus=0.7,
                    reason=f"Compound semiconductor ({', '.join(sorted(semi_present))})"
                )
            
            # Elemental semiconductor
            if 'Si' in elements or 'Ge' in elements:
                return Classification(
                    label="semiconductor",
                    confidence=0.9,
                    bonus=0.7,
                    reason=f"Elemental semiconductor ({', '.join(sorted(semi_present))})"
                )
        
        return None
    
    def _detect_superalloy(self, elements: Set[str], 
                          counts: Dict[str, int]) -> Classification:
        """
        Metallic superalloy: 3+ metals, primarily from superalloy set
        
        Examples:
        - Inconel (Ni-Cr-Fe)
        - Hastelloy (Ni-Mo-Cr-W)
        """
        metals_present = elements & self.metals
        superalloy_present = elements & self.superalloy_metals
        
        if len(metals_present) >= 3 and len(superalloy_present) >= 2:
            return Classification(
                label="metallic_superalloy",
                confidence=0.8,
                bonus=0.6,
                reason=f"Multi-metal alloy ({', '.join(sorted(superalloy_present))})"
            )
        
        return None
    
    def _detect_super_acid(self, elements: Set[str], 
                          counts: Dict[str, int]) -> Classification:
        """
        Organic super acid: Contains C, H, and strong acid elements (S, P, N, halogens)
        
        Examples:
        - Fluorosulfuric acid (HSO₃F)
        - Triflic acid (CF₃SO₃H)
        """
        has_carbon = 'C' in elements
        has_hydrogen = 'H' in elements
        acid_elements = elements & self.acid_elements
        
        if has_carbon and has_hydrogen and acid_elements:
            # Check for strong acid indicators
            if 'F' in elements or 'S' in elements:
                return Classification(
                    label="organic_super_acid",
                    confidence=0.7,
                    bonus=0.5,
                    reason=f"Organic + acid elements ({', '.join(sorted(acid_elements))})"
                )
        
        return None
    
    def _detect_super_base(self, elements: Set[str], 
                          counts: Dict[str, int]) -> Classification:
        """
        Organic super base: Contains C, H, N, and possibly metals
        
        Examples:
        - Lithium diisopropylamide (LDA)
        - DBU (1,8-Diazabicycloundec-7-ene)
        """
        has_carbon = 'C' in elements
        has_nitrogen = 'N' in elements
        has_metal = bool(elements & self.metals)
        
        N_count = counts.get('N', 0)
        
        if has_carbon and has_nitrogen:
            if N_count >= 2 or has_metal:
                return Classification(
                    label="organic_super_base",
                    confidence=0.7,
                    bonus=0.5,
                    reason=f"Organic + N (count={N_count})" + (", metal" if has_metal else "")
                )
        
        return None
    
    def _detect_explosive(self, elements: Set[str], 
                         counts: Dict[str, int]) -> Classification:
        """
        Explosive: High N and O content, or specific patterns
        
        Examples:
        - TNT (C₇H₅N₃O₆)
        - RDX (C₃H₆N₆O₆)
        - PETN (C₅H₈N₄O₁₂)
        """
        N_count = counts.get('N', 0)
        O_count = counts.get('O', 0)
        
        # High nitrogen + oxygen content
        if N_count >= 3 and O_count >= 3:
            N_O_ratio = N_count / max(1, O_count)
            
            # Typical explosive ratios
            if 0.3 <= N_O_ratio <= 1.5:
                return Classification(
                    label="explosive",
                    confidence=0.6,
                    bonus=0.3,
                    reason=f"High N+O content (N={N_count}, O={O_count})"
                )
        
        # Perchlorates (Cl + O)
        if 'Cl' in elements and O_count >= 4:
            return Classification(
                label="explosive",
                confidence=0.5,
                bonus=0.3,
                reason=f"Perchlorate pattern (Cl + O={O_count})"
            )
        
        return None

# Default classifier instance
default_classifier = MolecularClassifier()

def classify_molecule(element_counts: Dict[str, int]) -> List[Classification]:
    """
    Convenience function: classify molecule
    
    Returns:
        List of classifications (sorted by bonus, descending)
    """
    return default_classifier.classify(element_counts)

# Example usage
if __name__ == "__main__":
    test_cases = [
        ("Ferrocene", {"C": 10, "H": 10, "Fe": 1}),
        ("Grignard", {"C": 4, "H": 9, "Mg": 1, "Br": 1}),
        ("Teflon unit", {"C": 2, "F": 4}),
        ("Perfluorooctane", {"C": 8, "F": 18}),
        ("GaAs", {"Ga": 1, "As": 1}),
        ("Inconel", {"Ni": 60, "Cr": 20, "Fe": 20}),
        ("Triflic acid", {"C": 1, "H": 1, "F": 3, "S": 1, "O": 3}),
        ("TNT", {"C": 7, "H": 5, "N": 3, "O": 6}),
        ("Water", {"H": 2, "O": 1}),
    ]
    
    print("Molecular Classification Test")
    print("=" * 80)
    
    for name, elem_counts in test_cases:
        formula = "".join(f"{e}{c if c > 1 else ''}" for e, c in sorted(elem_counts.items()))
        
        classes = classify_molecule(elem_counts)
        
        print(f"\n{name} ({formula}):")
        if classes:
            for cls in classes:
                print(f"  [{cls.label}] +{cls.bonus:.1f} (conf={cls.confidence:.1f})")
                print(f"    Reason: {cls.reason}")
        else:
            print("  (no special classification)")
