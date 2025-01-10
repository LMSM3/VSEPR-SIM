# Compiling the Polarization Theory Document

## Quick Start

To generate the PDF from the LaTeX source:

```bash
cd docs
pdflatex section_polarization_models.tex
pdflatex section_polarization_models.tex  # Run twice for references
```

## Requirements

- **LaTeX Distribution:**
  - Windows: MiKTeX or TeX Live
  - macOS: MacTeX
  - Linux: TeX Live

- **Required Packages:** (usually included)
  - `amsmath`, `amssymb`, `amsthm`
  - `graphicx`
  - `hyperref`
  - `booktabs`
  - `cite`

## Document Contents

**Section Polarization Models** (7 pages + 3 figure placeholders)

1. **Introduction** (1 page)
   - Motivation: why polarization matters
   - Scope: three approaches evaluated

2. **Current Implementation** (1 page)
   - Fixed-charge Coulomb model
   - Limitations (10-50% errors)

3. **Method 1: Drude Oscillators** (1.5 pages)
   - Theory: explicit charged particles
   - Integration schemes (massive vs. massless)
   - Evaluation: ❌ Rejected (stiff dynamics)

4. **Method 2: Composite Atomic Structures** (1 page)
   - Theory: orbital rings/shells
   - Challenges: 300+ parameters, constraint drift
   - Evaluation: ❌ Rejected (computational catastrophe)

5. **Method 3: SCF Induced Dipoles** (2 pages)
   - Theory: self-consistent field iteration
   - Thole damping function
   - Energy and forces
   - Evaluation: ✅ Selected (optimal choice)

6. **Comparative Analysis** (0.5 page)
   - Decision matrix table
   - Head-to-head comparison

7. **Implementation Roadmap** (1 page)
   - 6-phase plan (7-12 weeks)
   - Validation benchmarks
   - Parameter database

## Figure Placeholders

The document reserves space for three figures:

1. **Figure 1:** Drude oscillator schematic
   - Core atom, Drude particle, spring, induced dipole vector
   
2. **Figure 2:** Composite atomic structure
   - Nucleon core, orbital rings, spring network
   
3. **Figure 3:** SCF iteration flowchart
   - Permanent field → SCF loop → Convergence → Forces

**Note:** Figures are currently placeholder boxes. Replace with actual diagrams before publication.

## Generating Figures (Optional)

### Using TikZ (LaTeX native)

Replace placeholder boxes with TikZ code:

```latex
\begin{figure}[h]
\centering
\begin{tikzpicture}
  % Draw Drude oscillator schematic
  \draw[fill=blue] (0,0) circle (0.3cm) node {Core};
  \draw[fill=red] (2,0) circle (0.2cm) node {Drude};
  \draw[<->] (0.3,0) -- (1.8,0) node[midway,above] {spring};
\end{tikzpicture}
\caption{Drude oscillator model}
\end{figure}
```

### Using External Graphics

1. Create diagrams in Inkscape/Illustrator
2. Export as PDF/PNG
3. Replace placeholder with `\includegraphics{filename.pdf}`

## Output

- **PDF:** `section_polarization_models.pdf`
- **Size:** ~7 pages (300-500 KB)
- **Quality:** Publication-ready

## Bibliography

The document includes 12 references in BibTeX format:
- Thole (1981) — Damping function
- Miller (1990) — Polarizabilities
- Applequist (1972) — Induced dipole theory
- Lamoureux & Roux (2003) — Drude model
- Ren & Ponder (2003) — AMOEBA force field
- And 7 more supporting references

## Integration with Main Methodology

This document can be included in the full methodology as:

```latex
% In main METHODOLOGY_12PAGE.tex
\section{Electronic Polarization}
\input{section_polarization_models}
```

Or distributed as a standalone supplement.

## Troubleshooting

**Missing packages?**
```bash
# Windows (MiKTeX)
mpm --install amsmath

# Linux (TeX Live)
sudo apt-get install texlive-latex-extra
```

**Bibliography not appearing?**
```bash
pdflatex section_polarization_models.tex
bibtex section_polarization_models
pdflatex section_polarization_models.tex
pdflatex section_polarization_models.tex
```

**Figure spacing issues?**
- Adjust `\vspace` in placeholder boxes
- Use `[H]` float specifier for exact placement

---

**Document Status:** Ready to compile  
**Last Updated:** January 18, 2025
