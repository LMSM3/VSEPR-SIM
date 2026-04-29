# thenewmethods/

Three standalone LaTeX documents capturing compact, invertible,
system-grouped notation from the VSEPR-SIM framework.

---

## Documents

| File | Contents |
|------|---------|
| `crystal_density_vacancy_diffraction.tex` | Crystal structure (FCC/BCC/HCP), density, vacancy, Bragg diffraction, ionic character, structure classification objective |
| `matrix_mechanics.tex` | Matrix force/moment/torsion systems, stiffness method, least-squares, Z-bead extension, unified block form |
| `stepcoad_iteration_framework.tex` | STEP + COAD iterative update framework: bead state, reduced scalars, Tables A/B/C (structural / thermal / chemical), worked example |

---

## Compile

```powershell
cd thenewmethods

# compile all three (requires pdflatex on PATH)
pdflatex crystal_density_vacancy_diffraction.tex
pdflatex matrix_mechanics.tex
pdflatex stepcoad_iteration_framework.tex
```

Or from inside `doc_shell.py` (project root):

```
compile thenewmethods\crystal_density_vacancy_diffraction.tex
compile thenewmethods\matrix_mechanics.tex
compile thenewmethods\stepcoad_iteration_framework.tex
```

---

## Design principles

- Every equation is invertible where possible
- Systems are grouped with brace notation
- Tables are report-ready, not explanatory prose
- Nothing is written like a textbook author was paid per symbol
