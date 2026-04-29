# reporting/ -- Automatic Report Generation

This directory contains the Python-driven report pipeline that converts
simulation outputs into structured Markdown and LaTeX documents.

---

## Files

  generate_report.py          Main entry point -- writes report.md + report.tex
  generate_layering_report.py Specialised layering analysis report
  report.md                   Generated Markdown output (human-readable)
  report.tex                  Generated LaTeX output (compile to PDF)
  layering_data.tex           Data fragment injected into the LaTeX report

---

## Running the report generator

From the project root with the venv active:

    python reporting\generate_report.py

The script:
  1. Imports results from out/ and reports/
  2. Runs summary statistics via pykernel/thermo_pipe.py
  3. Writes reporting/report.md and reporting/report.tex

Compile the LaTeX output to PDF:

    Option A -- from inside doc_shell.py
      run reporting\generate_report.py
      compile reportingeport.tex

    Option B -- direct
      python reporting\generate_report.py
      pdflatex reportingeport.tex

---

## Layering report

    python reporting\generate_layering_report.py

Produces reporting/layering_report.tex from layering_data.tex.

---

## C++ report engine (headless / build-time)

The CMake target vsepr_report (src/core/report_engine.cpp) runs autonomously
during simulation and writes structured CSV + JSON to out/.

    # Standard report
    .uild\Releaseeport_generator.exe --output outun_001

    # Bio/biochemical variant
    .uild\Releaseio_report_generator.exe --output outio_001

---

## Pipeline integration

    pykernel/pipe.py           typed stream records
    pykernel/thermo_pipe.py    batch thermal runner -> CSVSink / JSONSink
    reporting/generate_*.py    reads outputs, renders Markdown and LaTeX
    docs/                      final compiled PDFs land here after review

---

## Output directories

  reporting/   Generated .md and .tex source
  out/         Final PDFs, CSVs, PNGs from all generators
  reports/     Per-run simulation summaries
