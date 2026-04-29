/*
 * ti84.h - Common definitions for TI-83+/84+ native C programs
 *
 * Include in any program built with tibuild.ps1.
 * z88dk provides the actual hardware abstraction; this header
 * adds project-level conventions.
 *
 * Z80 constraints enforced:
 *   - No float == comparisons (use int sentinel)
 *   - No double (use float via --math32)
 *   - Minimal stack usage
 */

#ifndef TI84_H
#define TI84_H

#include <stdio.h>

/* --- Screen geometry --- */
#define TI_COLS_83  16      /* TI-83+: 16 columns */
#define TI_COLS_84  26      /* TI-84+: 26 columns */
#define TI_ROWS     8       /* Both: 8 rows in home screen */

/* --- Sentinel value (convention: integer comparison only) --- */
#define SENTINEL 999

/* --- Clear screen (ANSI escape, z88dk maps to ClrHome bcall) --- */
#define clrhome()  printf("\x1b[2J")

/* --- Keyboard polling (z88dk) --- */
extern int __LIB__ getk(void);

/* --- Key wait: waits for press then release (debounce) --- */
static void waitkey(void)
{
    while (getk()) ;    /* drain held key */
    while (!getk()) ;   /* wait for press */
    while (getk()) ;    /* wait for release */
}

/* --- Integer input with sentinel default --- */
static int iinput(const char *label, int def)
{
    int v;
    printf("%s(%d)?", label, def);
    if (scanf("%d", &v) != 1) return def;
    if (v == SENTINEL) {
        printf("=%d\n", def);
        return def;
    }
    return v;
}

/* --- Percentage input: enter 70 to get 0.70f --- */
static float pctinput(const char *label, int def_pct)
{
    int v;
    printf("%s(%d%%)?", label, def_pct);
    if (scanf("%d", &v) != 1) return (float)def_pct / 100.0f;
    if (v == SENTINEL) {
        printf("=%d%%\n", def_pct);
        return (float)def_pct / 100.0f;
    }
    return (float)v / 100.0f;
}

#endif /* TI84_H */
