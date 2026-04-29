// include/ufx_auto2/web_fetcher.hpp
// UFX_AUTO_2 Phase 5 -- Web Fetch Infrastructure
// VSEPR-SIM v5 beta8 -> beta9 target
//
// Defines:
//   WebSource      -- enum of supported external sources
//   WebQuery       -- a single property request to one source
//   WebResponse    -- result of a fetch (found / not found / error)
//   RateLimiter    -- token-bucket style single-thread rate limiter
//   IWebFetcher    -- abstract interface; cache-aware decorator pattern
//
// Hard rules (UFX_continual_2.tex §18):
//   - Missing webdata is NOT failure. It is a confidence cap of 0.85.
//   - Cache first. Fetch second. Never query the same (source, formula,
//     property) pair twice without a cache miss.
//   - Use conservative rates. PubChem: 5 req/s. NIST: 1 req/s.

#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <thread>

namespace vsepr::ufx {

// ============================================================================
// WebSource
// ============================================================================

enum class WebSource : int {
	PubChem,        // PubChem PUG REST — identity and descriptor checks
	NIST,           // NIST Chemistry WebBook — thermo, IR, spectra
	LocalReference  // local UFF reference provider (no network)
};

inline const char* to_string(WebSource s) noexcept {
	switch (s) {
		case WebSource::PubChem:       return "pubchem";
		case WebSource::NIST:          return "nist";
		case WebSource::LocalReference: return "local_reference";
	}
	return "unknown";
}

// ============================================================================
// WebQuery
// ============================================================================

struct WebQuery {
	WebSource   source;
	std::string material_key;   // UFX record key (used for cache lookup)
	std::string formula;        // e.g. "CH4", "Fe2O3"
	std::string property_name;  // e.g. "molecular_weight", "inchikey", "melting_point_K"
	std::string units;          // expected units of the property
};

// ============================================================================
// WebResponse
// ============================================================================

struct WebResponse {
	WebSource   source;
	std::string formula;
	std::string property_name;

	bool found = false;

	std::optional<double> numeric_value;
	std::string           value_text;
	std::string           units;

	// Audit trail
	std::string raw_response_path;  // path to cached raw response file
	std::string raw_response_hash;  // SHA-256 of raw response (hex string)
	std::string source_url_hash;    // hash of the URL queried

	int         status_code = 0;   // HTTP status; 0 = not attempted / cache hit
	std::string error_message;     // non-empty on network or parse error
};

// ============================================================================
// RateLimiter
// Per the design doc: use conservative rates.
//   PubChem: max 5 requests/second.
//   NIST:    max 1 request/second.
// ============================================================================

class RateLimiter {
public:
	explicit RateLimiter(double requests_per_second)
		: min_interval_(std::chrono::duration<double>(
			  1.0 / (requests_per_second > 0.0 ? requests_per_second : 1.0)))
	{}

	// Block until the minimum inter-request interval has elapsed.
	void wait_turn() {
		auto now     = Clock::now();
		auto elapsed = now - last_request_;

		if (elapsed < min_interval_) {
			std::this_thread::sleep_for(min_interval_ - elapsed);
		}

		last_request_ = Clock::now();
	}

private:
	using Clock = std::chrono::steady_clock;

	std::chrono::duration<double> min_interval_;
	Clock::time_point             last_request_ = Clock::now();
};

// ============================================================================
// IWebFetcher — abstract interface
// Implementations: CachingFetcher (decorator), PubChemFetcher, NISTFetcher
// ============================================================================

class IWebFetcher {
public:
	virtual WebResponse fetch(const WebQuery& query) = 0;
	virtual ~IWebFetcher() = default;
};

} // namespace vsepr::ufx
