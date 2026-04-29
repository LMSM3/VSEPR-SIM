#pragma once
/**
 * kernel_event_log.hpp — Thread-Safe Append-Only Kernel Event Log
 * ================================================================
 *
 * The KernelEventLog is the central registry for all KernelEvents
 * produced during a simulation run.
 *
 * Properties:
 *   - Append-only: events are never mutated after recording
 *   - Thread-safe: mutex-guarded append and iteration
 *   - Monotonic IDs: each event receives a unique uint64_t event_id
 *   - Filterable: query by KernelEventKind, frame_id range, source_formula
 *   - Exportable: JSON-lines (one event per line) and Markdown table
 *
 * Usage:
 *
 *   auto& log = vsepr::kernel::KernelEventLog::instance();
 *
 *   ReactionEvent ev("C6H12", frame_id);
 *   ev.reactants = {"A", "B"};
 *   ev.products  = {"C"};
 *   ev.reactant_energies = {-80.1, -39.7};
 *   ev.product_energies  = {-124.2};
 *   ev.compute_delta_E();
 *   log.record(ev);
 *
 * The log does not own the policy for what to log — callers decide.
 * Every module that computes a major result is expected to record it.
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include "kernel_event.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr::kernel {

// ============================================================================
// EventHandle — lightweight reference returned from record()
// ============================================================================

struct EventHandle {
	uint64_t event_id;
	KernelEventKind kind;
};

// ============================================================================
// KernelEventLog — the spine
// ============================================================================

class KernelEventLog {
public:
	// Singleton (one log per process — mirrors module_registry pattern)
	static KernelEventLog& instance() {
		static KernelEventLog log;
		return log;
	}

	// -----------------------------------------------------------------------
	// record — append a KernelEvent, assign event_id, return handle
	// -----------------------------------------------------------------------

	EventHandle record(KernelEvent ev) {
		std::lock_guard<std::mutex> lk(mutex_);
		ev.event_id = next_id_++;
		events_.push_back(ev);
		return { ev.event_id, ev.kind };
	}

	// Convenience overloads for derived types (slices to base)
	EventHandle record(const ReactionEvent&        ev) { return record(static_cast<const KernelEvent&>(ev)); }
	EventHandle record(const ChemicalStateEvent&   ev) { return record(static_cast<const KernelEvent&>(ev)); }
	EventHandle record(const FormationEvent&        ev) { return record(static_cast<const KernelEvent&>(ev)); }
	EventHandle record(const DefectEvent&           ev) { return record(static_cast<const KernelEvent&>(ev)); }
	EventHandle record(const TransportEvent&        ev) { return record(static_cast<const KernelEvent&>(ev)); }
	EventHandle record(const ContinualReportEvent&  ev) { return record(static_cast<const KernelEvent&>(ev)); }

	// -----------------------------------------------------------------------
	// Query
	// -----------------------------------------------------------------------

	size_t size() const {
		std::lock_guard<std::mutex> lk(mutex_);
		return events_.size();
	}

	/** Copy all events (snapshot — safe to iterate without holding lock). */
	std::vector<KernelEvent> snapshot() const {
		std::lock_guard<std::mutex> lk(mutex_);
		return events_;
	}

	/** Filter by event kind. */
	std::vector<KernelEvent> filter_by_kind(KernelEventKind k) const {
		std::lock_guard<std::mutex> lk(mutex_);
		std::vector<KernelEvent> out;
		for (const auto& e : events_)
			if (e.kind == k) out.push_back(e);
		return out;
	}

	/** Filter by frame_id range [lo, hi]. */
	std::vector<KernelEvent> filter_by_frame(uint64_t lo, uint64_t hi) const {
		std::lock_guard<std::mutex> lk(mutex_);
		std::vector<KernelEvent> out;
		for (const auto& e : events_)
			if (e.frame_id >= lo && e.frame_id <= hi) out.push_back(e);
		return out;
	}

	/** Filter by source formula (exact match). */
	std::vector<KernelEvent> filter_by_formula(const std::string& formula) const {
		std::lock_guard<std::mutex> lk(mutex_);
		std::vector<KernelEvent> out;
		for (const auto& e : events_)
			if (e.source_formula == formula) out.push_back(e);
		return out;
	}

	/** Find event by ID. Returns nullptr if not found. */
	const KernelEvent* find(uint64_t event_id) const {
		std::lock_guard<std::mutex> lk(mutex_);
		for (const auto& e : events_)
			if (e.event_id == event_id) return &e;
		return nullptr;
	}

	// -----------------------------------------------------------------------
	// Export — JSON Lines
	// -----------------------------------------------------------------------

	/**
	 * Export all events as JSON Lines (one JSON object per line).
	 * Each line is a self-contained audit record.
	 */
	std::string to_jsonl() const {
		std::lock_guard<std::mutex> lk(mutex_);
		std::ostringstream ss;
		for (const auto& e : events_) {
			ss << "{"
			   << "\"event_id\":"   << e.event_id    << ","
			   << "\"kind\":\""     << kind_name(e.kind) << "\","
			   << "\"frame_id\":"   << e.frame_id    << ","
			   << "\"formula\":\""  << e.source_formula << "\","
			   << "\"symbolic\":\""  << escape_json(e.equation_symbolic) << "\","
			   << "\"numeric\":\""   << escape_json(e.equation_numeric)  << "\","
			   << "\"result\":"     << e.result_value  << ","
			   << "\"unit\":\""     << e.result_unit   << "\","
			   << "\"valid\":"      << (e.is_valid ? "true" : "false");
			if (!e.warning.empty())
				ss << ",\"warning\":\"" << escape_json(e.warning) << "\"";
			ss << "}\n";
		}
		return ss.str();
	}

	// -----------------------------------------------------------------------
	// Export — Markdown table
	// -----------------------------------------------------------------------

	std::string to_markdown() const {
		std::lock_guard<std::mutex> lk(mutex_);
		std::ostringstream ss;
		ss << "| ID | Kind | Frame | Formula | Result | Unit | Valid |\n"
		   << "|---|---|---|---|---|---|---|\n";
		for (const auto& e : events_) {
			ss << "| " << e.event_id
			   << " | " << kind_name(e.kind)
			   << " | " << e.frame_id
			   << " | " << e.source_formula
			   << " | " << e.result_value
			   << " | " << e.result_unit
			   << " | " << (e.is_valid ? "✓" : "✗");
			if (!e.warning.empty()) ss << " ⚠";
			ss << " |\n";
		}
		return ss.str();
	}

	// -----------------------------------------------------------------------
	// Clear (for test isolation — not for production runs)
	// -----------------------------------------------------------------------

	void clear() {
		std::lock_guard<std::mutex> lk(mutex_);
		events_.clear();
		next_id_ = 1;
	}

private:
	KernelEventLog() = default;

	static std::string escape_json(const std::string& s) {
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			if (c == '"')  { out += "\\\""; }
			else if (c == '\\') { out += "\\\\"; }
			else if (c == '\n') { out += "\\n"; }
			else                { out += c; }
		}
		return out;
	}

	mutable std::mutex         mutex_;
	std::vector<KernelEvent>   events_;
	uint64_t                   next_id_ = 1;
};

// ============================================================================
// Convenience free function — record to global log
// ============================================================================

template<typename EventT>
inline EventHandle kernel_record(EventT&& ev) {
	return KernelEventLog::instance().record(std::forward<EventT>(ev));
}

} // namespace vsepr::kernel
