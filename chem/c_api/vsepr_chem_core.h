/* vsepr_chem_core.h
 * ==================
 * Generic chemistry ABI for VSEPR-SIM.
 *
 * WHAT BELONGS HERE
 * Physical facts the runtime can assign without domain analysis:
 * hybridization, bond kind, generic atom role, environment class,
 * formation class/state, annotation status, failure mode.
 *
 * WHAT DOES NOT BELONG HERE
 * Peptide roles, sidechain classes, fold states, ligand names,
 * coordination numbers, oxidation states.  Those live in domain
 * headers that depend on this one.
 *
 * DEPENDENCY DIRECTION
 *   vsepr_chem_core.h          <- generic runtime, no domain knowledge
 *        |
 *   vsepr_peptide_types.h      <- peptide domain  (future: crystal, salt...)
 *        |
 *   peptide_formation.hpp      <- analysis engines
 *
 * This is the template for every future VSIM domain module:
 *   [system] domain = "peptide"    -> vsepr_peptide_types.h
 *   [system] domain = "crystal"   -> vsepr_crystal_types.h  (future)
 *   [system] domain = "molten_salt"-> vsepr_salt_types.h    (future)
 *
 * C99-compatible.  Safe from C and C++.
 * VSEPR-SIM | v5.0.0-main
 */

#ifndef VSEPR_CHEM_CORE_H
#define VSEPR_CHEM_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * VSEPR_EntityKind — what kind of simulation object is this?
 * Assigned at creation, before any analysis.
 * ========================================================================== */
typedef enum VSEPR_EntityKind {
    VSEPR_ENTITY_UNKNOWN    = 0,
    VSEPR_ENTITY_ATOM       = 1,
    VSEPR_ENTITY_BOND       = 2,
    VSEPR_ENTITY_CONSTRAINT = 3,
    VSEPR_ENTITY_FIELD      = 4,
    VSEPR_ENTITY_SURFACE    = 5,
    VSEPR_ENTITY_EVENT      = 6
} VSEPR_EntityKind;

/* ==========================================================================
 * VSEPR_BondKind — physical character of a bond.
 * ========================================================================== */
typedef enum VSEPR_BondKind {
    VSEPR_BOND_UNKNOWN       = 0,
    VSEPR_BOND_SINGLE        = 1,
    VSEPR_BOND_DOUBLE        = 2,
    VSEPR_BOND_TRIPLE        = 3,
    VSEPR_BOND_AROMATIC_LIKE = 4,
    VSEPR_BOND_COORDINATION  = 5,
    VSEPR_BOND_DYNAMIC       = 6,
    VSEPR_BOND_CONTACT       = 7
} VSEPR_BondKind;

/* ==========================================================================
 * VSEPR_Hybridization — electronic geometry, derivable from Z + valence.
 * ========================================================================== */
typedef enum VSEPR_Hybridization {
    VSEPR_HYB_UNKNOWN  = 0,
    VSEPR_HYB_SP       = 1,
    VSEPR_HYB_SP2      = 2,
    VSEPR_HYB_SP3      = 3,
    VSEPR_HYB_SP3D     = 4,
    VSEPR_HYB_SP3D2    = 5,
    VSEPR_HYB_AROMATIC = 6
} VSEPR_Hybridization;

/* ==========================================================================
 * VSEPR_ChemRole — generic physical role of an atom.
 * Domain modules map fine-grained roles onto these for the generic runtime.
 * ========================================================================== */
typedef enum VSEPR_ChemRole {
    VSEPR_ROLE_UNKNOWN       = 0,
    VSEPR_ROLE_DONOR         = 1,
    VSEPR_ROLE_ACCEPTOR      = 2,
    VSEPR_ROLE_BRIDGE        = 3,
    VSEPR_ROLE_REACTIVE_SITE = 4,
    VSEPR_ROLE_TERMINAL      = 5,
    VSEPR_ROLE_SURFACE_SITE  = 6,
    VSEPR_ROLE_DEFECT_SITE   = 7,
    VSEPR_ROLE_SOLVENT_SITE  = 8,
    VSEPR_ROLE_PSEUDO        = 9
} VSEPR_ChemRole;

