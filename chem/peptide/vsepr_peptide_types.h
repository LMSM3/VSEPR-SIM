/* vsepr_peptide_types.h
 * =====================
 * Peptide domain vocabulary for VSEPR-SIM.
 *
 * DEPENDENCY DIRECTION
 *   This header depends on vsepr_chem_core.h.
 *   vsepr_chem_core.h does NOT depend on this header.
 *   The runtime does NOT depend on this header.
 *
 * WHAT BELONGS HERE
 * Types that are meaningful only AFTER the peptide analysis engine has run.
 * Backbone topology labels, residue roles, sidechain classifications, and
 * fold states are analysis results, not generative primitives.
 *
 * WHAT DOES NOT BELONG HERE
 * Generic hybridization, generic environment class, generic formation
 * state/class.  Those are in vsepr_chem_core.h.
 *
 * MAPPING CONTRACT
 * peptide_role_to_core_role() maps VSEPR_PeptideRole -> VSEPR_ChemRole so
 * the generic runtime can consume peptide annotations without knowing about
 * peptide vocabulary.  Every future domain header must provide the same
 * kind of mapping function.
 *
 * VSEPR-SIM | v5.0.0-main
 */

#ifndef VSEPR_PEPTIDE_TYPES_H
#define VSEPR_PEPTIDE_TYPES_H

#include "chem/c_api/vsepr_chem_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * VSEPR_PeptideRole
 * Fine-grained role of an atom within a detected peptide backbone or
 * sidechain.  Produced by the peptide topology analyser.  Never assigned
 * before analysis runs.
 * ========================================================================== */
typedef enum VSEPR_PeptideRole {
    VSEPR_PEPTIDE_ROLE_UNKNOWN    = 0,
    VSEPR_PEPTIDE_ROLE_BACKBONE_N = 1,   /* amide nitrogen                 */
    VSEPR_PEPTIDE_ROLE_ALPHA_C    = 2,   /* alpha carbon                   */
    VSEPR_PEPTIDE_ROLE_CARBONYL_C = 3,   /* carbonyl carbon                */
    VSEPR_PEPTIDE_ROLE_CARBONYL_O = 4,   /* carbonyl oxygen                */
    VSEPR_PEPTIDE_ROLE_SIDECHAIN  = 5    /* sidechain root atom            */
} VSEPR_PeptideRole;

/* ==========================================================================
 * VSEPR_PeptideSidechainClass
 * Polarity/reactivity class of a detected amino-acid sidechain.
 * Produced by residue analysis, not assigned by input.
 * ========================================================================== */
typedef enum VSEPR_PeptideSidechainClass {
    VSEPR_SIDECHAIN_UNKNOWN  = 0,
    VSEPR_SIDECHAIN_NONPOLAR = 1,   /* Ala, Val, Leu, Ile, Met, Phe, Pro   */
    VSEPR_SIDECHAIN_POLAR    = 2,   /* Ser, Thr, Cys, Tyr, Asn, Gln        */
    VSEPR_SIDECHAIN_POSITIVE = 3,   /* Lys, Arg, His+                      */
    VSEPR_SIDECHAIN_NEGATIVE = 4,   /* Asp, Glu                            */
    VSEPR_SIDECHAIN_AROMATIC = 5,   /* Phe, Tyr, Trp, His                  */
    VSEPR_SIDECHAIN_SPECIAL  = 6    /* Gly (no sidechain), Pro (cyclic)    */
} VSEPR_PeptideSidechainClass;

/* ==========================================================================
 * VSEPR_PeptideState
 * Topological / conformational state of a detected peptide structure.
 * These are analysis outputs, not pipeline stages.
 * Pipeline stages are in VSEPR_FormationState (vsepr_chem_core.h).
 * ========================================================================== */
typedef enum VSEPR_PeptideState {
    VSEPR_PEPTIDE_STATE_UNKNOWN      = 0,
    VSEPR_PEPTIDE_STATE_LINEAR       = 1,
    VSEPR_PEPTIDE_STATE_CYCLIC       = 2,
    VSEPR_PEPTIDE_STATE_BRANCHED     = 3,
    VSEPR_PEPTIDE_STATE_LOCAL_FOLDED = 4,   /* partial secondary structure  */
    VSEPR_PEPTIDE_STATE_GLOBAL_FOLD  = 5,   /* tertiary fold achieved       */
    VSEPR_PEPTIDE_STATE_INVALID      = 6
} VSEPR_PeptideState;

#ifdef __cplusplus
} /* extern "C" */

/* ==========================================================================
 * peptide_role_to_core_role()
 * Maps a fine-grained peptide atom role to the nearest generic VSEPR_ChemRole.
 * Allows the generic runtime and VSIM scripting layer to consume peptide
 * annotations through the core vocabulary without knowing peptide vocabulary.
 *
 * Every future domain header (crystal, molten_salt, coarse_grain ...) should
 * provide the same kind of mapping function.
 * ========================================================================== */
inline VSEPR_ChemRole peptide_role_to_core_role(VSEPR_PeptideRole role) noexcept {
    switch (role) {
        case VSEPR_PEPTIDE_ROLE_BACKBONE_N:  return VSEPR_ROLE_DONOR;
        case VSEPR_PEPTIDE_ROLE_CARBONYL_O:  return VSEPR_ROLE_ACCEPTOR;
        case VSEPR_PEPTIDE_ROLE_ALPHA_C:     return VSEPR_ROLE_BRIDGE;
        case VSEPR_PEPTIDE_ROLE_CARBONYL_C:  return VSEPR_ROLE_REACTIVE_SITE;
        case VSEPR_PEPTIDE_ROLE_SIDECHAIN:   return VSEPR_ROLE_TERMINAL;
        default:                             return VSEPR_ROLE_UNKNOWN;
    }
}

#endif /* __cplusplus */
#endif /* VSEPR_PEPTIDE_TYPES_H */
