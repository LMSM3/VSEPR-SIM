#pragma once

#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/vsepr.hpp"

#include <memory>
#include <string>
#include <vector>

namespace atomistic {
struct State;

namespace classify {

enum class PropertyKey {
	LocalGeometry,
	VseprLabel,
	HybridizationCoverage,
	OrganicFamily,
	FormalCharge,
	BondOrder
};

enum class Provenance {
	DeclaredByUser,
	DeterministicInference,
	ApproximateInference,
	DatabaseLookup,
	FallbackDefault,
	Unavailable
};

struct Diagnostic {
	std::string message;
};

struct DerivedProperty {
	PropertyKey key;
	std::string value;
	double confidence = 0.0;
	Provenance provenance = Provenance::Unavailable;
	std::string provider;
	std::vector<Diagnostic> diagnostics;
};

struct PropertyRequest {
	PropertyKey key;
};

class IPropertyProvider {
public:
	virtual ~IPropertyProvider() = default;
	virtual bool supports(const PropertyRequest& request) const = 0;
	virtual DerivedProperty evaluate(const State& state, const PropertyRequest& request) const = 0;
};

enum class ExpansionStatus {
	Runnable,
	PartiallyRunnable,
	DiagnosticOnly
};

struct ExpandedSimulationRequest {
	VSEPRReport vsepr;
	OrganicCandidate organic;
	std::vector<DerivedProperty> inferred;
	std::vector<PropertyRequest> unresolved;
	std::vector<Diagnostic> diagnostics;
	ExpansionStatus status = ExpansionStatus::DiagnosticOnly;
	bool runnable = false;
};

ExpandedSimulationRequest expand_scientific_state(
	const State& state,
	const std::vector<const IPropertyProvider*>& providers = {});

const char* to_string(PropertyKey key);
const char* to_string(Provenance provenance);
const char* to_string(ExpansionStatus status);

} // namespace classify
} // namespace atomistic
