#!/usr/bin/env python3
"""
Uniform Scoring Module - For Baseline Data Collection
All molecules get uniform score (no weighting bias)
"""

from typing import Dict, Tuple, List

def score_molecule_uniform(N: int,
                          element_counts: Dict[str, int],
                          total_charge: float = 0.0,
                          health: str = "bounded",
                          converged: bool = False,
                          bounded: bool = True,
                          has_long_range: bool = False) -> Tuple[float, Dict]:
    """
    Uniform scoring for baseline collection
    
    All molecules get score = 50.0
    All weights = 1.0 (neutral)
    
    This creates unbiased baseline data for later analysis
    """
    breakdown_dict = {
        "wN": 1.0,      # No size bias
        "wQ": 1.0,      # No charge bias
        "wM": 1.0,      # No metal bias
        "wD": 1.0,      # No diversity bias
        "wS": 1.0,      # No stability bias
        "wC": 1.0,      # No classification bias
        "cost": 0.0,    # No cost penalty
        "value": 1.0,   # Uniform value
        "classifications": []  # No classifications applied
    }
    
    # Everyone gets 50.0 (middle of 0-100 scale)
    return 50.0, breakdown_dict

# Backwards compatibility
score_molecule = score_molecule_uniform
