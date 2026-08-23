/**
 * test_bond_order_provider.cpp  --  Day 84 / WO-84D
 * ============================================================================
 * CTest Group 102 (part 1).
 *
 * Verifies valence-deficit bond-order inference and the BondOrderProvider:
 *
 *   ethane  (C2H6)  -> C-C single (1.0)
 *   ethene  (C2H4)  -> C=C double (2.0)
 *   ethyne  (C2H2)  -> C#C triple (3.0)
 *   acetone (C3H6O) -> one C=O double bond present
 *   benzene (C6H6)  -> 3 alternating C=C double bonds (Kekule)
 *
 * Acceptance (WO-84D, bond-order portion):
 *   - Bond order can be queried through the provider interface.   [checked]
 *   - Double/triple bonds are recognised.                         [checked]
 *   - Single-bond-only species report no pi bonds.                [checked]
 */

#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/classify/providers.hpp"
#include "atomistic/core/state.hpp"

#include <iostream>
#include <string>
#include <vector>

using atomistic::Edge;
using atomistic::State;
using atomistic::classify::BondOrderProvider;
using atomistic::classify::BondOrderProviderBuilder;
using atomistic::classify::BondOrderTable;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& what) {
	if (cond) { ++g_pass; std::cout << "  PASS: " << what << "\n"; }
	else      { ++g_fail; std::cout << "  FAIL: " << what << "\n"; }
}

State make_state(std::vector<uint32_t> types, std::vector<Edge> bonds) {
	State s;
	s.N = static_cast<uint32_t>(types.size());
	s.type = std::move(types);
	s.X.assign(s.N, {0.0, 0.0, 0.0});
	s.V.resize(s.N);
	s.Q.assign(s.N, 0.0);
	s.M.assign(s.N, 1.0);
	s.B = std::move(bonds);
	return s;
}

// ethane C2H6: C0-C1 with 3 H each.
State make_ethane() {
	return make_state({6,6,1,1,1,1,1,1},
		{{0,1},{0,2},{0,3},{0,4},{1,5},{1,6},{1,7}});
}

// ethene C2H4: C0=C1 with 2 H each.
State make_ethene() {
	return make_state({6,6,1,1,1,1},
		{{0,1},{0,2},{0,3},{1,4},{1,5}});
}

// ethyne C2H2: C0#C1 with 1 H each.
State make_ethyne() {
	return make_state({6,6,1,1},
		{{0,1},{0,2},{1,3}});
}

// acetone (CH3)2C=O:  C1 is the carbonyl carbon bonded to C0, C2, O3.
State make_acetone() {
	return make_state({6,6,6,8,1,1,1,1,1,1},
		{{1,0},{1,2},{1,3},          // C1-C0, C1-C2, C1-O3
		 {0,4},{0,5},{0,6},          // C0 methyl H
		 {2,7},{2,8},{2,9}});        // C2 methyl H
}

// benzene C6H6: 6-ring + one H per carbon.
State make_benzene() {
	return make_state({6,6,6,6,6,6,1,1,1,1,1,1},
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0},
		 {0,6},{1,7},{2,8},{3,9},{4,10},{5,11}});
}

void test_single_double_triple() {
	{
		BondOrderTable t = BondOrderProviderBuilder::infer(make_ethane());
		check(t.order_for(0,1) == 1.0, "ethane: C-C single (1.0)");
		check(!t.has_double_or_higher(), "ethane: no double/triple bonds");
	}
	{
		BondOrderTable t = BondOrderProviderBuilder::infer(make_ethene());
		check(t.order_for(0,1) == 2.0, "ethene: C=C double (2.0)");
		check(t.has_double_or_higher(), "ethene: double bond present");
	}
	{
		BondOrderTable t = BondOrderProviderBuilder::infer(make_ethyne());
		check(t.order_for(0,1) == 3.0, "ethyne: C#C triple (3.0)");
	}
	{
		BondOrderTable t = BondOrderProviderBuilder::infer(make_acetone());
		check(t.order_for(1,3) == 2.0, "acetone: C=O carbonyl double (2.0)");
		check(t.order_for(1,0) == 1.0, "acetone: C-C single (1.0)");
	}
}

void test_provider_interface_and_benzene() {
	BondOrderTable table;
	BondOrderProvider provider = BondOrderProviderBuilder{}.build(make_benzene(), table);
	check(provider.available(), "benzene: bond-order provider available");

	// Count ring C=C double bonds (should be 3 in a Kekule benzene).
	const int ring_bonds[6][2] = {{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}};
	int doubles = 0;
	for (auto& rb : ring_bonds) {
		auto o = provider.get(static_cast<std::size_t>(rb[0]),
							  static_cast<std::size_t>(rb[1]));
		check(o.has_value(), "benzene: ring bond order is queryable");
		if (o && *o >= 1.5) ++doubles;
	}
	check(doubles == 3, "benzene: 3 alternating C=C double bonds (Kekule)");

	// Unknown / non-existent bond returns nullopt.
	check(!provider.get(0, 3).has_value(), "benzene: non-bond (0,3) -> nullopt");
}

void test_typical_valence() {
	using B = BondOrderProviderBuilder;
	check(B::typical_valence(6) == 4, "valence(C) == 4");
	check(B::typical_valence(7) == 3, "valence(N) == 3");
	check(B::typical_valence(8) == 2, "valence(O) == 2");
	check(B::typical_valence(1) == 1, "valence(H) == 1");
	check(B::typical_valence(26) == 0, "valence(Fe) == 0 (conservative, no pi)");
}

} // namespace

int main() {
	std::cout << "Group 102a  --  WO-84D bond-order provider\n";
	test_single_double_triple();
	test_provider_interface_and_benzene();
	test_typical_valence();

	std::cout << "\n  Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "  PASS: all Group 102a bond-order checks satisfied\n";
		return 0;
	}
	std::cout << "  FAIL: Group 102a bond-order checks failed\n";
	return 1;
}
