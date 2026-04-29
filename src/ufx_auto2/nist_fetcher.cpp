// src/ufx_auto2/nist_fetcher.cpp
// UFX_AUTO_2 Phase 5 -- NIST Chemistry WebBook Fetcher Implementation
// VSEPR-SIM v5 beta8 -> beta9 target

#include "ufx_auto2/nist_fetcher.hpp"

#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif

namespace vsepr::ufx {

// ============================================================================
// Constructor
// ============================================================================

NISTFetcher::NISTFetcher(double requests_per_second)
	: rate_limiter_(requests_per_second)
{}

// ============================================================================
// build_url_
// NIST WebBook formula search: https://webbook.nist.gov/cgi/cbook.cgi?Formula=<formula>&NoIon=on&Units=SI
// For species existence checks, the same URL is used; a 200 response with
// recognisable compound title indicates the species is known.
// ============================================================================

std::string NISTFetcher::build_url_(const std::string& formula,
									const std::string& /*property_name*/) {
	return "https://webbook.nist.gov/cgi/cbook.cgi?Formula="
		   + formula + "&NoIon=on&Units=SI";
}

// ============================================================================
// http_get_  (WinHTTP implementation — identical structure to PubChemFetcher)
// ============================================================================

std::pair<int, std::string> NISTFetcher::http_get_(const std::string& url) {
#if defined(_WIN32)
	URL_COMPONENTS components = {};
	components.dwStructSize = sizeof(components);

	wchar_t host_buf[512]  = {};
	wchar_t path_buf[2048] = {};
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

	HINTERNET connect = WinHttpConnect(session, host_buf, components.nPort, 0);
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

	DWORD timeout_ms = 12000;
	WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
	WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
							WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
		|| !WinHttpReceiveResponse(request, nullptr)) {
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return {0, ""};
	}

	DWORD status_code = 0;
	DWORD status_size = sizeof(status_code);
	WinHttpQueryHeaders(request,
		WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

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
	(void)url;
	return {0, ""};
#endif
}

// ============================================================================
// sha256_hex_  (FNV-1a 64-bit placeholder — same as PubChemFetcher)
// ============================================================================

std::string NISTFetcher::sha256_hex_(const std::string& data) {
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
// Minimal HTML string-search parser.
//
// Species existence: look for the formula/name in the page title or heading.
// If the response redirects to a "Not Found" page, found=false.
// ============================================================================

WebResponse NISTFetcher::parse_response_(const WebQuery& query,
										 int status_code,
										 const std::string& body) {
	WebResponse resp;
	resp.source        = query.source;
	resp.formula       = query.formula;
	resp.property_name = query.property_name;
	resp.status_code   = status_code;

	if (status_code != 200 || body.empty()) {
		resp.found = false;
		if (status_code != 200 && status_code != 0)
			resp.error_message = "HTTP " + std::to_string(status_code);
		return resp;
	}

	// NIST returns "Not Found" page when species is unknown
	if (body.find("Not Found") != std::string::npos ||
		body.find("no results") != std::string::npos) {
		resp.found = false;
		return resp;
	}

	// At minimum: species was found in the WebBook
	resp.found = true;

	if (query.property_name == "species_exists") {
		resp.value_text = "true";
		return resp;
	}

	// For numeric properties, attempt a simple scan for the value near the
	// property keyword.  This is intentionally conservative: if parsing is
	// uncertain, return found=true with no numeric_value rather than
	// fabricating a number.

	auto search_near = [&](const std::string& keyword) -> std::string {
		auto pos = body.find(keyword);
		if (pos == std::string::npos) return "";
		// Scan forward up to 120 chars for a decimal number
		auto scan_start = pos + keyword.size();
		auto scan_end   = std::min(scan_start + 120, body.size());
		std::string segment = body.substr(scan_start, scan_end - scan_start);

		std::string num;
		bool in_num = false;
		for (char c : segment) {
			if (std::isdigit(c) || c == '.' || (c == '-' && num.empty())) {
				num += c;
				in_num = true;
			} else if (in_num) {
				break;
			}
		}
		return num;
	};

	std::string num_str;

	if (query.property_name == "melting_point_K" ||
		query.property_name == "melting_point") {
		num_str = search_near("Melting point");
		if (num_str.empty()) num_str = search_near("Fus");
	} else if (query.property_name == "boiling_point_K" ||
			   query.property_name == "boiling_point") {
		num_str = search_near("Boiling point");
		num_str = search_near("Vap");
	} else if (query.property_name == "delta_Hf_298_kJ_mol") {
		num_str = search_near("f,298");
		if (num_str.empty()) num_str = search_near("Hf");
	}

	if (!num_str.empty()) {
		try {
			resp.numeric_value = std::stod(num_str);
			resp.value_text    = num_str;
		} catch (...) {
			// Parsing failed; leave numeric_value empty
		}
	}

	return resp;
}

// ============================================================================
// fetch
// ============================================================================

WebResponse NISTFetcher::fetch(const WebQuery& query) {
	rate_limiter_.wait_turn();

	const std::string url = build_url_(query.formula, query.property_name);

	auto [status, body] = http_get_(url);

	WebResponse resp = parse_response_(query, status, body);
	resp.raw_response_hash = sha256_hex_(body);
	resp.source_url_hash   = sha256_hex_(url);

	return resp;
}

} // namespace vsepr::ufx
