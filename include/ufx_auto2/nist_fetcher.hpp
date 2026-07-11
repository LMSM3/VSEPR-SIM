// include/ufx_auto2/nist_fetcher.hpp
// UFX_AUTO_2 Phase 5 -- NIST Chemistry WebBook Fetcher
// VSEPR-SIM v5 beta8 -> beta9 target
//
// Queries NIST WebBook for thermochemical and species-existence checks:
//   species_exists, melting_point_K, boiling_point_K, delta_Hf_298
//
// Rate limit: 1 request/second (conservative; NIST has no published limit
// but aggressive scraping violates their acceptable use policy).
// Responses are HTML; a minimal string-search parser is used.

#pragma once

#include "ufx_auto2/web_fetcher.hpp"

namespace vsepr::ufx {

class NISTFetcher : public IWebFetcher {
public:
	// requests_per_second: default 1.0 — conservative for NIST HTML scraping
	explicit NISTFetcher(double requests_per_second = 1.0);

	WebResponse fetch(const WebQuery& query) override;

private:
	RateLimiter rate_limiter_;

	// Build the NIST WebBook search URL for a formula.
	static std::string build_url_(const std::string& formula,
								  const std::string& property_name);

	// Perform the HTTP GET and return (status_code, body).
	static std::pair<int, std::string> http_get_(const std::string& url);

	// Parse the NIST HTML response for the requested property.
	static WebResponse parse_response_(const WebQuery& query,
									   int status_code,
									   const std::string& body);

	// Compute hex SHA-256 of a string (for raw_response_hash).
	static std::string sha256_hex_(const std::string& data);
};

} // namespace vsepr::ufx
