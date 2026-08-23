#include "atomistic/classify/script_enrichment.hpp"

#include "atomistic/core/state.hpp"

#include <algorithm>
#include <sstream>

namespace atomistic::classify {
namespace {

class VseprGeometryProvider final : public IPropertyProvider {
public:
	bool supports(const PropertyRequest& request) const override {
		return request.key == PropertyKey::LocalGeometry ||
			   request.key == PropertyKey::VseprLabel ||
			   request.key == PropertyKey::HybridizationCoverage;
	}

	DerivedProperty evaluate(const State& state, const PropertyRequest& request) const override {
		const VSEPRReport report = classify_vsepr_sites(state);
		DerivedProperty property{request.key, {}, 0.0, Provenance::Unavailable, {}, {}};
		property.provider = "VseprGeometryProvider";
		property.provenance = Provenance::DeterministicInference;

		if (request.key == PropertyKey::LocalGeometry) {
			property.value = std::to_string(report.sites.size()) + "/" + std::to_string(state.N) + " atoms";
			property.confidence = state.N == 0 ? 0.0 : 1.0;
		} else if (request.key == PropertyKey::VseprLabel) {
			property.value = report.sites.empty() ? "unavailable" : report.sites.front().ax_label;
			property.confidence = report.sites.empty() ? 0.0 : report.sites.front().confidence;
		} else {
			std::size_t known = 0;
			for (const auto& site : report.sites) {
				if (hybridization_hint(site) != HybridizationHint::Unknown) {
					++known;
				}
			}
			property.value = std::to_string(known) + "/" + std::to_string(report.sites.size()) + " atoms";
			property.confidence = report.sites.empty() ? 0.0 : static_cast<double>(known) / report.sites.size();
		}
		return property;
	}
};

class OrganicDescriptorProvider final : public IPropertyProvider {
public:
	bool supports(const PropertyRequest& request) const override {
		return request.key == PropertyKey::OrganicFamily;
	}

	DerivedProperty evaluate(const State& state, const PropertyRequest& request) const override {
		OrganicClassifier classifier;
		const OrganicCandidate candidate = classifier.classify(state);
		DerivedProperty property{request.key, {}, 0.0, Provenance::Unavailable, {}, {}};
		property.value = organic_family_name(candidate.primary_family);
		property.confidence = candidate.family_source == FamilySource::Inferred ? 0.75 : 0.25;
		property.provenance = candidate.family_source == FamilySource::Inferred
			? Provenance::ApproximateInference : Provenance::FallbackDefault;
		property.provider = "OrganicDescriptorProvider";
		return property;
	}
};

class SafeDefaultProvider final : public IPropertyProvider {
public:
	bool supports(const PropertyRequest& request) const override {
		return request.key == PropertyKey::FormalCharge || request.key == PropertyKey::BondOrder;
	}

	DerivedProperty evaluate(const State&, const PropertyRequest& request) const override {
		DerivedProperty property{request.key, {}, 0.0, Provenance::Unavailable, {}, {}};
		property.provenance = Provenance::FallbackDefault;
		property.provider = "SafeDefaultProvider";
		property.confidence = 0.20;
		property.value = request.key == PropertyKey::FormalCharge ? "assumed neutral" : "partial inference";
		property.diagnostics.push_back({"No explicit provider data was available."});
		return property;
	}
};

} // namespace

ExpandedSimulationRequest expand_scientific_state(
	const State& state,
	const std::vector<const IPropertyProvider*>& providers)
{
	ExpandedSimulationRequest result;
	result.vsepr = classify_vsepr_sites(state);
	result.organic = OrganicClassifier{}.classify(state);

	VseprGeometryProvider geometry_provider;
	OrganicDescriptorProvider organic_provider;
	SafeDefaultProvider fallback_provider;
	const std::vector<const IPropertyProvider*> defaults = {
		&geometry_provider, &organic_provider, &fallback_provider
	};
	const auto& active_providers = providers.empty() ? defaults : providers;

	const std::vector<PropertyRequest> requests = {
		{PropertyKey::LocalGeometry}, {PropertyKey::VseprLabel},
		{PropertyKey::HybridizationCoverage}, {PropertyKey::OrganicFamily},
		{PropertyKey::FormalCharge}, {PropertyKey::BondOrder}
	};
	for (const auto& request : requests) {
		std::vector<DerivedProperty> candidates;
		for (const IPropertyProvider* provider : active_providers) {
			if (provider != nullptr && provider->supports(request)) {
				candidates.push_back(provider->evaluate(state, request));
			}
		}
		if (candidates.empty()) {
			result.unresolved.push_back(request);
			result.diagnostics.push_back({std::string("No provider supports ") + to_string(request.key)});
			continue;
		}
		std::sort(candidates.begin(), candidates.end(), [](const DerivedProperty& lhs, const DerivedProperty& rhs) {
			return lhs.provider < rhs.provider;
		});
		result.inferred.push_back(std::move(candidates.front()));
	}

	if (state.N == 0) {
		result.status = ExpansionStatus::DiagnosticOnly;
		result.diagnostics.push_back({"Cannot expand an empty atomistic state."});
	} else if (result.unresolved.empty()) {
		result.status = ExpansionStatus::Runnable;
		result.runnable = true;
	} else {
		result.status = ExpansionStatus::PartiallyRunnable;
	}
	return result;
}

const char* to_string(PropertyKey key) {
	switch (key) {
	case PropertyKey::LocalGeometry: return "local geometry";
	case PropertyKey::VseprLabel: return "VSEPR label";
	case PropertyKey::HybridizationCoverage: return "hybridization coverage";
	case PropertyKey::OrganicFamily: return "organic family";
	case PropertyKey::FormalCharge: return "formal charge";
	case PropertyKey::BondOrder: return "bond order";
	}
	return "unknown";
}

const char* to_string(Provenance provenance) {
	switch (provenance) {
	case Provenance::DeclaredByUser: return "declared_by_user";
	case Provenance::DeterministicInference: return "deterministic_inference";
	case Provenance::ApproximateInference: return "approximate_inference";
	case Provenance::DatabaseLookup: return "database_lookup";
	case Provenance::FallbackDefault: return "fallback_default";
	case Provenance::Unavailable: return "unavailable";
	}
	return "unavailable";
}

const char* to_string(ExpansionStatus status) {
	switch (status) {
	case ExpansionStatus::Runnable: return "runnable";
	case ExpansionStatus::PartiallyRunnable: return "partially runnable";
	case ExpansionStatus::DiagnosticOnly: return "diagnostic-only";
	}
	return "diagnostic-only";
}

} // namespace atomistic::classify
