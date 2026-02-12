/**
 * LabelMe - Molecular State Labeling Engine
 * VSEPR-Sim v2.3.1
 * 
 * Labels molecular phase state (SOLID, LIQUID, GAS, PLASMA) based on temperature
 * 
 * Build:
 *   gcc -O2 -std=c11 -Wall -Wextra -o labelme labelme.c
 * 
 * Usage:
 *   ./labelme states_db.csv H2O 298.15
 * 
 * Output:
 *   molecule,tempK,state,meltK,boilK,plasmaK
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_MAX_LEN 512
#define VERSION "2.3.1"

static void trim(char *s) {
    // Trim leading whitespace
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    // Trim trailing whitespace
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static int is_comment_or_empty(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return (*s == '\0' || *s == '#');
}

static int split_csv_4(char *line, char **a, char **b, char **c, char **d) {
    // Minimal CSV parser: no quotes, just comma-separated
    char *p1 = strchr(line, ','); if (!p1) return 0;
    *p1++ = '\0';
    char *p2 = strchr(p1, ','); if (!p2) return 0;
    *p2++ = '\0';
    char *p3 = strchr(p2, ','); if (!p3) return 0;
    *p3++ = '\0';

    *a = line; *b = p1; *c = p2; *d = p3;
    trim(*a); trim(*b); trim(*c); trim(*d);
    return 1;
}

static const char* label_state(double T, double melt, double boil, double plasma) {
    if (T >= plasma) return "PLASMA";
    
    // Normal case: melt < boil
    if (boil >= melt) {
        if (T < melt) return "SOLID";
        if (T < boil) return "LIQUID";
        return "GAS";
    } else {
        // Sublimation case (e.g., CO2): boil < melt
        // Simplified: below boil = SOLID, above = GAS
        if (T < boil) return "SOLID";
        return "GAS";
    }
}

static void print_help(const char *prog) {
    fprintf(stderr, "LabelMe v%s - Molecular State Labeling Engine\n", VERSION);
    fprintf(stderr, "\nUsage: %s <states_db.csv> <molecule_name> <temp_K>\n", prog);
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s data/states_db.csv H2O 298.15\n", prog);
    fprintf(stderr, "\nOutput: molecule,tempK,state,meltK,boilK,plasmaK\n");
}

int main(int argc, char **argv) {
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help(argv[0]);
        return 0;
    }
    
    if (argc != 4) {
        print_help(argv[0]);
        return 2;
    }

    const char *db_path = argv[1];
    const char *query_name = argv[2];
    char *endptr = NULL;
    double T = strtod(argv[3], &endptr);
    
    if (!endptr || *endptr != '\0') {
        fprintf(stderr, "Error: temp_K must be a number (got '%s')\n", argv[3]);
        return 2;
    }

    FILE *f = fopen(db_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open database '%s': ", db_path);
        perror("");
        return 2;
    }

    char line[LINE_MAX_LEN];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        line[strcspn(line, "\r\n")] = '\0';
        
        if (is_comment_or_empty(line)) continue;

        char *name=NULL, *melt_s=NULL, *boil_s=NULL, *plasma_s=NULL;
        if (!split_csv_4(line, &name, &melt_s, &boil_s, &plasma_s)) continue;

        if (strcmp(name, query_name) != 0) continue;

        // Parse transition temperatures
        char *e1=NULL, *e2=NULL, *e3=NULL;
        double melt = strtod(melt_s, &e1);
        double boil = strtod(boil_s, &e2);
        double plasma = strtod(plasma_s, &e3);

        if (!e1 || *e1!='\0' || !e2 || *e2!='\0' || !e3 || *e3!='\0') {
            fprintf(stderr, "Error: Invalid numeric values in database for '%s'\n", name);
            fclose(f);
            return 2;
        }

        const char *state = label_state(T, melt, boil, plasma);

        // Output: molecule,tempK,state,meltK,boilK,plasmaK
        printf("%s,%.6f,%s,%.6f,%.6f,%.6f\n", name, T, state, melt, boil, plasma);
        found = 1;
        break;
    }

    fclose(f);

    if (!found) {
        fprintf(stderr, "Error: Molecule '%s' not found in database\n", query_name);
        return 1;
    }

    return 0;
}
