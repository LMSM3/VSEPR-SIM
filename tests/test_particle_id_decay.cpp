/**
 * test_particle_id_decay.cpp
 * ==========================
 * Sanity check for the unified species code namespace, decay event
 * builders, and kinetic event record infrastructure.
 *
 * Tests:
 *   1. ParticleID constexpr predicates
 *      (is_reserved, is_decay_particle, is_transportable,
 *       is_bookkeeping_only, is_energy_carrier, is_defect_or_virtual,
 *       is_ghost)
 *   2. to_string labels for all 18 reserved entries + placeholder
 *   3. species_code helpers (species_code_to_label, is_atom_species,
 *      is_reserved_code)
 *   4. Alpha decay builder  (Pu-239 -> U-235 + alpha)
 *   5. Beta-minus decay builder (emits beta- + antineutrino)
 *   6. Gamma decay builder
 *   7. DecayEvent overflow guard (max 8 emitted)
 *   8. KineticEventRecord variant dispatch
 *   9. Daughter-state arithmetic
 */

#include "physics/particle_id.hpp"
#include "physics/decay_event.hpp"
#include "physics/decay_event_builder.hpp"
#include "kinetics/kinetic_event_kind.hpp"
#include "kinetics/kinetic_event_record.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace vsepr::physics;
using namespace vsepr::kinetics;

// ============================================================================
// Test helpers
// ============================================================================

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    ++tests_run; \
    if (cond) { ++tests_passed; std::printf("  [PASS] %s\n", msg); } \
    else      { std::printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); } \
} while(0)

// ============================================================================
// 1. Constexpr predicates
// ============================================================================

static void test_constexpr_queries() {
    std::puts("\n--- constexpr predicates ---");

    // is_reserved
    CHECK( is_reserved(ParticleID::placeholder),       "placeholder is reserved");
    CHECK( is_reserved(ParticleID::alpha),              "alpha is reserved");
    CHECK( is_reserved(ParticleID::transition_state),   "transition_state is reserved");
    CHECK( is_reserved(ParticleID::ghost),              "ghost is reserved");

    // is_decay_particle  [-1, -8]
    CHECK( is_decay_particle(ParticleID::alpha),        "alpha is decay_particle");
    CHECK( is_decay_particle(ParticleID::beta_minus),   "beta- is decay_particle");
    CHECK( is_decay_particle(ParticleID::gamma),        "gamma is decay_particle");
    CHECK( is_decay_particle(ParticleID::eta),          "eta is decay_particle");
    CHECK( is_decay_particle(ParticleID::antineutrino), "antineutrino is decay_particle");
    CHECK( is_decay_particle(ParticleID::neutrino),     "neutrino is decay_particle");
    CHECK( is_decay_particle(ParticleID::neutron_free), "neutron_free is decay_particle");
    CHECK( is_decay_particle(ParticleID::proton_free),  "proton_free is decay_particle");
    CHECK(!is_decay_particle(ParticleID::excitation_packet), "excitation NOT decay_particle");
    CHECK(!is_decay_particle(ParticleID::ghost),             "ghost NOT decay_particle");

    // is_transportable
    CHECK( is_transportable(ParticleID::alpha),         "alpha is transportable");
    CHECK( is_transportable(ParticleID::beta_minus),    "beta- is transportable");
    CHECK( is_transportable(ParticleID::gamma),         "gamma is transportable");
    CHECK( is_transportable(ParticleID::neutron_free),  "neutron is transportable");
    CHECK( is_transportable(ParticleID::proton_free),   "proton is transportable");
    CHECK(!is_transportable(ParticleID::eta),           "eta NOT transportable");
    CHECK(!is_transportable(ParticleID::antineutrino),  "antineutrino NOT transportable");
    CHECK(!is_transportable(ParticleID::heat_packet),   "heat NOT transportable");

    // is_bookkeeping_only
    CHECK( is_bookkeeping_only(ParticleID::eta),                "eta is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::antineutrino),       "antineutrino is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::neutrino),           "neutrino is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::excitation_packet),  "excitation is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::ionization_event),   "ionization is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::heat_packet),        "heat is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::charge_cloud),       "charge_cloud is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::field_source),       "field_source is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::field_probe),        "field_probe is bookkeeping");
    CHECK( is_bookkeeping_only(ParticleID::transition_state),   "transition_state is bookkeeping");
    CHECK(!is_bookkeeping_only(ParticleID::alpha),              "alpha NOT bookkeeping");
    CHECK(!is_bookkeeping_only(ParticleID::gamma),              "gamma NOT bookkeeping");

    // is_energy_carrier  [-9, -14]
    CHECK( is_energy_carrier(ParticleID::excitation_packet),  "excitation is energy_carrier");
    CHECK( is_energy_carrier(ParticleID::ionization_event),   "ionization is energy_carrier");
    CHECK( is_energy_carrier(ParticleID::heat_packet),        "heat is energy_carrier");
    CHECK( is_energy_carrier(ParticleID::charge_cloud),       "charge_cloud is energy_carrier");
    CHECK( is_energy_carrier(ParticleID::field_source),       "field_source is energy_carrier");
    CHECK( is_energy_carrier(ParticleID::field_probe),        "field_probe is energy_carrier");
    CHECK(!is_energy_carrier(ParticleID::alpha),              "alpha NOT energy_carrier");
    CHECK(!is_energy_carrier(ParticleID::vacancy),            "vacancy NOT energy_carrier");

    // is_defect_or_virtual  [-15, -18]
    CHECK( is_defect_or_virtual(ParticleID::vacancy),           "vacancy is defect_or_virtual");
    CHECK( is_defect_or_virtual(ParticleID::interstitial),      "interstitial is defect_or_virtual");
    CHECK( is_defect_or_virtual(ParticleID::ghost),             "ghost is defect_or_virtual");
    CHECK( is_defect_or_virtual(ParticleID::transition_state),  "transition_state is defect_or_virtual");
    CHECK(!is_defect_or_virtual(ParticleID::alpha),             "alpha NOT defect_or_virtual");
    CHECK(!is_defect_or_virtual(ParticleID::field_probe),       "field_probe NOT defect_or_virtual");

    // is_ghost
    CHECK( is_ghost(ParticleID::ghost),    "ghost is ghost");
    CHECK(!is_ghost(ParticleID::vacancy),  "vacancy NOT ghost");
    CHECK(!is_ghost(ParticleID::alpha),    "alpha NOT ghost");
}

