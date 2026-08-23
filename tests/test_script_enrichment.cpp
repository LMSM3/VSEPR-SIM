#include "atomistic/classify/script_enrichment.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>

using namespace atomistic;
using namespace atomistic::classify;

#define REQUIRE(condition) do { if (!(condition)) std::abort(); } while (false)

static State make_ch4()
{
	State state;
	state.N = 5;
	state.X = {{0.0, 0.0, 0.0}, {0.89, 0.89, 0.89}, {-0.89, -0.89, 0.89},
			   {-0.89, 0.89, -0.89}, {0.89, -0.89, -0.89}};
	state.V.assign(5, {0.0, 0.0, 0.0});
	state.Q.assign(5, 0.0);
	state.M.assign(5, 1.0);
	state.type = {6u, 1u, 1u, 1u, 1u};
	state.F.assign(5, {0.0, 0.0, 0.0});
	state.B = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
	return state;
}

static const DerivedProperty& property_for(const ExpandedSimulationRequest& result, PropertyKey key)
{
	for (const auto& property : result.inferred) {
		if (property.key == key) {
			return property;
		}
	}
	REQUIRE(false);
	return result.inferred.front();
}

class ChargeProvider final : public IPropertyProvider {
public:
	explicit ChargeProvider(const char* name) : name_(name) {}

	bool supports(const PropertyRequest& request) const override
	{
		return request.key == PropertyKey::FormalCharge;
	}

	DerivedProperty evaluate(const State&, const PropertyRequest& request) const override
	{
		return {request.key, "provider charge", 0.9, Provenance::DeterministicInference, name_, {}};
	}

private:
	std::string name_;
};

static void test_default_properties_have_provenance()
{
	const auto result = expand_scientific_state(make_ch4());
	REQUIRE(result.status == ExpansionStatus::Runnable);
	REQUIRE(result.runnable);
	REQUIRE(result.unresolved.empty());
	for (const auto& property : result.inferred) {
		REQUIRE(!property.provider.empty());
		REQUIRE(property.provenance != Provenance::Unavailable);
	}
	const auto& fallback = property_for(result, PropertyKey::FormalCharge);
	REQUIRE(fallback.provenance == Provenance::FallbackDefault);
	REQUIRE(fallback.confidence < 0.5);
}

static void test_provider_order_is_deterministic()
{
	ChargeProvider zeta("ZetaProvider");
	ChargeProvider alpha("AlphaProvider");
	const State state = make_ch4();
	const auto first = expand_scientific_state(state, {&zeta, &alpha});
	const auto second = expand_scientific_state(state, {&alpha, &zeta});
	REQUIRE(property_for(first, PropertyKey::FormalCharge).provider == "AlphaProvider");
	REQUIRE(property_for(first, PropertyKey::FormalCharge).provider ==
		   property_for(second, PropertyKey::FormalCharge).provider);
}

static void test_unsupported_requests_remain_unresolved()
{
	ChargeProvider charge("ChargeProvider");
	const auto result = expand_scientific_state(make_ch4(), {&charge});
	REQUIRE(result.status == ExpansionStatus::PartiallyRunnable);
	REQUIRE(!result.runnable);
	REQUIRE(!result.unresolved.empty());
	REQUIRE(!result.diagnostics.empty());
}

static void test_empty_state_is_diagnostic_only()
{
	State state;
	const auto result = expand_scientific_state(state);
	REQUIRE(result.status == ExpansionStatus::DiagnosticOnly);
	REQUIRE(!result.runnable);
	REQUIRE(!result.diagnostics.empty());
}

int main()
{
	test_default_properties_have_provenance();
	test_provider_order_is_deterministic();
	test_unsupported_requests_remain_unresolved();
	test_empty_state_is_diagnostic_only();
	std::puts("test_script_enrichment: PASS");
	return 0;
}
