// blxshell-store [id]: the plugin and theme store, a window of its own. It lists
// what the repositories offer (the official one, then config.json's
// plugins.repositories) and installs, updates, removes, enables and applies
// through the blxshell command line beside it, so a package goes through
// exactly the checks `blxshell install` makes: the index's sha256, the
// project, compatibility. The network never runs on the drawing thread.
#include <vw/vw.hpp>

#include <fcntl.h>
#include <spawn.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "packages.hpp"
#include "style.hpp"

extern char** environ;

namespace fs = std::filesystem;

namespace blx::store {
namespace {

namespace pk = packages;

// --- What the worker threads find, read by the drawing thread -----------------

struct Shared {
  std::mutex lock;
  bool loading = false;
  bool loaded = false;
  std::vector<pk::Remote> remotes;
  std::vector<std::string> problems;
  std::map<std::string, std::vector<std::string>> previews;  // id -> cached images, in order
  std::map<std::string, std::string> readmes;                // id -> README text ("" while loading)
  std::map<std::string, std::string> busy;                   // id -> "Installing…"
  std::map<std::string, std::string> pending;                // id -> what the switch shows meanwhile ("on"/"off")
  std::map<std::string, std::string> notes;                  // id -> the last failure
};

Shared& shared() {
  static Shared state;
  return state;
}

// Worker threads poke this; the app redraws.
int wake_fd() {
  static const int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  return fd;
}

void wake() {
  const std::uint64_t one = 1;
  if (write(wake_fd(), &one, sizeof one) < 0) {}
}

bool drain_wake() {
  std::uint64_t value = 0;
  while (read(wake_fd(), &value, sizeof value) > 0) {}
  return true;
}

// --- Installed state, read on the drawing thread ------------------------------

std::uint64_t& local_generation() {
  static std::uint64_t value = 1;
  return value;
}

const std::map<std::string, pk::Manifest>& installed() {
  static std::map<std::string, pk::Manifest> cache;
  static std::uint64_t generation = 0;
  if (generation != local_generation()) {
    cache.clear();
    for (const pk::Kind kind : {pk::Kind::theme, pk::Kind::plugin})
      for (pk::Manifest& m : pk::installed(kind)) cache.emplace(m.id, std::move(m));
    generation = local_generation();
  }
  return cache;
}

// --- Work in the background -------------------------------------------------

void load_previews(std::vector<pk::Remote> remotes) {
  for (const pk::Remote& remote : remotes) {
    for (const std::string& url : remote.previews) {
      const auto file = pk::cached_file(url);
      if (!file) continue;
      std::lock_guard guard(shared().lock);
      shared().previews[remote.id].push_back(file->string());
      wake();
    }
  }
}

void refresh() {
  ++local_generation();  // packages installed meanwhile from the command line
  {
    std::lock_guard guard(shared().lock);
    if (shared().loading) return;
    shared().loading = true;
    shared().previews.clear();
    shared().readmes.clear();
  }
  std::thread([] {
    std::vector<std::string> problems;
    auto remotes = pk::available(problems);
    {
      std::lock_guard guard(shared().lock);
      shared().remotes = remotes;
      shared().problems = std::move(problems);
      shared().loading = false;
      shared().loaded = true;
    }
    wake();
    load_previews(std::move(remotes));
  }).detach();
}

// The README, fetched fresh the first time its page opens.
void load_readme(const pk::Remote& remote) {
  {
    std::lock_guard guard(shared().lock);
    if (shared().readmes.contains(remote.id)) return;
    shared().readmes[remote.id] = "";
  }
  if (remote.readme.empty()) return;
  std::thread([id = remote.id, url = remote.readme] {
    std::string text;
    if (const auto file = pk::cached_file(url, true)) {
      std::ifstream input(*file);
      text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    std::lock_guard guard(shared().lock);
    shared().readmes[id] = text;
    wake();
  }).detach();
}

// blxshell itself: beside this program, else on PATH.
std::string blxshell_program() {
  std::error_code error;
  const fs::path self = fs::read_symlink("/proc/self/exe", error);
  if (!error) {
    const fs::path sibling = self.parent_path() / "blxshell";
    if (fs::is_regular_file(sibling, error)) return sibling.string();
  }
  return "blxshell";
}

// Runs `blxshell <arguments>`; its last line of complaint on failure.
std::optional<std::string> run_blxshell(std::vector<std::string> arguments) {
  arguments.insert(arguments.begin(), blxshell_program());
  std::vector<char*> argv;
  for (std::string& argument : arguments) argv.push_back(argument.data());
  argv.push_back(nullptr);
  int pipe_fds[2];
  if (pipe2(pipe_fds, O_CLOEXEC) != 0) return "can't start blxshell";
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
  pid_t pid = -1;
  const int failed = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fds[1]);
  std::string output;
  if (failed == 0) {
    char buffer[1024];
    ssize_t got = 0;
    while ((got = read(pipe_fds[0], buffer, sizeof buffer)) > 0 || (got < 0 && errno == EINTR))
      if (got > 0) output.append(buffer, static_cast<std::size_t>(got));
  }
  close(pipe_fds[0]);
  if (failed != 0) return "can't start blxshell";
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return std::nullopt;
  while (!output.empty() && (output.back() == '\n' || output.back() == ' ')) output.pop_back();
  const auto line = output.rfind('\n');
  std::string last = line == std::string::npos ? output : output.substr(line + 1);
  if (last.starts_with("blxshell: ")) last.erase(0, 10);
  return last.empty() ? std::string("it failed") : last;
}

// `switch_to` is what a plugin's switch shows until the work is done.
void act(const std::string& id, const std::string& doing, std::vector<std::string> arguments, const std::string& switch_to = "") {
  {
    std::lock_guard guard(shared().lock);
    if (shared().busy.contains(id)) return;
    shared().busy[id] = doing;
    if (!switch_to.empty()) shared().pending[id] = switch_to;
    shared().notes.erase(id);
  }
  std::thread([id, arguments = std::move(arguments)] {
    const auto failure = run_blxshell(arguments);
    {
      std::lock_guard guard(shared().lock);
      shared().busy.erase(id);
      shared().pending.erase(id);
      if (failure) shared().notes[id] = *failure;
    }
    wake();
  }).detach();
}

// --- Where we are -------------------------------------------------------------

enum class Filter { all, plugins, themes, installed, updates };

struct View {
  Filter filter = Filter::all;
  std::string search;
  std::string details;      // the package whose page is open; empty for the grid
  std::size_t picture = 0;  // which preview its page shows large
};

View& view() {
  static View state;
  return state;
}

void open_details(const std::string& id) {
  view().details = id;
  view().picture = 0;
  vw::set_scroll_offset("store-details", 0);
}

std::string lower(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

bool matches(const pk::Remote& remote, const std::string& search) {
  if (search.empty()) return true;
  const std::string needle = lower(search);
  if (lower(remote.name).find(needle) != std::string::npos || lower(remote.id).find(needle) != std::string::npos ||
      lower(remote.description).find(needle) != std::string::npos)
    return true;
  return std::ranges::any_of(remote.tags, [&](const std::string& tag) { return lower(tag).find(needle) != std::string::npos; });
}

bool shown(const pk::Remote& remote) {
  const auto found = installed().find(remote.id);
  const bool has = found != installed().end();
  switch (view().filter) {
    case Filter::all: break;
    case Filter::plugins: if (remote.type != "plugin") return false; break;
    case Filter::themes: if (remote.type != "theme") return false; break;
    case Filter::installed: if (!has) return false; break;
    case Filter::updates: if (!has || !pk::newer(remote.version, found->second.version)) return false; break;
  }
  return matches(remote, view().search);
}

// --- What a package can do here, for its card and its page --------------------

struct Snapshot {
  std::map<std::string, std::vector<std::string>> previews;
  std::map<std::string, std::string> readmes;
  std::map<std::string, std::string> busy;
  std::map<std::string, std::string> pending;
  std::map<std::string, std::string> notes;

  std::string get(const std::map<std::string, std::string>& map, const std::string& id) const {
    const auto found = map.find(id);
    return found == map.end() ? std::string() : found->second;
  }
};

struct Status {
  const pk::Manifest* local = nullptr;
  bool update = false;
  bool theme = false;
  bool enabled = false;  // plugins: as the switch shows it
  bool applied = false;  // themes
  bool removable = false;
  std::optional<std::string> problem;
  std::string busy;
  std::string note;
};

Status status_of(const pk::Remote& remote, const Snapshot& snapshot) {
  Status s;
  const auto found = installed().find(remote.id);
  if (found != installed().end()) s.local = &found->second;
  s.theme = remote.type == "theme";
  s.update = s.local != nullptr && pk::newer(remote.version, s.local->version);
  const auto& enabled = config().plugins_enabled;
  s.enabled = std::ranges::find(enabled, remote.id) != enabled.end();
  if (const std::string pending = snapshot.get(snapshot.pending, remote.id); !pending.empty()) s.enabled = pending == "on";
  s.applied = config().theme == remote.id;
  s.removable = s.local != nullptr &&
                s.local->directory.string().starts_with(pk::user_directory(s.theme ? pk::Kind::theme : pk::Kind::plugin).string());
  s.problem = pk::remote_incompatibility(remote);
  s.busy = snapshot.get(snapshot.busy, remote.id);
  s.note = snapshot.get(snapshot.notes, remote.id);
  return s;
}

// --- Drawing --------------------------------------------------------------------

constexpr float padding = 28;
constexpr float card_min_width = 300;
constexpr float card_height = 360;
constexpr float gap = 16;
constexpr float button_height = 40;
constexpr const char* list_id = "store-grid";

float button_width(const std::string& label, const std::string& glyph) {
  return 32 + draw::measure(label, 14, vw::weight::medium, font::utility) + (glyph.empty() ? 0 : 26);
}

// A pill button; `strong` is the filled primary one. Returns its width.
float button(vw::Ui& ui, float x, float y, const std::string& label, bool strong, bool enabled, std::function<void()> on_click,
             const std::string& glyph = "") {
  const Colours& c = colours();
  const float width = button_width(label, glyph);
  const vw::Color fill = !enabled ? c.surface_container_high : strong ? c.primary : c.secondary_container;
  const vw::Color ink = !enabled ? c.text_muted : strong ? c.primary_text : c.text;
  draw::rect(ui, x, y, width, button_height, button_height / 2, fill);
  float text_x = x + 16;
  if (!glyph.empty()) {
    draw::glyph(ui, glyph, text_x + 10, y + button_height / 2, 19, ink);
    text_x += 26;
  }
  draw::text_middle(ui, label, text_x, y + button_height / 2, {.size = 14, .color = ink, .weight = vw::weight::medium, .family = font::utility});
  if (enabled)
    draw::state_layer(ui, x, y, width, button_height, button_height / 2, ink,
                      {.on_click = [on_click = std::move(on_click)](const vw::ClickEvent&) { on_click(); }});
  return width;
}

constexpr float switch_width = 51;
constexpr float switch_height = 30;

// The sidebar's quick toggle: a track and a thumb with a check or a cross.
void quick_switch(vw::Ui& ui, float x, float y, const std::string& id, bool on, bool enabled, std::function<void()> on_click) {
  const Colours& c = colours();
  draw::rect(ui, x, y, switch_width, switch_height, switch_height / 2, on ? c.primary : c.surface_container_highest);
  const float travel = static_cast<float>(vw::animate("store.switch." + id, on ? 1 : 0, {.duration = 0.35, .easing = vw::Easing::out_back}));
  const float thumb_x = x + 2 + travel * (switch_width - 26 - 4);
  draw::rect(ui, thumb_x, y + 2, 26, 26, 13, on ? c.primary_text : c.outline);
  draw::glyph(ui, on ? "check" : "close", thumb_x + 13, y + switch_height / 2, 15, on ? c.primary : c.surface_container_highest);
  if (enabled)
    draw::state_layer(ui, x, y, switch_width, switch_height, switch_height / 2, on ? c.primary_text : c.text,
                      {.on_click = [on_click = std::move(on_click)](const vw::ClickEvent&) { on_click(); }});
}

// The actions for a package along a row from (x, y), `right` the row's end:
// buttons from the left, a plugin's switch at the right.
void actions(vw::Ui& ui, float x, float y, float right, const pk::Remote& remote, const Status& s) {
  const std::string id = remote.id;
  const float switch_y = y + (button_height - switch_height) / 2;
  // Flipping the switch shows on the switch itself; anything else on a button.
  const bool toggling = s.busy == "Enabling…" || s.busy == "Disabling…";
  if (!s.busy.empty() && !toggling) {
    button(ui, x, y, s.busy, true, false, [] {});
    if (s.local != nullptr && !s.theme) quick_switch(ui, right - switch_width, switch_y, id, s.enabled, false, [] {});
    return;
  }
  if (s.local == nullptr) {
    button(ui, x, y, "Install", true, !s.problem, [id] { act(id, "Installing…", {"install", id}); }, "download");
    return;
  }
  if (s.update) x += button(ui, x, y, "Update", true, s.busy.empty(), [id] { act(id, "Updating…", {"update", id}); }, "upgrade") + 8;
  if (s.theme) {
    x += button(ui, x, y, s.applied ? "Applied" : "Apply", !s.update && !s.applied, !s.applied && s.busy.empty(),
                [id] { act(id, "Applying…", {"theme", "apply", id}); }, s.applied ? "check" : "palette") + 8;
  }
  if (s.removable)
    button(ui, x, y, "Remove", false, s.busy.empty(), [id] { act(id, "Removing…", {"remove", id}); }, "delete");
  if (!s.theme) {
    const bool on = s.enabled;
    quick_switch(ui, right - switch_width, switch_y, id, on, s.busy.empty(), [id, on] {
      act(id, on ? "Disabling…" : "Enabling…", {"plugin", on ? "disable" : "enable", id}, on ? "off" : "on");
    });
  }
}

std::string version_line(const pk::Remote& remote, const Status& s) {
  const std::string kind = s.theme ? "Theme" : "Plugin";
  if (s.update) return std::format("{}  ·  {} → {}", kind, s.local->version, remote.version);
  if (s.local != nullptr) return std::format("{}  ·  {}  ·  installed", kind, remote.version);
  return std::format("{}  ·  {}", kind, remote.version);
}

// What it may do, why it can't run here, or what just went wrong.
std::pair<std::string, bool> notice_line(const pk::Remote& remote, const Status& s) {
  if (!s.note.empty()) return {s.note, true};
  if (s.problem) return {"Won't run here: " + *s.problem, true};
  if (s.theme) return {"Colours only", false};
  std::string permissions;
  for (const std::string& permission : remote.permissions) permissions += (permissions.empty() ? "" : ", ") + permission;
  return {permissions.empty() ? "No permissions" : "Needs " + permissions, false};
}

void picture(vw::Ui& ui, float x, float y, float width, float height, float radius, const std::string& path, bool theme,
             vw::ImageFit fit) {
  const Colours& c = colours();
  draw::rect(ui, x, y, width, height, radius, c.surface_container_high);
  if (!path.empty()) {
    try {
      ui.image({.path = path, .width = width, .height = height, .fit = fit, .radius = radius, .place = {.x = x, .y = y}});
      return;
    } catch (const std::exception&) {
    }
  }
  draw::glyph(ui, theme ? "palette" : "extension", x + width / 2, y + height / 2, 40, c.primary);
}

void draw_card(vw::Ui& ui, float x, float y, float width, const pk::Remote& remote, const Snapshot& snapshot) {
  const Colours& c = colours();
  const Status s = status_of(remote, snapshot);
  draw::rect(ui, x, y, width, card_height, 24, c.surface_container);
  // The whole card opens its page; the buttons drawn later take their own clicks.
  draw::state_layer(ui, x, y, width, card_height, 24, c.text,
                    {.on_click = [id = remote.id](const vw::ClickEvent&) { open_details(id); }});

  constexpr float inset = 10;
  constexpr float picture_height = 148;
  const auto previews = snapshot.previews.find(remote.id);
  const std::string first = previews == snapshot.previews.end() || previews->second.empty() ? "" : previews->second.front();
  picture(ui, x + inset, y + inset, width - inset * 2, picture_height, 16, first, s.theme, vw::ImageFit::cover);

  float cursor = y + inset + picture_height + 16;
  const float text_x = x + 18;
  const float text_width = width - 36;
  draw::text(ui, remote.name, text_x, cursor,
             {.size = 17, .color = c.text, .weight = vw::weight::semibold, .family = font::display, .max_width = text_width});
  cursor += 26;
  draw::text(ui, version_line(remote, s), text_x, cursor,
             {.size = 12, .color = s.update ? c.primary : c.text_muted, .weight = vw::weight::medium, .family = font::utility,
              .max_width = text_width});
  cursor += 26;
  draw::text(ui, remote.description, text_x, cursor,
             {.size = 13, .color = c.text_muted, .family = font::utility, .max_width = text_width, .max_lines = 2});

  // The notice and the actions keep to the bottom, clear of the text.
  const float buttons_y = y + card_height - 18 - button_height;
  const auto [line, bad] = notice_line(remote, s);
  draw::text(ui, line, text_x, buttons_y - 30,
             {.size = 12, .color = bad ? c.error : c.text_muted, .weight = vw::weight::medium, .family = font::utility,
              .max_width = text_width});
  actions(ui, text_x, buttons_y, x + width - 18, remote, s);
}

void draw_search(vw::Ui& ui, float width, bool loading) {
  const Colours& c = colours();
  constexpr float field_width = 320;
  const float field_x = width - padding - 48 - 10 - field_width;
  ui.text_field({.id = "store-search", .placeholder = "Search plugins and themes", .width = field_width, .height = 44, .radius = 22,
                 .border_width = 1, .color = c.surface_container_highest, .border_color = c.surface_container_highest,
                 .focus_color = c.primary, .text_color = c.text, .placeholder_color = c.text_muted, .caret_color = c.primary,
                 .font = font::utility, .size = 14,
                 .on_change = [](const std::string& text) {
                   view().search = text;
                   vw::set_scroll_offset(list_id, 0);
                 },
                 .place = {.x = field_x, .y = 22}});
  const float refresh_x = width - padding - 48;
  draw::rect(ui, refresh_x, 20, 48, 48, 24, c.secondary_container);
  draw::glyph(ui, loading ? "hourglass_top" : "refresh", refresh_x + 24, 44, 22, c.text);
  if (!loading) draw::state_layer(ui, refresh_x, 20, 48, 48, 24, c.text, {.on_click = [](const vw::ClickEvent&) { refresh(); }});
}

void draw_filters(vw::Ui& ui) {
  const Colours& c = colours();
  static constexpr std::pair<Filter, const char*> filters[] = {
      {Filter::all, "All"}, {Filter::plugins, "Plugins"}, {Filter::themes, "Themes"},
      {Filter::installed, "Installed"}, {Filter::updates, "Updates"}};
  float x = padding;
  for (const auto& [filter, label] : filters) {
    const bool chosen = view().filter == filter;
    const float chip = 32 + draw::measure(label, 14, vw::weight::medium, font::utility);
    draw::rect(ui, x, 84, chip, 36, 18, chosen ? c.secondary_container : c.surface_container);
    draw::text_middle(ui, label, x + 16, 102,
                      {.size = 14, .color = chosen ? c.text : c.text_muted, .weight = vw::weight::medium, .family = font::utility});
    draw::state_layer(ui, x, 84, chip, 36, 18, c.text, {.on_click = [filter](const vw::ClickEvent&) {
                        view().filter = filter;
                        vw::set_scroll_offset(list_id, 0);
                      }});
    x += chip + 8;
  }
}

void draw_grid(vw::Ui& ui, float width, float height, const std::vector<pk::Remote>& remotes,
               const std::vector<std::string>& problems, bool loaded, const Snapshot& snapshot) {
  const Colours& c = colours();
  draw::text(ui, "Store", padding, 26, {.size = 30, .color = c.text, .weight = vw::weight::semibold, .family = font::display});
  draw_filters(ui);

  std::vector<const pk::Remote*> list;
  for (const pk::Remote& remote : remotes)
    if (shown(remote)) list.push_back(&remote);

  float top = 140;
  std::string message;
  if (!loaded) message = "Loading what the repositories offer…";
  else if (list.empty()) message = remotes.empty() ? "The repositories couldn't be read." : "Nothing matches.";
  if (!message.empty()) {
    draw::text(ui, message, padding, top, {.size = 15, .color = c.text_muted, .weight = vw::weight::medium, .family = font::utility});
    top += 30;
  }
  for (const std::string& problem : problems) {
    draw::text(ui, problem, padding, top, {.size = 12, .color = c.error, .family = font::utility, .max_width = width - padding * 2});
    top += 22;
  }
  if (list.empty()) return;

  const float available = width - padding * 2;
  const auto columns = static_cast<std::size_t>(std::max(1.0F, std::floor((available + gap) / (card_min_width + gap))));
  const float card_width = (available - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  const std::size_t rows = (list.size() + columns - 1) / columns;
  ui.listview({.id = list_id, .width = available, .height = std::max(1.0F, height - top - 8), .item_height = card_height,
               .gap = gap, .scroll_step = 110, .count = rows,
               .render = [&](std::size_t row) -> std::optional<vw::Widget> {
                 return draw::capture(ui, available, card_height, [&] {
                   for (std::size_t column = 0; column < columns; ++column) {
                     const std::size_t index = row * columns + column;
                     if (index >= list.size()) break;
                     draw_card(ui, static_cast<float>(column) * (card_width + gap), 0, card_width, *list[index], snapshot);
                   }
                 });
               },
               .place = {.x = padding, .y = top}});
}

// --- The details page -------------------------------------------------------------

// Inline markdown reduced to its words: links to their text, no emphasis marks.
std::string plain(std::string text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '[') {
      const auto close = text.find("](", i);
      const auto end = close == std::string::npos ? std::string::npos : text.find(')', close);
      if (end != std::string::npos) {
        out += text.substr(i + 1, close - i - 1);
        i = end;
        continue;
      }
    }
    if (text[i] == '*' || text[i] == '`' || (text[i] == '_' && i + 1 < text.size() && text[i + 1] == '_')) continue;
    out += text[i];
  }
  return out;
}

const char* meaning(const std::string& permission) {
  if (permission == "shell") return "Runs blxshell commands: opens panels, the launcher or the dashboard, locks the screen.";
  if (permission == "notifications") return "Shows desktop notifications.";
  if (permission == "network") return "Talks to the network.";
  if (permission == "filesystem") return "Reads or writes files outside its own folder.";
  if (permission == "spawn") return "Starts other programs.";
  if (permission == "native") return "Includes C++ that runs inside the shell, built by CI from its source.";
  return "An unknown permission.";
}

// Text that wraps, as a block of the page.
vw::Widget paragraph(vw::Ui& ui, const std::string& text, float width, const draw::TextStyle& style) {
  draw::TextStyle wrapped = style;
  wrapped.max_width = width;
  wrapped.max_lines = 0;
  const vw::Widget text_widget = draw::text(ui, text, 0, 0, wrapped);
  return ui.column({text_widget}, {.width = width, .align = vw::Align::start});
}

void draw_details(vw::Ui& ui, float width, float height, const pk::Remote& remote, const Snapshot& snapshot) {
  const Colours& c = colours();
  const Status s = status_of(remote, snapshot);

  // Back, and the name, above the page.
  draw::rect(ui, padding, 22, 44, 44, 22, c.secondary_container);
  draw::glyph(ui, "arrow_back", padding + 22, 44, 22, c.text);
  draw::state_layer(ui, padding, 22, 44, 44, 22, c.text, {.on_click = [](const vw::ClickEvent&) { view().details.clear(); }});
  draw::text(ui, remote.name, padding + 60, 26,
             {.size = 28, .color = c.text, .weight = vw::weight::semibold, .family = font::display, .max_width = width - 400});

  const float column = std::min(860.0F, width - padding * 2);
  std::vector<vw::Widget> blocks;
  const auto block = [&](float block_height, const std::function<void()>& paint) {
    blocks.push_back(draw::capture(ui, column, block_height, paint));
  };
  const auto heading = [&](const std::string& text) {
    blocks.push_back(paragraph(ui, text, column, {.size = 19, .color = c.text, .weight = vw::weight::semibold, .family = font::display}));
  };
  const auto body = [&](const std::string& text, vw::Color colour) {
    blocks.push_back(paragraph(ui, text, column, {.size = 14, .color = colour, .family = font::utility}));
  };

  // The line under the name, and what can be done with it.
  block(64, [&] {
    draw::text(ui, version_line(remote, s), 0, 0,
               {.size = 13, .color = s.update ? c.primary : c.text_muted, .weight = vw::weight::medium, .family = font::utility});
    actions(ui, 0, 22, column, remote, s);
  });
  if (const auto [line, bad] = notice_line(remote, s); bad) body(line, c.error);

  // The pictures: one large, the rest to pick from.
  const auto found = snapshot.previews.find(remote.id);
  const std::vector<std::string> pictures = found == snapshot.previews.end() ? std::vector<std::string>{} : found->second;
  const float large = std::min(420.0F, column * 0.5F);
  block(large, [&] {
    const std::string shown_picture = pictures.empty() ? "" : pictures[std::min(view().picture, pictures.size() - 1)];
    picture(ui, 0, 0, column, large, 20, shown_picture, s.theme, vw::ImageFit::contain);
  });
  if (pictures.size() > 1) {
    block(72, [&] {
      float x = 0;
      for (std::size_t index = 0; index < pictures.size(); ++index) {
        picture(ui, x, 0, 112, 72, 12, pictures[index], s.theme, vw::ImageFit::cover);
        if (index == view().picture) draw::rect(ui, x + 40, 64, 32, 4, 2, c.primary);
        draw::state_layer(ui, x, 0, 112, 72, 12, c.text, {.on_click = [index](const vw::ClickEvent&) { view().picture = index; }});
        x += 120;
      }
    });
  }

  if (!remote.description.empty()) body(remote.description, c.text);

  // About.
  heading("About");
  std::vector<std::pair<std::string, std::string>> facts{{"Id", remote.id}, {"Version", remote.version}};
  if (s.local != nullptr && s.local->version != remote.version) facts.emplace_back("Installed", s.local->version);
  std::string where;
  for (const std::string& compositor : remote.compositors) where += (where.empty() ? "" : ", ") + compositor;
  if (!where.empty()) facts.emplace_back("Works under", where == "any" ? "any compositor" : where);
  std::string authors;
  for (const std::string& author : remote.authors) authors += (authors.empty() ? "" : ", ") + author;
  if (!authors.empty()) facts.emplace_back("By", authors);
  if (!remote.license.empty()) facts.emplace_back("License", remote.license);
  facts.emplace_back("From", remote.repository);
  block(static_cast<float>(facts.size()) * 28, [&] {
    float y = 0;
    for (const auto& [label, value] : facts) {
      draw::text(ui, label, 0, y, {.size = 14, .color = c.text_muted, .family = font::utility});
      draw::text(ui, value, 140, y, {.size = 14, .color = c.text, .family = font::utility, .max_width = column - 140});
      y += 28;
    }
  });

  // Permissions, plugins only.
  if (!s.theme) {
    heading("Permissions");
    if (remote.permissions.empty()) body("None: it only shows its views and reacts to clicks.", c.text_muted);
    for (const std::string& permission : remote.permissions) body(std::format("{}  —  {}", permission, meaning(permission)), c.text);
    body("A plugin runs as you, like any program you start. Permissions limit what it may ask blxshell to do; "
         "they aren't a sandbox. Everything in the official repository is reviewed before it's merged.",
         c.text_muted);
  }

  // The README, read as plain paragraphs.
  const std::string readme = snapshot.get(snapshot.readmes, remote.id);
  if (!remote.readme.empty()) {
    if (readme.empty()) {
      body("Loading its README…", c.text_muted);
    } else {
      std::istringstream input(readme);
      std::string line;
      std::string paragraph_text;
      bool code = false;
      const auto flush = [&] {
        if (!paragraph_text.empty()) body(plain(paragraph_text), c.text);
        paragraph_text.clear();
      };
      while (std::getline(input, line)) {
        if (line.starts_with("```")) {
          flush();
          code = !code;
          continue;
        }
        if (code) {
          blocks.push_back(paragraph(ui, line.empty() ? " " : line, column,
                                     {.size = 13, .color = c.primary, .family = "monospace"}));
          continue;
        }
        if (line.empty() || line.starts_with("<")) {
          flush();
          continue;
        }
        if (line.starts_with("#")) {
          flush();
          const auto start = line.find_first_not_of("# ");
          if (start != std::string::npos) heading(plain(line.substr(start)));
          continue;
        }
        if (line.starts_with("- ") || line.starts_with("* ")) {
          flush();
          body("•  " + plain(line.substr(2)), c.text);
          continue;
        }
        if (line.starts_with("|")) {  // tables: a row per line
          flush();
          std::string cells;
          std::stringstream row(line);
          std::string cell;
          while (std::getline(row, cell, '|')) {
            const auto a = cell.find_first_not_of(' ');
            if (a == std::string::npos || cell.find_first_not_of("-: ") == std::string::npos) continue;
            cells += (cells.empty() ? "" : "  ·  ") + cell.substr(a, cell.find_last_not_of(' ') - a + 1);
          }
          if (!cells.empty()) body(plain(cells), c.text);
          continue;
        }
        paragraph_text += (paragraph_text.empty() ? "" : " ") + line;
      }
      flush();
    }
  }

  const float top = 84;
  ui.scrollbox(blocks, {.id = "store-details", .width = column, .height = std::max(1.0F, height - top), .gap = 18,
                        .scroll_step = 90, .place = {.x = (width - column) / 2, .y = top}});
}

// --- The window -------------------------------------------------------------------

void build(vw::Ui& ui) {
  const Colours& c = colours();
  ui.toplevel({.width = 1100, .height = 780, .title = "blxshell Store", .app_id = "blxshell-store"});
  const vw::Size size = vw::toplevel_size();
  const float width = size.width > 0 ? static_cast<float>(size.width) : 1100;
  const float height = size.height > 0 ? static_cast<float>(size.height) : 780;
  ui.rectangle({.width = width, .height = height, .color = c.surface});

  // A copy of what the workers found, so they never wait on drawing.
  std::vector<pk::Remote> remotes;
  std::vector<std::string> problems;
  Snapshot snapshot;
  bool loading = false;
  bool loaded = false;
  {
    std::lock_guard guard(shared().lock);
    remotes = shared().remotes;
    problems = shared().problems;
    snapshot.previews = shared().previews;
    snapshot.readmes = shared().readmes;
    snapshot.busy = shared().busy;
    snapshot.pending = shared().pending;
    snapshot.notes = shared().notes;
    loading = shared().loading;
    loaded = shared().loaded;
  }
  // Work just finished: what's installed may have changed.
  static std::size_t was_busy = 0;
  if (snapshot.busy.size() < was_busy) ++local_generation();
  was_busy = snapshot.busy.size();

  // Escape goes back from a package's page.
  ui.keyboard([](const vw::KeyEvent& key) {
    if (key.name == "Escape" && !view().details.empty()) {
      view().details.clear();
      return true;
    }
    return false;
  });

  if (!view().details.empty()) {
    const auto found = std::ranges::find(remotes, view().details, &pk::Remote::id);
    if (found != remotes.end()) {
      load_readme(*found);
      draw_details(ui, width, height, *found, snapshot);
      return;
    }
    // Asked for before the list arrived: wait for it; not in it: the grid.
    if (!loaded) {
      draw::text(ui, "Loading…", padding, 30, {.size = 15, .color = c.text_muted, .family = font::utility});
      return;
    }
    view().details.clear();
  }
  draw_search(ui, width, loading);
  draw_grid(ui, width, height, remotes, problems, loaded, snapshot);
}

}  // namespace
}  // namespace blx::store

int main(int argc, char** argv) {
  using namespace blx;
  // `blxshell-store <id>` opens that package's page.
  if (argc > 1) store::open_details(argv[1]);
  sync_style();
  store::refresh();
  vw::App app(store::build);
  app.on_event(store::wake_fd(), store::drain_wake)
      .on_event(colours_event_fd(), reload_colours)
      .on_event(config_event_fd(), [] {
        const bool changed = reload_config();
        if (changed) sync_style();
        return changed;
      });
  return app.run();
}
