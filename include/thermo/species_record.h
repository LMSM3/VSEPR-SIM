/*
 * species_record.h — C clone of pykernel/species_record.py
 * =========================================================
 *
 * Bit-for-bit functional clone of the Python species record schema and
 * Shomate evaluation engine.  Pure C99, no dynamic allocation in the hot
 * path, no hidden state.
 *
 * Mirrors (exactly):
 *   SpeciesRecord          → species_rec_t
 *   ShomateRegion          → shomate_region_t  (+ cp / enthalpy / entropy)
 *   AtomEntry              → atom_entry_t
 *   StructureModel         → structure_model_t
 *   ReferenceState         → reference_state_t
 *   ThermoReference        → thermo_ref_t
 *   EngineFlags            → engine_flags_t
 *   parse_vsepr_text()     → species_parse_vsepr()
 *   to_vsepr_text()        → species_to_vsepr()
 *   cp() / enthalpy() /
 *   entropy() / gibbs()    → shomate_cp / shomate_H / shomate_S / shomate_G
 *
 * Chart / figure freeze:
 *   chart_palette_t        → frozen PALETTE from chart_helpers.py
 *   chart_line_csv()       → write a two-column CSV for a Cp(T) curve
 *   chart_multiline_csv()  → write multi-species comparison CSV
 *
 * VSEPR-SIM 3.0.0 — report subsystem
 */

#pragma once
#ifndef VSEPR_SPECIES_RECORD_H
#define VSEPR_SPECIES_RECORD_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Compile-time limits
 * ====================================================================== */
#define SR_ID_LEN        64
#define SR_NAME_LEN      128
#define SR_FORMULA_LEN   32
#define SR_STR_LEN       128
#define SR_MAX_ATOMS     32
#define SR_MAX_REGIONS   8

/* =========================================================================
 * Frozen chart palette  (mirrors PALETTE in chart_helpers.py)
 * ====================================================================== */
typedef struct { const char *name; const char *hex; } chart_color_t;

static const chart_color_t CHART_PALETTE[] = {
    { "blue",   "#2e86c1" },
    { "red",    "#e74c3c" },
    { "green",  "#27ae60" },
    { "purple", "#8e44ad" },
    { "orange", "#f39c12" },
    { "teal",   "#1abc9c" },
    { "navy",   "#1a5276" },
    { "pink",   "#e91e8e" },
    { "gray",   "#7f8c8d" },
    { "dark",   "#2c3e50" },
    { "light",  "#ecf0f1" },
    { "gold",   "#d4ac0d" },
    { NULL,     NULL      }
};

/* PALETTE_CYCLE — ordered for multi-series charts */
static const char *CHART_PALETTE_CYCLE[] = {
    "#e74c3c","#2e86c1","#27ae60","#f39c12","#8e44ad",
    "#1abc9c","#e67e22","#3498db","#9b59b6","#1a5276",
    "#d4ac0d","#c0392b","#16a085","#2980b9","#8e44ad",
    NULL
};

/* =========================================================================
 * Shomate region  (mirrors ShomateRegion dataclass)
 *
 *   t = T_K / 1000
 *   Cp(t) = A + B*t + C*t² + D*t³ + E/t²          [J/(mol·K)]
 *   H(t)  = A*t + B*t²/2 + C*t³/3 + D*t⁴/4 - E/t + F - H  [kJ/mol]
 *   S(t)  = A*ln(t) + B*t + C*t²/2 + D*t³/3 - E/(2t²) + G  [J/(mol·K)]
 * ====================================================================== */
typedef struct {
    double Tmin_K;
    double Tmax_K;
    double A, B, C, D, E, F, G, H;
} shomate_region_t;

static inline double shomate_cp(const shomate_region_t *r, double T_K) {
    double t = T_K / 1000.0;
    return r->A + r->B*t + r->C*t*t + r->D*t*t*t + r->E/(t*t);
}

static inline double shomate_H(const shomate_region_t *r, double T_K) {
    double t = T_K / 1000.0;
    return r->A*t + (r->B*t*t)/2.0 + (r->C*t*t*t)/3.0
         + (r->D*t*t*t*t)/4.0 - r->E/t + r->F - r->H;
}

