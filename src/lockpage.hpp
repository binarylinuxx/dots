// The lock screen's page: frosted wallpaper, status icons, the stacked clock,
// date and weather, and the hint at the bottom. The lock screen and the
// greeter both draw it, so the login screen is the lock screen; each adds its
// own sheet on top (a password, or a user and a session to pick).
#pragma once

#include <functional>
#include <string>

#include <vw/vw.hpp>

namespace blx {

struct LockPage {
  float width = 0;
  float height = 0;
  /// The sheet: 0 below the bottom edge, 1 fully raised.
  float reveal = 0;
  /// The entrance: 0 solid surface, 1 the page fully in.
  float intro = 1;
  std::string hint;
  std::string hint_glyph;
  /// Notifications and media: a locked session has them, a greeter does not.
  bool session_widgets = false;
  /// A click anywhere on the page, usually to raise the sheet.
  std::function<void()> on_click;
};

/// Height the sheet takes from the bottom edge when raised.
inline constexpr float lock_sheet_height = 312;

void draw_lock_page(vw::Ui& ui, const LockPage& page);

}  // namespace blx
