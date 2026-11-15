#pragma once

#include <vw/notifications.hpp>

#include <cstdint>
#include <string>
#include <vector>

// Notifications grouped by app, after ~/shell's Services/Notifications.
namespace blx::notify {

struct Group {
  std::string key;  // lower-case app name
  std::vector<vw::notifications::Notification> entries;  // newest first

  std::vector<std::uint32_t> ids() const;
};

// Views of vw's notification model, newest first, rebuilt only when the
// service changes.
// What should be popping up now: one single-entry group per notification
// (popups are never grouped).
std::vector<Group> popups();
// What the notification centre keeps, grouped by app.
std::vector<Group> history();
// The user dismissed them: closed and gone from the centre.
void dismiss_group(const Group& group);
// Stop popping up (the sidebar shows them instead); they stay in the centre.
void close_popups();
void clear_history();

// Body text without markup or line breaks.
std::string plain(const std::string& text);
// "now", "5m", "2h", "3d".
std::string age(std::int64_t timestamp);

}  // namespace blx::notify