static inline double shomate_S(const shomate_region_t *r, double T_K) {
    double t = T_K / 1000.0;
    double log_t = (t > 0.0) ? log(t) : 0.0;
    return r->A*log_t + r->B*t + (r->C*t*t)/2.0
         + (r->D*t*t*t)/3.0 - r->E/(2.0*t*t) + r->G;
}

static inline double shomate_G(const shomate_region_t *r, double T_K) {
    /* G = H - T*S  (H in kJ/mol → convert to J for consistency, then back) */
    double H_kJ = shomate_H(r, T_K);
    double S_J  = shomate_S(r, T_K);
    return H_kJ - T_K * S_J / 1000.0;   /* kJ/mol */
}

/* Find the active region for T_K; returns last region if T > all Tmax */
static inline const shomate_region_t *
shomate_region_for(const shomate_region_t *regions, int n, double T_K) {
    for (int i = 0; i < n; i++) {
        if (T_K <= regions[i].Tmax_K)
            return &regions[i];
    }
    return &regions[n - 1];
}

/* =========================================================================
 * Atom entry
 * ====================================================================== */
typedef struct {
    char element[8];
    int  count;
} atom_entry_t;

/* =========================================================================
 * Structure model  (mirrors StructureModel dataclass)
 * ====================================================================== */
typedef struct {
    char category[SR_STR_LEN];         /* diatomic, linear, tetrahedral … */
    int  vsepr_domain_count;
    char geometry[SR_STR_LEN];
    int  bond_order_hint;
    int  formal_charge;
    bool radical;
    char symmetry_hint[SR_STR_LEN];    /* C2v, Td, Dinfh … */
} structure_model_t;

/* =========================================================================
 * Reference state
 * ====================================================================== */
typedef struct {
    double T_ref_K;
    double P_std_bar;
    char   standard_state_note[SR_STR_LEN];
} reference_state_t;

/* =========================================================================
 * Thermo reference  (mirrors ThermoReference dataclass)
 * ====================================================================== */
typedef struct {
    double Hf298_kJmol;
    double S298_JmolK;
} thermo_ref_t;

/* =========================================================================
 * Engine flags  (mirrors EngineFlags dataclass)
 * ====================================================================== */
typedef struct {
    bool allow_ideal_gas;
    bool allow_real_gas_upgrade;
    bool allow_dissociation;
    bool allow_ionization;
    char reactive_family[SR_STR_LEN];
} engine_flags_t;

/* =========================================================================
 * Master species record  (mirrors SpeciesRecord dataclass)
 * ====================================================================== */
typedef struct {
    /* Identity */
    char id[SR_ID_LEN];
    char name[SR_NAME_LEN];
    char formula[SR_FORMULA_LEN];
    char phase[SR_STR_LEN];
    char source_family[SR_STR_LEN];
    char source_ref[SR_STR_LEN];
    char source_url[SR_STR_LEN];

    /* Composition */
    double molar_mass_gmol;
    char   cas_number[SR_STR_LEN];
    char   inchi[SR_STR_LEN];
    char   inchikey[SR_STR_LEN];
    atom_entry_t    atoms[SR_MAX_ATOMS];
    int             n_atoms;
    structure_model_t structure;

    /* Thermochemical source */
    reference_state_t ref_state;
    thermo_ref_t      thermo_ref;

    /* Shomate regions */
    char              cp_model[SR_STR_LEN];   /* "SHOMATE" */
    shomate_region_t  regions[SR_MAX_REGIONS];
    int               n_regions;

    /* Engine metadata */
    engine_flags_t engine;
} species_rec_t;

/* =========================================================================
 * Shomate dispatch — evaluate property at T using correct region
 * ====================================================================== */
static inline double species_cp(const species_rec_t *s, double T_K) {
    const shomate_region_t *r = shomate_region_for(s->regions, s->n_regions, T_K);
    return shomate_cp(r, T_K);
}
static inline double species_H(const species_rec_t *s, double T_K) {
    const shomate_region_t *r = shomate_region_for(s->regions, s->n_regions, T_K);
    return shomate_H(r, T_K);
}
static inline double species_S(const species_rec_t *s, double T_K) {
    const shomate_region_t *r = shomate_region_for(s->regions, s->n_regions, T_K);
    return shomate_S(r, T_K);
}
static inline double species_G(const species_rec_t *s, double T_K) {
    const shomate_region_t *r = shomate_region_for(s->regions, s->n_regions, T_K);
    return shomate_G(r, T_K);
}

