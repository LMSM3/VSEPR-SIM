/* decay_event.c
 * ==============
 * Plain C implementation of decay event init and emitted-particle add.
 */

#include "decay_event.h"

void decay_event_init(DecayEvent* ev) {
    size_t i;
    if (!ev) return;

    ev->parent_species_id = 0;
    ev->daughter_species_id = 0;
    ev->emitted_count = 0;
    ev->released_energy_eV = 0.0;
    ev->deposited_energy_fraction = 0.0;
    ev->transported_energy_fraction = 0.0;
    ev->local_damage_score = 0.0;
    ev->event_time_s = 0.0;
    ev->confidence = 0.0;

    for (i = 0; i < MAX_EMITTED_PARTICLES; ++i) {
        ev->emitted_particle_ids[i] = 0;
    }
}

int decay_event_add_emitted(DecayEvent* ev, int particle_id) {
    if (!ev) return 0;
    if (ev->emitted_count >= MAX_EMITTED_PARTICLES) return 0;

    ev->emitted_particle_ids[ev->emitted_count++] = particle_id;
    return 1;
}
