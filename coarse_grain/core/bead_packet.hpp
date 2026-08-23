#pragma once

#include "atomistic/core/state.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace coarse_grain {

enum class BeadBoundaryMode : std::uint8_t { Closed, Open };
enum class ExchangeKind : std::uint8_t { Mass, Momentum, Heat, Work, Species };

struct BeadIdentity {
	std::string packet_id;
	std::string species;
	std::string provenance;
};

struct BeadPacketState {
	BeadIdentity identity;
	double time_fs{};
	double mass{};
	atomistic::Vec3 position{};
	atomistic::Vec3 momentum{};
	double internal_energy{};
	std::map<std::string, double> species_mass;
};

struct BeadExchangeEvent {
	std::uint64_t sequence{};
	double time_fs{};
	ExchangeKind kind{ExchangeKind::Mass};
	double mass_delta{};
	atomistic::Vec3 momentum_delta{};
	double energy_delta{};
	std::string species;
	double species_mass_delta{};
	std::string source;
};

struct BeadConservationLedger {
	double initial_mass{};
	atomistic::Vec3 initial_momentum{};
	double initial_energy{};
	double mass_flux{};
	atomistic::Vec3 momentum_flux{};
	double energy_flux{};

	double mass_residual(const BeadPacketState& state) const {
		return state.mass - (initial_mass + mass_flux);
	}
	atomistic::Vec3 momentum_residual(const BeadPacketState& state) const {
		return state.momentum - (initial_momentum + momentum_flux);
	}
	double energy_residual(const BeadPacketState& state) const {
		return state.internal_energy - (initial_energy + energy_flux);
	}
};

class BeadPacketControlVolume {
public:
	BeadPacketControlVolume(BeadPacketState state, BeadBoundaryMode boundary)
		: state_(std::move(state)), boundary_(boundary) {
		ledger_.initial_mass = state_.mass;
		ledger_.initial_momentum = state_.momentum;
		ledger_.initial_energy = state_.internal_energy;
	}

	void apply(const BeadExchangeEvent& event) {
		const bool has_flux = event.mass_delta != 0.0 || event.energy_delta != 0.0 ||
			event.species_mass_delta != 0.0 || event.momentum_delta.x != 0.0 ||
			event.momentum_delta.y != 0.0 || event.momentum_delta.z != 0.0;
		if (boundary_ == BeadBoundaryMode::Closed && has_flux)
			throw std::logic_error("closed bead boundary rejects exchange events");
		if (!events_.empty() && event.sequence <= events_.back().sequence)
			throw std::logic_error("bead exchange sequence must increase");

		state_.time_fs = event.time_fs;
		state_.mass += event.mass_delta;
		state_.momentum = state_.momentum + event.momentum_delta;
		state_.internal_energy += event.energy_delta;
		if (!event.species.empty()) state_.species_mass[event.species] += event.species_mass_delta;
		if (state_.mass < 0.0 || state_.internal_energy < 0.0)
			throw std::logic_error("bead exchange produced negative conserved state");

		ledger_.mass_flux += event.mass_delta;
		ledger_.momentum_flux = ledger_.momentum_flux + event.momentum_delta;
		ledger_.energy_flux += event.energy_delta;
		events_.push_back(event);
		trajectory_.push_back(state_);
	}

	void capture_initial() { trajectory_.push_back(state_); }
	const BeadPacketState& state() const { return state_; }
	const BeadConservationLedger& ledger() const { return ledger_; }
	const std::vector<BeadExchangeEvent>& events() const { return events_; }
	const std::vector<BeadPacketState>& trajectory() const { return trajectory_; }
	BeadBoundaryMode boundary() const { return boundary_; }

private:
	BeadPacketState state_;
	BeadBoundaryMode boundary_;
	BeadConservationLedger ledger_;
	std::vector<BeadExchangeEvent> events_;
	std::vector<BeadPacketState> trajectory_;
};

inline const char* exchange_kind_name(ExchangeKind kind) {
	switch (kind) {
		case ExchangeKind::Mass: return "mass";
		case ExchangeKind::Momentum: return "momentum";
		case ExchangeKind::Heat: return "heat";
		case ExchangeKind::Work: return "work";
		case ExchangeKind::Species: return "species";
	}
	return "unknown";
}

} // namespace coarse_grain
