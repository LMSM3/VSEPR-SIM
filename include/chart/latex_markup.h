/*
 * latex_markup.h — Permanent hardcoded LaTeX revision-markup colour system
 * =========================================================================
 *
 * Provides a fixed, non-negotiable colour scheme for LaTeX document edits:
 *
 *     GREEN   (#27AE60) — additions / new text
 *     RED     (#E74C3C) — deletions / removed text
 *     YELLOW  (#F1C40F) — edits / modified text (with dark bg for readability)
 *     CYAN    (#1ABC9C) — figure references and figure environments
 *     DARKRED (#C0392B) — chart references and chart environments
 *
 * These colours are PERMANENT.  They must not be changed.  Every LaTeX
 * document produced by VSEPR-SIM uses this exact colour mapping for
 * revision tracking.
 *
 * The header provides:
 *   1. A LaTeX preamble string (LATEX_MARKUP_PREAMBLE) that defines
 *      the colours and the \vsadd, \vsdel, \vsedit, \vsfig, \vschart macros.
 *   2. C functions that wrap text in these macros for programmatic use.
 *   3. A preamble injector that inserts the block into a .tex file.
 *
 * Integration:
 *   - C/C++ report generators: #include this header, call latex_markup_*()
 *   - Shell scripts: use inject_latex_markup_preamble() or the companion .sh
 *   - Python: use pykernel/latex_markup.py (identical colour definitions)
 *
 * VSEPR-SIM 3.0.0 — permanent infrastructure
 */

#pragma once
#ifndef VSEPR_LATEX_MARKUP_H
#define VSEPR_LATEX_MARKUP_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Hardcoded colour hex values — DO NOT CHANGE
 * ====================================================================== */
#define MARKUP_GREEN_HEX    "27AE60"
#define MARKUP_RED_HEX      "E74C3C"
#define MARKUP_YELLOW_HEX   "F1C40F"
#define MARKUP_YELLOW_BG    "FEF9E7"
#define MARKUP_CYAN_HEX     "1ABC9C"
#define MARKUP_DARKRED_HEX  "C0392B"

/* =========================================================================
 * LaTeX preamble block
 *
 * Drop this verbatim into any .tex preamble (after \usepackage{xcolor}).
 * Defines five named colours and five semantic markup commands:
 *   \vsadd{text}    — green highlight for additions
 *   \vsdel{text}    — red strikethrough for deletions
 *   \vsedit{text}   — yellow-highlighted edit marker
 *   \vsfig{text}    — cyan for figure references
 *   \vschart{text}  — dark red for chart references
 *
 * Also defines framed environments for block-level markup:
 *   \begin{vsaddblock}...\end{vsaddblock}
 *   \begin{vsdelblock}...\end{vsdelblock}
 *   \begin{vseditblock}...\end{vseditblock}
 *   \begin{vsfigblock}...\end{vsfigblock}
 *   \begin{vschartblock}...\end{vschartblock}
 * ====================================================================== */
static const char LATEX_MARKUP_PREAMBLE[] =
    "%% ═══════════════════════════════════════════════════════════════════\n"
    "%% VSEPR-SIM Revision Markup — permanent colour definitions\n"
    "%% DO NOT EDIT these definitions.  They are hardcoded project-wide.\n"
    "%% ═══════════════════════════════════════════════════════════════════\n"
    "\\usepackage{xcolor}\n"
    "\\usepackage{soul}\n"
    "\\usepackage[normalem]{ulem}\n"
    "\\usepackage{tcolorbox}\n"
    "\\tcbuselibrary{skins,breakable}\n"
    "\n"
    "%% ── Hardcoded colours ──────────────────────────────────────────────\n"
    "\\definecolor{vsgreen}{HTML}{27AE60}     %% additions\n"
    "\\definecolor{vsred}{HTML}{E74C3C}       %% deletions\n"
    "\\definecolor{vsyellow}{HTML}{F1C40F}    %% edits\n"
    "\\definecolor{vsyellowbg}{HTML}{FEF9E7}  %% edit background\n"
    "\\definecolor{vscyan}{HTML}{1ABC9C}      %% figures\n"
    "\\definecolor{vsdarkred}{HTML}{C0392B}   %% charts\n"
    "\n"
    "%% ── Inline markup commands ────────────────────────────────────────\n"
    "\\newcommand{\\vsadd}[1]{\\textcolor{vsgreen}{\\textbf{+}~#1}}\n"
    "\\newcommand{\\vsdel}[1]{\\textcolor{vsred}{\\sout{#1}}}\n"
    "\\newcommand{\\vsedit}[1]{\\sethlcolor{vsyellowbg}\\hl{\\textcolor{black}{#1}}}\n"
    "\\newcommand{\\vsfig}[1]{\\textcolor{vscyan}{\\textit{[Fig]~#1}}}\n"
    "\\newcommand{\\vschart}[1]{\\textcolor{vsdarkred}{\\textit{[Chart]~#1}}}\n"
    "\n"
    "%% ── Margin annotations ────────────────────────────────────────────\n"
    "\\newcommand{\\vsaddnote}[1]{\\marginpar{\\tiny\\textcolor{vsgreen}{+#1}}}\n"
    "\\newcommand{\\vsdelnote}[1]{\\marginpar{\\tiny\\textcolor{vsred}{-#1}}}\n"
    "\\newcommand{\\vseditnote}[1]{\\marginpar{\\tiny\\textcolor{vsyellow}{\\textrm{#1}}}}\n"
    "\n"
    "%% ── Block-level environments (tcolorbox) ──────────────────────────\n"
    "\\newtcolorbox{vsaddblock}{%\n"
    "  colback=vsgreen!8, colframe=vsgreen, title=\\textbf{Addition},\n"
    "  fonttitle=\\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}\n"
    "\\newtcolorbox{vsdelblock}{%\n"
    "  colback=vsred!8, colframe=vsred, title=\\textbf{Deletion},\n"
    "  fonttitle=\\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}\n"
    "\\newtcolorbox{vseditblock}{%\n"
    "  colback=vsyellowbg, colframe=vsyellow, title=\\textbf{Edit},\n"
    "  fonttitle=\\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}\n"
    "\\newtcolorbox{vsfigblock}{%\n"
    "  colback=vscyan!8, colframe=vscyan, title=\\textbf{Figure},\n"
    "  fonttitle=\\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}\n"
    "\\newtcolorbox{vschartblock}{%\n"
    "  colback=vsdarkred!8, colframe=vsdarkred, title=\\textbf{Chart},\n"
    "  fonttitle=\\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}\n"
    "%% ═══════════════════════════════════════════════════════════════════\n"
    "%% END VSEPR-SIM Revision Markup\n"
    "%% ═══════════════════════════════════════════════════════════════════\n"
    "\n";

