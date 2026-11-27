// The lock screen's page: frosted wallpaper, status icons, the stacked clock,
// date and weather, and the hint at the bottom. The lock screen and the
// greeter both draw it, so the login screen is the lock screen; each adds its
// own sheet on top (a password, or a user and a session to pick).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "config.hpp"

#include <vw/vw.hpp>

namespace blx {

/// The clock and look of the page: the lock screen's settings, or the
/// login screen's.
struct LockLook {
  std::string clock_format;
  std::string date_format;
  std::string clock_style;
  int clock_size = 132;
  float tint = 0.45F;
  int blur_passes = 4;
  float blur_offset = 2;
  bool show_status = true;
  bool show_weather = true;
  std::string wallpaper;  // empty: the current wallpaper
  bool blur = true;       // off: sharp, only tinted
};

LockLook lock_look();
/// The login screen's look: its own settings, or the lock screen's.
LockLook greeter_look();
/// greeter.wallpaper with ~ expanded against $HOME; empty when unset.
std::string greeter_wallpaper_path();

/// Where an item of a custom layout landed this frame, for the editor.
struct PlacedItem {
  std::size_t index = 0;
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

/// The classic arrangement as layout items, for the editor to start from.
std::vector<LockItem> default_lock_layout(bool greeter);
/// What the editor calls an item: "Clock", "Text: Hello", "Plugin · View".
std::string lock_item_label(const LockItem& item);
/// Whether the item belongs on that screen: the login screen has no
/// session (notifications, media, plugins), the lock screen no power buttons.
bool lock_item_allowed(const std::string& id, bool greeter);
/// The layout the lock screen, or the login screen, draws; null for the
/// classic arrangement.
const std::vector<LockItem>* lock_layout_for(bool greeter);

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
  /// How much of the bottom the raised sheet covers: the clock moves above it.
  float sheet_room = 0;
  /// Unset: the lock screen's settings.
  std::optional<LockLook> look;
  /// A custom layout; null draws the classic arrangement.
  const std::vector<LockItem>* layout = nullptr;
  /// In the layout editor: placeholders for what is empty now, nothing
  /// clickable.
  bool editing = false;
  bool greeter = false;
  /// The "user" item: picture and name.
  std::string user_name;
  std::string user_picture;
  std::string avatar_shape = "cookie9sided";
  /// Filled with each drawn item's box, when given.
  std::vector<PlacedItem>* placed = nullptr;
};

/// Height the lock screen's sheet takes from the bottom edge when raised.
inline constexpr float lock_sheet_height = 380;

/// Where a sheet card sits: the floating card both sheets are drawn in.
struct SheetCard {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  float centre = 0;
};

/// The floating card, risen from below the bottom edge (`reveal` 0) to just
/// above it (1), with its shadow and grab handle.
SheetCard draw_sheet_card(vw::Ui& ui, float width, float height, float card_height, float reveal);

/// A user's picture in a cookie shape; a person glyph without one.
void draw_avatar(vw::Ui& ui, const std::string& id, const std::string& picture, float centre_x, float y, float size,
                 const std::string& shape = "cookie9sided");

void draw_lock_page(vw::Ui& ui, const LockPage& page);

/// The lock screen as it looks while locked, without locking anything: for
/// blxshell-preview. `sheet` draws it with the password sheet raised.
void draw_lock_preview(vw::Ui& ui, float width, float height, bool sheet);

}  // namespace blx
