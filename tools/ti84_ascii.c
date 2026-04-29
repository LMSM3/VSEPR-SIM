/**
 * ti84_ascii.c - TI-84 ASCII String <-> List Encoder
 * VSEPR-Sim Tools
 *
 * Encodes ASCII strings into TI-84 integer list notation for storage
 * in named lists (LUNIT, LCONF, etc.), and decodes lists back to strings.
 *
 * Build:
 *   gcc -O2 -std=c11 -Wall -Wextra -o ti84_ascii ti84_ascii.c
 *
 * Usage:
 *   ./ti84_ascii encode "OHMS"          -> {79,72,77,83}
 *   ./ti84_ascii decode 79 72 77 83     -> OHMS
 *   ./ti84_ascii table                  -> print full TI-ASCII code table
 *
 * TI-84 convention:
 *   char()  converts integer -> character
 *   ord()   converts character -> integer
 *   Codes 32-126 = printable ASCII (standard)
 *   Codes 0-31, 127+ = control/extended (avoid in user strings)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define VERSION "1.0.0"
#define MAX_STR 100  /* TI-84 Str variables cap around 99 chars */

/* ========================================================================= */
/*  Encode: ASCII string -> TI-84 list notation                             */
/* ========================================================================= */

static void encode(const char *s)
{
    size_t len = strlen(s);
    if (len == 0) {
        fprintf(stderr, "Error: empty string\n");
        return;
    }
    if (len > MAX_STR) {
        fprintf(stderr, "Warning: string exceeds TI-84 Str limit (%d chars)\n",
                MAX_STR);
    }

    /* TI-BASIC list literal: {n1,n2,n3,...} */
    printf("{");
    for (size_t i = 0; i < len; i++) {
        if (i > 0) printf(",");
        printf("%d", (unsigned char)s[i]);
    }
    printf("}\n");

    /* Also print the assignment line ready to paste */
    printf("; TI-BASIC: {");
    for (size_t i = 0; i < len; i++) {
        if (i > 0) printf(",");
        printf("%d", (unsigned char)s[i]);
    }
    printf("}->LSTR\n");

    /* Character breakdown */
    printf("; Breakdown: ");
    for (size_t i = 0; i < len; i++) {
        if (i > 0) printf(", ");
        printf("'%c'=%d", s[i], (unsigned char)s[i]);
    }
    printf("\n");
}

/* ========================================================================= */
/*  Decode: integer codes -> ASCII string                                    */
/* ========================================================================= */

static void decode(int argc, char **argv, int start)
{
    printf("\"");
    for (int i = start; i < argc; i++) {
        int code = atoi(argv[i]);
        if (code < 32 || code > 126) {
            printf("?");
            fprintf(stderr, "Warning: code %d is non-printable\n", code);
        } else {
            printf("%c", (char)code);
        }
    }
    printf("\"\n");
}

/* ========================================================================= */
/*  Table: print all printable TI-ASCII codes                                */
/* ========================================================================= */

static void print_table(void)
{
    printf("TI-84 ASCII Code Table (printable range 32-126)\n");
    printf("================================================\n");
    printf("Code  Char  |  Code  Char  |  Code  Char\n");
    printf("------+-----+-------+-----+-------+-----\n");

    for (int i = 32; i <= 126; i++) {
        printf(" %3d   '%c'", i, (char)i);
        if ((i - 32) % 3 == 2 || i == 126)
            printf("\n");
        else
            printf("  |  ");
    }

    printf("\n");
    printf("Key codes for getKey (common):\n");
    printf("  105 = Enter\n");
    printf("   22 = 2nd\n");
    printf("   45 = Clear\n");
    printf("   55 = Del\n");
    printf("   25 = Up     34 = Down\n");
    printf("   24 = Left   26 = Right\n");
    printf("  102 = 0     92 = 1     93 = 2     94 = 3\n");
    printf("   82 = 4     83 = 5     84 = 6\n");
    printf("   72 = 7     73 = 8     74 = 9\n");
    printf("   95 = (-)    104 = .    85 = )\n");
}

/* ========================================================================= */
/*  Validate: check a string for TI-84 compatibility                         */
/* ========================================================================= */

static void validate(const char *s)
{
    size_t len = strlen(s);
    int ok = 1;

    printf("Validating: \"%s\" (%zu chars)\n", s, len);

    if (len > MAX_STR) {
        printf("  FAIL: exceeds %d char Str limit\n", MAX_STR);
        ok = 0;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c > 126) {
            printf("  FAIL: char[%zu] = %d (0x%02X) non-printable\n",
                   i, c, c);
            ok = 0;
        }
    }

    if (ok) {
        printf("  OK: all chars printable, length within limit\n");
    }
}

/* ========================================================================= */
/*  Help                                                                     */
/* ========================================================================= */

static void print_help(const char *prog)
{
    fprintf(stderr, "ti84_ascii v%s - TI-84 String <-> List Encoder\n\n", VERSION);
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s encode <string>          Encode string to TI-84 list\n", prog);
    fprintf(stderr, "  %s decode <n1> <n2> ...     Decode integer codes to string\n", prog);
    fprintf(stderr, "  %s validate <string>        Check TI-84 string compatibility\n", prog);
    fprintf(stderr, "  %s table                    Print ASCII + getKey code table\n", prog);
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s encode \"OHMS\"             -> {79,72,77,83}\n", prog);
    fprintf(stderr, "  %s decode 79 72 77 83        -> OHMS\n", prog);
    fprintf(stderr, "\nConvention:\n");
    fprintf(stderr, "  K     = global key buffer (reserved, getKey->K)\n");
    fprintf(stderr, "  LCONF = global config list (index-mapped defaults)\n");
    fprintf(stderr, "  999   = sentinel (triggers default load at Prompt)\n");
}

/* ========================================================================= */
/*  Main                                                                     */
/* ========================================================================= */

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "encode") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: encode requires a string argument\n");
            return 1;
        }
        encode(argv[2]);
    } else if (strcmp(argv[1], "decode") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: decode requires integer arguments\n");
            return 1;
        }
        decode(argc, argv, 2);
    } else if (strcmp(argv[1], "validate") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: validate requires a string argument\n");
            return 1;
        }
        validate(argv[2]);
    } else if (strcmp(argv[1], "table") == 0) {
        print_table();
    } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_help(argv[0]);
        return 1;
    }

    return 0;
}
