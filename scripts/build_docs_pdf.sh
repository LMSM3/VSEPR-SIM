#!/usr/bin/env bash
# VSEPR-SIM — LaTeX → PDF build script
# Usage: ./scripts/build_docs_pdf.sh [OPTIONS]
#
# Options:
#   -d, --docs-dir DIR    LaTeX source directory    (default: ./docs)
#   -o, --out-dir  DIR    PDF output directory      (default: ./docs/pdf)
#   -j, --jobs J          parallel pdflatex jobs    (default: nproc)
#   -f, --file FILE       compile one specific .tex file only
#   -c, --clean           remove auxiliary files after build
#   -v, --verbose         verbose pdflatex output
#   -h, --help            show this message

set -euo pipefail

readonly SCRIPT_NAME="$(basename "$0")"

# ── defaults ──────────────────────────────────────────────────────────────────
DOCS_DIR="./docs"
OUT_DIR="./docs/pdf"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
SINGLE_FILE=""
CLEAN_AUX=0
VERBOSE=0

# ── colours ───────────────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
    GRN='\033[0;32m' RED='\033[0;31m' YEL='\033[0;33m'
    CYN='\033[0;36m' BOLD='\033[1m'   RST='\033[0m'
else
    GRN='' RED='' YEL='' CYN='' BOLD='' RST=''
fi

pass() { printf "${GRN}[PDF]${RST}  %s\n" "$*"; }
fail() { printf "${RED}[FAIL]${RST} %s\n" "$*" >&2; }
info() { printf "${CYN}[INFO]${RST} %s\n" "$*"; }
step() { printf "\n${BOLD}── %s${RST}\n" "$*"; }

# ── argument parsing ──────────────────────────────────────────────────────────
usage() { grep '^#' "$0" | sed 's/^# \{0,1\}//' | tail -n +2; exit 0; }

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--docs-dir) DOCS_DIR=$2;     shift 2 ;;
        -o|--out-dir)  OUT_DIR=$2;      shift 2 ;;
        -j|--jobs)     JOBS=$2;         shift 2 ;;
        -f|--file)     SINGLE_FILE=$2;  shift 2 ;;
        -c|--clean)    CLEAN_AUX=1;     shift   ;;
        -v|--verbose)  VERBOSE=1;       shift   ;;
        -h|--help)     usage ;;
        *) printf '%s: unknown option: %s\n' "$SCRIPT_NAME" "$1" >&2; exit 1 ;;
    esac
done

# ── environment check ─────────────────────────────────────────────────────────
check_deps() {
    local ok=1
    for bin in pdflatex; do
        if ! command -v "$bin" >/dev/null 2>&1; then
            fail "Required tool not found: $bin"
            ok=0
        fi
    done
    [[ $ok -eq 1 ]] || exit 1
}

# ── compile one .tex file ─────────────────────────────────────────────────────
# compile_tex SRC_TEX WORK_DIR OUT_DIR
compile_tex() {
    local src="$1"
    local work="$2"
    local outd="$3"
    local base
    base=$(basename "$src" .tex)
    local log="$work/${base}.log"

    mkdir -p "$work"

    local latex_flags=(-halt-on-error -interaction=nonstopmode
                       -output-directory "$work")
    [[ $VERBOSE -eq 1 ]] || latex_flags+=(-quiet)

    # Two passes for cross-references / ToC
    if pdflatex "${latex_flags[@]}" "$src" > "$log" 2>&1 && \
       pdflatex "${latex_flags[@]}" "$src" >> "$log" 2>&1; then
        cp "$work/${base}.pdf" "$outd/${base}.pdf"
        pass "$base.pdf"
        if [[ $CLEAN_AUX -eq 1 ]]; then
            rm -f "$work/${base}".{aux,log,toc,out,fls,fdb_latexmk}
        fi
        return 0
    else
        fail "$base — see $log"
        tail -20 "$log" >&2
        return 1
    fi
}

# ── build all docs ────────────────────────────────────────────────────────────
build_all() {
    step "Building LaTeX documents → PDF"

    check_deps

    if [[ ! -d "$DOCS_DIR" ]]; then
        fail "Docs directory not found: $DOCS_DIR"
        exit 1
    fi

    mkdir -p "$OUT_DIR"

    local -a targets=()

    if [[ -n "$SINGLE_FILE" ]]; then
        [[ -f "$SINGLE_FILE" ]] || { fail "File not found: $SINGLE_FILE"; exit 1; }
        targets=("$SINGLE_FILE")
    else
        # Discover all .tex files with a \documentclass preamble
        while IFS= read -r -d '' f; do
            grep -q '\\documentclass' "$f" 2>/dev/null && targets+=("$f")
        done < <(find "$DOCS_DIR" -maxdepth 1 -name '*.tex' -print0 | sort -z)
    fi

    if [[ ${#targets[@]} -eq 0 ]]; then
        info "No standalone .tex files found in $DOCS_DIR"
        exit 0
    fi

    info "Found ${#targets[@]} document(s) to compile"

    local pass_count=0 fail_count=0
    local work_base
    work_base=$(mktemp -d)
    trap 'rm -rf "$work_base"' EXIT

    # Compile in parallel up to $JOBS
    local slot=0
    local -a pids=()
    local -a names=()

    for src in "${targets[@]}"; do
        local base
        base=$(basename "$src" .tex)
        local work="$work_base/$base"

        compile_tex "$src" "$work" "$OUT_DIR" &
        pids+=($!)
        names+=("$base")

        slot=$(( slot + 1 ))
        if [[ $slot -ge $JOBS ]]; then
            wait "${pids[0]}" && pass_count=$(( pass_count + 1 )) \
                              || fail_count=$(( fail_count + 1 ))
            pids=("${pids[@]:1}")
            names=("${names[@]:1}")
            slot=$(( slot - 1 ))
        fi
    done

    # Drain remaining jobs
    for pid in "${pids[@]}"; do
        wait "$pid" && pass_count=$(( pass_count + 1 )) \
                    || fail_count=$(( fail_count + 1 ))
    done

    step "PDF Build Summary"
    printf '  Compiled : %b%d%b\n' "$GRN" "$pass_count" "$RST"
    printf '  Failed   : %b%d%b\n' "$RED" "$fail_count" "$RST"
    printf '  Output   : %s\n'     "$OUT_DIR"

    [[ $fail_count -eq 0 ]]
}

main() { build_all; }
trap 'printf "\n%bInterrupted%b\n" "$RED" "$RST"; exit 130' INT TERM
main "$@"