// ============================================================================
// 2. to_string labels
// ============================================================================

static void test_to_string() {
    std::puts("\n--- to_string labels ---");

    CHECK(to_string(ParticleID::placeholder)       == "X",                "str: placeholder");
    CHECK(to_string(ParticleID::alpha)             == "alpha",            "str: alpha");
    CHECK(to_string(ParticleID::beta_minus)        == "beta-",           "str: beta-");
    CHECK(to_string(ParticleID::gamma)             == "gamma",            "str: gamma");
    CHECK(to_string(ParticleID::eta)               == "eta",              "str: eta");
    CHECK(to_string(ParticleID::antineutrino)      == "antineutrino",     "str: antineutrino");
    CHECK(to_string(ParticleID::neutrino)          == "neutrino",         "str: neutrino");
    CHECK(to_string(ParticleID::neutron_free)      == "neutron",          "str: neutron");
    CHECK(to_string(ParticleID::proton_free)       == "proton",           "str: proton");
    CHECK(to_string(ParticleID::excitation_packet) == "excitation",       "str: excitation");
    CHECK(to_string(ParticleID::ionization_event)  == "ionization",       "str: ionization");
    CHECK(to_string(ParticleID::heat_packet)       == "heat",             "str: heat");
    CHECK(to_string(ParticleID::charge_cloud)      == "charge_cloud",     "str: charge_cloud");
    CHECK(to_string(ParticleID::field_source)      == "field_source",     "str: field_source");
    CHECK(to_string(ParticleID::field_probe)       == "field_probe",      "str: field_probe");
    CHECK(to_string(ParticleID::vacancy)           == "vacancy",          "str: vacancy");
    CHECK(to_string(ParticleID::interstitial)      == "interstitial",     "str: interstitial");
    CHECK(to_string(ParticleID::ghost)             == "ghost",            "str: ghost");
    CHECK(to_string(ParticleID::transition_state)  == "transition_state", "str: transition_state");

    // Unknown ID
    CHECK(to_string(static_cast<ParticleID>(-42)) == "unknown", "str: unknown");
}

