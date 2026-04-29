// src/ufx_auto2/pubchem_fetcher.cpp
// UFX_AUTO_2 Phase 5 -- PubChem PUG REST Fetcher Implementation
// VSEPR-SIM v5 beta8 -> beta9 target

#include "ufx_auto2/pubchem_fetcher.hpp"

#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif

namespace vsepr::ufx {

// ============================================================================
// PubChem PUG REST property name map
// query.property_name -> PUG REST property token
// ============================================================================

static const struct { const char* ufx_name; const char* pug_name; } k_property_map[] = {
	{ "molecular_weight",   "MolecularWeight" },
	{ "inchikey",           "InChIKey"        },
	{ "inchi",              "InChI"           },
	{ "canonical_smiles",   "CanonicalSMILES" },
	{ "formal_charge",      "Charge"          },
	{ "heavy_atom_count",   "HeavyAtomCount"  },
	{ "xlogp",              "XLogP"           },
	{ "tpsa",               "TPSA"            },
	{ "rotatable_bond_count", "RotatableBondCount" },
	{ "hbond_donor_count",  "HBondDonorCount" },
	{ "hbond_acceptor_count", "HBondAcceptorCount" },
};

// ============================================================================
// Constructor
// ============================================================================

PubChemFetcher::PubChemFetcher(double requests_per_second)
	: rate_limiter_(requests_per_second)
{}

// ============================================================================
// build_url_
// Format: https://pubchem.ncbi.nlm.nih.gov/rest/pug/compound/formula/<formula>/property/<Prop>/JSON
// ============================================================================

std::string PubChemFetcher::build_url_(const std::string& formula,
									   const std::string& property_name) {
	// Map UFX property name to PUG REST token
	std::string pug = "MolecularWeight";  // fallback
	for (const auto& row : k_property_map) {
		if (property_name == row.ufx_name) {
			pug = row.pug_name;
			break;
		}
	}

	return "https://pubchem.ncbi.nlm.nih.gov/rest/pug/compound/formula/"
		   + formula + "/property/" + pug + "/JSON";
}

// ============================================================================
// http_get_  (WinHTTP implementation)
// Returns {status_code, body}. On error: {0, ""}.
// ============================================================================

std::pair<int, std::string> PubChemFetcher::http_get_(const std::string& url) {
#if defined(_WIN32)
	// Parse URL
	URL_COMPONENTS components = {};
	components.dwStructSize = sizeof(components);

	wchar_t host_buf[512]   = {};
	wchar_t path_buf[2048]  = {};
	components.lpszHostName     = host_buf;
	components.dwHostNameLength = 511;
	components.lpszUrlPath      = path_buf;
	components.dwUrlPathLength  = 2047;

	std::wstring wurl(url.begin(), url.end());
	if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &components))
		return {0, ""};

	HINTERNET session = WinHttpOpen(
		L"VSEPR-SIM UFX_AUTO_2/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) return {0, ""};

	HINTERNET connect = WinHttpConnect(
		session, host_buf, components.nPort, 0);
	if (!connect) { WinHttpCloseHandle(session); return {0, ""}; }

	DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS)
				  ? WINHTTP_FLAG_SECURE : 0;

	HINTERNET request = WinHttpOpenRequest(
		connect, L"GET", path_buf,
		nullptr, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!request) {
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return {0, ""};
	}

	// Set a 10 second connect + receive timeout
	DWORD timeout_ms = 10000;
	WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT,    &timeout_ms, sizeof(timeout_ms));
	WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT,    &timeout_ms, sizeof(timeout_ms));

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
							WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
		|| !WinHttpReceiveResponse(request, nullptr)) {
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return {0, ""};
	}

	// Read status code
	DWORD status_code = 0;
	DWORD status_size = sizeof(status_code);
	WinHttpQueryHeaders(request,
		WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

	// Read body
	std::string body;
	DWORD bytes_available = 0;
	while (WinHttpQueryDataAvailable(request, &bytes_available) && bytes_available > 0) {
		std::string chunk(bytes_available, '\0');
		DWORD bytes_read = 0;
		if (!WinHttpReadData(request, &chunk[0], bytes_available, &bytes_read)) break;
		body.append(chunk.data(), bytes_read);
	}

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);

	return {static_cast<int>(status_code), std::move(body)};
#else
	// Non-Windows stub. Phase 5 targeting Windows; libcurl can be added later.
	(void)url;
	return {0, ""};
#endif
}

// ============================================================================
// sha256_hex_  (simple FNV-1a 64-bit placeholder)
// A real SHA-256 implementation can be substituted; for now we use
// a deterministic 64-bit hash encoded as a 16-char hex string.
// The raw_response_hash is an audit trail, not a security primitive.
// ============================================================================

std::string PubChemFetcher::sha256_hex_(const std::string& data) {
	// FNV-1a 64-bit
	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char c : data) {
		hash ^= static_cast<uint64_t>(c);
		hash *= 1099511628211ULL;
	}
	char buf[17];
	std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
	return buf;
}

// ============================================================================
// parse_response_
// Minimal JSON string search — no full JSON parser dependency.
// Extracts the first numeric value for the requested PUG property.
// ============================================================================

static std::string find_json_value(const std::string& body, const std::string& key) {
	// Find "key": value  (handles both string and numeric)
	std::string search = "\"" + key + "\":";
	auto pos = body.find(search);
	if (pos == std::string::npos) return "";

	auto start = pos + search.size();
	// Skip whitespace
	while (start < body.size() && (body[start] == ' ' || body[start] == '\t')) ++start;
	if (start >= body.size()) return "";

	if (body[start] == '"') {
		// String value
		auto end = body.find('"', start + 1);
		if (end == std::string::npos) return "";
		return body.substr(start + 1, end - start - 1);
	} else {
		// Numeric value
		auto end = start;
		while (end < body.size() && (std::isdigit(body[end]) || body[end] == '.' || body[end] == '-'))
			++end;
		return body.substr(start, end - start);
	}
}

WebResponse PubChemFetcher::parse_response_(const WebQuery& query,
											int status_code,
											const std::string& body) {
	WebResponse resp;
	resp.source        = query.source;
	resp.formula       = query.formula;
	resp.property_name = query.property_name;
	resp.status_code   = status_code;

	if (status_code == 404 || body.empty()) {
		resp.found = false;
		return resp;
	}

	if (status_code != 200) {
		resp.found = false;
		resp.error_message = "HTTP " + std::to_string(status_code);
		return resp;
	}

	// Map UFX property_name -> PUG REST token
	std::string pug = "MolecularWeight";
	for (const auto& row : k_property_map) {
		if (query.property_name == row.ufx_name) {
			pug = row.pug_name;
			break;
		}
	}

	std::string val = find_json_value(body, pug);
	if (val.empty()) {
		resp.found = false;
		return resp;
	}

	resp.found      = true;
	resp.value_text = val;

	// Attempt numeric parse
	try {
		resp.numeric_value = std::stod(val);
	} catch (...) {
		// String-only property (e.g. InChIKey)
	}

	return resp;
}

// ============================================================================
// fetch
// ============================================================================

WebResponse PubChemFetcher::fetch(const WebQuery& query) {
	rate_limiter_.wait_turn();

	const std::string url = build_url_(query.formula, query.property_name);

	auto [status, body] = http_get_(url);

	WebResponse resp = parse_response_(query, status, body);
	resp.raw_response_hash = sha256_hex_(body);

	// URL hash (used for deduplication audit)
	resp.source_url_hash = sha256_hex_(url);

	return resp;
}

} // namespace vsepr::ufx
