/* decay_event.h
 * ==============
 * Plain C decay event structure for legacy/portable code paths.
 */

#ifndef DECAY_EVENT_H
#define DECAY_EVENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_EMITTED_PARTICLES 8

typedef struct DecayEvent {
    int parent_species_id;
    int daughter_species_id;

    int emitted_particle_ids[MAX_EMITTED_PARTICLES];
    size_t emitted_count;

    double released_energy_eV;
    double deposited_energy_fraction;
    double transported_energy_fraction;
    double local_damage_score;

    double event_time_s;
    double confidence;
} DecayEvent;

void decay_event_init(DecayEvent* ev);
int decay_event_add_emitted(DecayEvent* ev, int particle_id);

#ifdef __cplusplus
}
#endif

#endif /* DECAY_EVENT_H */
