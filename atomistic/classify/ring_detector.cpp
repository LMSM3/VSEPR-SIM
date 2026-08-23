/**
 * ring_detector.cpp  --  Day 84 / WO-84C
 * ============================================================================
 * Deterministic State-based ring detection.  See ring_detector.hpp.
 *
 * Algorithm:
 *   1. Build the heavy-atom (non-H) undirected graph.
 *   2. circuit_rank = E - V + C  (independent cycle count; ground truth).
 *   3. Ring bonds = non-bridge edges, found by brute-force reachability
 *      (remove edge, BFS; if endpoints still connected the edge is in a cycle).
 *   4. SSSR-lite: for each ring bond, BFS the shortest alternate path to form
 *      a candidate cycle; dedup; sort by size; greedily keep cycles that are
 *      linearly independent over GF(2) on the edge space until we have
 *      circuit_rank of them.
 *   5. Fused pairs = selected ring pairs sharing at least one edge.
 *
 * Molecules handled by the organic classifier are small, so the brute-force
 * bridge test (O(E*(V+E))) is entirely adequate and keeps the code obvious.
 * ============================================================================
 */

#include "atomistic/classify/ring_detector.hpp"
#include "atomistic/core/state.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace atomistic {
namespace classify {

int RingSystem::strained_rings() const {
	int n = 0;
	for (const auto& r : rings) {
		if (r.strained()) ++n;
	}
	return n;
}

bool RingSystem::is_ring_bond(std::size_t i, std::size_t j) const {
	const std::size_t a = std::min(i, j);
	const std::size_t b = std::max(i, j);
	for (const auto& e : ring_bonds) {
		if (e.first == a && e.second == b) return true;
	}
	return false;
}

namespace {

static bool is_hydrogen(uint32_t z) { return z == 1; }

// Compact heavy-atom graph plus index maps between original and heavy space.
struct HeavyGraph {
	std::vector<std::size_t> original_of_heavy;   // heavy idx -> original idx
	std::vector<int>         heavy_of_original;    // original idx -> heavy idx (-1)
	// adjacency: heavy idx -> list of (neighbor heavy idx, edge id)
	std::vector<std::vector<std::pair<std::size_t, int>>> adj;
	// edges: edge id -> (heavy a, heavy b) with a < b
	std::vector<std::pair<std::size_t, std::size_t>> edges;

