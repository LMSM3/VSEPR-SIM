#!/usr/bin/env python3
"""
generate_layering_report.py — VSEPR-SIM Automatic Layering Report Generator

Introspects the codebase to extract layer status, module inventory,
dependency graph, boundary checks, and gate statistics, then writes
a LaTeX data file consumed by reporting/layering_report.tex.

Workflow:
  1. Walk the source tree and classify files by layer
  2. Parse layer_manifest.hpp and layer_stack.hpp for authoritative status
  3. Count headers, source files, and lines per layer
  4. Detect boundary checks in layer_boundary.hpp
  5. Write reporting/layering_data.tex (\\input'd by the main document)

Anti-black-box: every metric is derived from file-system counts and grep.
Deterministic: same source tree → identical data file.

Usage:
  python reporting/generate_layering_report.py
  python reporting/generate_layering_report.py --root <project_root>

Reference: .github/copilot-instructions.md §2, §5, §9
"""

import argparse
import datetime
import os
import re
import sys


# ===========================================================================
# Layer Classification
# ===========================================================================

# Map directory prefixes to layer assignments
LAYER_MAP = {
    # L1 — Paper Identity
    'src/build':          ('L1', 'Formation Priors'),
    'src/core/periodic':  ('L1', 'Formation Priors'),

    # L2 — Atomistic / Physical Kernel
    'atomistic/core':     ('L2', 'Atomistic Core'),
    'atomistic/integrators': ('L2', 'Integrators'),
    'atomistic/potentials':  ('L2', 'Potentials'),
    'atomistic/ff':       ('L2', 'Force Fields'),
    'src/pot':            ('L2', 'Potentials'),
    'src/sim':            ('L2', 'Simulation'),
    'src/int':            ('L2', 'Integrators'),
    'src/box':            ('L2', 'PBC / Box'),
    'src/nl':             ('L2', 'Neighbor Lists'),
    'src/core':           ('L2', 'Core'),

    # L3 — Molecular 3D / Verification
    'atomistic/geometry': ('L3', 'Geometry'),
    'atomistic/validation': ('L3', 'Validation'),
    'atomistic/report':   ('L3', 'Atomistic Report'),
    'atomistic/analysis': ('L3', 'Analysis'),

    # L4 — Atomistic Bead / Coarse-Grained
    'coarse_grain/core':     ('L4', 'CG Core'),
    'coarse_grain/mapping':  ('L4', 'Atom-to-Bead Mapping'),
    'coarse_grain/models':   ('L4', 'CG Models'),
    'coarse_grain/report':   ('L4', 'CG Reporting'),
    'coarse_grain/metals':   ('L4', 'Metals'),

    # L5 — Macro / CAD / Visualization / API
    'src/vis':            ('L5', 'Visualization'),
    'src/render':         ('L5', 'Rendering'),
    'src/gui':            ('L5', 'GUI'),
    'src/cli':            ('L5', 'CLI'),
    'src/api':            ('L5', 'API Facade'),
    'src/io':             ('L5', 'I/O'),
    'src/demo':           ('L5', 'Demo'),

    # Cross-cutting
    'include':            ('Cross', 'Include'),
    'reporting':          ('Cross', 'Reporting'),
    'scripts':            ('Cross', 'Scripts'),
    'tests':              ('Cross', 'Tests'),
    'apps':               ('Cross', 'Applications'),
    'tools':              ('Cross', 'Tools'),
    'examples':           ('Cross', 'Examples'),
    'docs':               ('Cross', 'Documentation'),
}


def classify_file(relpath):
    """Classify a file into a layer based on its path prefix."""
    normalized = relpath.replace('\\', '/')
    # Try longest prefix first for specificity
    best_match = None
    best_len = 0
    for prefix, (layer, module) in LAYER_MAP.items():
        if normalized.startswith(prefix) and len(prefix) > best_len:
            best_match = (layer, module)
            best_len = len(prefix)
    return best_match if best_match else ('Unclassified', 'Other')


