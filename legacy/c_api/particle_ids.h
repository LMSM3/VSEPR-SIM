/* particle_ids.h
 * ===============
 * Plain C interface for the unified species code namespace.
 *
 * species_code >= 0  standard atom/species templates (Z = 1..118, 0 = placeholder)
 * species_code <  0  reserved engine entities
 *
 * Physical emission / decay particles:  -1 .. -8
 * Energy / field / abstract carriers:   -9 .. -14
 * Defect / structural / virtual:        -15 .. -18
 */

#ifndef PARTICLE_IDS_H
#define PARTICLE_IDS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ParticleID {
    PARTICLE_PLACEHOLDER        =  0,

    /* Physical emission / decay particles */
    PARTICLE_ALPHA              = -1,                                                                                                       
    PARTICLE_BETA_MINUS         = -2,
    PARTICLE_GAMMA              = -3,
    PARTICLE_ETA                = -4,   /* QMD energy packet */
    PARTICLE_ANTINEUTRINO       = -5,
    PARTICLE_NEUTRINO           = -6,
    PARTICLE_NEUTRON_FREE       = -7,
    PARTICLE_PROTON_FREE        = -8,

    /* Energy / field / abstract carriers
    *
    * Including heat as a particle goes against design within other aspects of the simulation
    * but we utilize it to have a near-match to the U of S entire energy state from the documentation.
    */
    PARTICLE_EXCITATION         = -9,
    PARTICLE_IONIZATION         = -10,
    PARTICLE_HEAT_PACKET        = -11,
    PARTICLE_CHARGE_CLOUD       = -12,
    PARTICLE_FIELD_SOURCE       = -13,
    PARTICLE_FIELD_PROBE        = -14,

    /* Defect / structural / virtual */
    PARTICLE_VACANCY            = -15,
    PARTICLE_INTERSTITIAL       = -16,
    PARTICLE_GHOST              = -17,
    PARTICLE_TRANSITION_STATE   = -18
} ParticleID;

const char* particle_id_name(int id);
int particle_id_is_reserved(int id);
int particle_id_is_transportable(int id);
int particle_id_is_bookkeeping_only(int id);
int particle_id_is_atom_species(int id);

#ifdef __cplusplus
}
#endif

#endif /* PARTICLE_IDS_H */