// ============================================================================
// 3. species_code helpers
// ============================================================================

static void test_species_code_helpers() {
    std::puts("\n--- species_code helpers ---");

    // is_atom_species
    CHECK( is_atom_species(1),    "H (Z=1) is atom");
    CHECK( is_atom_species(6),    "C (Z=6) is atom");
    CHECK( is_atom_species(118),  "Og (Z=118) is atom");
    CHECK(!is_atom_species(0),    "0 NOT atom");
    CHECK(!is_atom_species(-1),   "-1 NOT atom");
    CHECK(!is_atom_species(119),  "119 NOT atom");

    // is_reserved_code
    CHECK( is_reserved_code(0),    "0 is reserved_code");
    CHECK( is_reserved_code(-1),   "-1 is reserved_code");
    CHECK( is_reserved_code(-18),  "-18 is reserved_code");
    CHECK( is_reserved_code(-99),  "-99 is reserved_code");
    CHECK(!is_reserved_code(1),    "1 NOT reserved_code");
    CHECK(!is_reserved_code(-100), "-100 NOT reserved_code");

    // species_code_to_label
    CHECK(species_code_to_label(6)   == "?",     "positive -> ?");
    CHECK(species_code_to_label(0)   == "X",     "zero -> X");
    CHECK(species_code_to_label(-1)  == "alpha", "-1 -> alpha");
    CHECK(species_code_to_label(-17) == "ghost", "-17 -> ghost");
    CHECK(species_code_to_label(-42) == "unknown", "-42 -> unknown");
}

// ============================================================================
// 4. Alpha decay builder (Pu-239 -> U-235)
// ============================================================================

static void test_alpha_decay() {
    std::puts("\n--- alpha decay (Pu-239 -> U-235) ---");

    auto ev = make_alpha_decay(
        /*parent_species_id=*/942390,
        /*daughter_species_id=*/922350,
        /*released_energy_eV=*/5.24e6,
        /*event_time_s=*/1.0,
        /*confidence=*/0.98
    );

    CHECK(ev.parent_species_id   == 942390, "parent ID");
    CHECK(ev.daughter_species_id == 922350, "daughter ID");
    CHECK(ev.emitted_count == 1,            "one emitted particle");
    CHECK(ev.emitted_particle_ids[0] == ParticleID::alpha, "emitted alpha");
    CHECK(ev.released_energy_eV > 5.0e6,   "energy > 5 MeV");
    CHECK(ev.deposited_energy_fraction > 0.8,   "high deposition");
    CHECK(ev.local_damage_score > 0.8,          "high damage");
    CHECK(ev.confidence > 0.9,                  "high confidence");

    std::printf("  Parent:   %d\n",    ev.parent_species_id);
    std::printf("  Daughter: %d\n",    ev.daughter_species_id);
    std::printf("  Energy:   %.2e eV\n", ev.released_energy_eV);
    std::printf("  Emitted:  %zu  -> %.*s\n",
        ev.emitted_count,
        static_cast<int>(to_string(ev.emitted_particle_ids[0]).size()),
        to_string(ev.emitted_particle_ids[0]).data());
}

// ============================================================================
// 5. Beta-minus decay builder
// ============================================================================

static void test_beta_minus_decay() {
    std::puts("\n--- beta-minus decay ---");

    auto ev = make_beta_minus_decay(100, 200, 1.0e6, 2.0, 0.95);

    CHECK(ev.emitted_count == 2, "two emitted (beta- + antineutrino)");
    CHECK(ev.emitted_particle_ids[0] == ParticleID::beta_minus,    "first = beta-");
    CHECK(ev.emitted_particle_ids[1] == ParticleID::antineutrino,  "second = antineutrino");
    CHECK(ev.deposited_energy_fraction < 0.5, "moderate deposition");
    CHECK(ev.local_damage_score < 0.5,        "moderate damage");
}

