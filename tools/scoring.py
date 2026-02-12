#!/usr/bin/env python3
"""
Scoring Module - Explicit Multi-Factor Weighting
NO guessing, deterministic scoring with breakdown
"""

import math
from typing import Dict, Tuple, List
from dataclasses import dataclass

@dataclass
class ScoreBreakdown:
    """Transparent score breakdown"""
    wN: float      # Size preference (dual Gaussian)
    wQ: float      # Charge neutrality
    wM: float      # Metal richness
    wD: float      # Element diversity
    wS: float      # Stability gate
    wC: float      # Classification bonus
    cost: float    # Computational cost
    value: float   # Scientific value (V)
    priority: float  # Final priority score (S)
    classifications: List[str]  # Applied classification labels

class MolecularScorer:
    """
    Multi-factor scoring with transparent breakdown

    Score = V / (1 + λC)
    where V = w_N · w_Q · w_M · w_D · w_S · w_C

    NEW: w_C = classification bonus
    """

    def __init__(self,
                 # Size preference (dual Gaussian)
                 mu1: float = 8.0,    # Small molecules peak
                 mu2: float = 56.0,   # Exploratory bonding stage peak
                 sigma1: float = 3.0,
                 sigma2: float = 12.0,
                 a: float = 1.0,      # Weight for peak 1
                 b: float = 0.8,      # Weight for peak 2

                 # Charge neutrality
                 k_charge: float = 2.0,  # Penalty steepness

                 # Metal richness
                 alpha_metal: float = 0.3,  # Reward coefficient

                 # Element diversity
                 beta_diversity: float = 0.5,

                 # Cost weighting
                 lambda_cost: float = 0.01,

                 # Classification enabled
                 use_classification: bool = True):

        self.mu1 = mu1
        self.mu2 = mu2
        self.sigma1 = sigma1
        self.sigma2 = sigma2
        self.a = a
        self.b = b
        self.k_charge = k_charge
        self.alpha_metal = alpha_metal
        self.beta_diversity = beta_diversity
        self.lambda_cost = lambda_cost
        self.use_classification = use_classification
        
        # Metal elements (atomic numbers)
        self.metals = {
            3, 4,  # Li, Be
            11, 12, 13,  # Na, Mg, Al
            19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,  # K-Ga
            37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  # Rb-In
            55, 56, 57, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,  # Cs-Tl
            87, 88, 89  # Fr, Ra, Ac
        }
    
    def compute_wN(self, N: int) -> float:
        """
        Size preference using dual Gaussian (two-spike curve)
        
        w_N = a·exp(-(N-μ₁)²/(2σ₁²)) + b·exp(-(N-μ₂)²/(2σ₂²))
        
        Peaks at:
        - μ₁ ≈ 8 (small molecules)
        - μ₂ ≈ 56 (exploratory bonding stage)
        """
        term1 = self.a * math.exp(-((N - self.mu1)**2) / (2 * self.sigma1**2))
        term2 = self.b * math.exp(-((N - self.mu2)**2) / (2 * self.sigma2**2))
        return term1 + term2
    
    def compute_wQ(self, total_charge: float) -> float:
        """
        Charge neutrality penalty
        
        w_Q = exp(-k·|Σq_i|)
        
        Bulk/crystal must be near neutral
        """
        return math.exp(-self.k_charge * abs(total_charge))
    
    def compute_wM(self, element_counts: Dict[str, int]) -> float:
        """
        Metal richness reward
        
        w_M = 1 + α·(N_metal / N)
        
        Rewards metal-rich compositions
        """
        # Count metals (need to map element symbols to atomic numbers)
        # For now, use simple heuristic
        metal_symbols = {'Li', 'Be', 'Na', 'Mg', 'Al', 'K', 'Ca', 'Sc', 'Ti', 
                        'V', 'Cr', 'Mn', 'Fe', 'Co', 'Ni', 'Cu', 'Zn', 'Ga',
                        'Rb', 'Sr', 'Y', 'Zr', 'Nb', 'Mo', 'Tc', 'Ru', 'Rh',
                        'Pd', 'Ag', 'Cd', 'In', 'Cs', 'Ba', 'La', 'Hf', 'Ta',
                        'W', 'Re', 'Os', 'Ir', 'Pt', 'Au', 'Hg', 'Tl', 'Fr',
                        'Ra', 'Ac'}
        
        N_total = sum(element_counts.values())
        if N_total == 0:
            return 1.0
        
        N_metal = sum(count for elem, count in element_counts.items() 
                     if elem in metal_symbols)
        
        return 1.0 + self.alpha_metal * (N_metal / N_total)
    
    def compute_wD(self, element_counts: Dict[str, int]) -> float:
        """
        Element diversity reward
        
        w_D = 1 + β·(#unique_elements / log(1+N))
        
        Avoids trivial duplicates, rewards interesting compositions
        """
        N = sum(element_counts.values())
        unique = len(element_counts)
        
        if N <= 1:
            return 1.0
        
        return 1.0 + self.beta_diversity * (unique / math.log(1 + N))
    
    def compute_wC(self, element_counts: Dict[str, int]) -> Tuple[float, List[str]]:
        """
        Classification bonus

        w_C = 1 + Σ(classification bonuses)

        Returns:
            (w_C, list_of_labels)
        """
        if not self.use_classification:
            return 1.0, []

        # Import here to avoid circular dependency
        from classification import classify_molecule

        classifications = classify_molecule(element_counts)

        if not classifications:
            return 1.0, []

        # Sum bonuses (can exceed 1.0 for multiple classifications)
        total_bonus = sum(c.bonus for c in classifications)
        labels = [c.label for c in classifications]

        return 1.0 + total_bonus, labels

    def compute_wS(self, health: str, converged: bool, bounded: bool) -> float:
        """
        Stability gate (categorical)
        
        w_S = 1.0   if bounded & converged
              0.3   if bounded but not converged
              0.05  if exploded
        """
        if health == "converged" or (bounded and converged):
            return 1.0
        elif health == "bounded" or bounded:
            return 0.3
        else:  # exploded or invalid
            return 0.05
    
    def compute_cost(self, N: int, has_long_range: bool = False) -> float:
        """
        Computational cost estimate
        
        C = N² (if long-range) else N^1.5
        
        Normalized to [0, 1] range approximately
        """
        if has_long_range:
            raw_cost = N * N
        else:
            raw_cost = N ** 1.5
        
        # Normalize (assuming max N ~ 2000)
        return raw_cost / (2000 ** 1.5)
    
    def score(self,
              N: int,
              element_counts: Dict[str, int],
              total_charge: float,
              health: str,
              converged: bool,
              bounded: bool,
              has_long_range: bool = False) -> ScoreBreakdown:
        """
        Compute full score with breakdown
        
        Returns:
            ScoreBreakdown with all components
        """
        # Compute all weights
        wN = self.compute_wN(N)
        wQ = self.compute_wQ(total_charge)
        wM = self.compute_wM(element_counts)
        wD = self.compute_wD(element_counts)
        wC, class_labels = self.compute_wC(element_counts)
        wS = self.compute_wS(health, converged, bounded)

        # Scientific value
        V = wN * wQ * wM * wD * wC * wS

        # Computational cost
        C = self.compute_cost(N, has_long_range)

        # Priority score
        S = V / (1.0 + self.lambda_cost * C)

        # Normalize to 0-100
        S_normalized = min(100.0, S * 50.0)  # Scale factor tuned empirically

        return ScoreBreakdown(
            wN=wN,
            wQ=wQ,
            wM=wM,
            wD=wD,
            wS=wS,
            wC=wC,
            cost=C,
            value=V,
            priority=S_normalized,
            classifications=class_labels
        )

