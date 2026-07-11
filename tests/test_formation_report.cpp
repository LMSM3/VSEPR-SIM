#include "coarse_grain/report/formation_report.hpp"
#include "coarse_grain/report/formation_analyzers.hpp"
#include <cstdio>
#include <iostream>

int main() {
	// Build a minimal bead set and neighbor list
	std::vector<coarse_grain::Bead> beads(8);
	coarse_grain::NeighborList nl;
	nl.neighbors.resize(8);
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			if (i != j) nl.neighbors[i].push_back(j);
		}
	}

	// Create analyzer with tetrahedral detector
	coarse_grain::FormationAnalyzer analyzer;
	analyzer.add_motif_detector(
		std::make_unique<coarse_grain::TetrahedronDetector>());

	// Run full pipeline
	auto report = analyzer.analyze(beads, nl);

	// Output all three formats
	std::cout << coarse_grain::generate_report_text(report);
	std::cout << "\n--- CSV ---\n";
	std::cout << coarse_grain::csv_header() << "\n";
	std::cout << coarse_grain::generate_csv_row(report) << "\n";
	std::cout << "\n--- JSON ---\n";
	std::cout << coarse_grain::generate_json_report(report);

	return 0;
}