// ============================================================================
// 6. Gamma decay builder
// ============================================================================

static void test_gamma_decay() {
    std::puts("\n--- gamma decay ---");

    auto ev = make_gamma_decay(100, 100, 0.5e6, 3.0, 0.99);

    CHECK(ev.emitted_count == 1, "one emitted (gamma)");
    CHECK(ev.emitted_particle_ids[0] == ParticleID::gamma, "emitted gamma");
    CHECK(ev.deposited_energy_fraction < 0.2,    "low deposition");
    CHECK(ev.transported_energy_fraction > 0.8,  "high transport");
    CHECK(ev.local_damage_score < 0.1,           "low damage");
}

// ============================================================================
// 7. DecayEvent overflow guard
// ============================================================================

static void test_overflow_guard() {
    std::puts("\n--- overflow guard ---");

    DecayEvent ev {};
    for (int i = 0; i < 8; ++i) {
        CHECK(ev.add_emitted(ParticleID::gamma), "add succeeds");
    }
    CHECK(!ev.add_emitted(ParticleID::gamma), "9th add fails (overflow)");
    CHECK(ev.emitted_count == 8, "count capped at 8");
}

// ============================================================================
// 8. KineticEventRecord variant dispatch
// ============================================================================

static void test_kinetic_record() {
    std::puts("\n--- kinetic event record ---");

    auto decay_ev = make_alpha_decay(942390, 922350, 5.24e6, 1.0, 0.98);

    KineticEventRecord rec {};
    rec.kind = EventKind::decay;
    rec.intensity = 1.0e-17;
    rec.barrier_score = 0.0;
    rec.environment_multiplier = 1.0;
    rec.payload = decay_ev;

    CHECK(rec.kind == EventKind::decay, "kind = decay");

    // Variant dispatch
    bool dispatched = false;
    if (auto* d = std::get_if<vsepr::physics::DecayEvent>(&rec.payload)) {
        dispatched = true;
        CHECK(d->parent_species_id == 942390, "variant: parent ID");
        CHECK(d->emitted_count == 1,          "variant: emitted count");
    }
    CHECK(dispatched, "variant dispatch succeeded");

    // Transport event
    GenericTransportEvent te {};
    te.source_id = 1;
    te.target_id = 2;
    te.time_s = 5.0;
    te.confidence = 0.7;

    KineticEventRecord trec {};
    trec.kind = EventKind::transport;
    trec.payload = te;

    bool t_dispatched = false;
    if (auto* t = std::get_if<GenericTransportEvent>(&trec.payload)) {
        t_dispatched = true;
        CHECK(t->source_id == 1, "transport: source");
        CHECK(t->target_id == 2, "transport: target");
    }
    CHECK(t_dispatched, "transport variant dispatch succeeded");
}

// ============================================================================
// 9. Daughter-state arithmetic
// ============================================================================

static void test_daughter_state() {
    std::puts("\n--- daughter-state arithmetic ---");

    NuclearState pu239 { 942390, 94, 239 };

    auto da = alpha_daughter(pu239);
    CHECK(da.Z == 92, "alpha daughter Z = 92 (U)");
    CHECK(da.A == 235, "alpha daughter A = 235");

    auto db = beta_minus_daughter(pu239);
    CHECK(db.Z == 95, "beta- daughter Z = 95 (Am)");
    CHECK(db.A == 239, "beta- daughter A = 239");

    auto dp = beta_plus_daughter(pu239);
    CHECK(dp.Z == 93, "beta+ daughter Z = 93 (Np)");
    CHECK(dp.A == 239, "beta+ daughter A = 239");
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::puts("==============================================");
    std::puts("  Species Code / Decay Event Test Suite v2.0  ");
    std::puts("==============================================");

    test_constexpr_queries();
    test_to_string();
    test_species_code_helpers();
    test_alpha_decay();
    test_beta_minus_decay();
    test_gamma_decay();
    test_overflow_guard();
    test_kinetic_record();
    test_daughter_state();

    std::printf("\n==============================================\n");
    std::printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    std::printf("==============================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
