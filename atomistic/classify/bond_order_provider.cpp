/**
 * bond_order_provider.cpp  --  Day 84 / WO-84D
 * ============================================================================
 * Implementation of the valence-deficit bond-order inference.  See header.
 * ============================================================================
 */

#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/core/state.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace atomistic {
namespace classify {

int BondOrderProviderBuilder::typical_valence(int z) {
	switch (z) {
		case 1:  return 1;   // H
		case 5:  return 3;   // B
		case 6:  return 4;   // C
		case 7:  return 3;   // N
		case 8:  return 2;   // O
		case 9:  return 1;   // F
		case 14: return 4;   // Si
		case 15: return 3;   // P (common)
		case 16: return 2;   // S (common)
		case 17: return 1;   // Cl
		case 35: return 1;   // Br
		case 53: return 1;   // I
		default: return 0;   // unknown / TM -> no pi accounting (single bonds)
	}
}

BondOrderTable BondOrderProviderBuilder::infer(const State& state) {
	BondOrderTable table;
	if (state.N == 0 || state.B.empty()) return table;

	// Canonical undirected edge list (dedup) and per-atom adjacency by edge id.
	std::vector<std::pair<std::size_t, std::size_t>> edges;
	std::vector<std::vector<std::size_t>> incident(state.N);   // atom -> edge ids
	{
		std::vector<std::pair<std::size_t, std::size_t>> seen;
		for (const Edge& e : state.B) {
			if (e.i >= state.N || e.j >= state.N || e.i == e.j) continue;
			std::size_t a = e.i, b = e.j;
			if (a > b) std::swap(a, b);
			auto key = std::make_pair(a, b);
			if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
			seen.push_back(key);
			const std::size_t id = edges.size();
			edges.push_back(key);
			incident[a].push_back(id);
			incident[b].push_back(id);
		}
	}

	// Initialise every real bond to single order.
	std::vector<double> order(edges.size(), 1.0);
	for (const auto& e : edges) {
		table.orders[e] = 1.0;
	}

	// sigma degree = number of incident bonds (H included).
	std::vector<int> deficit(state.N, 0);
	for (std::size_t i = 0; i < state.N; ++i) {
		const int z = (i < state.type.size()) ? static_cast<int>(state.type[i]) : 0;
		const int val = typical_valence(z);
		const int sigma = static_cast<int>(incident[i].size());
		deficit[i] = std::max(0, val - sigma);
	}

	// Greedy deterministic pi assignment: iterate edges in canonical order,
	// promote a bond whenever both endpoints still have valence deficit.  Up to
	// two promotions per bond (single->double->triple).
	bool progress = true;
	while (progress) {
		progress = false;
		for (std::size_t id = 0; id < edges.size(); ++id) {
			const auto [a, b] = edges[id];
			if (order[id] >= 3.0) continue;
			if (deficit[a] > 0 && deficit[b] > 0) {
				order[id] += 1.0;
				--deficit[a];
				--deficit[b];
				table.orders[edges[id]] = order[id];
				progress = true;
			}
		}
	}

	return table;
}

BondOrderProvider BondOrderProviderBuilder::build(const State& state) const {
	BondOrderTable table;
	return build(state, table);
}

BondOrderProvider BondOrderProviderBuilder::build(const State& state,
												  BondOrderTable& out_table) const {
	out_table = infer(state);

	BondOrderProvider provider;
	provider.fn = [orders = out_table.orders](std::size_t i, std::size_t j)
			-> std::optional<double> {
		const auto key = std::make_pair(std::min(i, j), std::max(i, j));
		auto it = orders.find(key);
		if (it == orders.end()) return std::nullopt;
		return it->second;
	};
	return provider;
}

} // namespace classify
} // namespace atomistic
