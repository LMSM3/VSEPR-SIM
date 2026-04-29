// src/ufx_auto2/materials_project_fetcher.cpp
// UFX_AUTO_2 Phase 8 -- Materials Project API Fetcher
// VSEPR-SIM v5 beta9
//
// Cache-read-only when MP_API_KEY is not set.
// Cache stored at: web_cache/mp/{sha256-like key}.json

#include "ufx_auto2/materials_project_fetcher.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif

namespace vsepr::ufx {

// ============================================================================
// Simple hex hash for cache key (djb2 -> hex string)
// ============================================================================

static std::string djb2_hex(const std::string& s) {
	unsigned long hash = 5381;
	for (unsigned char c : s)
		hash = ((hash << 5) + hash) + c;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%016lx", hash);
	return std::string(buf);
}

// ============================================================================
// Constructor
// ============================================================================

MaterialsProjectFetcher::MaterialsProjectFetcher(const std::string& cache_dir,
												  const std::string& api_key)
	: cache_dir_(cache_dir)
{
	if (!api_key.empty()) {
		api_key_ = api_key;
	} else {
		const char* env = std::getenv("MP_API_KEY");
		if (env) api_key_ = env;
	}

	// Ensure cache directory exists
	std::error_code ec;
	std::filesystem::create_directories(cache_dir_, ec);
}

// ============================================================================
// Cache key / path
// ============================================================================

std::string MaterialsProjectFetcher::cache_key_(const std::string& formula) const {
	return djb2_hex("mp_summary_" + formula);
}

std::string MaterialsProjectFetcher::cache_path_(const std::string& formula) const {
	return cache_dir_ + "/" + cache_key_(formula) + ".json";
}

// ============================================================================
// from_cache_
// ============================================================================

MPResponse MaterialsProjectFetcher::from_cache_(const std::string& formula) const {
	std::string path = cache_path_(formula);
	std::ifstream f(path);
	if (!f.is_open()) return {};

	std::ostringstream buf;
	buf << f.rdbuf();
	MPResponse r = parse_json_(buf.str(), formula);
	r.from_cache = true;
	return r;
}

// ============================================================================
// parse_json_  (minimal hand-rolled JSON extraction)
// Parses the Materials Project /materials/summary/ response.
// ============================================================================

static std::string extract_field(const std::string& json,
								 const std::string& key) {
	auto pos = json.find("\"" + key + "\"");
	if (pos == std::string::npos) return "";
	pos = json.find(':', pos);
	if (pos == std::string::npos) return "";
	pos++;
	// Skip whitespace
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r'))
		++pos;
	if (pos >= json.size()) return "";

	if (json[pos] == '"') {
		// String value
		auto end = json.find('"', pos + 1);
		if (end == std::string::npos) return "";
		return json.substr(pos + 1, end - pos - 1);
	} else {
		// Numeric or keyword value
		auto end = pos;
		while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']')
			++end;
		std::string v = json.substr(pos, end - pos);
		// trim
		while (!v.empty() && (v.back() == ' ' || v.back() == '\n')) v.pop_back();
		return v;
	}
}

MPResponse MaterialsProjectFetcher::parse_json_(const std::string& body,
												 const std::string& formula) const {
	MPResponse r;
	if (body.empty() || body.find("data") == std::string::npos) return r;

	r.formula = formula;

	// Try to extract key fields from the first element of "data" array
	std::string sg = extract_field(body, "symmetry");
	if (sg.empty()) sg = extract_field(body, "spacegroup");
	r.space_group = sg;

	std::string sgn = extract_field(body, "number");
	if (!sgn.empty()) {
		try { r.space_group_number = std::stoi(sgn); } catch(...) {}
	}

	std::string fe = extract_field(body, "formation_energy_per_atom");
	if (!fe.empty()) {
		try { r.formation_energy_eV_atom = std::stod(fe); } catch(...) {}
	}

	std::string hull = extract_field(body, "energy_above_hull");
	if (!hull.empty()) {
		try { r.energy_above_hull_eV_atom = std::stod(hull); } catch(...) {}
	}

	std::string bg = extract_field(body, "band_gap");
	if (!bg.empty()) {
		try {
			r.band_gap_eV = std::stod(bg);
			r.is_metallic = (r.band_gap_eV < 0.01);
		} catch(...) {}
	}

	std::string mpid = extract_field(body, "material_id");
	r.mp_id = mpid;

	r.found = !r.mp_id.empty() || r.space_group_number > 0 || r.formation_energy_eV_atom != 0.0;
	return r;
}

// ============================================================================
// from_network_  (WinHTTP on Windows)
// ============================================================================

MPResponse MaterialsProjectFetcher::from_network_(const std::string& formula) const {
#if defined(_WIN32)
	if (api_key_.empty()) return {};   // cache-read-only mode

	// Build URL
	// GET https://api.materialsproject.org/materials/summary/?formula=<formula>&fields=...
	std::string path = "/materials/summary/?formula=" + formula +
					   "&fields=material_id,symmetry,formation_energy_per_atom,"
					   "energy_above_hull,band_gap&_limit=1";

	HINTERNET hSession = WinHttpOpen(L"UFX_AUTO2/1.0 (VSEPR-SIM)",
									 WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
									 WINHTTP_NO_PROXY_NAME,
									 WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return {};

	HINTERNET hConnect = WinHttpConnect(hSession,
										L"api.materialsproject.org",
										INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }

	std::wstring wpath(path.begin(), path.end());
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
											nullptr, WINHTTP_NO_REFERER,
											WINHTTP_DEFAULT_ACCEPT_TYPES,
											WINHTTP_FLAG_SECURE);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return {};
	}

	// Add API key header
	std::wstring auth_hdr = L"X-API-KEY: " + std::wstring(api_key_.begin(), api_key_.end());
	WinHttpAddRequestHeaders(hRequest, auth_hdr.c_str(), (DWORD)-1,
							 WINHTTP_ADDREQ_FLAG_ADD);

	BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
								  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return {};
	}

	std::string body;
	DWORD bytes_avail = 0;
	while (WinHttpQueryDataAvailable(hRequest, &bytes_avail) && bytes_avail > 0) {
		std::string chunk(bytes_avail, '\0');
		DWORD bytes_read = 0;
		WinHttpReadData(hRequest, &chunk[0], bytes_avail, &bytes_read);
		body.append(chunk.data(), bytes_read);
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (body.empty()) return {};

	// Cache the response
	std::string cpath = cache_path_(formula);
	std::ofstream f(cpath);
	if (f.is_open()) f << body;

	return parse_json_(body, formula);
#else
	(void)formula;
	return {};   // Non-Windows: cache-only
#endif
}

// ============================================================================
// fetch
// ============================================================================

MPResponse MaterialsProjectFetcher::fetch(const std::string& formula) const {
	// Try cache first
	MPResponse cached = from_cache_(formula);
	if (cached.found) return cached;

	// Network if key available
	if (!api_key_.empty()) {
		MPResponse net = from_network_(formula);
		if (net.found) return net;
	}

	return {};
}

} // namespace vsepr::ufx
