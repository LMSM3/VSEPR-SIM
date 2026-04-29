// src/v4/uff/uff_table.cpp
// Formation Engine v4.1.0 -- UFF table implementation

#include "uff_table.hpp"
#include <stdexcept>

namespace vsepr::uff {

void UFFTable::insert(UFFEntry entry) {
	std::lock_guard<std::mutex> lk(mutex_);
	const std::string key = entry.atom_type;
	entries_[key] = std::move(entry);
}

std::optional<UFFEntry> UFFTable::lookup(const std::string& atom_type) const {
	std::lock_guard<std::mutex> lk(mutex_);
	auto it = entries_.find(atom_type);
	if (it == entries_.end()) return std::nullopt;
	return it->second;
}

std::optional<UFFEntry> UFFTable::lookup_by_element(const std::string& element) const {
	std::lock_guard<std::mutex> lk(mutex_);
	for (const auto& [key, entry] : entries_) {
		if (entry.element == element) return entry;
	}
	return std::nullopt;
}

bool UFFTable::contains(const std::string& atom_type) const {
	std::lock_guard<std::mutex> lk(mutex_);
	return entries_.count(atom_type) > 0;
}

std::size_t UFFTable::size() const {
	std::lock_guard<std::mutex> lk(mutex_);
	return entries_.size();
}

std::vector<UFFEntry> UFFTable::all_entries() const {
	std::lock_guard<std::mutex> lk(mutex_);
	std::vector<UFFEntry> out;
	out.reserve(entries_.size());
	for (const auto& [key, entry] : entries_) {
		out.push_back(entry);
	}
	return out;
}

bool UFFTable::erase(const std::string& atom_type) {
	std::lock_guard<std::mutex> lk(mutex_);
	return entries_.erase(atom_type) > 0;
}

void UFFTable::clear() {
	std::lock_guard<std::mutex> lk(mutex_);
	entries_.clear();
}

} // namespace vsepr::uff