	std::size_t V() const { return original_of_heavy.size(); }
	std::size_t E() const { return edges.size(); }
};

static HeavyGraph build_heavy_graph(const State& state) {
	HeavyGraph g;
	g.heavy_of_original.assign(state.N, -1);

	for (std::size_t i = 0; i < state.N; ++i) {
		const uint32_t z = (i < state.type.size()) ? state.type[i] : 0;
		if (is_hydrogen(z)) continue;
		g.heavy_of_original[i] = static_cast<int>(g.original_of_heavy.size());
		g.original_of_heavy.push_back(i);
	}

	g.adj.resize(g.V());

	// Deduplicate undirected heavy edges via a canonical (min,max) map.
	std::map<std::pair<std::size_t, std::size_t>, int> edge_id;
	for (const Edge& e : state.B) {
		if (e.i >= state.N || e.j >= state.N || e.i == e.j) continue;
		const int ha = g.heavy_of_original[e.i];
		const int hb = g.heavy_of_original[e.j];
		if (ha < 0 || hb < 0) continue;  // skip edges touching H
		std::size_t a = static_cast<std::size_t>(ha);
		std::size_t b = static_cast<std::size_t>(hb);
		if (a > b) std::swap(a, b);
		auto key = std::make_pair(a, b);
		if (edge_id.count(key)) continue;
		const int id = static_cast<int>(g.edges.size());
		edge_id[key] = id;
		g.edges.push_back(key);
		g.adj[a].push_back({b, id});
		g.adj[b].push_back({a, id});
	}
	return g;
}

// Count connected components of the heavy graph (for circuit rank).
static int count_components(const HeavyGraph& g) {
	const std::size_t n = g.V();
	std::vector<char> seen(n, 0);
	int comps = 0;
	for (std::size_t s = 0; s < n; ++s) {
		if (seen[s]) continue;
		++comps;
		std::queue<std::size_t> q;
		q.push(s);
		seen[s] = 1;
		while (!q.empty()) {
			const std::size_t u = q.front();
			q.pop();
			for (const auto& [v, eid] : g.adj[u]) {
				(void)eid;
				if (!seen[v]) { seen[v] = 1; q.push(v); }
			}
		}
	}
	return comps;
}

// True if edge (a,b) with id skip_id is a NON-bridge (part of a cycle):
// remove it and test whether a can still reach b.
static bool edge_is_in_cycle(const HeavyGraph& g,
							 std::size_t a, std::size_t b, int skip_id) {
	std::vector<char> seen(g.V(), 0);
	std::queue<std::size_t> q;
	q.push(a);
	seen[a] = 1;
	while (!q.empty()) {
		const std::size_t u = q.front();
		q.pop();
		if (u == b) return true;
		for (const auto& [v, eid] : g.adj[u]) {
			if (eid == skip_id) continue;   // the edge under test
			if (!seen[v]) { seen[v] = 1; q.push(v); }
		}
	}
	return false;
}

// Shortest alternate cycle through edge (a,b, skip_id): BFS a->b without that
// edge, return the cycle as an ordered list of heavy atom indices (a..b), or
// empty if none.
static std::vector<std::size_t> shortest_cycle_through_edge(
		const HeavyGraph& g, std::size_t a, std::size_t b, int skip_id) {
	std::vector<int> parent(g.V(), -1);
	std::vector<char> seen(g.V(), 0);
	std::queue<std::size_t> q;
	q.push(a);
	seen[a] = 1;
	bool found = false;
	while (!q.empty() && !found) {
		const std::size_t u = q.front();
		q.pop();
		for (const auto& [v, eid] : g.adj[u]) {
			if (eid == skip_id) continue;
			if (seen[v]) continue;
			seen[v] = 1;
			parent[v] = static_cast<int>(u);
			if (v == b) { found = true; break; }
			q.push(v);
		}
	}
	std::vector<std::size_t> path;
	if (!found) return path;
	for (int cur = static_cast<int>(b); cur != -1; cur = parent[cur]) {
		path.push_back(static_cast<std::size_t>(cur));
		if (cur == static_cast<int>(a)) break;
	}
	std::reverse(path.begin(), path.end());
	return path;  // a ... b  (edge b-a closes the ring)
}

// --- GF(2) xor-basis over edge ids -----------------------------------------

using EdgeMask = std::vector<uint64_t>;

static void mask_set(EdgeMask& m, int bit) {
	const std::size_t w = static_cast<std::size_t>(bit) / 64;
	if (w >= m.size()) m.resize(w + 1, 0);
	m[w] |= (uint64_t{1} << (bit % 64));
}

static int mask_highest_bit(const EdgeMask& m) {
	for (std::size_t w = m.size(); w-- > 0;) {
		if (m[w]) {
			int b = 63;
			while (b >= 0 && !((m[w] >> b) & 1)) --b;
			return static_cast<int>(w) * 64 + b;
		}
	}
	return -1;
}

static void mask_xor(EdgeMask& a, const EdgeMask& b) {
	if (a.size() < b.size()) a.resize(b.size(), 0);
	for (std::size_t i = 0; i < b.size(); ++i) a[i] ^= b[i];
}

// Try to add mask to the pivoted basis; return true if independent (added).
static bool basis_add(std::map<int, EdgeMask>& basis, EdgeMask v) {
	for (;;) {
		const int h = mask_highest_bit(v);
		if (h < 0) return false;                 // reduced to zero -> dependent
		auto it = basis.find(h);
		if (it == basis.end()) { basis[h] = v; return true; }
		mask_xor(v, it->second);
	}
}

// Edge ids for a cycle expressed as an ordered atom path plus its closing edge.
static std::vector<int> cycle_edge_ids(const HeavyGraph& g,
									   const std::vector<std::size_t>& path) {
	std::vector<int> ids;
	auto find_edge = [&](std::size_t u, std::size_t v) -> int {
		for (const auto& [w, eid] : g.adj[u]) {
			if (w == v) return eid;
		}
		return -1;
	};
	for (std::size_t k = 0; k + 1 < path.size(); ++k) {
		const int id = find_edge(path[k], path[k + 1]);
		if (id >= 0) ids.push_back(id);
	}
	if (path.size() >= 2) {
		const int closing = find_edge(path.back(), path.front());
		if (closing >= 0) ids.push_back(closing);
	}
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	return ids;
}

} // namespace

RingSystem RingDetector::detect(const State& state) const {
	RingSystem result;
	result.atom_ring_sizes.assign(state.N, {});

	const HeavyGraph g = build_heavy_graph(state);
	const int V = static_cast<int>(g.V());
	const int E = static_cast<int>(g.E());
	if (V == 0 || E == 0) return result;

	const int C = count_components(g);
	result.circuit_rank = std::max(0, E - V + C);

	// Ring bonds = non-bridge edges. Record in original-index (min,max) form.
	std::vector<int> ring_edge_ids;
	for (int id = 0; id < E; ++id) {
		const auto [a, b] = g.edges[static_cast<std::size_t>(id)];
		if (edge_is_in_cycle(g, a, b, id)) {
			ring_edge_ids.push_back(id);
			const std::size_t oa = g.original_of_heavy[a];
			const std::size_t ob = g.original_of_heavy[b];
			result.ring_bonds.push_back({std::min(oa, ob), std::max(oa, ob)});
		}
	}

	if (result.circuit_rank == 0) return result;

	// Build candidate cycles from ring bonds, dedup by canonical atom set.
	struct Candidate {
		std::vector<std::size_t> atoms_heavy;   // ordered cycle
		std::vector<int>         edge_ids;       // sorted
		EdgeMask                 mask;
	};
	std::vector<Candidate> candidates;
	std::vector<std::vector<std::size_t>> seen_atomsets;

	for (int id : ring_edge_ids) {
		if (id >= static_cast<int>(g.edges.size())) continue;
		const auto [a, b] = g.edges[static_cast<std::size_t>(id)];
		auto path = shortest_cycle_through_edge(g, a, b, id);
		if (path.size() < 3) continue;
		if (static_cast<int>(path.size()) > max_ring_size_) continue;

		std::vector<std::size_t> canon = path;
		std::sort(canon.begin(), canon.end());
		if (std::find(seen_atomsets.begin(), seen_atomsets.end(), canon)
				!= seen_atomsets.end()) {
			continue;
		}
		seen_atomsets.push_back(canon);

		Candidate cand;
		cand.atoms_heavy = std::move(path);
		cand.edge_ids = cycle_edge_ids(g, cand.atoms_heavy);
		for (int eid : cand.edge_ids) mask_set(cand.mask, eid);
		candidates.push_back(std::move(cand));
	}

	std::stable_sort(candidates.begin(), candidates.end(),
		[](const Candidate& x, const Candidate& y) {
			if (x.atoms_heavy.size() != y.atoms_heavy.size())
				return x.atoms_heavy.size() < y.atoms_heavy.size();
			return x.atoms_heavy < y.atoms_heavy;   // deterministic tie-break
		});

	// Greedy SSSR-lite selection over the GF(2) edge basis.
	std::map<int, EdgeMask> basis;
	std::vector<const Candidate*> selected;
	for (const auto& cand : candidates) {
		if (static_cast<int>(selected.size()) >= result.circuit_rank) break;
		if (basis_add(basis, cand.mask)) {
			selected.push_back(&cand);
		}
	}

	// Materialize selected rings, per-atom sizes, and fused-pair count.
	for (const Candidate* c : selected) {
		DetectedRing ring;
		ring.atoms.reserve(c->atoms_heavy.size());
		for (std::size_t h : c->atoms_heavy) {
			ring.atoms.push_back(g.original_of_heavy[h]);
		}
		const int sz = static_cast<int>(ring.atoms.size());
		for (std::size_t orig : ring.atoms) {
			result.atom_ring_sizes[orig].push_back(sz);
		}
		result.rings.push_back(std::move(ring));
	}

	for (std::size_t i = 0; i < selected.size(); ++i) {
		for (std::size_t j = i + 1; j < selected.size(); ++j) {
			std::vector<int> shared;
			std::set_intersection(
				selected[i]->edge_ids.begin(), selected[i]->edge_ids.end(),
				selected[j]->edge_ids.begin(), selected[j]->edge_ids.end(),
				std::back_inserter(shared));
			if (!shared.empty()) ++result.fused_ring_pairs;
		}
	}

	for (auto& sizes : result.atom_ring_sizes) {
		std::sort(sizes.begin(), sizes.end());
	}
	return result;
}

RingSystem detect_ring_system(const State& state, int max_ring_size) {
	return RingDetector(max_ring_size).detect(state);
}

} // namespace classify
} // namespace atomistic