# Default scorer instance
default_scorer = MolecularScorer()

def score_molecule(N: int,
                   element_counts: Dict[str, int],
                   total_charge: float = 0.0,
                   health: str = "bounded",
                   converged: bool = False,
                   bounded: bool = True,
                   has_long_range: bool = False) -> Tuple[float, Dict]:
    """
    Convenience function: score molecule and return (priority, breakdown_dict)
    """
    breakdown = default_scorer.score(
        N, element_counts, total_charge, health, converged, bounded, has_long_range
    )
    
    breakdown_dict = {
        "wN": round(breakdown.wN, 3),
        "wQ": round(breakdown.wQ, 3),
        "wM": round(breakdown.wM, 3),
        "wD": round(breakdown.wD, 3),
        "wS": round(breakdown.wS, 3),
        "wC": round(breakdown.wC, 3),
        "cost": round(breakdown.cost, 3),
        "value": round(breakdown.value, 3),
        "classifications": breakdown.classifications
    }

    return breakdown.priority, breakdown_dict

# Example usage
if __name__ == "__main__":
    # Test cases
    test_cases = [
        # (N, element_counts, charge, health, converged, bounded)
        (8, {"H": 2, "O": 1, "C": 5}, 0.0, "converged", True, True),  # Small organic
        (56, {"Na": 28, "Cl": 28}, 0.0, "converged", True, True),  # NaCl bulk
        (120, {"Al": 80, "O": 40}, 0.0, "bounded", False, True),  # Al2O3 cluster
        (3, {"Ar": 3}, 0.0, "converged", True, True),  # Ar3 cluster
        (200, {"Fe": 200}, 0.0, "exploded", False, False),  # Iron bulk (exploded)
    ]
    
    print("Molecular Scoring Test")
    print("=" * 80)
    
    for N, elem_counts, charge, health, conv, bound in test_cases:
        formula = "".join(f"{e}{c if c > 1 else ''}" for e, c in sorted(elem_counts.items()))
        
        score, breakdown = score_molecule(N, elem_counts, charge, health, conv, bound)
        
        print(f"\n{formula} (N={N}, {health}):")
        print(f"  Priority Score: {score:.1f}")
        print(f"  Breakdown:")
        for key, val in breakdown.items():
            print(f"    {key}: {val}")
