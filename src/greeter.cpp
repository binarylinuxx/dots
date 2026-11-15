// blxshell-greeter: a greetd login screen in the lock screen's style — the
// same stacked clock and date over the frosted wallpaper, and the same sheet
// rising from the bottom edge, with a user and a session to pick.
//
//   blxshell-greeter                   run under greetd (see README)
//   blxshell-greeter --layer           as layer surfaces, one per monitor, for
//                                      compositors with layer shell
//   blxshell-greeter --export DIR [--from HOME] [--user NAME]
//                                      copy your theme, wallpaper and avatar
//                                      to DIR, where the greeter reads them
//
// greetd runs the greeter as its own user, which cannot read your home. It
// reads the theme from $BLXSHELL_GREETER_DIR, else /var/lib/blxshell-greeter:
// a folder laid out like a home directory (.config/blxshell/config.json,
// .local/state/blxshell/{matugen-colours.json,wallpaper}, avatars/<user>).
#include <vw/vw.hpp>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "blob.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "lockpage.hpp"
#include "shapes.hpp"
#include "style.hpp"
#include "surfaces.hpp"
#include "system.hpp"

namespace fs = std::filesystem;

namespace blx::greeter {
namespace {

using Clock = std::chrono::steady_clock;
namespace greetd = vw::greetd;

constexpr const char* field_id = "greeter-answer";
constexpr float sheet_height = 380;
constexpr float sheet_width = 560;

struct Greeter {
  std::vector<greetd::User> users;
  std::size_t user = 0;
  std::vector<greetd::Session> sessions;
  std::size_t session = 0;

  bool sheet = false;
  Clock::time_point opened_at{};
  Clock::time_point last_input{};
  Clock::time_point failed_at{};
  bool denied = false;    // the last attempt was a wrong password
  std::string notice;     // what greetd said, or why it failed
  bool notice_is_error = false;
  std::string typed;      // the field's text, for the arrow button

