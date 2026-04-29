# scripts/ -- Shell Interaction Pathways

This directory is the operational nerve-centre of VSEPR-Sim.
Everything you do day-to-day starts here.

---

## Primary entry point

Activate the project venv first (once per shell session):

    .venv\Scripts\Activate.ps1

Then launch the interactive docs/reports shell:

    python scripts\doc_shell.py

The shell opens a REPL. Type help to see all commands.

---

## doc_shell.py -- command quick-reference

Navigation
  ls [path]          list files in current or named directory
  tree [path]        recursive tree view
  cd <path>          change working directory
  pwd                show current directory

Viewing
  cat <file>         print full file
  head <file> [n]    first n lines (default 20)
  tail <file> [n]    last n lines  (default 20)
  info <file>        size, date, word count

Editing
  edit <file>        open in VS Code / EDITOR
  new  <file>        create a blank file and open it
  append <file>      append lines interactively

Search
  find <pattern>     find files matching glob pattern
  grep <pat> [dir]   search file contents
  recent [n]         n most-recently modified files

LaTeX workflow
  compile <file.tex> pdflatex compile (in its directory)
  clean   [dir]      remove .aux .log .out files
  texdiff <a> <b>    latexdiff two .tex files

Report workflow
  run <script.py>    execute a Python report script
  status             git status + recent outputs summary
  validate [dir]     check LaTeX / Markdown for common errors

File ops
  cp / mv / rm       copy, move, delete (with confirmation)
  diff <a> <b>       unified diff

Export
  export <file>      copy file to out/
  zip [name]         zip docs/ + out/ into a dated archive

Shell escape
  !<cmd>             run any shell command directly

---

## Key automation scripts

  run_report.sh           Run the full report batch (bash)
  build_all.sh            Full CMake build from clean
  build_and_package.ps1   Build + installer packaging (Windows)
  continual_launcher.ps1  Start the FastAPI continual reaction stream
  continual_orchestrator.py  Python orchestration wrapper
  demo_discovery.ps1      Run all visual demos in sequence
  walkaway.ps1            Unattended overnight batch run
  env_check.sh            Verify build dependencies

---

## Demos (scripts/demos/)

    python scripts\demos\demo_helium_room_3d.py
    python scripts\demos\demo_calibration_3d.py

---

## Typical daily workflow

    1. .venv\Scripts\Activate.ps1
    2. python scripts\doc_shell.py
       Inside the shell:
         run reporting/generate_report.py
         compile docs/ALPHA_MODEL_BOOKLET.tex
         export docs/ALPHA_MODEL_BOOKLET.pdf
         zip
