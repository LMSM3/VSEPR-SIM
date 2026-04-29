// ============================================================================
// ring_detect.cpp — Glass Module: Small Cycle Detection
// ============================================================================
// Bounded-DFS ring finder.  For each bond, attempts to close a cycle back
// to the start atom via a short path (≤ max_size).  Deduplicates by
// canonical rotation of atom-index lists.
// ============================================================================

#include "ring_detect.hpp"
#include <algorithm>
#include <set>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Canonical form: rotate smallest-first, then compare lexicographically.
// -----------------------------------------------------------------------
static std::vector<uint32_t> canonical(std::vector<uint32_t> r) {
    if (r.empty()) return r;
    auto it = std::min_element(r.begin(), r.end());
    std::rotate(r.begin(), it, r.end());
    // Also consider reverse (rings are undirected)
    std::vector<uint32_t> rev(r.rbegin(), r.rend());
    auto it2 = std::min_element(rev.begin(), rev.end());
    std::rotate(rev.begin(), it2, rev.end());
    return (r < rev) ? r : rev;
}

std::vector<Ring> detect_rings(const GlassMolecule& mol, uint32_t max_size) {
    std::set<std::vector<uint32_t>> found;
    std::vector<Ring> result;

    const auto n = static_cast<uint32_t>(mol.atoms.size());
    if (n == 0 || max_size < 3) return result;

    // For each atom, run bounded DFS looking for paths that return to it
    for (uint32_t start = 0; start < n; ++start) {
        struct Frame { uint32_t atom; std::vector<uint32_t> path; };
        std::vector<Frame> stack;
        stack.push_back({start, {start}});

        while (!stack.empty()) {
            auto [cur, path] = std::move(stack.back());
            stack.pop_back();

            if (path.size() > max_size) continue;

            for (uint32_t bid : mol.atoms[cur].bond_ids) {
                uint32_t nbr = mol.other(bid, cur);

                // Found ring back to start?
                if (nbr == start && path.size() >= 3) {
                    auto c = canonical(path);
                    if (found.insert(c).second) {
                        Ring ring;
                        ring.atom_indices = std::move(c);
                        result.push_back(std::move(ring));
                    }
                    continue;
                }

                // Don't revisit atoms already in path (simple cycle only)
                if (std::find(path.begin(), path.end(), nbr) != path.end())
                    continue;

                // Don't exceed max depth
                if (path.size() >= max_size) continue;

                auto next_path = path;
                next_path.push_back(nbr);
                stack.push_back({nbr, std::move(next_path)});
            }
        }
    }

    // Sort by ring size (smallest first)
    std::sort(result.begin(), result.end(),
              [](const Ring& a, const Ring& b) { return a.size() < b.size(); });

    return result;
}

} // namespace glass
} // namespace vsepr
