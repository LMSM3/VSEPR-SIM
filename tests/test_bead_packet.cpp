#include "coarse_grain/core/bead_packet.hpp"

#include <cassert>
#include <stdexcept>

int main() {
	coarse_grain::BeadPacketState state;
	state.identity = {"test-packet", "H2O", "unit-test"};
	state.mass = 10.0;
	state.momentum = {1.0, 2.0, 3.0};
	state.internal_energy = 5.0;

	coarse_grain::BeadPacketControlVolume open(state, coarse_grain::BeadBoundaryMode::Open);
	open.capture_initial();
	coarse_grain::BeadExchangeEvent event;
	event.sequence = 1;
	event.time_fs = 1.0;
	event.mass_delta = 2.0;
	event.momentum_delta = {0.5, -0.5, 1.0};
	event.energy_delta = 3.0;
	event.species = "H2O";
	event.species_mass_delta = 2.0;
	open.apply(event);
	assert(open.ledger().mass_residual(open.state()) == 0.0);
	assert(open.ledger().energy_residual(open.state()) == 0.0);
	const auto residual = open.ledger().momentum_residual(open.state());
	assert(residual.x == 0.0 && residual.y == 0.0 && residual.z == 0.0);

	coarse_grain::BeadPacketControlVolume closed(state, coarse_grain::BeadBoundaryMode::Closed);
	bool rejected = false;
	try { closed.apply(event); } catch (const std::logic_error&) { rejected = true; }
	assert(rejected);
	return 0;
}
