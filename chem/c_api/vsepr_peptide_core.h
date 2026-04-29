/* vsepr_peptide_core.h
   Day 48A - Organic / Peptide Formation Core
   C-compatible interface for VSEPR-SIM

   This is the low-level shared ABI layer.
   All structures are POD, all functions are extern "C".
   No hidden state. Every field is inspectable and deterministic.
*/
#ifndef VSEPR_PEPTIDE_CORE_H
#define VSEPR_PEPTIDE_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* -----------------------------
   Fundamental limits
----------------------------- */
#define VSEPR_MAX_ATOMS_PER_UNIT        128
#define VSEPR_MAX_RESIDUES_PER_CHAIN    4096
#define VSEPR_MAX_BONDS_PER_CHAIN       8192
#define VSEPR_MAX_NAME_LEN              32

/* -----------------------------
   Chemical enums
----------------------------- */
typedef enum VSEPR_Hybridization {
    VSEPR_HYB_UNKNOWN = 0,
    VSEPR_HYB_SP,
    VSEPR_HYB_SP2,
    VSEPR_HYB_SP3,
    VSEPR_HYB_AROMATIC
} VSEPR_Hybridization;

typedef enum VSEPR_ChemRole {
    VSEPR_ROLE_NONE = 0,
    VSEPR_ROLE_BACKBONE_N,
    VSEPR_ROLE_ALPHA_C,
    VSEPR_ROLE_CARBONYL_C,
    VSEPR_ROLE_CARBONYL_O,
    VSEPR_ROLE_AMIDE_N,
    VSEPR_ROLE_SIDECHAIN,
    VSEPR_ROLE_HYDROGEN_DONOR,
    VSEPR_ROLE_HYDROGEN_ACCEPTOR,
    VSEPR_ROLE_SULFUR_LINKABLE
} VSEPR_ChemRole;

typedef enum VSEPR_FormationClass {
    VSEPR_FORM_UNKNOWN = 0,
    VSEPR_FORM_INORGANIC,
    VSEPR_FORM_ORGANIC,
    VSEPR_FORM_PEPTIDE,
    VSEPR_FORM_PROTEIN_LIKE,
    VSEPR_FORM_HYBRID_ORGANIC_METAL
} VSEPR_FormationClass;

typedef enum VSEPR_FormationState {
    VSEPR_STATE_PREFORM = 0,
    VSEPR_STATE_LOCAL_GROUP_FORMED,
    VSEPR_STATE_MOLECULE_FORMED,
    VSEPR_STATE_LINKED,
    VSEPR_STATE_CONFORMATIONAL_SOLVE,
    VSEPR_STATE_LOCAL_FOLDED,
    VSEPR_STATE_MACRO_STABLE,
    VSEPR_STATE_DEGRADED
} VSEPR_FormationState;

typedef enum VSEPR_SidechainClass {
    VSEPR_SIDECHAIN_NONE = 0,
    VSEPR_SIDECHAIN_HYDROPHOBIC,
    VSEPR_SIDECHAIN_POLAR,
    VSEPR_SIDECHAIN_ACIDIC,
    VSEPR_SIDECHAIN_BASIC,
    VSEPR_SIDECHAIN_AROMATIC,
    VSEPR_SIDECHAIN_SULFUR
} VSEPR_SidechainClass;

typedef enum VSEPR_EnvironmentClass {
    VSEPR_ENV_UNKNOWN = 0,
    VSEPR_ENV_VACUUM,
    VSEPR_ENV_DRY_CONDENSED,
    VSEPR_ENV_POLAR_SOLVENT,
    VSEPR_ENV_NONPOLAR_SOLVENT,
    VSEPR_ENV_REACTIVE_FIELD,
    VSEPR_ENV_RADIATIVE
} VSEPR_EnvironmentClass;

/* -----------------------------
   Geometric primitives
----------------------------- */
typedef struct VSEPR_Vec3 {
    double x;
    double y;
    double z;
} VSEPR_Vec3;

