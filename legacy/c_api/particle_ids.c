/* particle_ids.c
 * ===============
 * Plain C implementation of species code query functions.
 */

#include "particle_ids.h"

const char* particle_id_name(int id) {
    switch (id) {
        case PARTICLE_PLACEHOLDER:       return "X";
        case PARTICLE_ALPHA:             return "alpha";
        case PARTICLE_BETA_MINUS:        return "beta-";
        case PARTICLE_GAMMA:             return "gamma";
        case PARTICLE_ETA:               return "eta";
        case PARTICLE_ANTINEUTRINO:      return "antineutrino";
        case PARTICLE_NEUTRINO:          return "neutrino";
        case PARTICLE_NEUTRON_FREE:      return "neutron";
        case PARTICLE_PROTON_FREE:       return "proton";
        case PARTICLE_EXCITATION:        return "excitation";
        case PARTICLE_IONIZATION:        return "ionization";
        case PARTICLE_HEAT_PACKET:       return "heat";
        case PARTICLE_CHARGE_CLOUD:      return "charge_cloud";
        case PARTICLE_FIELD_SOURCE:      return "field_source";
        case PARTICLE_FIELD_PROBE:       return "field_probe";
        case PARTICLE_VACANCY:           return "vacancy";
        case PARTICLE_INTERSTITIAL:      return "interstitial";
        case PARTICLE_GHOST:             return "ghost";
        case PARTICLE_TRANSITION_STATE:  return "transition_state";
        default:                         return "unknown";
    }
}

int particle_id_is_reserved(int id) {
    return (id <= 0 && id >= -99);
}

int particle_id_is_transportable(int id) {
    switch (id) {
        case PARTICLE_ALPHA:
        case PARTICLE_BETA_MINUS:
        case PARTICLE_GAMMA:
        case PARTICLE_NEUTRON_FREE:
        case PARTICLE_PROTON_FREE:
            return 1;
        default:
            return 0;
    }
}

int particle_id_is_bookkeeping_only(int id) {
    return (id <= -4 && id >= -14) ||
           (id == PARTICLE_TRANSITION_STATE);
}

int particle_id_is_atom_species(int id) {
    return (id >= 1 && id <= 118);
}