def count_lines(filepath):
    """Count non-empty lines in a file."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            return sum(1 for line in f if line.strip())
    except Exception:
        return 0


def is_source_file(name):
    """Check if a file is a C++ source or header."""
    return name.endswith(('.cpp', '.hpp', '.h', '.cxx', '.cc', '.cu'))


# ===========================================================================
# Layer Manifest Parser
# ===========================================================================

def parse_layer_manifest(root):
    """Parse atomistic/core/layer_manifest.hpp for layer status."""
    manifest_path = os.path.join(root, 'atomistic', 'core', 'layer_manifest.hpp')
    entries = []
    if not os.path.isfile(manifest_path):
        return entries

    with open(manifest_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Match entries like:  { EngineLayer::..., "...", "...", "..." },
    pattern = re.compile(
        r'\{\s*EngineLayer::(\w+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*'
        r'"([^"]+)"\s*\}',
        re.MULTILINE
    )
    for m in pattern.finditer(content):
        entries.append({
            'enum': m.group(1),
            'name': m.group(2),
            'status': m.group(3),
            'note': m.group(4),
        })
    return entries


# ===========================================================================
# Boundary Check Parser
# ===========================================================================

def parse_boundary_checks(root):
    """Parse layer_boundary.hpp for B1-B5 check descriptions."""
    boundary_path = os.path.join(root, 'coarse_grain', 'models', 'layer_boundary.hpp')
    checks = []
    if not os.path.isfile(boundary_path):
        return checks

    with open(boundary_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Match [B1] ... [B5] descriptions from C block comment.
    # Each entry starts with "[Bx]" and runs until the next "[Bx]" or
    # end of the comment block.  Lines are prefixed with " *   ".
    pattern = re.compile(
        r'\[B(\d)\]\s*(.+?)(?=\[B\d\]|\n\s*\*\s*\n\s*\*\s*Anti|\*\/)',
        re.DOTALL
    )
    for m in pattern.finditer(content):
        desc = ' '.join(m.group(2).split())
        desc = desc.replace('*', '').strip()
        # Trim trailing metadata
        for sentinel in ['Anti-black-box:', 'Reference:', 'Deterministic:']:
            idx = desc.find(sentinel)
            if idx > 0:
                desc = desc[:idx].strip()
        checks.append({
            'id': f'B{m.group(1)}',
            'description': desc,
        })
    return checks


# ===========================================================================
# Gate Structure Parser
# ===========================================================================

def parse_gates(root):
    """Detect LayerGate functions in layer_stack.hpp."""
    stack_path = os.path.join(root, 'include', 'layer_stack.hpp')
    gates = []
    if not os.path.isfile(stack_path):
        return gates

    with open(stack_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Match inline LayerGateResult l*_gate(...)
    pattern = re.compile(r'inline\s+LayerGateResult\s+(l\d+_gate)\(', re.MULTILINE)
    for m in pattern.finditer(content):
        gates.append(m.group(1))
    return gates


# ===========================================================================
# CMake Library Parser
# ===========================================================================

def parse_cmake_libraries(root):
    """Parse root CMakeLists.txt for library targets."""
    cmake_path = os.path.join(root, 'CMakeLists.txt')
    libs = []
    if not os.path.isfile(cmake_path):
        return libs

    with open(cmake_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Match add_library(name ...)
    pattern = re.compile(r'add_library\(\s*(\w+)\s+(STATIC|INTERFACE|SHARED)', re.MULTILINE)
    seen = set()
    for m in pattern.finditer(content):
        name = m.group(1)
        if name not in seen:
            seen.add(name)
            libs.append({
                'name': name,
                'type': m.group(2),
            })
    return libs


# ===========================================================================
# Report Generation Functions
# ===========================================================================

def parse_report_modules(root):
    """Discover C++ report-generation modules."""
    report_dirs = [
        os.path.join(root, 'coarse_grain', 'report'),
        os.path.join(root, 'atomistic', 'report'),
    ]
    modules = []
    for d in report_dirs:
        if not os.path.isdir(d):
            continue
        for fname in sorted(os.listdir(d)):
            if fname.endswith('.hpp'):
                fpath = os.path.join(d, fname)
                lines = count_lines(fpath)
                relpath = os.path.relpath(fpath, root).replace('\\', '/')
                modules.append({
                    'file': relpath,
                    'name': fname.replace('.hpp', '').replace('_', ' ').title(),
                    'lines': lines,
                })
    return modules


# ===========================================================================
# Tree Walk and Aggregation
# ===========================================================================

def walk_source_tree(root):
    """Walk the source tree and aggregate per-layer statistics."""
    stats = {}  # layer -> {files, headers, sources, lines, modules}

    skip_dirs = {'.git', 'build', 'third_party', '.venv', 'outputs',
                 '__pycache__', 'node_modules', '.vs'}

    for dirpath, dirnames, filenames in os.walk(root):
        # Prune irrelevant directories
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]

        for fname in filenames:
            if not is_source_file(fname):
                continue

            fpath = os.path.join(dirpath, fname)
            relpath = os.path.relpath(fpath, root)
            layer, module = classify_file(relpath)

            if layer not in stats:
                stats[layer] = {
                    'files': 0, 'headers': 0, 'sources': 0,
                    'lines': 0, 'modules': set()
                }

            s = stats[layer]
            s['files'] += 1
            s['modules'].add(module)
            s['lines'] += count_lines(fpath)

            if fname.endswith(('.hpp', '.h')):
                s['headers'] += 1
            else:
                s['sources'] += 1

    return stats


# ===========================================================================
# LaTeX Data File Writer
# ===========================================================================

def escape_latex(text):
    """Escape special LaTeX characters and replace Unicode with commands."""
    # Step 1: Escape LaTeX special characters first
    replacements = {
        '&': r'\&', '%': r'\%', '$': r'\$', '#': r'\#',
        '_': r'\_', '{': r'\{', '}': r'\}', '~': r'\textasciitilde{}',
        '^': r'\textasciicircum{}',
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    # Step 2: Replace Unicode characters with LaTeX commands
    # (these produce $...$ which must not be re-escaped, so they run after)
    unicode_map = {
        '\u03a3': r'$\Sigma$',     # Σ
        '\u03b1': r'$\alpha$',     # α
        '\u03b5': r'$\varepsilon$',# ε
        '\u03b7': r'$\eta$',       # η
        '\u03c3': r'$\sigma$',     # σ
        '\u03c1': r'$\rho$',       # ρ
        '\u0394': r'$\Delta$',     # Δ
        '\u2192': r'$\to$',        # →
        '\u2264': r'$\leq$',       # ≤
        '\u2265': r'$\geq$',       # ≥
        '\u00b3': r'$^3$',         # ³
        '\u00b2': r'$^2$',         # ²
        '\u00b7': r'$\cdot$',      # ·
        '\u00c5': r'\AA{}',        # Å
        '\u2013': r'--',           # –
        '\u2014': r'---',          # —
    }
    for char, repl in unicode_map.items():
        text = text.replace(char, repl)

    return text


def write_data_file(out_path, stats, manifest, boundary_checks,
                    gates, cmake_libs, report_modules, root):
    """Write the layering_data.tex file consumed by the main document."""
    timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(f'% Auto-generated by generate_layering_report.py\n')
        f.write(f'% Timestamp: {timestamp}\n')
        f.write(f'% Source root: {escape_latex(root)}\n\n')

        # ── Global metrics ──────────────────────────────────────────────
        total_files = sum(s['files'] for s in stats.values())
        total_lines = sum(s['lines'] for s in stats.values())
        total_headers = sum(s['headers'] for s in stats.values())
        total_sources = sum(s['sources'] for s in stats.values())

        f.write(f'\\newcommand{{\\ReportTimestamp}}{{{timestamp}}}\n')
        f.write(f'\\newcommand{{\\TotalFiles}}{{{total_files}}}\n')
        f.write(f'\\newcommand{{\\TotalLines}}{{{total_lines:,}}}\n')
        f.write(f'\\newcommand{{\\TotalHeaders}}{{{total_headers}}}\n')
        f.write(f'\\newcommand{{\\TotalSources}}{{{total_sources}}}\n')
        f.write(f'\\newcommand{{\\NumCMakeLibs}}{{{len(cmake_libs)}}}\n')
        f.write(f'\\newcommand{{\\NumBoundaryChecks}}{{{len(boundary_checks)}}}\n')
        f.write(f'\\newcommand{{\\NumGates}}{{{len(gates)}}}\n')
        f.write(f'\\newcommand{{\\NumReportModules}}{{{len(report_modules)}}}\n\n')

        # ── Per-layer statistics table ──────────────────────────────────
        f.write('% --- Per-Layer Statistics ---\n')
        f.write('\\newcommand{\\LayerStatsTable}{\n')
        f.write('\\begin{tabular}{lrrrrp{5.5cm}}\n')
        f.write('\\toprule\n')
        f.write('\\textbf{Layer} & \\textbf{Files} & \\textbf{Headers} '
                '& \\textbf{Sources} & \\textbf{Lines} & \\textbf{Modules} \\\\\n')
        f.write('\\midrule\n')

        layer_order = ['L1', 'L2', 'L3', 'L4', 'L5', 'Cross', 'Unclassified']
        layer_names = {
            'L1': 'L1 --- Paper Identity',
            'L2': 'L2 --- Atomistic Kernel',
            'L3': 'L3 --- Molecular 3D',
            'L4': 'L4 --- Atomistic Bead (CG)',
            'L5': 'L5 --- Macro / CAD / IO',
            'Cross': 'Cross-cutting',
            'Unclassified': 'Unclassified',
        }

        for layer in layer_order:
            if layer not in stats:
                continue
            s = stats[layer]
            modules_str = ', '.join(sorted(s['modules']))
            name = layer_names.get(layer, layer)
            f.write(f'{escape_latex(name)} & {s["files"]} & {s["headers"]} '
                    f'& {s["sources"]} & {s["lines"]:,} '
                    f'& \\footnotesize {escape_latex(modules_str)} \\\\\n')

        f.write('\\midrule\n')
        f.write(f'\\textbf{{Total}} & \\textbf{{{total_files}}} '
                f'& \\textbf{{{total_headers}}} & \\textbf{{{total_sources}}} '
                f'& \\textbf{{{total_lines:,}}} & \\\\\n')
        f.write('\\bottomrule\n')
        f.write('\\end{tabular}\n')
        f.write('}\n\n')

        # ── Layer manifest table ────────────────────────────────────────
        f.write('% --- Layer Manifest ---\n')
        f.write('\\newcommand{\\LayerManifestTable}{\n')
        f.write('\\begin{tabular}{llp{8cm}}\n')
        f.write('\\toprule\n')
        f.write('\\textbf{Layer} & \\textbf{Status} & \\textbf{Notes} \\\\\n')
        f.write('\\midrule\n')

        status_icon = {
            'complete': '$\\checkmark$ complete',
            'partial': '$\\sim$ partial',
            'absent': '$\\times$ absent',
        }

        for entry in manifest:
            icon = status_icon.get(entry['status'], entry['status'])
            f.write(f'{escape_latex(entry["name"])} & {icon} '
                    f'& \\footnotesize {escape_latex(entry["note"])} \\\\\n')

        f.write('\\bottomrule\n')
        f.write('\\end{tabular}\n')
        f.write('}\n\n')

        # ── Boundary checks table ───────────────────────────────────────
        f.write('% --- Boundary Checks ---\n')
        f.write('\\newcommand{\\BoundaryChecksTable}{\n')
        f.write('\\begin{tabular}{lp{10cm}}\n')
        f.write('\\toprule\n')
        f.write('\\textbf{Check} & \\textbf{Description} \\\\\n')
        f.write('\\midrule\n')

        for check in boundary_checks:
            f.write(f'\\texttt{{{check["id"]}}} '
                    f'& \\footnotesize {escape_latex(check["description"])} \\\\\n')

        f.write('\\bottomrule\n')
        f.write('\\end{tabular}\n')
        f.write('}\n\n')

        # ── CMake libraries table ───────────────────────────────────────
        f.write('% --- CMake Libraries ---\n')
        f.write('\\newcommand{\\CMakeLibsTable}{\n')
        f.write('\\begin{tabular}{ll}\n')
        f.write('\\toprule\n')
        f.write('\\textbf{Target} & \\textbf{Type} \\\\\n')
        f.write('\\midrule\n')

        for lib in cmake_libs:
            f.write(f'\\texttt{{{escape_latex(lib["name"])}}} '
                    f'& {lib["type"]} \\\\\n')

        f.write('\\bottomrule\n')
        f.write('\\end{tabular}\n')
        f.write('}\n\n')

        # ── Report modules table ────────────────────────────────────────
        f.write('% --- Report Modules ---\n')
        f.write('\\newcommand{\\ReportModulesTable}{\n')
        f.write('\\begin{tabular}{lrl}\n')
        f.write('\\toprule\n')
        f.write('\\textbf{Module} & \\textbf{Lines} & \\textbf{File} \\\\\n')
        f.write('\\midrule\n')

        for mod in report_modules:
            f.write(f'{escape_latex(mod["name"])} & {mod["lines"]} '
                    f'& \\texttt{{\\footnotesize {escape_latex(mod["file"])}}} \\\\\n')

        f.write('\\bottomrule\n')
        f.write('\\end{tabular}\n')
        f.write('}\n\n')

        # ── Gate functions list ─────────────────────────────────────────
        f.write('% --- Gate Functions ---\n')
        gate_list = ', '.join(f'\\texttt{{{escape_latex(g)}}}' for g in gates)
        f.write(f'\\newcommand{{\\GateFunctions}}{{{gate_list if gate_list else "None detected"}}}\n\n')

    print(f'[OK] Wrote {out_path}')


# ===========================================================================
# Main
# ===========================================================================

def main():
    parser = argparse.ArgumentParser(
        description='VSEPR-SIM Automatic Layering Report Data Generator')
    parser.add_argument('--root', default=None,
                        help='Project root directory (auto-detected if omitted)')
    args = parser.parse_args()

    # Auto-detect root
    if args.root:
        root = os.path.abspath(args.root)
    else:
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

    if not os.path.isfile(os.path.join(root, 'CMakeLists.txt')):
        print(f'[ERROR] CMakeLists.txt not found in {root}')
        return 1

    print(f'[*] VSEPR-SIM Layering Report Generator')
    print(f'[*] Root: {root}')
    print()

    # 1. Walk source tree
    print('[*] Scanning source tree...')
    stats = walk_source_tree(root)
    total = sum(s['files'] for s in stats.values())
    print(f'[OK] {total} source files classified across {len(stats)} layers')

    # 2. Parse layer manifest
    print('[*] Parsing layer manifest...')
    manifest = parse_layer_manifest(root)
    print(f'[OK] {len(manifest)} manifest entries')

    # 3. Parse boundary checks
    print('[*] Parsing boundary checks...')
    boundary_checks = parse_boundary_checks(root)
    print(f'[OK] {len(boundary_checks)} boundary checks')

    # 4. Parse gate functions
    print('[*] Parsing gate functions...')
    gates = parse_gates(root)
    print(f'[OK] {len(gates)} gate functions')

    # 5. Parse CMake libraries
    print('[*] Parsing CMake libraries...')
    cmake_libs = parse_cmake_libraries(root)
    print(f'[OK] {len(cmake_libs)} library targets')

    # 6. Parse report modules
    print('[*] Parsing report modules...')
    report_modules = parse_report_modules(root)
    print(f'[OK] {len(report_modules)} report modules')

    # 7. Write data file
    print()
    out_path = os.path.join(root, 'reporting', 'layering_data.tex')
    write_data_file(out_path, stats, manifest, boundary_checks,
                    gates, cmake_libs, report_modules, root)

    print()
    print('[DONE] Run scripts/build_layering_report.ps1 to compile the PDF.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
