#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
doc_shell.py -- Interactive Shell for docs/ and out/ Maintenance
================================================================

A readline-enabled REPL for browsing, editing, compiling, and
managing files in the VSEPR-SIM docs/ and out/ directories.

ROOTS managed:
  docs/          LaTeX sources, Markdown, PDFs, notebooks
  out/           Generated outputs (reports, CSVs, plots, logs)
  reports/       Simulation report outputs (BIO-*, TMS-*, nuclear)
  reporting/     Python report-generation scripts

Commands  (type  help <cmd>  for full detail)
-----------------------------------------------------------------
NAVIGATION
  ls [path]        List files in current or given directory
  tree [path]      Recursive tree view with file sizes
  cd <path>        Change working directory (inside a managed root)
  pwd              Print current directory

VIEWING
  cat  <file>      Print full file content
  head <file> [n]  Print first n lines (default 30)
  tail <file> [n]  Print last  n lines (default 30)
  info <file>      File metadata: size, modified, line count, type

EDITING
  edit  <file>     Open file in $EDITOR / VS Code / notepad
  new   <file>     Create new file from template (md/tex/py/csv)
  append <file>    Append text to file interactively

SEARCH
  find  <pattern>  Find files by name pattern (glob) in all roots
  grep  <text>     Search for text content across managed files
  recent [n]       Show n most recently modified files (default 20)

LATEX
  compile <file.tex>   Run pdflatex (twice for refs) on a .tex file
  clean   [path]       Remove LaTeX aux/log/out/.toc/.synctex.gz files
  texdiff <a> <b>      Side-by-side line diff of two .tex files

REPORTS
  run    <script.py>   Execute a Python report script
  status               Counts, sizes, newest file per root directory
  validate             Check for broken references / orphaned files

FILE OPS  (all destructive ops ask for confirmation)
  cp  <src> <dst>      Copy file
  mv  <src> <dst>      Move / rename file
  rm  <file>           Delete file (with confirmation)
  diff <a> <b>         Unified diff of two text files

OPEN
  open  <file>         Open with OS default viewer (PDF, PNG, etc.)
  vscode <file>        Open file in VS Code

EXPORT
  export <src> <dst>   Copy file/directory to external destination
  zip    <name>        Zip the entire docs/ tree to out/exports/

SHELL
  !<command>           Run a raw shell command
  q / quit / exit      Exit the shell

