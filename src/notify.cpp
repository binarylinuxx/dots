#include "notify.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <string_view>

namespace blx::notify {
namespace {

std::string group_key(const vw::notifications::Notification& entry) {
  std::string key = entry.app_name.empty() ? entry.app_icon : entry.app_name;
  if (key.empty()) key = "system";
  std::ranges::transform(key, key.begin(), [](unsigned char c) { return std::tolower(c); });
  return key;
}

std::vector<Group> grouped(const std::vector<vw::notifications::Notification>& entries) {
  std::vector<Group> groups;
  for (const auto& entry : entries) {
    const std::string key = group_key(entry);
    auto found = std::ranges::find(groups, key, &Group::key);
    if (found == groups.end()) {
      groups.push_back({key, {}});
      found = groups.end() - 1;
    }
    found->entries.push_back(entry);
  }
  return groups;
}

// Popups are not grouped: one card per notification, keyed by its id.
std::vector<Group> separate(const std::vector<vw::notifications::Notification>& entries) {
  std::vector<Group> groups;
  groups.reserve(entries.size());
  for (const auto& entry : entries) groups.push_back({"popup." + std::to_string(entry.id), {entry}});
  return groups;
}

// Grouped once per change of the service, however many surfaces ask per frame.
struct Cached {
  std::uint64_t generation = ~0ULL;
  std::vector<Group> groups;
};

const std::vector<Group>& cached(Cached& cache, std::vector<vw::notifications::Notification> (*source)(),
                                 std::vector<Group> (*arrange)(const std::vector<vw::notifications::Notification>&)) {
  static const std::vector<Group> none;
  if (!vw::notifications::running()) return none;
  const std::uint64_t generation = vw::notifications::generation();
  if (cache.generation != generation) {
    cache.generation = generation;
    cache.groups = arrange(source());
  }
  return cache.groups;
}

}  // namespace

std::vector<std::uint32_t> Group::ids() const {
  std::vector<std::uint32_t> result;
  for (const auto& entry : entries) result.push_back(entry.id);
  return result;
}

std::vector<Group> popups() {
  static Cached cache;
  return cached(cache, vw::notifications::popups, separate);
}

std::vector<Group> history() {
  static Cached cache;
  return cached(cache, vw::notifications::history, grouped);
}

void dismiss_group(const Group& group) {
  for (const auto& entry : group.entries) vw::notifications::dismiss(entry.id);
}

void close_popups() { vw::notifications::hide_popups(); }

void clear_history() { vw::notifications::clear(); }

std::string plain(const std::string& text) {
  std::string result;
  bool tag = false;
  for (char character : text) {
    if (character == '<') tag = true;
    else if (character == '>') tag = false;
    else if (!tag) result += character == '\n' ? ' ' : character;
  }
  for (const auto& [entity, value] : {std::pair{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
                                       {"&quot;", "\""}, {"&apos;", "'"}})
    for (auto at = result.find(entity); at != std::string::npos; at = result.find(entity, at))
      result.replace(at, std::string_view(entity).size(), value);
  return result;
}

std::string age(std::int64_t timestamp) {
  const auto seconds = std::time(nullptr) - timestamp;
  if (seconds < 60) return "now";
  if (seconds < 3600) return std::format("{}m", seconds / 60);
  if (seconds < 86400) return std::format("{}h", seconds / 3600);
  return std::format("{}d", seconds / 86400);
}

}  // namespace blx::notify
