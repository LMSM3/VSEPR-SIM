// src/ufx_auto2/web_cache.cpp
// UFX_AUTO_2 Phase 5 -- Web Cache Implementation
// VSEPR-SIM v5 beta8 -> beta9 target

#include "ufx_auto2/web_cache.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <filesystem>

namespace vsepr::ufx {

namespace fs = std::filesystem;

// ============================================================================
// Constructor
// ============================================================================

WebCache::WebCache(const std::string& cache_dir)
	: cache_dir_(cache_dir)
{
	fs::create_directories(cache_dir_);
	fs::create_directories(cache_dir_ + "/pubchem");
	fs::create_directories(cache_dir_ + "/nist");
	fs::create_directories(cache_dir_ + "/local_reference");
}

// ============================================================================
// make_key
// key = hex of FNV-1a 64-bit over "source|formula|property_name"
// ============================================================================

std::string WebCache::make_key(const WebQuery& query) {
	std::string seed = std::string(to_string(query.source))
					 + "|" + query.formula
					 + "|" + query.property_name;

	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char c : seed) {
		hash ^= static_cast<uint64_t>(c);
		hash *= 1099511628211ULL;
	}
	char buf[17];
	std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
	return buf;
}

// ============================================================================
// cache_path
// ============================================================================

std::string WebCache::cache_path(const WebQuery& query) const {
	const char* ext = (query.source == WebSource::PubChem)       ? ".json"
					: (query.source == WebSource::NIST)           ? ".html"
					: ".txt";
	return cache_dir_ + "/" + to_string(query.source) + "/"
		   + make_key(query) + ext;
}

// ============================================================================
// has
// ============================================================================

bool WebCache::has(const WebQuery& query) const {
	return fs::exists(cache_path(query));
}

// ============================================================================
// load_raw
// ============================================================================

std::optional<std::string> WebCache::load_raw(const WebQuery& query) const {
	const std::string path = cache_path(query);
	if (!fs::exists(path)) return std::nullopt;

	std::ifstream f(path, std::ios::binary);
	if (!f) return std::nullopt;

	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// ============================================================================
// store
// ============================================================================

bool WebCache::store(const WebQuery& query,
					 const std::string& raw_body,
					 const std::string& raw_hash,
					 int status_code) {
	const std::string path = cache_path(query);

	// Write raw response file
	{
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f) return false;
		f << raw_body;
	}

	append_index_(query, make_key(query), raw_hash, status_code);
	return true;
}

// ============================================================================
// ensure_source_dir_
// ============================================================================

void WebCache::ensure_source_dir_(WebSource source) const {
	fs::create_directories(cache_dir_ + "/" + to_string(source));
}

// ============================================================================
// append_index_
// One JSON line per entry.
// ============================================================================

void WebCache::append_index_(const WebQuery& query,
							  const std::string& key,
							  const std::string& raw_hash,
							  int status_code) const {
	std::ofstream idx(cache_dir_ + "/index.jsonl",
					  std::ios::app | std::ios::out);
	if (!idx) return;

	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	idx << "{"
		<< "\"key\":\"" << key << "\","
		<< "\"source\":\"" << to_string(query.source) << "\","
		<< "\"formula\":\"" << query.formula << "\","
		<< "\"property\":\"" << query.property_name << "\","
		<< "\"hash\":\"" << raw_hash << "\","
		<< "\"status\":" << status_code << ","
		<< "\"ts\":" << now_ms
		<< "}\n";
}

// ============================================================================
// CachingFetcher::fetch
// Check cache -> on miss delegate to inner -> store result
// ============================================================================

WebResponse CachingFetcher::fetch(const WebQuery& query) {
	// Cache hit
	auto cached = cache_.load_raw(query);
	if (cached.has_value()) {
		// Reconstruct a minimal WebResponse from cached file.
		// We delegate to the inner fetcher's parse logic by calling fetch with
		// a synthetic response — instead, we build a minimal "cache hit" response.
		WebResponse resp;
		resp.source            = query.source;
		resp.formula           = query.formula;
		resp.property_name     = query.property_name;
		resp.raw_response_path = cache_.cache_path(query);
		resp.status_code       = 0;  // 0 = cache hit, no network
		resp.found             = !cached->empty() && cached->find("Not Found") == std::string::npos;
		resp.value_text        = "(cached)";
		return resp;
	}

	// Cache miss — delegate to inner fetcher
	WebResponse resp = inner_.fetch(query);

	// Store result regardless of found/not-found (prevents repeat queries)
	cache_.store(query, resp.value_text, resp.raw_response_hash, resp.status_code);
	resp.raw_response_path = cache_.cache_path(query);

	return resp;
}

} // namespace vsepr::ufx
