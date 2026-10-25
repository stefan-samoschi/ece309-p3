// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table. No std::unordered_map / std::map / std::set.
// Every function below is a stub so the starter compiles. Replace each body.

#include "tools/tool_registry.h"

#include <algorithm>
#include <cstdint>

namespace {
std::size_t next_power_of_two(std::size_t value) {
    if (value == 0) return 1;
    std::size_t n = 1;
    while (n < value) n <<= 1;
    return n;
}
}  // namespace

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}
ToolRegistry::~ToolRegistry() = default;

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> new_buckets(new_bucket_count);
    for (Bucket& bucket : buckets_) {
        for (auto& tool : bucket) {
            const std::string name = tool->name();
            const std::string_view key = name;
            const std::size_t index = ToolRegistry::hash(key) & (new_bucket_count - 1);
            new_buckets[index].push_back(std::move(tool));
        }
    }
    buckets_ = std::move(new_buckets);
}

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::size_t hash = 14695981039346656037ull;
    for (unsigned char ch : key) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;

    const std::string name = tool->name();
    const std::string_view key = name;
    const std::size_t index = hash(key) & (buckets_.size() - 1);
    for (const auto& existing : buckets_[index]) {
        if (existing->name() == key) return false;
    }

    if ((size_ + 1) > static_cast<std::size_t>(buckets_.size() * kMaxLoadFactor)) {
        const std::size_t new_bucket_count = next_power_of_two(buckets_.size() * 2);
        rehash(new_bucket_count);
    }

    const std::size_t new_index = hash(key) & (buckets_.size() - 1);
    buckets_[new_index].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) return nullptr;
    const std::size_t index = hash(name) & (buckets_.size() - 1);
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) return tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const Bucket& bucket : buckets_) {
        for (const auto& tool : bucket) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    if (buckets_.empty()) return 0.0;
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const Bucket& bucket : buckets_) {
        longest = std::max(longest, bucket.size());
    }
    return longest;
}
