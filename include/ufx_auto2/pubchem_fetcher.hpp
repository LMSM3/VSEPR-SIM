// include/ufx_auto2/pubchem_fetcher.hpp
// UFX_AUTO_2 Phase 5 -- PubChem PUG REST Fetcher
// VSEPR-SIM v5 beta8 -> beta9 target
//
// Queries PubChem PUG REST for identity-level checks:
//   molecular_weight, inchikey, formal_charge, heavy_atom_count
//
// Rate limit: 5 requests/second (PubChem public API policy).
// All responses are raw JSON; parsing handled inside fetch().

#pragma once

#include "ufx_auto2/web_fetcher.hpp"

namespace vsepr::ufx {

class PubChemFetcher : public IWebFetcher {
public:
	// requests_per_second: conservative default 4 (below the 5/s PubChem limit)
	explicit PubChemFetcher(double requests_per_second = 4.0);

	WebResponse fetch(const WebQuery& query) override;

private:
	RateLimiter rate_limiter_;

	// Build the PUG REST URL for a given formula and property.
	static std::string build_url_(const std::string& formula,
								  const std::string& property_name);

	// Perform the HTTP GET and return (status_code, body).
	// Uses WinHTTP on Windows, libcurl on other platforms.
	static std::pair<int, std::string> http_get_(const std::string& url);

	// Parse the PUG REST JSON response for the requested property.
	static WebResponse parse_response_(const WebQuery& query,
									   int status_code,
									   const std::string& body);

	// Compute hex SHA-256 of a string (for raw_response_hash).
	static std::string sha256_hex_(const std::string& data);
};

} // namespace vsepr::ufx