/* =========================================================================
 * Inline markup string builders
 *
 * Each returns the number of bytes written to buf (excluding NUL).
 * If buf is NULL, returns the required size.
 * ====================================================================== */

static inline int latex_markup_add(char *buf, size_t sz, const char *text) {
    return snprintf(buf, sz, "\\vsadd{%s}", text);
}
static inline int latex_markup_del(char *buf, size_t sz, const char *text) {
    return snprintf(buf, sz, "\\vsdel{%s}", text);
}
static inline int latex_markup_edit(char *buf, size_t sz, const char *text) {
    return snprintf(buf, sz, "\\vsedit{%s}", text);
}
static inline int latex_markup_fig(char *buf, size_t sz, const char *text) {
    return snprintf(buf, sz, "\\vsfig{%s}", text);
}
static inline int latex_markup_chart(char *buf, size_t sz, const char *text) {
    return snprintf(buf, sz, "\\vschart{%s}", text);
}

/* =========================================================================
 * Block-level wrappers
 * ====================================================================== */

static inline int latex_markup_add_block(char *buf, size_t sz, const char *body) {
    return snprintf(buf, sz,
        "\\begin{vsaddblock}\n%s\n\\end{vsaddblock}\n", body);
}
static inline int latex_markup_del_block(char *buf, size_t sz, const char *body) {
    return snprintf(buf, sz,
        "\\begin{vsdelblock}\n%s\n\\end{vsdelblock}\n", body);
}
static inline int latex_markup_edit_block(char *buf, size_t sz, const char *body) {
    return snprintf(buf, sz,
        "\\begin{vseditblock}\n%s\n\\end{vseditblock}\n", body);
}
static inline int latex_markup_fig_block(char *buf, size_t sz, const char *body) {
    return snprintf(buf, sz,
        "\\begin{vsfigblock}\n%s\n\\end{vsfigblock}\n", body);
}
static inline int latex_markup_chart_block(char *buf, size_t sz, const char *body) {
    return snprintf(buf, sz,
        "\\begin{vschartblock}\n%s\n\\end{vschartblock}\n", body);
}

/* =========================================================================
 * Preamble writer — write the preamble block to a FILE*
 * ====================================================================== */
static inline void latex_markup_write_preamble(FILE *fp) {
    fputs(LATEX_MARKUP_PREAMBLE, fp);
}

/* =========================================================================
 * Preamble injector
 *
 * Reads *src_path*, injects LATEX_MARKUP_PREAMBLE after the first
 * \documentclass line, writes the result to *dst_path*.
 * Returns 0 on success, -1 on error.
 *
 * If the preamble marker ("VSEPR-SIM Revision Markup") already exists
 * in the file, injection is skipped (idempotent).
 * ====================================================================== */
static inline int inject_latex_markup_preamble(const char *src_path,
                                                const char *dst_path) {
    FILE *fin = fopen(src_path, "r");
    if (!fin) return -1;

    /* Read entire file into memory */
    fseek(fin, 0, SEEK_END);
    long len = ftell(fin);
    rewind(fin);
    if (len <= 0) { fclose(fin); return -1; }

    char *content = (char *)malloc((size_t)len + 1);
    if (!content) { fclose(fin); return -1; }
    size_t nr = fread(content, 1, (size_t)len, fin);
    content[nr] = '\0';
    fclose(fin);

    /* Check idempotency */
    if (strstr(content, "VSEPR-SIM Revision Markup")) {
        /* Already injected — just copy unchanged */
        FILE *fout = fopen(dst_path, "w");
        if (!fout) { free(content); return -1; }
        fputs(content, fout);
        fclose(fout);
        free(content);
        return 0;
    }

    /* Find insertion point: after first \documentclass line */
    char *insert_pt = strstr(content, "\\documentclass");
    if (!insert_pt) {
        /* No \documentclass — prepend */
        FILE *fout = fopen(dst_path, "w");
        if (!fout) { free(content); return -1; }
        fputs(LATEX_MARKUP_PREAMBLE, fout);
        fputs(content, fout);
        fclose(fout);
        free(content);
        return 0;
    }

    /* Skip to end of that line */
    char *eol = strchr(insert_pt, '\n');
    if (!eol) eol = content + nr;
    else eol++;  /* past the newline */

    FILE *fout = fopen(dst_path, "w");
    if (!fout) { free(content); return -1; }
    fwrite(content, 1, (size_t)(eol - content), fout);
    fputs("\n", fout);
    fputs(LATEX_MARKUP_PREAMBLE, fout);
    fputs(eol, fout);
    fclose(fout);
    free(content);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* VSEPR_LATEX_MARKUP_H */