  fs::path theme;         // where the theme was read from
  fs::path memory;        // the last user and session, if writable
};

Greeter& greeter() {
  static Greeter state;
  return state;
}

// --- choices --------------------------------------------------------------

const greetd::User* chosen_user() {
  const Greeter& g = greeter();
  return g.users.empty() ? nullptr : &g.users[g.user % g.users.size()];
}

const greetd::Session* chosen_session() {
  const Greeter& g = greeter();
  return g.sessions.empty() ? nullptr : &g.sessions[g.session % g.sessions.size()];
}

std::string display_name(const greetd::User& user) {
  return user.full_name.empty() ? user.name : user.full_name;
}

void remember() {
  const Greeter& g = greeter();
  if (g.memory.empty()) return;
  std::error_code error;
  fs::create_directories(g.memory.parent_path(), error);
  std::ofstream output(g.memory);
  if (const auto* user = chosen_user()) output << user->name << '\n';
  if (const auto* session = chosen_session()) output << session->name << '\n';
}

void recall() {
  Greeter& g = greeter();
  std::ifstream input(g.memory);
  std::string user;
  std::string session;
  std::getline(input, user);
  std::getline(input, session);
  for (std::size_t index = 0; index < g.users.size(); ++index)
    if (g.users[index].name == user) g.user = index;
  for (std::size_t index = 0; index < g.sessions.size(); ++index)
    if (g.sessions[index].name == session) g.session = index;
}

// --- the login, driven by greetd's replies ----------------------------------

void begin_login() {
  Greeter& g = greeter();
  vw::reset_text_field(field_id);
  g.typed.clear();
  if (!greetd::available()) {
    g.notice = "greetd is not running — this is a preview";
    g.notice_is_error = false;
    return;
  }
  if (const auto* user = chosen_user()) {
    try {
      greetd::begin(user->name);
    } catch (const std::exception& error) {
      g.notice = error.what();
      g.notice_is_error = true;
    }
  }
}

void open_sheet() {
  Greeter& g = greeter();
  if (!g.sheet) {
    g.opened_at = Clock::now();
    g.notice.clear();
    g.denied = false;
    begin_login();
  }
  g.sheet = true;
  g.last_input = Clock::now();
}

void close_sheet() {
  Greeter& g = greeter();
  g.sheet = false;
  vw::reset_text_field(field_id);
  if (greetd::available() && greetd::state().stage != greetd::Stage::idle) {
    try {
      greetd::cancel();
    } catch (const std::exception&) {}
  }
}

void pick_user(int step) {
  Greeter& g = greeter();
  if (g.users.size() < 2) return;
  g.user = (g.user + (step > 0 ? 1 : g.users.size() - 1)) % g.users.size();
  g.notice.clear();
  g.denied = false;
  if (g.sheet) begin_login();  // begin() cancels the login already under way
}

void pick_session(int step) {
  Greeter& g = greeter();
  if (g.sessions.size() < 2) return;
  g.session = (g.session + (step > 0 ? 1 : g.sessions.size() - 1)) % g.sessions.size();
}

// After every greetd reply: answer what needs no input, start the session
// once authenticated, and start over after a failure.
bool react() {
  if (!greetd::drain_events()) return false;
  Greeter& g = greeter();
  const greetd::State state = greetd::state();
  switch (state.stage) {
    case greetd::Stage::prompting:
      if (state.prompt == greetd::Prompt::info || state.prompt == greetd::Prompt::error) {
        g.notice = state.message;
        g.notice_is_error = state.prompt == greetd::Prompt::error;
        greetd::answer();
      }
      break;
    case greetd::Stage::authenticated:
      if (const auto* session = chosen_session()) {
        remember();
        std::vector<std::string> environment{
            std::format("XDG_SESSION_TYPE={}", session->wayland ? "wayland" : "x11"),
            std::format("XDG_SESSION_DESKTOP={}", fs::path(session->file).stem().string()),
            std::format("XDG_CURRENT_DESKTOP={}", session->name)};
        greetd::start(session->command, environment);
      } else {
        g.notice = "No session to start: install one into wayland-sessions";
        g.notice_is_error = true;
        greetd::cancel();
      }
      break;
    case greetd::Stage::started:
      // greetd takes it from here.
      std::exit(EXIT_SUCCESS);
    case greetd::Stage::failed:
      g.denied = state.denied;
      g.failed_at = Clock::now();
      g.notice = state.denied ? "Wrong password" : state.message;
      g.notice_is_error = true;
      if (g.sheet) begin_login();  // ready for the next attempt
      break;
    case greetd::Stage::idle:
    case greetd::Stage::waiting:
      break;
  }
  return true;
}

void submit(const std::string& text) {
  Greeter& g = greeter();
  g.last_input = Clock::now();
  if (!greetd::available()) return;
  const greetd::State state = greetd::state();
  if (state.stage != greetd::Stage::prompting) return;
  g.notice.clear();
  greetd::answer(text);
  vw::reset_text_field(field_id);
  g.typed.clear();
}

// --- drawing ----------------------------------------------------------------


float chip_width(const std::string& glyph, const std::string& label) {
  return draw::measure(label, 15, vw::weight::medium, font::utility) + 32 + (glyph.empty() ? 0 : 26);
}

void chip(vw::Ui& ui, float x, float y, const std::string& glyph, const std::string& label,
          const std::function<void()>& on_click = {}) {
  const Colours& c = colours();
  constexpr float height = 40;
  const float width = chip_width(glyph, label);
  draw::rect(ui, x, y, width, height, height / 2, with_alpha(c.surface_container_high, 0.9F));
  float text_x = x + 16;
  if (!glyph.empty()) {
    draw::glyph(ui, glyph, text_x + 10, y + height / 2, 20, c.primary);
    text_x += 26;
  }
  draw::text_middle(ui, label, text_x, y + height / 2, {.size = 15, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
  if (on_click)
    draw::state_layer(ui, x, y, width, height, height / 2, c.text,
                      {.on_click = [on_click](const vw::ClickEvent&) { on_click(); }});
}

// Restart and power off, top left: a greeter is where you turn the machine off.
void draw_power(vw::Ui& ui) {
  float x = 24;
  const float y = 20;
  chip(ui, x, y, "power_settings_new", "Power off", [] { vw::spawn({"systemctl", "poweroff"}); });
  x += chip_width("power_settings_new", "Power off") + 8;
  chip(ui, x, y, "restart_alt", "Restart", [] { vw::spawn({"systemctl", "reboot"}); });
}

// The user's picture: exported with the theme, else AccountsService, else
// their ~/.face when the greeter may read it.
std::string avatar_for(const greetd::User& user) {
  const Greeter& g = greeter();
  for (const fs::path& path : {g.theme / "avatars" / user.name, fs::path("/var/lib/AccountsService/icons") / user.name,
                               fs::path(user.home) / ".face"}) {
    std::error_code error;
    if (fs::is_regular_file(path, error) && access(path.c_str(), R_OK) == 0) return path.string();
  }
  return {};
}

void arrow(vw::Ui& ui, float centre_x, float centre_y, const char* glyph, int step, bool enabled) {
  const Colours& c = colours();
  constexpr float size = 40;
  draw::glyph(ui, glyph, centre_x, centre_y, 26, enabled ? c.text : c.outline_variant);
  if (enabled)
    draw::state_layer(ui, centre_x - size / 2, centre_y - size / 2, size, size, size / 2, c.text,
                      {.on_click = [step](const vw::ClickEvent&) { pick_user(step); }});
}

// The sheet, risen from the bottom edge; `reveal` 0 is below it.
void draw_sheet(vw::Ui& ui, float width, float height, float reveal, float shake) {
  const Colours& c = colours();
  const Greeter& g = greeter();
  const float sheet = std::min(sheet_width, width - 32);
  const float x = (width - sheet) / 2;
  const float y = height - sheet_height * reveal;
  ui.rectangle({.width = sheet, .height = sheet_height + 40, .color = c.surface_container,
                .corner_radii = {40, 40, 0, 0}, .place = {.x = x, .y = y}});
  const float centre = width / 2;
  draw::rect(ui, centre - 16, y + 12, 32, 4, 2, c.outline_variant);

  // Who: the avatar between arrows when there is more than one user.
  float cursor = y + 32;
  constexpr float badge = 88;
  const greetd::User* user = chosen_user();
  shapes::draw(ui, "greeter.avatar", "cookie9sided", centre - badge / 2, cursor, badge, c.primary_container, 0, c.primary_container);
  const std::string picture = user != nullptr ? avatar_for(*user) : std::string();
  if (picture.empty() || !draw::icon(ui, picture, "", centre - badge / 2, cursor, badge, std::nullopt, 0,
                                     vw::ImageFit::cover, {}, shapes::mask("cookie9sided")))
    draw::glyph(ui, "person", centre, cursor + badge / 2, 40, c.primary);
  const bool several = g.users.size() > 1;
  arrow(ui, centre - badge / 2 - 36, cursor + badge / 2, "chevron_left", -1, several);
  arrow(ui, centre + badge / 2 + 36, cursor + badge / 2, "chevron_right", 1, several);
  cursor += badge + 10;
  draw::text_centered(ui, user != nullptr ? display_name(*user) : "No users", centre, cursor + 12,
                      {.size = 18, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
  cursor += 34;

  // What greetd asks: usually the password.
  const greetd::State state = greetd::available() ? greetd::state() : greetd::State{};
  const bool asking = state.stage == greetd::Stage::prompting &&
                      (state.prompt == greetd::Prompt::secret || state.prompt == greetd::Prompt::visible);
  const bool secret = !asking || state.prompt == greetd::Prompt::secret;
  std::string placeholder = asking ? state.message : state.stage == greetd::Stage::waiting ? "Checking…" : "Password";
  while (!placeholder.empty() && (placeholder.back() == ' ' || placeholder.back() == ':')) placeholder.pop_back();

  const bool failed = g.denied && g.notice_is_error;
  constexpr float field_height = 60;
  const float field_width = std::min(460.0F, sheet - 48);
  const float field_x = centre - field_width / 2 + shake;
  ui.rectangle({.width = field_width, .height = field_height, .radius = field_height / 2, .color = c.surface_container_highest,
                .border_width = failed ? 2.0F : 0.0F, .border_color = c.error, .place = {.x = field_x, .y = cursor}});
  draw::glyph(ui, secret ? "lock" : "person", field_x + 32, cursor + field_height / 2, 22, failed ? c.error : c.primary);
  ui.text_field({.id = field_id, .placeholder = placeholder, .width = field_width - 128, .height = field_height, .radius = 0,
                 .border_width = 0, .padding = 4, .color = with_alpha(c.surface_container_highest, 0),
                 .focus_color = with_alpha(c.primary, 0), .text_color = c.text, .placeholder_color = c.text_muted,
                 .caret_color = c.primary, .font = font::utility, .size = 17, .password = secret, .autofocus = true,
                 .on_change = [](const std::string& text) { greeter().typed = text; },
                 .on_submit = [](const std::string& text) { submit(text); },
                 .place = {.x = field_x + 58, .y = cursor}});
  constexpr float go = 44;
  const float go_x = field_x + field_width - go - 8;
  draw::rect(ui, go_x, cursor + (field_height - go) / 2, go, go, go / 2, c.primary);
  draw::glyph(ui, "arrow_forward", go_x + go / 2, cursor + field_height / 2, 22, c.primary_text);
  draw::state_layer(ui, go_x, cursor + (field_height - go) / 2, go, go, go / 2, c.primary_text,
                    {.on_click = [](const vw::ClickEvent&) { submit(greeter().typed); }});
  cursor += field_height + 22;

  const std::string line = !g.notice.empty() ? g.notice : "Press Enter to log in";
  draw::text_centered(ui, line, centre, cursor,
                      {.size = 13, .color = g.notice_is_error ? c.error : c.text_muted, .weight = vw::weight::medium,
                       .family = font::utility, .max_width = sheet - 48});
  cursor += 26;

  // Into what: the session, a chip that cycles through them.
  if (const auto* session = chosen_session()) {
    const std::string label = session->name;
    const float width_of = chip_width("desktop_windows", label);
    chip(ui, centre - width_of / 2, cursor, "desktop_windows", label,
         g.sessions.size() > 1 ? std::function<void()>([] { pick_session(1); }) : std::function<void()>());
  } else {
    draw::text_centered(ui, "No sessions found", centre, cursor + 20,
                        {.size = 13, .color = c.error, .weight = vw::weight::medium, .family = font::utility});
  }
}

void draw_greeter(vw::Ui& ui, float width, float height) {
  Greeter& g = greeter();

  // Idle: the sheet goes back down and takes what was typed with it.
  if (g.sheet && Clock::now() - g.last_input > std::chrono::seconds(60)) close_sheet();

  ui.keyboard(
      [](const vw::KeyEvent& key) {
        Greeter& state = greeter();
        if (key.name == "Escape") {
          close_sheet();
          return true;
        }
        // The first key only raises the sheet, as on the lock screen.
        if (!state.sheet) {
          open_sheet();
          return true;
        }
        if (key.repeat && Clock::now() - state.opened_at < std::chrono::milliseconds(800)) return true;
        state.last_input = Clock::now();
        if (key.name == "Up") { pick_user(-1); return true; }
        if (key.name == "Down") { pick_user(1); return true; }
        if (key.name == "Tab") { pick_session(key.shift ? -1 : 1); return true; }
        return false;
      },
      vw::Keyboard::exclusive);

  const float reveal = std::clamp(static_cast<float>(vw::animate("greeter.sheet", g.sheet ? 1 : 0, motion::bounce())), 0.0F, 1.0F);
  const float intro = std::clamp(static_cast<float>(vw::animate("greeter.intro", 1, {.duration = 0.9, .initial = 0, .easing = vw::Easing::out_expo})), 0.0F, 1.0F);

  // The lock screen itself: the login screen is the same page, so logging in
  // looks exactly like unlocking.
  draw_lock_page(ui, {.width = width, .height = height, .reveal = reveal, .intro = intro,
                      .hint = g.users.empty() ? "No users to log in" : "Press any key to log in",
                      .hint_glyph = "login", .session_widgets = false, .on_click = [] { open_sheet(); }});
  if (reveal < 0.99F) draw_power(ui);

  // A short decaying shake on a wrong password.
  const float since = std::chrono::duration<float>(Clock::now() - g.failed_at).count();
  float shake = 0;
  if (g.denied && since < 0.45F) {
    shake = std::sin(since * 42) * 12 * (1 - since / 0.45F);
    vw::request_animation_continuation();
  }
  draw_sheet(ui, width, height, reveal, shake);
}

// --- setup ------------------------------------------------------------------

// The theme folder, used as HOME so the shell's code finds its files there.
fs::path theme_directory() {
  if (const char* named = std::getenv("BLXSHELL_GREETER_DIR"); named != nullptr && *named != '\0') return named;
  std::error_code error;
  if (fs::is_directory("/var/lib/blxshell-greeter", error)) return "/var/lib/blxshell-greeter";
  return {};
}

void prepare() {
  Greeter& g = greeter();
  const char* home = std::getenv("HOME");
  g.theme = theme_directory();
  if (!g.theme.empty()) setenv("HOME", g.theme.c_str(), 1);
  g.memory = (g.theme.empty() ? fs::path(home != nullptr ? home : "/tmp") : g.theme) /
             ".local/state/blxshell-greeter/last";
  // greetd starts greeters with a bare environment; NixOS keeps sessions in
  // the system profile.
  if (const char* dirs = std::getenv("XDG_DATA_DIRS"); dirs == nullptr || *dirs == '\0')
    setenv("XDG_DATA_DIRS", "/run/current-system/sw/share:/usr/local/share:/usr/share", 1);

  g.users = greetd::users();
  for (auto& session : greetd::sessions())
    if (session.wayland) g.sessions.push_back(std::move(session));
  recall();
  sync_style();
}

// --- export -----------------------------------------------------------------

// The value of `key=` in an ini-style file, or empty.
std::string ini_value(const fs::path& file, std::string_view key) {
  std::ifstream input(file);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.starts_with(key) || line.size() <= key.size() || line[key.size()] != '=') continue;
    std::string value = line.substr(key.size() + 1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\r')) value.pop_back();
    return value;
  }
  return {};
}

struct Cursor {
  std::string name;
  fs::path directory;  // the icons folder holding <name>/cursors, outside the home
  int size = 24;
};

// The user's cursor as home-manager (or GTK) sets it: the theme named by
// ~/.icons/default/index.theme or GTK's settings, found where the greeter may
// read it (a Nix store path through the ~/.icons link, or a system folder).
std::optional<Cursor> find_cursor(const fs::path& from, const std::string& user) {
  Cursor cursor;
  cursor.name = ini_value(from / ".icons/default/index.theme", "Inherits");
  if (cursor.name.empty()) cursor.name = ini_value(from / ".config/gtk-3.0/settings.ini", "gtk-cursor-theme-name");
  if (cursor.name.empty() || cursor.name.find_first_of("'\"\\$`\n") != std::string::npos) return std::nullopt;
  if (const std::string size = ini_value(from / ".config/gtk-3.0/settings.ini", "gtk-cursor-theme-size"); !size.empty()) {
    try {
      cursor.size = std::clamp(std::stoi(size), 8, 256);
    } catch (const std::exception&) {}
  }
  std::error_code error;
  for (const fs::path& icons : {from / ".icons", from / ".local/share/icons", from / ".nix-profile/share/icons",
                                fs::path("/etc/profiles/per-user") / user / "share/icons",
                                fs::path("/run/current-system/sw/share/icons"), fs::path("/usr/share/icons")}) {
    const fs::path theme = fs::canonical(icons / cursor.name, error);
    if (error || !fs::is_directory(theme / "cursors", error)) continue;
    // Inside the home the greeter cannot follow the path; the store copy it can.
    if (theme.string().starts_with(from.string() + "/")) continue;
    cursor.directory = theme.parent_path();
    return cursor;
  }
  return std::nullopt;
}

int export_theme(const fs::path& target, const fs::path& from, const std::string& user) {
  std::error_code error;
  const auto copy = [&](const fs::path& source, const fs::path& destination) {
    if (!fs::is_regular_file(source, error)) return false;
    fs::create_directories(destination.parent_path(), error);
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
    if (error) {
      std::cerr << "blxshell-greeter: could not copy " << source << ": " << error.message() << '\n';
      return false;
    }
    chmod(destination.c_str(), 0644);
    std::cout << "  " << destination.string() << '\n';
    return true;
  };
  std::cout << "Exporting to " << target.string() << ":\n";
  copy(from / ".config/blxshell/config.json", target / ".config/blxshell/config.json");
  copy(from / ".local/state/blxshell/matugen-colours.json", target / ".local/state/blxshell/matugen-colours.json");

  // The wallpaper: copied in, and the state file pointed at the copy.
  std::ifstream input(from / ".local/state/blxshell/wallpaper");
  std::string wallpaper;
  std::getline(input, wallpaper);
  if (!wallpaper.empty()) {
    // One copy at a time: drop the last wallpaper, whatever its extension.
    for (const auto& entry : fs::directory_iterator(target / ".local/state/blxshell", error))
      if (entry.path().stem() == "wallpaper-image") fs::remove(entry.path(), error);
    const fs::path copied = target / ".local/state/blxshell" / ("wallpaper-image" + fs::path(wallpaper).extension().string());
    if (copy(wallpaper, copied)) {
      std::ofstream(target / ".local/state/blxshell/wallpaper") << copied.string() << '\n';
      chmod((target / ".local/state/blxshell/wallpaper").c_str(), 0644);
    }
  }
  if (!user.empty()) copy(from / ".face", target / "avatars" / user);

  // The cursor, as shell assignments the greeter's session script reads.
  if (const auto cursor = find_cursor(from, user)) {
    std::ofstream(target / "cursor") << "XCURSOR_PATH='" << cursor->directory.string() << "'\n"
                                     << "XCURSOR_THEME='" << cursor->name << "'\n"
                                     << "XCURSOR_SIZE=" << cursor->size << '\n';
    chmod((target / "cursor").c_str(), 0644);
    std::cout << "  " << (target / "cursor").string() << " (" << cursor->name << ", " << cursor->size << ")\n";
  }

  // The greeter remembers the last user and session here.
  const fs::path memory = target / ".local/state/blxshell-greeter";
  fs::create_directories(memory, error);
  if (const passwd* account = getpwnam("greeter"); account != nullptr) {
    if (chown(memory.c_str(), account->pw_uid, account->pw_gid) != 0) {}
  }
  for (const auto& entry : fs::recursive_directory_iterator(target, error))
    if (entry.is_directory(error)) chmod(entry.path().c_str(), 0755);
  chmod(target.c_str(), 0755);
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace blx::greeter

int main(int argc, char** argv) {
  using namespace blx::greeter;
  std::vector<std::string_view> arguments(argv + 1, argv + argc);
  bool layer = false;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if (arguments[index] == "--layer") {
      layer = true;
    } else if (arguments[index] == "--export" && index + 1 < arguments.size()) {
      const fs::path target(arguments[index + 1]);
      const char* home = std::getenv("HOME");
      fs::path from = home != nullptr ? home : ".";
      const char* sudo_user = std::getenv("SUDO_USER");
      const char* login = std::getenv("USER");
      std::string user = sudo_user != nullptr ? sudo_user : login != nullptr ? login : "";
      for (std::size_t option = index + 2; option + 1 < arguments.size(); option += 2) {
        if (arguments[option] == "--from") from = arguments[option + 1];
        else if (arguments[option] == "--user") user = arguments[option + 1];
      }
      return export_theme(target, from, user);
    } else {
      std::cerr << "usage: blxshell-greeter [--layer] | --export DIR [--from HOME] [--user NAME]\n";
      return EXIT_FAILURE;
    }
  }

  prepare();
  vw::App app;
  if (layer) {
    // One layer surface per monitor; the login lives on each.
    app.per_output([](vw::Ui& ui, const vw::Output& output) {
      draw_greeter(ui, static_cast<float>(output.width), static_cast<float>(output.height));
      ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::overlay,
                  .exclusive_zone = -1, .surface_namespace = "blxshell-greeter", .refresh_interval = 1});
    });
  } else {
    // A window: cage and other kiosk compositors make it fill the screen.
    app.surface([](vw::Ui& ui) {
      vw::Size size = vw::toplevel_size();
      if (size.width == 0 || size.height == 0) {
        const vw::Output output = vw::current_output();
        size = {output.width > 0 ? output.width : 1280, output.height > 0 ? output.height : 720};
      }
      draw_greeter(ui, static_cast<float>(size.width), static_cast<float>(size.height));
      ui.toplevel({.width = static_cast<float>(size.width), .height = static_cast<float>(size.height),
                   .refresh_interval = 1, .title = "blxshell-greeter", .app_id = "blxshell-greeter",
                   .on_close = [] {}});
    });
  }
  app.on_event(greetd::event_fd(), react)
      .on_event(blx::colours_event_fd(), blx::reload_colours)
      .on_event(vw::network::event_fd(), vw::network::drain_events)
      .on_event(vw::upower::event_fd(), vw::upower::drain_events)
      .on_event(vw::bluetooth::event_fd(), vw::bluetooth::drain_events);
  return app.run();
}
