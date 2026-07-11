// include/ufx_auto2/materials_project_fetcher.hpp
// UFX_AUTO_2 Phase 8 -- Materials Project API Fetcher
// VSEPR-SIM v5 beta9
//
// MaterialsProjectFetcher queries the Materials Project REST API for
// formation energy, space group, and band gap data.
//
// If MP_API_KEY env variable is not set, the fetcher operates in
// cache-read-only mode: no new HTTP requests, existing cached responses used.
//
// Cache: web_cache/mp/{sha256_key}.json

#pragma once

#include <string>

namespace vsepr::ufx {

struct MPResponse {
	bool        found           = false;
	bool        from_cache      = false;
	std::string formula;
	std::string space_group;
	int         space_group_number = 0;
	double      formation_energy_eV_atom = 0.0;
	double      energy_above_hull_eV_atom = 0.0;
	double      band_gap_eV     = 0.0;
	bool        is_metallic     = false;
	std::string mp_id;
	std::string error_message;
};

class MaterialsProjectFetcher {
public:
	// api_key: empty = try MP_API_KEY env var; if still empty, cache-read-only.
	explicit MaterialsProjectFetcher(const std::string& cache_dir = "web_cache/mp",
									 const std::string& api_key   = "");

	// Fetch data for the given element formula.
	// Returns the best matching MP entry or MPResponse{found=false}.
	MPResponse fetch(const std::string& formula) const;

	bool is_cache_only() const noexcept { return api_key_.empty(); }

private:
	std::string cache_dir_;
	std::string api_key_;

	std::string cache_key_ (const std::string& formula) const;
	std::string cache_path_(const std::string& formula) const;

	MPResponse  from_cache_(const std::string& formula)   const;
	MPResponse  from_network_(const std::string& formula) const;
	MPResponse  parse_json_  (const std::string& body,
							  const std::string& formula) const;
};

} // namespace vsepr::ufx