VSEPR-SIM 4.0.4 — doc_shell
"""

from __future__ import annotations

import io
import sys

# Force UTF-8 stdout so box/arrow chars survive Windows cp1252 consoles
if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

import cmd
import csv
import difflib
import fnmatch
import glob
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import textwrap
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple


# ================================================================
# Lazy workspace_manager loader
# ================================================================

_wsm: Optional[Any] = None  # module handle, loaded on first wsm use

def _load_wsm() -> Any:
    global _wsm
    if _wsm is None:
        import importlib.util, pathlib as _pl
        _spec = importlib.util.spec_from_file_location(
            "workspace_manager",
            _pl.Path(__file__).parent / "workspace_manager.py",
        )
        _wsm = importlib.util.module_from_spec(_spec)
        sys.modules["workspace_manager"] = _wsm
        _spec.loader.exec_module(_wsm)
    return _wsm

_WSM_SUBCMDS: Dict[str, str] = {
    "status":          "Scan and report workspace pollution",
    "clean":           "Remove pycache + root temp files (safe)",
    "clean --dry-run": "Preview clean without deleting",
    "reset":           "Wipe all generated outputs  (3-step confirm)",
    "uninstall":       "Remove ALL artefacts         (4-step confirm)",
    "log-molecules":   "Scan and log all molecule/formula references",
    "rerun-molecules": "Replay computations from molecule recovery log",
}


# ================================================================
# Configuration
# ================================================================

WORKSPACE = Path(__file__).resolve().parent.parent

MANAGED_ROOTS = {
    "docs":      WORKSPACE / "docs",
    "out":       WORKSPACE / "out",
    "reports":   WORKSPACE / "reports",
    "reporting": WORKSPACE / "reporting",
}

ANSI = {
    "reset":  "\033[0m",
    "bold":   "\033[1m",
    "dim":    "\033[2m",
    "red":    "\033[31m",
    "green":  "\033[32m",
    "yellow": "\033[33m",
    "cyan":   "\033[36m",
    "blue":   "\033[34m",
    "magenta":"\033[35m",
    "white":  "\033[97m",
    "bg_dark":"\033[40m",
}
RST  = "\033[0m"
BOLD = "\033[1m"

_ANSI_ENABLED = sys.stdout.isatty() or os.environ.get("FORCE_COLOR")

def _c(name: str, text: str) -> str:
    if not _ANSI_ENABLED:
        return text
    return ANSI.get(name, "") + text + ANSI["reset"]

def _header(text: str, width: int = 72) -> str:
        line = "-" * width
        return "\n" + _c("cyan", line) + "\n  " + _c("bold", text) + "\n" + _c("cyan", line)

# File-type colour
EXT_COLOR = {
    ".tex": "yellow", ".md": "green", ".txt": "white",
    ".py":  "cyan",   ".csv": "blue", ".json": "blue",
    ".pdf": "magenta", ".png": "magenta", ".svg": "magenta",
    ".log": "dim",    ".aux": "dim",   ".out": "dim",
    ".jsonl": "blue", ".ipynb": "cyan",
}

def _file_color(path: Path) -> str:
    return EXT_COLOR.get(path.suffix.lower(), "white")


# ================================================================
# Path resolution helper
# ================================================================

def _resolve(cwd: Path, token: str) -> Path:
    """Resolve a user-supplied path token relative to cwd or workspace."""
    p = Path(token)
    if p.is_absolute():
        return p.resolve()
    candidate = (cwd / p).resolve()
    if candidate.exists():
        return candidate
    # Try relative to workspace
    candidate2 = (WORKSPACE / p).resolve()
    if candidate2.exists():
        return candidate2
    return candidate   # caller will check existence


def _ensure_in_roots(path: Path) -> bool:
    """Return True if path is inside any managed root."""
    for root in MANAGED_ROOTS.values():
        try:
            path.relative_to(root)
            return True
        except ValueError:
            pass
    return False


# ================================================================
# File utilities
# ================================================================

def _human(size: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024:
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} TB"

def _line_count(path: Path) -> int:
    try:
        with path.open("rb") as f:
            return sum(1 for _ in f)
    except Exception:
        return -1

def _is_text(path: Path) -> bool:
    text_exts = {".tex", ".md", ".txt", ".py", ".csv", ".json",
                 ".jsonl", ".log", ".aux", ".out", ".html",
                 ".rst", ".cfg", ".ini", ".toml", ".yaml", ".yml",
                 ".sh", ".ps1", ".bat", ".R", ".m", ".ipynb"}
    if path.suffix.lower() in text_exts:
        return True
    try:
        with path.open("rb") as f:
            return b"\x00" not in f.read(1024)
    except Exception:
        return False

def _read_lines(path: Path) -> List[str]:
    for enc in ("utf-8", "utf-8-sig", "latin-1"):
        try:
            with path.open(encoding=enc) as f:
                return f.readlines()
        except UnicodeDecodeError:
            continue
    return []

def _open_os(path: Path):
    """Open a file with the OS default application."""
    if platform.system() == "Windows":
        os.startfile(str(path))
    elif platform.system() == "Darwin":
        subprocess.run(["open", str(path)])
    else:
        subprocess.run(["xdg-open", str(path)])

def _get_editor() -> List[str]:
    """Return editor command as a list."""
    ed = os.environ.get("EDITOR", "").strip()
    if ed:
        return ed.split()
    # VS Code preferred, then notepad
    for prog in ("code", "notepad"):
        if shutil.which(prog):
            return [prog]
    return ["notepad"]


# ================================================================
# Template generator
# ================================================================

def _file_template(path: Path) -> str:
    ext = path.suffix.lower()
    name = path.stem
    date = datetime.now().strftime("%Y-%m-%d")
    if ext == ".md":
        return f"# {name}\n\nDate: {date}\n\n## Overview\n\n\n## Notes\n\n"
    if ext == ".tex":
        return (
            f"% {name}.tex -- VSEPR-SIM {date}\n"
            "\\documentclass[12pt]{article}\n"
            "\\usepackage{amsmath,amssymb,geometry,hyperref}\n"
            "\\geometry{margin=2.5cm}\n"
            f"\\title{{{name.replace('_', ' ').title()}}}\n"
            f"\\date{{{date}}}\n"
            "\\begin{document}\n\\maketitle\n\n% Content here\n\n\\end{document}\n"
        )
    if ext == ".py":
        return (
            f'"""\n{name}.py -- VSEPR-SIM {date}\n"""\n\n'
            "from __future__ import annotations\n\n\ndef main():\n    pass\n\n\n"
            'if __name__ == "__main__":\n    main()\n'
        )
    if ext == ".csv":
        return "# col1,col2,col3\n"
    return f"# {name}\n# Created: {date}\n"


# ================================================================
# LaTeX compilation
# ================================================================

def _pdflatex(tex_path: Path, n_passes: int = 2) -> Tuple[bool, str]:
    """Run pdflatex n_passes times. Returns (success, log_tail)."""
    if not shutil.which("pdflatex"):
        return False, "pdflatex not found in PATH"
    out_dir = tex_path.parent
    log_lines = []
    for i in range(n_passes):
        result = subprocess.run(
            ["pdflatex", "-interaction=nonstopmode",
             f"-output-directory={out_dir}", str(tex_path)],
            capture_output=True, text=True, cwd=str(out_dir),
        )
        log_lines.append(f"── Pass {i+1} ──")
        log_lines.extend(result.stdout.splitlines()[-20:])
        if result.returncode != 0 and i == n_passes - 1:
            return False, "\n".join(log_lines)
    return True, "\n".join(log_lines)

LATEX_JUNK = {".aux", ".log", ".out", ".toc", ".lof",
              ".lot", ".synctex.gz", ".bbl", ".blg", ".fls", ".fdb_latexmk"}

def _clean_latex(directory: Path) -> List[Path]:
    removed = []
    for p in directory.rglob("*"):
        if p.suffix in LATEX_JUNK and p.is_file():
            p.unlink()
            removed.append(p)
    return removed


# ================================================================
# Interactive confirmation
# ================================================================

def _confirm(prompt: str) -> bool:
    try:
        ans = input(_c("yellow", f"  {prompt} [y/N] ")).strip().lower()
    except (EOFError, KeyboardInterrupt):
        return False
    return ans in ("y", "yes")


# ================================================================
# Shell class
# ================================================================

class DocShell(cmd.Cmd):
    intro = (
        _header("VSEPR-SIM  Interactive Shell  4.0.4.02", 72) +
        "\n\n  Managed roots:\n" +
        "".join(f"    {_c('cyan', k):30s}  {v}\n"
                for k, v in MANAGED_ROOTS.items()) +
        "\n  ── Core commands ──────────────────────────────────────────────\n"
        "  ls · tree · cd · cat · head · tail · info · find · grep · recent\n"
        "  edit · new · append · cp · mv · rm · diff · open · vscode\n"
        "  compile · clean · texdiff · run · status · validate · export · zip\n"
        "\n  ── Workspace & Simulation ─────────────────────────────────────\n"
        f"  {_c('bold', 'wsm status')}              scan workspace pollution & disk usage\n"
        f"  {_c('bold', 'wsm clean [--dry-run]')}   remove pycache + root temp files\n"
        f"  {_c('bold', 'wsm reset')}               wipe generated outputs  (3-step confirm)\n"
        f"  {_c('bold', 'wsm uninstall')}            nuclear remove ALL artefacts\n"
        f"  {_c('bold', 'wsm log-molecules')}        snapshot molecule references → recovery log\n"
        f"  {_c('bold', 'wsm rerun-molecules')}      replay lost computations from log\n"
        f"  {_c('bold', 'pillar [n]')}              spawn n randomised colour-pillar plants\n"
        "\n  Type  " + _c("bold", "help") + "  for full command list,  " +
        _c("bold", "q") + "  to quit.\n"
    )
    prompt = _c("green", "doc-shell") + " " + _c("dim", "›") + " "

    def __init__(self):
        super().__init__()
        self._cwd: Path = WORKSPACE
        self._history: List[Path] = []

    # ── Prompt update ─────────────────────────────────────────────
    def postcmd(self, stop, line):
        rel = self._cwd.relative_to(WORKSPACE) if self._cwd != WORKSPACE else Path(".")
        self.prompt = (
            _c("green", "doc") + _c("dim", ":") +
            _c("cyan", str(rel)) + " " + _c("dim", ">") + " "
        )
        return stop

    # ── NAVIGATION ────────────────────────────────────────────────

    def do_pwd(self, _):
        """Print current working directory."""
        print(_c("cyan", str(self._cwd)))

    def do_cd(self, arg):
        """cd <path>   Change directory (absolute or relative to cwd/roots)."""
        if not arg.strip():
            self._cwd = WORKSPACE
            return
        target = _resolve(self._cwd, arg.strip())
        if not target.exists():
            print(_c("red", f"  No such directory: {target}"))
            return
        if not target.is_dir():
            print(_c("red", f"  Not a directory: {target}"))
            return
        self._cwd = target

    def do_ls(self, arg):
        """ls [path]   List files in directory with sizes and timestamps."""
        path = _resolve(self._cwd, arg.strip()) if arg.strip() else self._cwd
        if not path.exists():
            print(_c("red", f"  Path not found: {path}"))
            return
        if path.is_file():
            self._print_info(path)
            return
        items = sorted(path.iterdir(), key=lambda p: (p.is_file(), p.name.lower()))
        total_files = total_size = 0
        print(_header(f"ls  {path.relative_to(WORKSPACE) if path != WORKSPACE else '.'}"))
        for item in items:
            if item.name.startswith("."):
                continue
            if item.is_dir():
                n_children = sum(1 for _ in item.iterdir())
                print(f"  {_c('blue', 'd')} {_c('blue', item.name + '/'):<45s} "
                      f"{_c('dim', f'({n_children} items)')}")
            else:
                sz = item.stat().st_size
                mtime = datetime.fromtimestamp(item.stat().st_mtime).strftime("%Y-%m-%d %H:%M")
                col = _file_color(item)
                total_files += 1
                total_size += sz
                print(f"  {_c('dim', 'f')} {_c(col, item.name):<45s} "
                      f"{_human(sz):>8}  {_c('dim', mtime)}")
        print(_c("dim", f"\n  {total_files} files, {_human(total_size)} total"))

    def do_tree(self, arg):
        """tree [path]  Recursive tree view with sizes (depth limited to 4)."""
        path = _resolve(self._cwd, arg.strip()) if arg.strip() else self._cwd
        if not path.is_dir():
            print(_c("red", f"  Not a directory: {path}"))
            return
        print(_header(f"tree  {path.relative_to(WORKSPACE) if path != WORKSPACE else '.'}"))
        self._tree(path, "", 0)

    def _tree(self, path: Path, prefix: str, depth: int):
        if depth > 4:
            print(prefix + _c("dim", "  ..."))
            return
        items = sorted(path.iterdir(), key=lambda p: (p.is_file(), p.name.lower()))
        items = [i for i in items if not i.name.startswith(".")]
        for i, item in enumerate(items):
            connector = "`-- " if i == len(items) - 1 else "|-- "
            ext_col = _file_color(item) if item.is_file() else "blue"
            name_str = item.name + ("/" if item.is_dir() else "")
            sz_str = ""
            if item.is_file():
                sz_str = _c("dim", f"  ({_human(item.stat().st_size)})")
            print(prefix + _c("dim", connector) + _c(ext_col, name_str) + sz_str)
            if item.is_dir():
                extension = "    " if i == len(items) - 1 else "|   "
                self._tree(item, prefix + extension, depth + 1)

    # ── VIEWING ───────────────────────────────────────────────────

    def _print_info(self, path: Path):
        stat = path.stat()
        lines = _line_count(path) if _is_text(path) else -1
        print(_header(f"info  {path.name}"))
        print(f"  Path     : {path}")
        print(f"  Size     : {_human(stat.st_size)}")
        print(f"  Modified : {datetime.fromtimestamp(stat.st_mtime)}")
        print(f"  Created  : {datetime.fromtimestamp(stat.st_ctime)}")
        print(f"  Lines    : {lines if lines >= 0 else 'binary'}")
        md5 = hashlib.md5(path.read_bytes()).hexdigest()[:12]
        print(f"  MD5(12)  : {md5}")

    def do_info(self, arg):
        """info <file>  File metadata."""
        if not arg.strip():
            print(_c("red", "  Usage: info <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.exists():
            print(_c("red", f"  File not found: {path}"))
            return
        self._print_info(path)

    def do_cat(self, arg):
        """cat <file>   Print full file content."""
        if not arg.strip():
            print(_c("red", "  Usage: cat <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.is_file():
            print(_c("red", f"  File not found: {path}"))
            return
        if not _is_text(path):
            print(_c("yellow", "  Binary file — use  open  to view."))
            return
        lines = _read_lines(path)
        print(_header(f"cat  {path.name}  ({len(lines)} lines)"))
        for n, line in enumerate(lines, 1):
            print(f"  {_c('dim', f'{n:4d}')} {line}", end="")
        print()

    def do_head(self, arg):
        """head <file> [n]  Print first n lines (default 30)."""
        parts = arg.strip().split()
        if not parts:
            print(_c("red", "  Usage: head <file> [n]"))
            return
        path = _resolve(self._cwd, parts[0])
        n = int(parts[1]) if len(parts) > 1 else 30
        if not path.is_file():
            print(_c("red", f"  File not found: {path}"))
            return
        lines = _read_lines(path)[:n]
        print(_header(f"head  {path.name}  (first {n})"))
        for i, line in enumerate(lines, 1):
            print(f"  {_c('dim', f'{i:4d}')} {line}", end="")

    def do_tail(self, arg):
        """tail <file> [n]  Print last n lines (default 30)."""
        parts = arg.strip().split()
        if not parts:
            print(_c("red", "  Usage: tail <file> [n]"))
            return
        path = _resolve(self._cwd, parts[0])
        n = int(parts[1]) if len(parts) > 1 else 30
        if not path.is_file():
            print(_c("red", f"  File not found: {path}"))
            return
        lines = _read_lines(path)
        chunk = lines[-n:]
        total = len(lines)
        print(_header(f"tail  {path.name}  (last {n} of {total})"))
        for i, line in enumerate(chunk, total - len(chunk) + 1):
            print(f"  {_c('dim', f'{i:4d}')} {line}", end="")

    # ── EDITING ───────────────────────────────────────────────────

    def do_edit(self, arg):
        """edit <file>  Open file in $EDITOR / VS Code / notepad."""
        if not arg.strip():
            print(_c("red", "  Usage: edit <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.exists():
            if _confirm(f"File not found. Create {path.name}?"):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(_file_template(path), encoding="utf-8")
                print(_c("green", f"  Created: {path}"))
            else:
                return
        cmd_list = _get_editor() + [str(path)]
        try:
            subprocess.Popen(cmd_list)
            print(_c("green", f"  Opened {path.name} in {cmd_list[0]}"))
        except FileNotFoundError:
            print(_c("red", f"  Editor not found: {cmd_list[0]}"))

    def do_new(self, arg):
        """new <file>   Create new file from template (md/tex/py/csv)."""
        if not arg.strip():
            print(_c("red", "  Usage: new <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if path.exists():
            print(_c("yellow", f"  File already exists: {path}"))
            if not _confirm("Overwrite?"):
                return
        path.parent.mkdir(parents=True, exist_ok=True)
        content = _file_template(path)
        path.write_text(content, encoding="utf-8")
        print(_c("green", f"  Created: {path}"))
        print(_c("dim", content[:300]))

    def do_append(self, arg):
        """append <file>   Append lines interactively (blank line to stop)."""
        if not arg.strip():
            print(_c("red", "  Usage: append <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.exists():
            print(_c("red", f"  File not found: {path}"))
            return
        print(_c("dim", "  Enter lines (empty line to finish):"))
        lines = []
        while True:
            try:
                line = input("  > ")
            except (EOFError, KeyboardInterrupt):
                break
            if line == "":
                break
            lines.append(line)
        if lines:
            with path.open("a", encoding="utf-8") as f:
                f.write("\n" + "\n".join(lines) + "\n")
            print(_c("green", f"  Appended {len(lines)} lines to {path.name}"))

    # ── SEARCH ────────────────────────────────────────────────────

    def do_find(self, arg):
        """find <pattern>  Find files by name glob in all managed roots."""
        if not arg.strip():
            print(_c("red", "  Usage: find <glob-pattern>  e.g.  find *.tex"))
            return
        pattern = arg.strip()
        print(_header(f"find  {pattern}"))
        found = 0
        for root_name, root in MANAGED_ROOTS.items():
            if not root.exists():
                continue
            for p in sorted(root.rglob(pattern)):
                if any(part.startswith(".") for part in p.parts):
                    continue
                rel = p.relative_to(WORKSPACE)
                col = _file_color(p)
                sz = _human(p.stat().st_size) if p.is_file() else ""
                print(f"  [{_c('dim', root_name):10s}] {_c(col, str(rel))}  {_c('dim', sz)}")
                found += 1
        print(_c("dim", f"\n  {found} matches"))

    def do_grep(self, arg):
        """grep <text>   Search file content in all managed roots."""
        if not arg.strip():
            print(_c("red", "  Usage: grep <text>"))
            return
        pattern = arg.strip()
        try:
            regex = re.compile(pattern, re.IGNORECASE)
        except re.error as e:
            print(_c("red", f"  Invalid regex: {e}"))
            return
        print(_header(f"grep  {pattern!r}"))
        hits = 0
        for root in MANAGED_ROOTS.values():
            if not root.exists():
                continue
            for p in sorted(root.rglob("*")):
                if not p.is_file():
                    continue
                if any(part.startswith(".") or part == "__pycache__"
                       for part in p.parts):
                    continue
                if not _is_text(p):
                    continue
                lines = _read_lines(p)
                file_hits = []
                for n, line in enumerate(lines, 1):
                    if regex.search(line):
                        file_hits.append((n, line.rstrip()))
                if file_hits:
                    col = _file_color(p)
                    print(f"\n  {_c(col, str(p.relative_to(WORKSPACE)))}")
                    for n, line in file_hits[:8]:
                        hi = regex.sub(lambda m: _c("yellow", m.group()), line)
                        print(f"    {_c('dim', f'{n:4d}')}  {hi}")
                    if len(file_hits) > 8:
                        print(_c("dim", f"    ... and {len(file_hits)-8} more"))
                    hits += len(file_hits)
        print(_c("dim", f"\n  {hits} total matches"))

    def do_recent(self, arg):
        """recent [n]  Show n most recently modified files (default 20)."""
        n = int(arg.strip()) if arg.strip().isdigit() else 20
        all_files = []
        for root in MANAGED_ROOTS.values():
            if not root.exists():
                continue
            for p in root.rglob("*"):
                if p.is_file() and not any(
                    part.startswith(".") or part == "__pycache__"
                    for part in p.parts
                ):
                    all_files.append(p)
        all_files.sort(key=lambda p: p.stat().st_mtime, reverse=True)
        print(_header(f"recent  (top {n})"))
        for p in all_files[:n]:
            mtime = datetime.fromtimestamp(p.stat().st_mtime).strftime("%Y-%m-%d %H:%M:%S")
            col = _file_color(p)
            rel = str(p.relative_to(WORKSPACE))
            print(f"  {_c('dim', mtime)}  {_c(col, rel)}")

    # ── LATEX ─────────────────────────────────────────────────────

    def do_compile(self, arg):
        """compile <file.tex>  Run pdflatex (2 passes) on a .tex file."""
        if not arg.strip():
            print(_c("red", "  Usage: compile <file.tex>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.is_file():
            print(_c("red", f"  File not found: {path}"))
            return
        if path.suffix.lower() != ".tex":
            print(_c("yellow", "  Warning: not a .tex file"))
        print(_c("cyan", f"  Compiling {path.name} ..."))
        t0 = time.time()
        ok, log = _pdflatex(path)
        elapsed = time.time() - t0
        col = "green" if ok else "red"
        status = "OK" if ok else "FAILED"
        print(_c(col, f"  {status}  ({elapsed:.1f}s)"))
        print(_c("dim", "\n".join("  " + l for l in log.splitlines()[-25:])))
        if ok:
            pdf = path.with_suffix(".pdf")
            if pdf.exists():
                print(_c("green", f"  Output: {pdf}"))

    def do_clean(self, arg):
        """clean [path]  Remove LaTeX auxiliary files (.aux .log .out etc.)."""
        path = _resolve(self._cwd, arg.strip()) if arg.strip() else self._cwd
        if not path.is_dir():
            path = path.parent
        removed = _clean_latex(path)
        if removed:
            print(_c("green", f"  Removed {len(removed)} auxiliary files:"))
            for p in removed:
                print(_c("dim", f"    {p.name}"))
        else:
            print(_c("dim", "  No auxiliary files found."))

    def do_texdiff(self, arg):
        """texdiff <a.tex> <b.tex>  Side-by-side diff of two .tex files."""
        parts = arg.strip().split()
        if len(parts) < 2:
            print(_c("red", "  Usage: texdiff <a.tex> <b.tex>"))
            return
        pa = _resolve(self._cwd, parts[0])
        pb = _resolve(self._cwd, parts[1])
        if not pa.is_file() or not pb.is_file():
            print(_c("red", "  Both files must exist."))
            return
        la = _read_lines(pa)
        lb = _read_lines(pb)
        diff = list(difflib.unified_diff(la, lb,
                                         fromfile=str(pa.name),
                                         tofile=str(pb.name), n=3))
        print(_header(f"texdiff  {pa.name}  ↔  {pb.name}"))
        for line in diff:
            if line.startswith("+++") or line.startswith("---"):
                print(_c("bold", line), end="")
            elif line.startswith("+"):
                print(_c("green", line), end="")
            elif line.startswith("-"):
                print(_c("red", line), end="")
            elif line.startswith("@@"):
                print(_c("cyan", line), end="")
            else:
                print(_c("dim", line), end="")
        if not diff:
            print(_c("green", "  Files are identical."))

    # ── REPORTS ───────────────────────────────────────────────────

    def do_run(self, arg):
        """run <script.py>   Execute a Python report script."""
        if not arg.strip():
            print(_c("red", "  Usage: run <script.py>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.is_file():
            print(_c("red", f"  Script not found: {path}"))
            return
        print(_c("cyan", f"  Running {path.name} ..."))
        t0 = time.time()
        result = subprocess.run(
            [sys.executable, str(path)],
            capture_output=False, cwd=str(WORKSPACE),
        )
        elapsed = time.time() - t0
        col = "green" if result.returncode == 0 else "red"
        print(_c(col, f"\n  Exit {result.returncode}  ({elapsed:.2f}s)"))

    def do_status(self, _):
        """status  Counts, sizes, newest file per managed root."""
        print(_header("Workspace Status"))
        for root_name, root in MANAGED_ROOTS.items():
            if not root.exists():
                print(f"  {_c('dim', root_name):12s}  {_c('red', '(missing)')}")
                continue
            files = [p for p in root.rglob("*") if p.is_file()
                     and not any(x.startswith(".") or x == "__pycache__"
                                 for x in p.parts)]
            total_size = sum(p.stat().st_size for p in files)
            newest = max(files, key=lambda p: p.stat().st_mtime, default=None)
            n_tex = sum(1 for p in files if p.suffix == ".tex")
            n_md  = sum(1 for p in files if p.suffix == ".md")
            n_pdf = sum(1 for p in files if p.suffix == ".pdf")
            n_py  = sum(1 for p in files if p.suffix == ".py")
            n_csv = sum(1 for p in files if p.suffix == ".csv")
            print(f"\n  {_c('cyan', root_name.upper())}")
            print(f"    path     : {root}")
            print(f"    files    : {len(files)}  ({_human(total_size)})")
            print(f"    .tex={n_tex}  .md={n_md}  .pdf={n_pdf}  .py={n_py}  .csv={n_csv}")
            if newest:
                mtime = datetime.fromtimestamp(newest.stat().st_mtime).strftime("%Y-%m-%d %H:%M")
                print(f"    newest   : {newest.name}  ({mtime})")

    def do_validate(self, _):
        """validate  Check for orphaned .tex (no matching .pdf) and broken links."""
        print(_header("Validation"))
        issues = 0
        # 1. .tex without matching .pdf
        for p in MANAGED_ROOTS["docs"].rglob("*.tex"):
            pdf = p.with_suffix(".pdf")
            if not pdf.exists():
                print(_c("yellow", f"  [no-pdf] {p.relative_to(WORKSPACE)}"))
                issues += 1
        # 2. .pdf without matching .tex (orphan output)
        for p in MANAGED_ROOTS["docs"].rglob("*.pdf"):
            tex = p.with_suffix(".tex")
            if not tex.exists():
                print(_c("dim", f"  [no-tex] {p.relative_to(WORKSPACE)}"))
        # 3. Empty files
        for root in MANAGED_ROOTS.values():
            if not root.exists():
                continue
            for p in root.rglob("*"):
                if p.is_file() and p.stat().st_size == 0:
                    print(_c("red", f"  [empty]  {p.relative_to(WORKSPACE)}"))
                    issues += 1
        # 4. Duplicate basenames in docs
        names: dict = {}
        for p in MANAGED_ROOTS["docs"].rglob("*"):
            if p.is_file():
                key = p.stem.lower()
                names.setdefault(key, []).append(p)
        for key, paths in names.items():
            if len(paths) > 1:
                rel_paths = [str(p.relative_to(WORKSPACE)) for p in paths]
                print(_c("yellow", f"  [dup-stem] {key}: {', '.join(rel_paths)}"))
        col = "green" if issues == 0 else "yellow"
        print(_c(col, f"\n  Validation done — {issues} issue(s) found."))

    # ── FILE OPS ──────────────────────────────────────────────────

    def do_cp(self, arg):
        """cp <src> <dst>  Copy file."""
        parts = arg.strip().split(None, 1)
        if len(parts) < 2:
            print(_c("red", "  Usage: cp <src> <dst>"))
            return
        src = _resolve(self._cwd, parts[0])
        dst = _resolve(self._cwd, parts[1])
        if not src.is_file():
            print(_c("red", f"  Source not found: {src}"))
            return
        if dst.is_dir():
            dst = dst / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(src), str(dst))
        print(_c("green", f"  Copied → {dst}"))

    def do_mv(self, arg):
        """mv <src> <dst>  Move or rename file."""
        parts = arg.strip().split(None, 1)
        if len(parts) < 2:
            print(_c("red", "  Usage: mv <src> <dst>"))
            return
        src = _resolve(self._cwd, parts[0])
        dst = _resolve(self._cwd, parts[1])
        if not src.exists():
            print(_c("red", f"  Source not found: {src}"))
            return
        if dst.is_dir():
            dst = dst / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(src), str(dst))
        print(_c("green", f"  Moved → {dst}"))

    def do_rm(self, arg):
        """rm <file>  Delete file (with confirmation)."""
        if not arg.strip():
            print(_c("red", "  Usage: rm <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.exists():
            print(_c("red", f"  File not found: {path}"))
            return
        print(_c("yellow", f"  About to delete: {path}"))
        if _confirm("Confirm delete?"):
            if path.is_file():
                path.unlink()
            else:
                shutil.rmtree(str(path))
            print(_c("green", "  Deleted."))
        else:
            print(_c("dim", "  Cancelled."))

    def do_diff(self, arg):
        """diff <a> <b>  Unified diff of two text files."""
        parts = arg.strip().split()
        if len(parts) < 2:
            print(_c("red", "  Usage: diff <a> <b>"))
            return
        pa = _resolve(self._cwd, parts[0])
        pb = _resolve(self._cwd, parts[1])
        la = _read_lines(pa) if pa.is_file() else []
        lb = _read_lines(pb) if pb.is_file() else []
        diff = list(difflib.unified_diff(la, lb,
                                         fromfile=parts[0], tofile=parts[1], n=3))
        for line in diff:
            if line.startswith("+"):
                print(_c("green", line), end="")
            elif line.startswith("-"):
                print(_c("red", line), end="")
            elif line.startswith("@@"):
                print(_c("cyan", line), end="")
            else:
                print(line, end="")
        if not diff:
            print(_c("green", "  Files are identical."))

    # ── OPEN ──────────────────────────────────────────────────────

    def do_open(self, arg):
        """open <file>  Open with OS default viewer."""
        if not arg.strip():
            print(_c("red", "  Usage: open <file>"))
            return
        path = _resolve(self._cwd, arg.strip())
        if not path.exists():
            print(_c("red", f"  File not found: {path}"))
            return
        print(_c("cyan", f"  Opening {path.name} ..."))
        _open_os(path)

    def do_vscode(self, arg):
        """vscode <file>  Open file in VS Code."""
        target = arg.strip() or "."
        path = _resolve(self._cwd, target)
        if not shutil.which("code"):
            print(_c("red", "  VS Code (code) not in PATH"))
            return
        subprocess.Popen(["code", str(path)])
        print(_c("green", f"  VS Code → {path}"))

    # ── EXPORT ────────────────────────────────────────────────────

    def do_export(self, arg):
        """export <src> <dst>  Copy file or directory to external path."""
        parts = arg.strip().split(None, 1)
        if len(parts) < 2:
            print(_c("red", "  Usage: export <src> <dst>"))
            return
        src = _resolve(self._cwd, parts[0])
        dst = Path(parts[1]).expanduser().resolve()
        if not src.exists():
            print(_c("red", f"  Source not found: {src}"))
            return
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.is_file():
            shutil.copy2(str(src), str(dst))
        else:
            shutil.copytree(str(src), str(dst), dirs_exist_ok=True)
        print(_c("green", f"  Exported {src.name} → {dst}"))

    def do_zip(self, arg):
        """zip <name>  Zip the docs/ tree to out/exports/<name>.zip."""
        name = arg.strip() or f"docs_export_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        out_dir = WORKSPACE / "out" / "exports"
        out_dir.mkdir(parents=True, exist_ok=True)
        zip_path = out_dir / (name if name.endswith(".zip") else name + ".zip")
        src = MANAGED_ROOTS["docs"]
        if not src.exists():
            print(_c("red", "  docs/ not found"))
            return
        print(_c("cyan", f"  Zipping docs/ → {zip_path} ..."))
        shutil.make_archive(str(zip_path.with_suffix("")), "zip", str(src))
        sz = _human(zip_path.stat().st_size)
        print(_c("green", f"  Done: {zip_path.name}  ({sz})"))

    # ── SHELL ESCAPE ──────────────────────────────────────────────

    def do_shell(self, arg):
        """!<command>  Run a raw shell command."""
        if arg.strip():
            subprocess.run(arg, shell=True, cwd=str(self._cwd))

    def default(self, line):
        if line.startswith("!"):
            self.do_shell(line[1:])
        else:
            print(_c("red", f"  Unknown command: {line.split()[0]}  (type  help  for list)"))

    # ── WORKSPACE MANAGER ─────────────────────────────────────────

    def do_wsm(self, arg: str):
        """wsm <subcmd>  Workspace housekeeping and molecule recovery.

  Subcommands:
    status            scan and report workspace pollution
    clean             remove pycache + root temp files (safe)
    clean --dry-run   preview clean without deleting
    reset             wipe all generated outputs  (3-step confirm)
    uninstall         remove ALL artefacts         (4-step confirm)
    log-molecules     scan and log all molecule/formula references
    rerun-molecules   replay computations from molecule recovery log

  Example:
    wsm status
    wsm clean --dry-run
    wsm log-molecules
