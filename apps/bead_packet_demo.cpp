#include "coarse_grain/core/bead_packet.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

using coarse_grain::BeadBoundaryMode;
using coarse_grain::BeadExchangeEvent;
using coarse_grain::BeadPacketControlVolume;
using coarse_grain::BeadPacketState;
using coarse_grain::ExchangeKind;

ExchangeKind parse_kind(const std::string& value) {
	if (value == "mass") return ExchangeKind::Mass;
	if (value == "momentum") return ExchangeKind::Momentum;
	if (value == "heat") return ExchangeKind::Heat;
	if (value == "work") return ExchangeKind::Work;
	if (value == "species") return ExchangeKind::Species;
	throw std::runtime_error("unknown exchange kind: " + value);
}

BeadPacketControlVolume load_script(const std::filesystem::path& path) {
	std::ifstream input(path);
	if (!input) throw std::runtime_error("unable to open bead script: " + path.string());

	BeadPacketState state;
	BeadBoundaryMode boundary = BeadBoundaryMode::Closed;
	std::vector<BeadExchangeEvent> events;
	std::string line;
	while (std::getline(input, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream row(line);
		std::string command;
		row >> command;
		if (command == "bead") {
			std::string boundary_name;
			row >> state.identity.packet_id >> state.identity.species >> state.mass
				>> state.position.x >> state.position.y >> state.position.z
				>> state.momentum.x >> state.momentum.y >> state.momentum.z
				>> state.internal_energy >> boundary_name >> state.identity.provenance;
			boundary = boundary_name == "open" ? BeadBoundaryMode::Open : BeadBoundaryMode::Closed;
			state.species_mass[state.identity.species] = state.mass;
		} else if (command == "exchange") {
			BeadExchangeEvent event;
			std::string kind;
			row >> event.sequence >> event.time_fs >> kind >> event.mass_delta
				>> event.momentum_delta.x >> event.momentum_delta.y >> event.momentum_delta.z
				>> event.energy_delta >> event.species >> event.species_mass_delta >> event.source;
			event.kind = parse_kind(kind);
			events.push_back(std::move(event));
		} else {
			throw std::runtime_error("unknown bead script command: " + command);
		}
		if (!row) throw std::runtime_error("invalid bead script row: " + line);
	}

	if (state.identity.packet_id.empty()) throw std::runtime_error("bead script has no bead declaration");
	BeadPacketControlVolume volume(std::move(state), boundary);
	volume.capture_initial();
	for (const auto& event : events) volume.apply(event);
	return volume;
}

void write_artifacts(const std::filesystem::path& output, const BeadPacketControlVolume& volume) {
	std::filesystem::create_directories(output);
	std::ofstream trajectory(output / "bead_trajectory.csv");
	std::ofstream events(output / "bead_events.csv");
	std::ofstream ledger(output / "bead_ledger.json");
	trajectory << std::setprecision(12);
	events << std::setprecision(12);
	trajectory << "sample,time_fs,packet_id,species,mass,x,y,z,px,py,pz,internal_energy\n";
	for (size_t i = 0; i < volume.trajectory().size(); ++i) {
		const auto& state = volume.trajectory()[i];
		trajectory << i << ',' << state.time_fs << ',' << state.identity.packet_id << ','
			<< state.identity.species << ',' << state.mass << ',' << state.position.x << ','
			<< state.position.y << ',' << state.position.z << ',' << state.momentum.x << ','
			<< state.momentum.y << ',' << state.momentum.z << ',' << state.internal_energy << '\n';
	}
	events << "sequence,time_fs,kind,mass_delta,px_delta,py_delta,pz_delta,energy_delta,species,species_mass_delta,source\n";
	for (const auto& event : volume.events()) {
		events << event.sequence << ',' << event.time_fs << ',' << coarse_grain::exchange_kind_name(event.kind)
			<< ',' << event.mass_delta << ',' << event.momentum_delta.x << ',' << event.momentum_delta.y
			<< ',' << event.momentum_delta.z << ',' << event.energy_delta << ',' << event.species
			<< ',' << event.species_mass_delta << ',' << event.source << '\n';
	}
	const auto& state = volume.state();
	const auto& balance = volume.ledger();
	const auto momentum_residual = balance.momentum_residual(state);
	ledger << "{\n  \"schema\": \"vsepr.bead_packet.v1\",\n"
		<< "  \"packet_id\": \"" << state.identity.packet_id << "\",\n"
		<< "  \"boundary\": \"" << (volume.boundary() == BeadBoundaryMode::Open ? "open" : "closed") << "\",\n"
		<< "  \"event_count\": " << volume.events().size() << ",\n"
		<< "  \"mass\": " << state.mass << ",\n"
		<< "  \"momentum\": [" << state.momentum.x << ',' << state.momentum.y << ',' << state.momentum.z << "],\n"
		<< "  \"internal_energy\": " << state.internal_energy << ",\n"
		<< "  \"mass_residual\": " << balance.mass_residual(state) << ",\n"
		<< "  \"momentum_residual\": [" << momentum_residual.x << ',' << momentum_residual.y << ',' << momentum_residual.z << "],\n"
		<< "  \"energy_residual\": " << balance.energy_residual(state) << "\n}\n";
}

} // namespace

int main(int argc, char** argv) {
	std::filesystem::path script = "examples/day89/bead_packet.vsim";
	std::filesystem::path output = "out/day89_bead";
	for (int i = 1; i < argc; ++i) {
		const std::string argument = argv[i];
		if (argument == "--script" && i + 1 < argc) script = argv[++i];
		else if (argument == "--output" && i + 1 < argc) output = argv[++i];
	}
	try {
		const auto volume = load_script(script);
		write_artifacts(output, volume);
		std::cout << "BEAD_PACKET_OK events=" << volume.events().size()
			<< " mass_residual=" << volume.ledger().mass_residual(volume.state()) << '\n';
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "Bead packet error: " << error.what() << '\n';
		return 2;
	}
}
