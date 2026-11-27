#pragma once

#include <nlohmann/json.hpp>
#include <vw/facet_runtime.hpp>
#include <vw/ui.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Running plugins (api 2). A plugin has any of three entry points:
//
//   facet   its views in Facet, drawn by the shell (the usual way)
//   exec    a process in any language behind them, speaking JSON lines on
//           stdin/stdout (docs/packages.md has the protocol)
//   native  a C++ library with views drawn by vw (<blxshell/plugin.hpp>);
//           runs inside the shell, so it needs the "native" permission
//
// Views named Bar… are bar modules, "<plugin id>:<view>"; a view named
// Settings is the plugin's page in the settings app; a view named Wallpaper
// is drawn full-screen over the wallpaper (the first enabled plugin's wins),
// under the desktop icons and widgets. Views named Lock… are lock screen
// widgets the user places with the layout editor (`blxshell lockscreen edit`).
//
// Every plugin has one store, a JSON object kept in
// ~/.local/state/blxshell/plugins/<id>.json: its settings and whatever else it
// wants to keep. Views read and write it, its settings page edits it, and its
// process gets it in hello and every time it changes.
//
// Processes run as the user: permissions gate what a plugin may ask the shell
// to do (shell commands, notifications, spawning) and tell reviewers what it
// does. Only Facet views are confined to what the shell hands them.
namespace blx::plugins {

// Facet views get (state, actions):
//
//   state.store      the plugin's store
//   state.backend    what its process last sent as {"type": "state", ...}
//   state.ready      true once it has sent something (read backend fields
//                    only then: a missing field is an error in Facet)
//   state.colours    the colour roles, as colours
//   state.fonts      text, display and icon: the shell's fonts
//   state.time       hour, minute, second, and text ("14:05")
//   state.plugin     id and version
//   state.screen     width and height (Wallpaper views)
//   state.workspace  index (0-based) and count (Wallpaper views)
//   actions.get(key, fallback) a stored value, or the fallback
//   actions.set(key, value)    store a value
//   actions.send(value)        to its process, as {"type": "action", "data": value}
//   actions.refresh(ms)        draw again after ms (clocks, timers)
//   actions.run("launcher toggle")   a blxshell command   (permission "shell")
//   actions.notify(summary, body)    a notification       (permission "notifications")
//   actions.spawn(["prog", "arg"]), actions.open(path)    (permission "spawn")
//   actions.log(text)
//
// Native(name: Icon, glyph: "timer", size: 20, color: …) draws a Material
// Symbols icon, Native(name: Ring, progress: 0.4, size: 26, color: …) the bar's
// progress ring, and Native(name: Pill, color: …, on_click: …, on_right_click:
// …, on_middle_click: …, on_scroll: …) { … } lays its children in a row on a
// rounded background sized to them, like the bar's own modules.
// Native(name: Shape, shape: "cookie9sided", size: 40, color: …, on_click: …)
// draws a Material shape.
// Native(name: Parallax, layers: [{path, depth, opacity}], shift: 0..1,
// strength: 0.05, glide: 700) stacks full-screen images, back first: each
// slides by depth * strength of the screen width as shift goes 0 to 1 (and
// glides there over glide ms); opacity crossfades.

// An action in a desktop item's popup menu.
struct DesktopAction {
  std::string id;
  std::string label;
  std::string glyph;    // a Material Symbols name
  bool danger = false;  // shown in the error colour (delete, trash, ...)
};

// One icon on the desktop, as last sent by a plugin's process.
struct DesktopItem {
  std::string id;
  std::string label;
  std::vector<std::string> icons;  // theme icon names or files, the first that loads wins
  std::string image;               // an image file shown as the icon (a thumbnail)
  std::string glyph;               // Material Symbols fallback
  std::vector<DesktopAction> actions;
  // How the icon is drawn, from the message's "style" or the item's own:
  // shape is a Material shape name ("cookie9sided", "clover4leaf", ...) the
  // icon sits on, like Android's adaptive icons, or empty for none; tint is
  // "colour" (recoloured toward the theme), "themed" (monochrome in the
  // accent, like Android's themed icons) or empty for none.
  std::string shape;
  std::string tint;
};

// A plugin's desktop area: "<plugin id>:<area id>" and its items.
struct Desktop {
  std::string source;
  std::vector<DesktopItem> items;
};

// One epoll fd covering every plugin's socket, the refresh timer and the
// store watch; pass to vw::App::on_event.
int event_fd();
// Reads what plugins sent; true when anything shown changed.
bool drain();
// Starts and stops plugins to match config().plugins_enabled. Call at
// startup and after the config changes.
void sync();
// Tells running plugins the colour scheme changed.
void colours_changed();
// Stops every plugin (on exit).
void stop_all();

// Bar modules: views named Bar…, as "<plugin id>:<view>".
bool is_plugin_module(const std::string& bar_id);
std::vector<std::string> bar_views();
bool has_view(const std::string& bar_id);
// Draws a running plugin's view; nullopt when it isn't running or the view
// failed (the error is logged). `extra` adds fields to its state.
std::optional<vw::Widget> draw_view(vw::Ui& ui, const std::string& bar_id);
std::optional<vw::Widget> draw_view(vw::Ui& ui, const std::string& bar_id, const nlohmann::json& extra);
// Lock screen widgets: views named Lock…, as "<plugin id>:<view>". They get
// state.lock = {scale, editing, greeter, sheet}: draw at `scale` (the user
// resizes them in the layout editor). On the lock screen they are shown
// only, never clicked: a locked session takes no orders from a plugin.
std::vector<std::string> lock_views();
// The lock screen's material: the first running plugin's lockMaterial, as
// an absolute shader path and its three parameters from the plugin's store.
struct Material {
  std::string shader;
  std::array<float, 3> params{};
  std::vector<float> data;  // shader_data, from lockMaterial.data
  // Passes before it (lockMaterial.passes), when they apply: then the
  // shader is the last pass, reading `inputs`, and the rectangle itself is
  // transparent with the backdrop colour in color2.
  std::vector<vw::ShaderPass> passes;
  std::vector<std::string> inputs;
  float padding = 0;
};
std::optional<Material> lock_material();

// One option a plugin declares for one of its Lock views: the layout
// editor shows it for each such widget, and the view reads the widget's
// values as state.lock.options (Host::context for C++).
struct LockOption {
  std::string key;
  std::string label;
  std::string type;  // toggle, choice, slider, text, colour
  nlohmann::json fallback;  // "default"
  std::vector<std::pair<std::string, std::string>> choices;  // choice: (value, label)
  float min = 0;
  float max = 1;
  float step = 0;  // slider; 0 is continuous
  std::string unit;         // slider: shown after the value
  std::string placeholder;  // text
};
// The options of "<plugin id>:<view>", none when it declares none or isn't
// running.
std::vector<LockOption> lock_options(const std::string& view);
// A running plugin's name from its manifest; the id when it isn't running.
std::string plugin_name(const std::string& id);

// The first running plugin's Wallpaper view, full-screen at (0, 0); false
// when no plugin has one (or it failed).
bool draw_wallpaper(vw::Ui& ui, float width, float height, int workspace, int workspaces);

// Every running plugin's desktop items, in plugin order.
std::vector<Desktop> desktops();
// The user picked `action` on `item` of a desktop area (source as above).
void desktop_action(const std::string& source, const std::string& item, const std::string& action);

// For IPC: `plugin call <id> <command> [args…]` (sent to its process),
// `plugin restart <id>`, `plugin status`, `plugin views`.
std::string call(const std::string& id, const std::string& command, const std::vector<std::string>& args, bool& ok);
std::string restart(const std::string& id, bool& ok);
std::string status();
std::string views();

// Shared with the settings app, which draws plugins' Settings views.
std::filesystem::path store_path(const std::string& id);
nlohmann::json load_store(const std::string& id);
void save_store(const std::string& id, const nlohmann::json& store);
facet::Value to_value(const nlohmann::json& value);
nlohmann::json to_json(const facet::Value& value);
facet::Value colour_values();
facet::Value font_values();
// Icon, Ring, Pill, Shape and Parallax, for Native(name: …).
void register_components(facet::Plugin& plugin);

}  // namespace blx::plugins
