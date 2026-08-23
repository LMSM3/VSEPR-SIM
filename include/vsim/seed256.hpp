#pragma once
/**
 * seed256.hpp
 * ===========
 * WO-66K-AUDIT  |  256-bit Seed Type
 *
 * Provides Seed256 — a 256-bit seed value stored as four uint64_t words
 * (little-endian: w[0] is the least-significant 64 bits).
 *
 * Supported parse formats:
 *   Integer (u64)  — existing single uint64_t; placed in w[0], upper words = 0
 *   Hex string     — 1..64 hex chars, e.g. "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
 *   Base64 string  — 44-char padded base64, e.g. "//////////////////////////////////////////8="
 *   Binary string  — 1..256 consecutive '0'/'1' chars, e.g. "111...111"
 *
 * Canonical max value:
 *   Binary : 256 × '1'
 *   Hex    : 64 × 'f'
 *   Base64 : "//////////////////////////////////////////8="
 *   All three encode 2^256 − 1.
 *
 * Dual-seed contract (Kernel Audit):
 *   A particle's birth hash is computed in two passes:
 *     Pass 1 (run_seed)   — particle-instance context (type, index, position)
 *     Pass 2 (world_seed) — world-level context (simulation-wide constant)
 *   If world_seed is zero (not set), only pass 1 fires.
 *   If world_seed is nonzero, pass 2 mixes the world constant into the hash.
 *   This enables deterministic uncertainty resolution for atomic bonding
 *   process evolution equations that require a shared environmental constant.
 *
 * v5.1.4  |  WO-66K-AUDIT  |  v5.0.0-main
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <array>
#include <stdexcept>
#include <algorithm>

namespace vsim {

// ============================================================================
// Seed256 — 256-bit seed value
// ============================================================================

struct Seed256 {
	// w[0] = bits  0.. 63  (least significant)
	// w[1] = bits 64..127
	// w[2] = bits 128..191
	// w[3] = bits 192..255  (most significant)
	uint64_t w[4] = {0, 0, 0, 0};

	// Construct from a simple uint64 (most common case — existing scripts)
	Seed256() noexcept = default;
	explicit Seed256(uint64_t v) noexcept : w{v, 0, 0, 0} {}

	// True if any word is nonzero
	bool is_set() const noexcept {
		return (w[0] | w[1] | w[2] | w[3]) != 0;
	}

	// Equality
	bool operator==(const Seed256& o) const noexcept {
		return w[0]==o.w[0] && w[1]==o.w[1] && w[2]==o.w[2] && w[3]==o.w[3];
	}
	bool operator!=(const Seed256& o) const noexcept { return !(*this == o); }

	// Low 64 bits (used when a subsystem only needs a uint64)
	uint64_t low64() const noexcept { return w[0]; }

	// Hex string output (64 lower-case hex chars, big-endian display order)
	std::string to_hex() const {
		static const char* hex = "0123456789abcdef";
		std::string s(64, '0');
		for (int word = 3; word >= 0; --word) {
			int offset = (3 - word) * 16;
			uint64_t v = w[word];
			for (int i = 15; i >= 0; --i) {
				s[offset + i] = hex[v & 0xF];
				v >>= 4;
			}
		}
		return s;
	}
};

// ============================================================================
// parse_seed256()
//
// Accepts:
//   - Pure decimal integer string (all digits, ≤ 20 chars): uint64 in w[0]
//   - Hex string prefixed with "0x" or "0X", or exactly 1..64 hex chars:
//       right-fills into w[3..0] (most-significant word first in string)
//   - Base64 (44 chars ending in '='): decodes 33 bytes → 264 bits,
//       truncated to 256 bits (w[3..0])
//   - Binary string of 1..256 '0'/'1' characters: MSB first
//
// On empty input returns Seed256{} (zero).
// On parse error throws std::invalid_argument.
// ============================================================================

namespace detail {

inline bool all_digits(const std::string& s) {
	for (char c : s) if (c < '0' || c > '9') return false;
	return !s.empty();
}

inline bool all_hex(const std::string& s) {
	for (char c : s) {
		if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))
			return false;
	}
	return !s.empty();
}

inline bool all_binary(const std::string& s) {
	for (char c : s) if (c != '0' && c != '1') return false;
	return !s.empty();
}

inline Seed256 from_hex_str(const std::string& hex) {
	// hex is already stripped of "0x" prefix, length 1..64
	if (hex.size() > 64)
		throw std::invalid_argument("Seed256: hex string too long (max 64 chars)");
	// Pad to 64 chars on the left with '0'
	std::string padded(64 - hex.size(), '0');
	padded += hex;
	// Convert to lowercase
	for (char& c : padded)
		if (c >= 'A' && c <= 'F') c = static_cast<char>(c + 32);

	auto nibble = [](char c) -> uint64_t {
		if (c >= '0' && c <= '9') return static_cast<uint64_t>(c - '0');
		return static_cast<uint64_t>(c - 'a' + 10);
	};

	Seed256 r;
	// padded[0..15] → w[3], padded[16..31] → w[2], etc.
	for (int word = 0; word < 4; ++word) {
		int offset = (3 - word) * 16;
		uint64_t v = 0;
		for (int i = 0; i < 16; ++i) {
			v = (v << 4) | nibble(padded[offset + i]);
		}
		r.w[word] = v;
	}
	return r;
}

inline Seed256 from_base64_str(const std::string& b64) {
	// Standard base64 alphabet
	static const std::string alpha =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	auto val = [&](char c) -> int {
		if (c == '=') return 0;
		auto pos = alpha.find(c);
		if (pos == std::string::npos)
			throw std::invalid_argument("Seed256: invalid base64 character");
		return static_cast<int>(pos);
	};

	// Decode to bytes (up to 33 bytes for 44-char base64; we only need 32)
	std::array<uint8_t, 33> bytes{};
	std::size_t b_idx = 0;
	for (std::size_t i = 0; i + 3 < b64.size() && b_idx < 33; i += 4) {
		int v0 = val(b64[i]);
		int v1 = val(b64[i+1]);
		int v2 = val(b64[i+2]);
		int v3 = val(b64[i+3]);
		if (b_idx < 33) bytes[b_idx++] = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
		if (b_idx < 33) bytes[b_idx++] = static_cast<uint8_t>((v1 << 4) | (v2 >> 2));
		if (b_idx < 33) bytes[b_idx++] = static_cast<uint8_t>((v2 << 6) | v3);
	}

	// Map first 32 bytes into Seed256 (big-endian byte order: byte 0 → MSByte of w[3])
	Seed256 r;
	for (int word = 3; word >= 0; --word) {
		int base = (3 - word) * 8;
		uint64_t v = 0;
		for (int i = 0; i < 8; ++i)
			v = (v << 8) | bytes[base + i];
		r.w[word] = v;
	}
	return r;
}

inline Seed256 from_binary_str(const std::string& bin) {
	if (bin.size() > 256)
		throw std::invalid_argument("Seed256: binary string too long (max 256 chars)");
	Seed256 r;
	// MSB first: bin[0] is bit 255
	for (std::size_t i = 0; i < bin.size(); ++i) {
		if (bin[i] == '1') {
					// MSB first means bin[0] is the highest bit in the string's range
					int bit = static_cast<int>(bin.size() - 1 - i);
			int word = bit / 64;
			int off  = bit % 64;
			r.w[word] |= (uint64_t{1} << off);
		}
	}
	return r;
}

} // namespace detail

inline Seed256 parse_seed256(const std::string& s) {
	if (s.empty()) return Seed256{};

	// 1. Decimal integer (≤ 20 chars, all digits)
	if (detail::all_digits(s) && s.size() <= 20) {
		uint64_t v = std::stoull(s);
		return Seed256{v};
	}

	// 2. Hex with 0x/0X prefix
	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		std::string hex = s.substr(2);
		if (hex.empty() || hex.size() > 64)
			throw std::invalid_argument("Seed256: invalid hex string length");
		return detail::from_hex_str(hex);
	}

	// 3. Pure hex string (1..64 hex chars, no prefix)
	if (s.size() <= 64 && detail::all_hex(s)) {
		return detail::from_hex_str(s);
	}

	// 4. Base64 (44 chars ending in '=', or length % 4 == 0)
	if (!s.empty() && s.back() == '=') {
		return detail::from_base64_str(s);
	}

	// 5. Binary string (1..256 chars of '0' and '1')
	if (s.size() <= 256 && detail::all_binary(s)) {
		return detail::from_binary_str(s);
	}

	throw std::invalid_argument("Seed256: cannot parse seed value: " + s);
}

// Convenience: parse from a uint64 literal (no conversion needed)
inline Seed256 parse_seed256(uint64_t v) {
	return Seed256{v};
}

// ============================================================================
// Canonical max-value constants
// ============================================================================

// All 256 bits set (2^256 − 1)
inline Seed256 seed256_max() noexcept {
	Seed256 r;
	r.w[0] = r.w[1] = r.w[2] = r.w[3] = UINT64_MAX;
	return r;
}

// ============================================================================
// FNV-1a 256-bit seed mixer
//
// Mixes a Seed256 into an existing FNV-1a 64-bit hash state.
// Used for the second pass (world_seed) in dual-seed birth-hash computation.
// ============================================================================

inline void fnv1a_mix_seed256(uint64_t& h, const Seed256& s) noexcept {
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;
	const uint8_t* p = reinterpret_cast<const uint8_t*>(s.w);
	for (std::size_t i = 0; i < sizeof(s.w); ++i) {
		h ^= p[i];
		h *= FNV_PRIME;
	}
}

} // namespace vsim