/* =========================================================================
 * VSEPR text serialiser  (mirrors to_vsepr_text())
 * Writes a SPECIES_BEGIN … SPECIES_END block to *fp*.
 * ====================================================================== */
static inline void species_to_vsepr(const species_rec_t *s, FILE *fp) {
    fprintf(fp, "SPECIES_BEGIN\n");
    fprintf(fp, "  id                  = %s\n", s->id);
    fprintf(fp, "  name                = %s\n", s->name);
    fprintf(fp, "  formula             = %s\n", s->formula);
    fprintf(fp, "  phase               = %s\n", s->phase);
    fprintf(fp, "  source_family       = %s\n", s->source_family);
    fprintf(fp, "  source_ref          = %s\n", s->source_ref);
    fprintf(fp, "  source_url          = %s\n", s->source_url);
    fprintf(fp, "\n");
    fprintf(fp, "  molar_mass_gmol     = %.4f\n", s->molar_mass_gmol);
    fprintf(fp, "  cas_number          = %s\n", s->cas_number);
    fprintf(fp, "  inchi               = %s\n", s->inchi);
    fprintf(fp, "  inchikey            = %s\n", s->inchikey);
    fprintf(fp, "\n");
    fprintf(fp, "  atoms = [\n");
    for (int i = 0; i < s->n_atoms; i++)
        fprintf(fp, "    {element = %s, count = %d}\n",
                s->atoms[i].element, s->atoms[i].count);
    fprintf(fp, "  ]\n\n");
    fprintf(fp, "  structure_model = {\n");
    fprintf(fp, "    category            = %s\n", s->structure.category);
    fprintf(fp, "    vsepr_domain_count  = %d\n", s->structure.vsepr_domain_count);
    fprintf(fp, "    geometry            = %s\n", s->structure.geometry);
    fprintf(fp, "    bond_order_hint     = %d\n", s->structure.bond_order_hint);
    fprintf(fp, "    formal_charge       = %d\n", s->structure.formal_charge);
    fprintf(fp, "    radical             = %s\n", s->structure.radical ? "true" : "false");
    fprintf(fp, "    symmetry_hint       = %s\n", s->structure.symmetry_hint);
    fprintf(fp, "  }\n\n");
    fprintf(fp, "  reference_state = {\n");
    fprintf(fp, "    T_ref_K             = %.2f\n", s->ref_state.T_ref_K);
    fprintf(fp, "    P_std_bar           = %.1f\n", s->ref_state.P_std_bar);
    fprintf(fp, "    standard_state_note = %s\n", s->ref_state.standard_state_note);
    fprintf(fp, "  }\n\n");
    fprintf(fp, "  thermo_reference = {\n");
    fprintf(fp, "    Hf298_kJmol         = %.4f\n", s->thermo_ref.Hf298_kJmol);
    fprintf(fp, "    S298_JmolK          = %.4f\n", s->thermo_ref.S298_JmolK);
    fprintf(fp, "  }\n\n");
    fprintf(fp, "  cp_model = %s\n\n", s->cp_model);
    fprintf(fp, "  regions = [\n");
    for (int i = 0; i < s->n_regions; i++) {
        const shomate_region_t *r = &s->regions[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      Tmin_K = %.1f\n", r->Tmin_K);
        fprintf(fp, "      Tmax_K = %.1f\n", r->Tmax_K);
        fprintf(fp, "      A = %.5f\n", r->A);
        fprintf(fp, "      B = %.5f\n", r->B);
        fprintf(fp, "      C = %.5f\n", r->C);
        fprintf(fp, "      D = %.5f\n", r->D);
        fprintf(fp, "      E = %.6f\n", r->E);
        fprintf(fp, "      F = %.5f\n", r->F);
        fprintf(fp, "      G = %.4f\n", r->G);
        fprintf(fp, "      H = %.4f\n", r->H);
        fprintf(fp, "    }%s\n", (i < s->n_regions - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n\n");
    fprintf(fp, "  engine_flags = {\n");
    fprintf(fp, "    allow_ideal_gas         = %s\n", s->engine.allow_ideal_gas ? "true":"false");
    fprintf(fp, "    allow_real_gas_upgrade  = %s\n", s->engine.allow_real_gas_upgrade ? "true":"false");
    fprintf(fp, "    allow_dissociation      = %s\n", s->engine.allow_dissociation ? "true":"false");
    fprintf(fp, "    allow_ionization        = %s\n", s->engine.allow_ionization ? "true":"false");
    fprintf(fp, "    reactive_family         = %s\n", s->engine.reactive_family);
    fprintf(fp, "  }\n");
    fprintf(fp, "SPECIES_END\n");
}

/* =========================================================================
 * Chart CSV helpers  (mirrors chart_helpers.py — frozen formatting)
 *
 * chart_line_csv()       — single species Cp(T) curve as CSV
 * chart_multiline_csv()  — multiple species Cp(T) comparison CSV
 * ====================================================================== */

static inline int chart_line_csv(const species_rec_t *s,
                                  double T_start, double T_end,
                                  int n_points, FILE *fp) {
    if (n_points < 2) return -1;
    fprintf(fp, "T_K,Cp_JmolK,H_kJmol,S_JmolK,G_kJmol\n");
    double dT = (T_end - T_start) / (n_points - 1);
    for (int i = 0; i < n_points; i++) {
        double T = T_start + i * dT;
        fprintf(fp, "%.2f,%.5f,%.5f,%.5f,%.5f\n",
                T, species_cp(s, T), species_H(s, T),
                   species_S(s, T), species_G(s, T));
    }
    return n_points;
}

typedef struct {
    const species_rec_t *rec;
    const char          *label;   /* NULL → use rec->formula */
} chart_series_t;

static inline int chart_multiline_csv(const chart_series_t *series, int n_series,
                                       double T_start, double T_end,
                                       int n_points, FILE *fp) {
    if (n_points < 2 || n_series < 1) return -1;
    fprintf(fp, "T_K");
    for (int j = 0; j < n_series; j++) {
        const char *lbl = series[j].label ? series[j].label : series[j].rec->formula;
        fprintf(fp, ",Cp_%s", lbl);
    }
    fprintf(fp, "\n");
    double dT = (T_end - T_start) / (n_points - 1);
    for (int i = 0; i < n_points; i++) {
        double T = T_start + i * dT;
        fprintf(fp, "%.2f", T);
        for (int j = 0; j < n_series; j++)
            fprintf(fp, ",%.5f", species_cp(series[j].rec, T));
        fprintf(fp, "\n");
    }
    return n_points;
}

/* =========================================================================
 * VSPES JSON output  (mirrors render_vspes_format())
 * ====================================================================== */
static inline void species_to_vspes_json(const species_rec_t *s, FILE *fp) {
    fprintf(fp, "{\n");
    fprintf(fp, "  \"id\": \"%s\",\n", s->id);
    fprintf(fp, "  \"formula\": \"%s\",\n", s->formula);
    fprintf(fp, "  \"phase\": \"%s\",\n", s->phase);
    fprintf(fp, "  \"molar_mass_gmol\": %.4f,\n", s->molar_mass_gmol);
    fprintf(fp, "  \"Hf298_kJmol\": %.4f,\n", s->thermo_ref.Hf298_kJmol);
    fprintf(fp, "  \"S298_JmolK\": %.4f,\n",  s->thermo_ref.S298_JmolK);
    fprintf(fp, "  \"cp_model\": \"%s\",\n",   s->cp_model);
    fprintf(fp, "  \"n_regions\": %d,\n",      s->n_regions);
    fprintf(fp, "  \"engine_flags\": {\n");
    fprintf(fp, "    \"allow_ideal_gas\": %s,\n",
            s->engine.allow_ideal_gas ? "true" : "false");
    fprintf(fp, "    \"reactive_family\": \"%s\"\n", s->engine.reactive_family);
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* VSEPR_SPECIES_RECORD_H */