"""
        sub = arg.strip()
        if not sub or sub in ("help", "?"):
            lines = [
                "",
                _c("cyan", "  Workspace Manager subcommands:"),
                "",
            ]
            for name, desc in _WSM_SUBCMDS.items():
                lines.append(f"    {_c('bold', name):<28s}  {_c('dim', desc)}")
            lines.append("")
            print("\n".join(lines))
            return

        wsm = _load_wsm()

        dispatch: Dict[str, Callable] = {
            "status":          wsm.cmd_status,
            "clean":           wsm.cmd_clean,
            "clean --dry-run": lambda: wsm.cmd_clean(dry_run=True),
            "reset":           wsm.cmd_reset,
            "uninstall":       wsm.cmd_uninstall,
            "log-molecules":   wsm.cmd_log_molecules,
            "rerun-molecules": wsm.cmd_rerun_molecules,
        }

        fn = dispatch.get(sub)
        if fn is None:
            print(_c("red", f"  Unknown wsm subcommand: '{sub}'"))
            print(_c("dim", f"  Available: {', '.join(dispatch.keys())}"))
            return
        fn()

    do_workspace = do_wsm   # verbose alias

    def complete_wsm(self, text: str, line: str, begidx: int, endidx: int) -> List[str]:
        return [k for k in _WSM_SUBCMDS if k.startswith(text)]

    complete_workspace = complete_wsm

    # ── PILLAR ────────────────────────────────────────────────────

    def do_pillar(self, arg: str):
        """pillar [n]  Spawn n randomised colour-pillar power plants (default 1).

  Runs colour_pillar_plant_gen.py as a subprocess so results stream
  live and outputs land in out/colour_pillar_plants/ as usual.

  Examples:
    pillar          run 1 plant
    pillar 5        run 5 randomised plants
