/* vsepr_peptide_core.h  —  LEGACY FORWARDING SHIM
 * ==================================================
 * This header is no longer the authoritative chemistry vocabulary.
 * It is kept only to preserve build compatibility for any consumer
 * that has not yet migrated to the split architecture.
 *
 * DO NOT extend this file.
 * DO NOT include it from new code.
 *
 * Authoritative replacements:
 *   chem/c_api/vsepr_chem_core.h        generic runtime primitives
 *   chem/peptide/vsepr_peptide_types.h  peptide domain vocabulary
 *
 * Migration pattern for future domain modules:
 *   generic core  ->  domain types  ->  mapping bridge  ->  runtime/parser
 *
 * VSEPR-SIM | v5.0.0-main
 */

#ifndef VSEPR_PEPTIDE_CORE_H
#define VSEPR_PEPTIDE_CORE_H

#include "vsepr_chem_core.h"
#include "../peptide/vsepr_peptide_types.h"

/* The C ABI structs (VSEPR_ChemAtom, VSEPR_ResidueUnit, VSEPR_PeptideChain,
 * VSEPR_FormationReport) and the extern "C" function declarations below are
 * peptide-domain objects and remain here until the C API consumers migrate
 * to the C++ peptide formation engine.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define VSEPR_MAX_ATOMS_PER_UNIT        128
#define VSEPR_MAX_RESIDUES_PER_CHAIN    4096
#define VSEPR_MAX_BONDS_PER_CHAIN       8192
#define VSEPR_MAX_NAME_LEN              32

typedef struct VSEPR_Vec3 {
    double x, y, z;
} VSEPR_Vec3;

typedef struct VSEPR_ChemAtom {
    int32_t atom_id;
    int32_t atomic_number;
    int32_t isotope;
    int32_t formal_charge;
    char    atom_name[VSEPR_MAX_NAME_LEN];
    char    element_symbol[8];
    VSEPR_Vec3 position;
    VSEPR_Vec3 velocity;
    int32_t bond_count;
    int32_t bond_ids[8];
    int32_t bond_order_flags[8];
    int32_t residue_id;
    int32_t molecule_id;
    VSEPR_Hybridization       hybridization;
    VSEPR_PeptideRole         chem_role;     /* was VSEPR_ChemRole — now peptide-domain */
    double partial_charge;
    double covalent_radius_pm;
    double vdw_radius_pm;
    double mass_u;
} VSEPR_ChemAtom;

typedef struct VSEPR_ResidueUnit {
    int32_t residue_id;
    char    residue_name[16];
    int32_t atom_ids[VSEPR_MAX_ATOMS_PER_UNIT];
    int32_t atom_count;
    int32_t backbone_N;
    int32_t backbone_CA;
    int32_t backbone_C;
    int32_t backbone_O;
    int32_t sidechain_root;
    int32_t charge_state;
    VSEPR_PeptideSidechainClass sidechain_class;  /* was VSEPR_SidechainClass */
    double phi_deg;
    double psi_deg;
    double omega_deg;
} VSEPR_ResidueUnit;

typedef struct VSEPR_BondPair {
    int32_t a, b, order;
} VSEPR_BondPair;

typedef struct VSEPR_PeptideChain {
    int32_t chain_id;
    char    chain_name[VSEPR_MAX_NAME_LEN];
    int32_t residue_ids[VSEPR_MAX_RESIDUES_PER_CHAIN];
    int32_t residue_count;
    VSEPR_BondPair bonds[VSEPR_MAX_BONDS_PER_CHAIN];
    int32_t bond_count;
    VSEPR_FormationClass    formation_class;
    VSEPR_FormationState    formation_state;
    VSEPR_EnvironmentClass  environment_class;
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
