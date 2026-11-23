#pragma once

#include <optional>
#include <string>
#include <vector>

// Running plugins. An enabled plugin with "exec" is a child process of the
// shell, in any language, that speaks JSON lines on stdin/stdout: one JSON
// object per line each way. The protocol is in docs/packages.md.
//
// The process runs as the user, like anything else they start: permissions
// in the manifest gate what it may ask *the shell* to do (shell commands,
// notifications) and tell the store's reviewers what it does; they are not
// a sandbox.
namespace blx::plugins {

// What a plugin's bar module shows, as last sent by the plugin.
struct Module {
  std::string text;
  std::string icon;     // a Material Symbols name
  std::string tone;     // a colour role: "primary", "error", ... ("" is the default)
  std::optional<float> progress;  // 0..1: a ring around the icon
  bool visible = true;
};

// An action in a desktop item's popup menu.
struct DesktopAction {
  std::string id;
  std::string label;
  std::string glyph;    // a Material Symbols name
  bool danger = false;  // shown in the error colour (delete, trash, ...)
};

// One icon on the desktop, as last sent by the plugin.
struct DesktopItem {
  std::string id;
  std::string label;
  std::vector<std::string> icons;  // theme icon names or files, the first that loads wins
  std::string image;               // an image file shown as the icon (a thumbnail)
  std::string glyph;               // Material Symbols fallback
  std::vector<DesktopAction> actions;
};

// A plugin's desktop area: "<plugin id>:<area id>" and its items.
struct Desktop {
  std::string source;
  std::vector<DesktopItem> items;
};

// One epoll fd covering every plugin's socket; pass to vw::App::on_event.
int event_fd();
// Reads what plugins sent; true when anything shown changed.
bool drain();
// Starts and stops plugins to match config().plugins_enabled, and sends
// changed settings. Call at startup and after the config changes.
void sync();
// Tells running plugins the colour scheme changed.
void colours_changed();
// Stops every plugin (on exit).
void stop_all();

// A bar module by its bar id, "<plugin id>:<module id>"; nullptr when the
// plugin isn't running or hasn't sent it yet.
const Module* module(const std::string& bar_id);
bool is_plugin_module(const std::string& bar_id);
// Pointer input on a bar module: event "click" (button "left", "right",
// "middle") or "scroll" (delta).
void click(const std::string& bar_id, const std::string& button);
void scroll(const std::string& bar_id, double delta);

// Every running plugin's desktop items, in plugin order.
std::vector<Desktop> desktops();
// The user picked `action` on `item` of a desktop area (source as above).
void desktop_action(const std::string& source, const std::string& item, const std::string& action);

// For IPC: `plugin call <id> <command> [args…]`, `plugin restart <id>`,
// `plugin status`. Returns the reply text; ok is false on an error.
std::string call(const std::string& id, const std::string& command, const std::vector<std::string>& args, bool& ok);
std::string restart(const std::string& id, bool& ok);
std::string status();

}  // namespace blx::plugins