"""
        try:
            n = int(arg.strip()) if arg.strip() else 1
        except ValueError:
            print(_c("red", "  Usage: pillar [n]  (n must be an integer)"))
            return

        gen = WORKSPACE / "scripts" / "colour_pillar_plant_gen.py"
        if not gen.is_file():
            print(_c("red", f"  Generator not found: {gen}"))
            return

        print(_c("cyan", f"  Spawning {n} colour-pillar plant(s) ...\n"))
        for i in range(n):
            if n > 1:
                print(_c("dim", f"  ── Plant {i+1}/{n} ──"))
            t0 = time.time()
            result = subprocess.run(
                [sys.executable, str(gen)],
                cwd=str(WORKSPACE),
            )
            elapsed = time.time() - t0
            col = "green" if result.returncode == 0 else "red"
            print(_c(col, f"\n  Plant {i+1} exit {result.returncode}  ({elapsed:.1f}s)\n"))

    # ── QUIT ──────────────────────────────────────────────────────


    def do_q(self, _):
        """q  Quit the shell."""
        print(_c("dim", "\n  Bye.\n"))
        return True

    do_quit = do_q
    do_exit = do_q

    def do_EOF(self, _):
        return self.do_q(_)

    # ── TAB COMPLETION ────────────────────────────────────────────

    def _complete_path(self, text: str, line: str, opts) -> List[str]:
        """Generic filesystem completer."""
        base = self._cwd / text if text else self._cwd
        base_str = str(base)
        try:
            parent = Path(text).parent if text else self._cwd
            parent = (self._cwd / parent).resolve()
            prefix = Path(text).name if text else ""
            return [
                str(p.relative_to(self._cwd)) + ("/" if p.is_dir() else "")
                for p in parent.iterdir()
                if p.name.startswith(prefix)
            ]
        except Exception:
            return []

    def completedefault(self, text, line, begidx, endidx):
        return self._complete_path(text, line, None)

    # Attach completion to file-taking commands
    for _cmd in ("cat", "head", "tail", "info", "edit", "open",
                 "vscode", "cp", "mv", "rm", "diff", "texdiff",
                 "compile", "clean", "run", "export", "cd", "ls", "tree"):
        locals()[f"complete_{_cmd}"] = completedefault


# ================================================================
# Entry point
# ================================================================

def main():
    # Enable ANSI on Windows
    if platform.system() == "Windows":
        os.system("")

    # ── Precursor / MOTD renderer ─────────────────────────────────
    # Try the compiled C binary first; fall back to a Python splash.
    _entry_candidates = [
        WORKSPACE / "build" / "vsepr-entry",
        WORKSPACE / "build" / "vsepr-entry.exe",
        WORKSPACE / "build" / "apps" / "vsepr-entry",
        WORKSPACE / "build" / "apps" / "vsepr-entry.exe",
        WORKSPACE / "bin"   / "vsepr-entry",
        WORKSPACE / "bin"   / "vsepr-entry.exe",
    ]
    _binary_ran = False
    if sys.stdout.isatty():  # skip in piped/CI contexts
        for _entry in _entry_candidates:
            if _entry.is_file():
                try:
                    subprocess.run([str(_entry)], check=False)
                    _binary_ran = True
                except Exception:
                    pass
                break
        if not _binary_ran:
            # Python fallback splash
            _lines = [
                "",
                _c("cyan",   "  ╔══════════════════════════════════════════════════════════════╗"),
                _c("cyan",   "  ║") + BOLD + _c("white", "  V") + _c("green", "S") +
                    _c("yellow", "E") + _c("magenta", "P") + _c("red", "R") +
                    _c("white", "-SIM") + RST + _c("dim",
                    "  Valence Shell Electron Pair Repulsion     ") + _c("cyan", "║"),
                _c("cyan",   "  ║") + _c("dim",
                    "  v4.0.4  \"Chromatic-Pillar\"                              ") + _c("cyan", "║"),
                _c("cyan",   "  ║") + _c("dim",
                    "  Build vsepr-entry for full MOTD animation                ") + _c("cyan", "║"),
                _c("cyan",   "  ╚══════════════════════════════════════════════════════════════╝"),
                "",
            ]
            for ln in _lines:
                print(ln)

    try:
        import readline  # enables arrow-key history on Linux/macOS
        readline.parse_and_bind("tab: complete")
    except ImportError:
        pass

    shell = DocShell()
    # Initial status overview
    shell.do_status("")
    print()
    try:
        shell.cmdloop()
    except KeyboardInterrupt:
        print(_c("dim", "\n  Interrupted. Bye."))


if __name__ == "__main__":
    main()