/* ==========================================================================
 * VSEPR_EnvironmentClass — physical thermodynamic environment.
 * Physical states only.  "Polar solvent" is a conclusion, not a state.
 * ========================================================================== */
typedef enum VSEPR_EnvironmentClass {
    VSEPR_ENV_UNKNOWN          = 0,
    VSEPR_ENV_VACUUM           = 1,
    VSEPR_ENV_GAS              = 2,
    VSEPR_ENV_LIQUID           = 3,
    VSEPR_ENV_SOLID            = 4,
    VSEPR_ENV_SURFACE          = 5,
    VSEPR_ENV_POROUS_MEDIA     = 6,
    VSEPR_ENV_STRUCTURED_MEDIUM= 7,
    VSEPR_ENV_POLAR_MEDIUM     = 8,
    VSEPR_ENV_APOLAR_MEDIUM    = 9
} VSEPR_EnvironmentClass;

/* ==========================================================================
 * VSEPR_TopologyClass — large-scale connectivity, assigned by analysis.
 * ========================================================================== */
typedef enum VSEPR_TopologyClass {
    VSEPR_TOPOLOGY_UNKNOWN   = 0,
    VSEPR_TOPOLOGY_CHAIN     = 1,
    VSEPR_TOPOLOGY_RING      = 2,
    VSEPR_TOPOLOGY_BRANCHED  = 3,
    VSEPR_TOPOLOGY_NETWORK   = 4,
    VSEPR_TOPOLOGY_FRAMEWORK = 5,
    VSEPR_TOPOLOGY_CRYSTAL   = 6,
    VSEPR_TOPOLOGY_AMORPHOUS = 7,
    VSEPR_TOPOLOGY_BULK      = 8
} VSEPR_TopologyClass;

/* ==========================================================================
 * VSEPR_FormationClass — broad class of the formed object.  Generic.
 * ========================================================================== */
typedef enum VSEPR_FormationClass {
    VSEPR_FORM_UNKNOWN   = 0,
    VSEPR_FORM_CHAIN     = 1,
    VSEPR_FORM_RING      = 2,
    VSEPR_FORM_FRAMEWORK = 3,
    VSEPR_FORM_CRYSTAL   = 4,
    VSEPR_FORM_BULK      = 5,
    VSEPR_FORM_CLUSTER   = 6,
    VSEPR_FORM_SURFACE   = 7
} VSEPR_FormationClass;

/* ==========================================================================
 * VSEPR_FormationState — pipeline stage.
 * ========================================================================== */
typedef enum VSEPR_FormationState {
    VSEPR_STATE_PREFORM   = 0,
    VSEPR_STATE_BUILDING  = 1,
    VSEPR_STATE_RELAXING  = 2,
    VSEPR_STATE_ANALYZING = 3,
    VSEPR_STATE_COMPLETE  = 4,
    VSEPR_STATE_FAILED    = 5
} VSEPR_FormationState;

/* ==========================================================================
 * VSEPR_AnnotationStatus — provenance of a field value.
 * ========================================================================== */
typedef enum VSEPR_AnnotationStatus {
    VSEPR_ANNOTATION_UNKNOWN    = 0,
    VSEPR_ANNOTATION_EXPLICIT   = 1,
    VSEPR_ANNOTATION_INFERRED   = 2,
    VSEPR_ANNOTATION_UNAVAILABLE= 3,
    VSEPR_ANNOTATION_REJECTED   = 4
} VSEPR_AnnotationStatus;

/* ==========================================================================
 * VSEPR_FailureMode — class of error that caused a pipeline failure.
 * ========================================================================== */
typedef enum VSEPR_FailureMode {
    VSEPR_FAILURE_NONE               = 0,
    VSEPR_FAILURE_BOND_ORDER_DRIFT   = 1,
    VSEPR_FAILURE_COORDINATION_RANGE = 2,
    VSEPR_FAILURE_TOPOLOGY_INVALID   = 3,
    VSEPR_FAILURE_VALENCE_EXCEEDED   = 4,
    VSEPR_FAILURE_GEOMETRY_CLASH     = 5,
    VSEPR_FAILURE_MISSING_DATA       = 6,
    VSEPR_FAILURE_DOMAIN_MISMATCH    = 7
} VSEPR_FailureMode;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VSEPR_CHEM_CORE_H */