/* -----------------------------
   Atom-level data
----------------------------- */
typedef struct VSEPR_ChemAtom {
    int32_t atom_id;
    int32_t atomic_number;
    int32_t isotope;
    int32_t formal_charge;

    char atom_name[VSEPR_MAX_NAME_LEN];
    char element_symbol[8];

    VSEPR_Vec3 position;
    VSEPR_Vec3 velocity;

    int32_t bond_count;
    int32_t bond_ids[8];

    int32_t bond_order_flags[8];

    int32_t residue_id;
    int32_t molecule_id;

    VSEPR_Hybridization hybridization;
    VSEPR_ChemRole chem_role;

    double partial_charge;
    double covalent_radius_pm;
    double vdw_radius_pm;
    double mass_u;
} VSEPR_ChemAtom;

/* -----------------------------
   Residue / monomer unit
----------------------------- */
typedef struct VSEPR_ResidueUnit {
    int32_t residue_id;
    char residue_name[16];

    int32_t atom_ids[VSEPR_MAX_ATOMS_PER_UNIT];
    int32_t atom_count;

    int32_t backbone_N;
    int32_t backbone_CA;
    int32_t backbone_C;
    int32_t backbone_O;

    int32_t sidechain_root;

    int32_t charge_state;
    VSEPR_SidechainClass sidechain_class;

    double phi_deg;
    double psi_deg;
    double omega_deg;
} VSEPR_ResidueUnit;

/* -----------------------------
   Bond pair
----------------------------- */
typedef struct VSEPR_BondPair {
    int32_t a;
    int32_t b;
    int32_t order;
} VSEPR_BondPair;

/* -----------------------------
   Peptide chain object
----------------------------- */
typedef struct VSEPR_PeptideChain {
    int32_t chain_id;
    char chain_name[VSEPR_MAX_NAME_LEN];

    int32_t residue_ids[VSEPR_MAX_RESIDUES_PER_CHAIN];
    int32_t residue_count;

    VSEPR_BondPair bonds[VSEPR_MAX_BONDS_PER_CHAIN];
    int32_t bond_count;

    VSEPR_FormationClass formation_class;
    VSEPR_FormationState formation_state;
    VSEPR_EnvironmentClass environment_class;

    double total_energy_kj_mol;
    double bond_energy_kj_mol;
    double angle_energy_kj_mol;
    double torsion_energy_kj_mol;
    double improper_energy_kj_mol;
    double vdw_energy_kj_mol;
    double coulomb_energy_kj_mol;
    double hbond_energy_kj_mol;
    double solvation_energy_kj_mol;
    double formation_energy_kj_mol;

    double steric_score;
    double hbond_score;
    double electrostatic_score;
    double hydrophobic_score;
    double planarity_score;
} VSEPR_PeptideChain;

/* -----------------------------
   Report object
----------------------------- */
typedef struct VSEPR_FormationReport {
    int32_t object_id;
    VSEPR_FormationClass formation_class;
    VSEPR_FormationState formation_state;

    int32_t atom_count;
    int32_t residue_count;
    int32_t bond_count;

    int32_t chemical_validity_pass;
    int32_t valence_validity_pass;

    double total_energy_kj_mol;
    double steric_score;
    double hbond_score;
    double planarity_score;
    double confidence_score;
} VSEPR_FormationReport;

/* -----------------------------
   C API
----------------------------- */
int vsepr_validate_residue_unit(const VSEPR_ResidueUnit* unit);
int vsepr_can_form_peptide_bond(const VSEPR_ResidueUnit* lhs,
                                const VSEPR_ResidueUnit* rhs,
                                const VSEPR_ChemAtom* atoms,
                                size_t atom_count);

int vsepr_form_peptide_bond(VSEPR_PeptideChain* chain,
                            const VSEPR_ResidueUnit* lhs,
                            const VSEPR_ResidueUnit* rhs,
                            VSEPR_ChemAtom* atoms,
                            size_t atom_count);

int vsepr_score_peptide_chain(VSEPR_PeptideChain* chain,
                              const VSEPR_ChemAtom* atoms,
                              size_t atom_count,
                              VSEPR_FormationReport* out_report);

#ifdef __cplusplus
}
#endif

#endif /* VSEPR_PEPTIDE_CORE_H */
