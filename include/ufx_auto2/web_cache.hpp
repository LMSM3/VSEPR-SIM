// include/ufx_auto2/web_cache.hpp
// UFX_AUTO_2 Phase 5 -- Web Response Cache
// VSEPR-SIM v5 beta8 -> beta9 target
//
// WebCache stores and retrieves raw web responses from disk.
//
// Layout:
//   <cache_dir>/pubchem/<query_hash>.json
//   <cache_dir>/nist/<query_hash>.html
//   <cache_dir>/index.jsonl
//
// Cache key = hash of (source, formula, property_name).
// A hit means: file exists on disk and index.jsonl has the entry.
//
// Hard rule: Cache first. Fetch second.
// Never repeatedly query the same source for the same property.

#pragma once

#include "ufx_auto2/web_fetcher.hpp"
#include <string>
#include <optional>

namespace vsepr::ufx {

// ============================================================================
// WebCache
// ============================================================================

class WebCache {
public:
	explicit WebCache(const std::string& cache_dir);

	// Compute the cache key for a given query.
	// key = lowercase hex of std::hash over "source|formula|property_name"
	static std::string make_key(const WebQuery& query);

	// Return the cache file path for a query (may or may not exist).
	std::string cache_path(const WebQuery& query) const;

	// Return true if a cached response file exists for this query.
	bool has(const WebQuery& query) const;

	// Load a cached response from disk. Returns std::nullopt on miss.
	std::optional<std::string> load_raw(const WebQuery& query) const;

	// Store a raw response string to disk and append to index.jsonl.
	// raw_hash should be the hex SHA-256 of raw_body (computed by caller).
	bool store(const WebQuery& query,
			   const std::string& raw_body,
			   const std::string& raw_hash,
			   int status_code);

	const std::string& cache_dir() const noexcept { return cache_dir_; }

private:
	std::string cache_dir_;

	// Ensure the source subdirectory exists.
	void ensure_source_dir_(WebSource source) const;

	// Append one JSON line to index.jsonl.
	void append_index_(const WebQuery& query,
					   const std::string& key,
					   const std::string& raw_hash,
					   int status_code) const;
};

// ============================================================================
// CachingFetcher — IWebFetcher decorator that wraps any IWebFetcher
//                  with a WebCache: check cache, on miss delegate to inner,
//                  store result, return.
// ============================================================================

class CachingFetcher : public IWebFetcher {
public:
	CachingFetcher(IWebFetcher& inner, WebCache& cache)
		: inner_(inner), cache_(cache) {}

	WebResponse fetch(const WebQuery& query) override;

private:
	IWebFetcher& inner_;
	WebCache&    cache_;
};

} // namespace vsepr::ufx
