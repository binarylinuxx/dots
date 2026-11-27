#include "plugins.hpp"

#include <blxshell/plugin.hpp>
#include <vw/facet.hpp>
#include <vw/vw.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <spawn.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <mutex>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include "blob.hpp"
#include "config.hpp"
#include "draw.hpp"
#include "ipc.hpp"
#include "packages.hpp"
#include "shapes.hpp"
#include "style.hpp"

extern char** environ;

namespace blx::plugins {

using nlohmann::json;

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t max_line = 64 * 1024;
constexpr std::size_t max_buffer = 1 << 20;
// More restarts than this in the window and the plugin stays stopped.
constexpr int max_restarts = 5;
constexpr auto restart_window = std::chrono::seconds(60);

constexpr const char* colour_role_names[] = {
    "primary", "primaryText", "primaryContainer", "secondary", "secondaryContainer", "tertiary",
    "tertiaryContainer", "error", "errorText", "errorContainer", "errorContainerText", "surface",
    "surfaceContainer", "surfaceContainerLow", "surfaceContainerHigh", "surfaceContainerHighest",
    "surfaceVariant", "text", "textMuted", "outline", "outlineVariant"};

struct Process;

// What a native plugin sees: the same as Facet's state and actions.
class NativeHost final : public blxshell::Host {
 public:
  explicit NativeHost(std::string id) : id_(std::move(id)) {}
  std::string id() const override { return id_; }
  std::string get(std::string_view key) const override;
  void set(std::string_view key, std::string_view value) override;
  std::string backend() const override;
  void send(std::string_view value) override;
  void refresh(int milliseconds) override;
  vw::Color colour(std::string_view role) const override { return role_colour(std::string(role)); }
  std::string font(std::string_view which) const override {
    return which == "display" ? font::display : which == "icon" ? font::icon : font::utility;
  }
  bool run(std::string_view command) override;
  void notify(std::string_view summary, std::string_view body) override;
  void log(std::string_view text) override;
  void desktop(std::string_view json) override;
  std::string applications(std::string_view query) override;
  bool launch(std::string_view id, const std::vector<std::string>& files) override;
  std::string context() const override { return context_; }
  void set_context(std::string context) { context_ = std::move(context); }

 private:
  std::string id_;
  std::string context_ = "{}";
};

// Desktop items native plugins sent, maybe from their threads, until drain()
// takes them up on the shell's: (plugin id, message).
std::mutex& native_mutex() {
  static std::mutex mutex;
  return mutex;
}
std::vector<std::pair<std::string, std::string>>& native_queue() {
  static std::vector<std::pair<std::string, std::string>> queue;
  return queue;
}
int native_tag = 0;
int epoll();
int native_event() {
  static const int fd = [] {
    const int event = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event >= 0) {
      epoll_event watch{.events = EPOLLIN, .data = {.ptr = &native_tag}};
      epoll_ctl(epoll(), EPOLL_CTL_ADD, event, &watch);
    }
    return event;
  }();
  return fd;
}

std::string NativeHost::applications(std::string_view query) {
  json list = json::array();
  try {
    for (const vw::applications::Application& app : vw::applications::search(query, 0))
      list.push_back({{"id", app.id}, {"name", app.display_name.empty() ? app.name : app.display_name},
                      {"icon", app.icon}, {"iconPath", app.icon_path}, {"file", app.filename},
                      {"exec", app.commandline}, {"terminal", app.terminal}, {"categories", app.categories}});
  } catch (const std::exception&) {
  }
  return list.dump();
}

// An Exec line's words, quoting as the desktop entry spec has it.
std::vector<std::string> exec_words(const std::string& line) {
  std::vector<std::string> words;
  std::string word;
  bool quoted = false;
  bool any = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (quoted && c == '\\' && i + 1 < line.size()) word.push_back(line[++i]);
    else if (c == '"') quoted = !quoted, any = true;
    else if (!quoted && (c == ' ' || c == '\t')) {
      if (any || !word.empty()) words.push_back(std::move(word));
      word.clear();
      any = false;
    } else word.push_back(c);
  }
  if (any || !word.empty()) words.push_back(std::move(word));
  return words;
}

bool NativeHost::launch(std::string_view id, const std::vector<std::string>& files) {
  try {
    if (files.empty()) {
      vw::applications::launch(id);
      return true;
    }
    for (const vw::applications::Application& app : vw::applications::search(id, 0)) {
      if (app.id != id) continue;
      std::vector<std::string> argv;
      bool placed = false;
      for (const std::string& word : exec_words(app.commandline)) {
        if (word == "%f" || word == "%u") {
          argv.push_back(files.front());
          placed = true;
        } else if (word == "%F" || word == "%U") {
          argv.insert(argv.end(), files.begin(), files.end());
          placed = true;
        } else if (word.size() == 2 && word[0] == '%') {
          if (word[1] == '%') argv.emplace_back("%");
        } else {
          std::string expanded;
          for (std::size_t i = 0; i < word.size(); ++i) {
            if (word[i] == '%' && i + 1 < word.size()) {
              if (word[++i] == '%') expanded.push_back('%');
            } else expanded.push_back(word[i]);
          }
          argv.push_back(std::move(expanded));
        }
      }
      // An Exec without a field code still gets the file, as GLib does.
      if (!placed) argv.push_back(files.front());
      if (argv.empty()) return false;
      if (app.terminal) argv.insert(argv.begin(), "xdg-terminal-exec");
      return vw::spawn(argv) > 0;
    }
  } catch (const std::exception& error) {
    log(std::string("launch ") + std::string(id) + ": " + error.what());
  }
  return false;
}

void NativeHost::desktop(std::string_view json) {
  {
    std::lock_guard lock(native_mutex());
    if (native_queue().size() < 256) native_queue().emplace_back(id_, std::string(json));
  }
  const std::uint64_t one = 1;
  const auto written = ::write(native_event(), &one, sizeof one);
  (void)written;
}

// A loaded C++ plugin library.
struct Native {
  void* library = nullptr;
  std::unique_ptr<blxshell::Plugin> plugin;
  std::unique_ptr<NativeHost> host;
  ~Native() {
    plugin.reset();  // its code lives in the library: destroy it first
    // The library stays mapped: handlers it gave vw may still be queued.
  }
};

struct Process {
  packages::Manifest manifest;
  pid_t pid = -1;
  int fd = -1;  // our end of the socketpair: its stdin and stdout
  std::string buffer;
  std::map<std::string, std::vector<DesktopItem>> desktop;  // area id -> items
  std::unique_ptr<facet::Plugin> views;  // its Facet file
  std::unique_ptr<Native> native;        // its C++ library
  json store = json::object();
  json backend = json::object();
  Clock::time_point checked{};
  std::string last_error;
  std::vector<Clock::time_point> starts;
  std::string state = "stopped";  // running, exited, crashed, gave up
  bool stopping = false;
};

std::map<std::string, std::unique_ptr<Process>>& running() {
  static std::map<std::string, std::unique_ptr<Process>> processes;
  return processes;
}
// Exited children not reaped yet.
std::vector<pid_t>& dying() {
  static std::vector<pid_t> pids;
  return pids;
}

Process* find_running(const std::string& id) {
  const auto it = running().find(id);
  return it == running().end() ? nullptr : it->second.get();
}

int epoll() {
  static const int fd = epoll_create1(EPOLL_CLOEXEC);
  return fd;
}

// epoll tags for the two fds that aren't plugin sockets.
int refresh_tag = 0;
int store_tag = 0;

void log(const Process& process, std::string_view message) {
  std::cerr << "blxshell: plugin " << process.manifest.id << ": " << message << '\n';
}

std::string hex(vw::Color colour) {
  const auto byte = [](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255)); };
  return std::format("#{:02x}{:02x}{:02x}", byte(colour[0]), byte(colour[1]), byte(colour[2]));
}

json colour_roles() {
  json roles = json::object();
  for (const char* role : colour_role_names) roles[role] = hex(role_colour(role));
  return roles;
}

void send(Process& process, const json& message) {
  if (process.fd < 0) return;
  const std::string line = message.dump() + '\n';
  // A plugin that stops reading loses messages rather than stalling the shell.
  const ssize_t sent = ::send(process.fd, line.data(), line.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
  if (sent != static_cast<ssize_t>(line.size()) && !(sent < 0 && errno == EPIPE)) log(process, "isn't reading its input; a message was dropped");
}

void close_fd(Process& process) {
  if (process.fd < 0) return;
  epoll_ctl(epoll(), EPOLL_CTL_DEL, process.fd, nullptr);
  ::close(process.fd);
  process.fd = -1;
}

void reap() {
  std::erase_if(dying(), [](pid_t pid) {
    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    return result == pid || (result < 0 && errno == ECHILD);
  });
}

// --- The store -----------------------------------------------------------------

// The store as last written here, to tell our own writes from others'.
std::map<std::string, std::string>& written() {
  static std::map<std::string, std::string> texts;
  return texts;
}

int store_watch() {
  static const int fd = [] {
    const int watch = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (watch < 0) return -1;
    std::error_code error;
    std::filesystem::create_directories(store_path("x").parent_path(), error);
    inotify_add_watch(watch, store_path("x").parent_path().c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
    epoll_event event{.events = EPOLLIN, .data = {.ptr = &store_tag}};
    epoll_ctl(epoll(), EPOLL_CTL_ADD, watch, &event);
    return watch;
  }();
  return fd;
}

// The store changed here: keep it, and tell the process.
void store_changed(Process& process) {
  save_store(process.manifest.id, process.store);
  written()[process.manifest.id] = process.store.dump();
  send(process, {{"type", "store"}, {"store", process.store}});
}

// Someone else (the settings app) wrote a store: reload the running plugins'.
bool reload_stores() {
  alignas(inotify_event) char buffer[4096];
  bool any = false;
  while (::read(store_watch(), buffer, sizeof buffer) > 0) any = true;
  if (!any) return false;
  bool changed = false;
  for (auto& [id, process] : running()) {
    json fresh = load_store(id);
    if (fresh.dump() == process->store.dump()) continue;
    process->store = std::move(fresh);
    written()[id] = process->store.dump();
    send(*process, {{"type", "store"}, {"store", process->store}});
    if (process->native) {
      try {
        process->native->plugin->store_changed(*process->native->host);
      } catch (const std::exception& error) {
        log(*process, std::string("store_changed: ") + error.what());
      }
    }
    changed = true;
  }
  return changed;
}

// --- Redraws a view asked for --------------------------------------------------

int refresh_timer() {
  static const int fd = [] {
    const int timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer >= 0) {
      epoll_event event{.events = EPOLLIN, .data = {.ptr = &refresh_tag}};
      epoll_ctl(epoll(), EPOLL_CTL_ADD, timer, &event);
    }
    return timer;
  }();
  return fd;
}

Clock::time_point& next_refresh() {
  static Clock::time_point when = Clock::time_point::max();
  return when;
}

// One timer for every plugin: armed for the earliest redraw asked for.
void request_refresh(double milliseconds) {
  const auto when = Clock::now() + std::chrono::milliseconds(static_cast<long>(std::clamp(milliseconds, 16.0, 3'600'000.0)));
  if (when >= next_refresh() && next_refresh() > Clock::now()) return;
  next_refresh() = when;
  const auto delay = std::chrono::duration_cast<std::chrono::nanoseconds>(when - Clock::now()).count();
  itimerspec spec{};
  spec.it_value.tv_sec = std::max<long>(0, delay / 1'000'000'000);
  spec.it_value.tv_nsec = std::max<long>(1, delay % 1'000'000'000);
  timerfd_settime(refresh_timer(), 0, &spec, nullptr);
}

// --- What plugins may ask of the shell -----------------------------------------

bool permitted(const Process& process, const char* permission) {
  if (process.manifest.has_permission(permission)) return true;
  log(process, std::format("needs the \"{}\" permission for that", permission));
  return false;
}

void run_command(Process& process, std::vector<std::string> words) {
  if (!permitted(process, "shell")) return;
  // A plugin can't drive the package manager or other plugins through this.
  if (words.empty() || words[0] == "plugin" || words[0] == "plugins") return;
  bool ok = true, changed = false;
  if (const std::string reply = ipc::execute(words, ok, changed); !ok) log(process, reply);
}

std::vector<std::string> words_of(std::string_view text) {
  std::vector<std::string> words;
  std::istringstream stream{std::string(text)};
  for (std::string word; stream >> word;) words.push_back(word);
  return words;
}

void notify(Process& process, const std::string& summary, const std::string& body) {
  if (!permitted(process, "notifications")) return;
  vw::spawn({"notify-send", "--app-name", process.manifest.name.empty() ? process.manifest.id : process.manifest.name,
             summary, body});
}

// --- Facet views ---------------------------------------------------------------

std::string text_arg(facet::Arguments arguments, std::size_t index) {
  return index < arguments.size() ? arguments[index].to_string() : std::string();
}

facet::Value actions_for(const std::string& id) {
  const auto with = [id](auto body) {
    return facet::Value([id, body](facet::Arguments arguments) -> facet::Value {
      if (Process* process = find_running(id)) body(*process, arguments);
      return facet::Value();
    });
  };
  facet::Value::Object actions;
  // get(key, fallback): a stored value, or the fallback while there is none
  // (reading a missing field of state.store is an error in Facet).
  actions.emplace("get", facet::Value([id](facet::Arguments arguments) -> facet::Value {
    const facet::Value fallback = arguments.size() > 1 ? arguments[1] : facet::Value();
    const Process* process = find_running(id);
    if (process == nullptr || arguments.empty()) return fallback;
    const auto it = process->store.find(arguments[0].to_string());
    return it == process->store.end() || it->is_null() ? fallback : to_value(*it);
  }));
  actions.emplace("set", with([](Process& process, facet::Arguments arguments) {
    if (arguments.empty()) return;
    process.store[arguments[0].to_string()] = arguments.size() > 1 ? to_json(arguments[1]) : json(nullptr);
    store_changed(process);
  }));
  actions.emplace("send", with([](Process& process, facet::Arguments arguments) {
    send(process, {{"type", "action"}, {"data", arguments.empty() ? json(nullptr) : to_json(arguments[0])}});
  }));
  actions.emplace("refresh", with([](Process&, facet::Arguments arguments) {
    request_refresh(arguments.empty() || !arguments[0].is_number() ? 1000 : arguments[0].number());
  }));
  actions.emplace("log", with([](Process& process, facet::Arguments arguments) { log(process, text_arg(arguments, 0)); }));
  actions.emplace("run", with([](Process& process, facet::Arguments arguments) {
    std::vector<std::string> words;
    if (!arguments.empty() && arguments[0].is_list())
      for (const auto& word : arguments[0].list()) words.push_back(word.to_string());
    else
      words = words_of(text_arg(arguments, 0));
    run_command(process, std::move(words));
  }));
  actions.emplace("notify", with([](Process& process, facet::Arguments arguments) {
    notify(process, text_arg(arguments, 0), text_arg(arguments, 1));
  }));
  actions.emplace("spawn", with([](Process& process, facet::Arguments arguments) {
    if (!permitted(process, "spawn") || arguments.empty() || !arguments[0].is_list()) return;
    std::vector<std::string> argv;
    for (const auto& word : arguments[0].list()) argv.push_back(word.to_string());
    if (!argv.empty()) vw::spawn(argv);
  }));
  actions.emplace("open", with([](Process& process, facet::Arguments arguments) {
    if (permitted(process, "spawn") && !arguments.empty()) vw::spawn({"xdg-open", arguments[0].to_string()});
  }));
  return facet::Value(std::move(actions));
}

facet::Value state_for(const Process& process, const json& extra = json::object()) {
  const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
  const auto day = std::chrono::floor<std::chrono::days>(now);
  const std::chrono::hh_mm_ss clock{std::chrono::floor<std::chrono::seconds>(now - day)};
  facet::Value::Object time{{"hour", facet::Value(static_cast<int>(clock.hours().count()))},
                            {"minute", facet::Value(static_cast<int>(clock.minutes().count()))},
                            {"second", facet::Value(static_cast<int>(clock.seconds().count()))},
                            {"text", facet::Value(std::format("{:02}:{:02}", clock.hours().count(), clock.minutes().count()))}};
  facet::Value::Object state{{"store", to_value(process.store)},
                             {"backend", to_value(process.backend)},
                             {"ready", facet::Value(!process.backend.empty())},
                             {"colours", colour_values()},
                             {"fonts", font_values()},
                             {"time", facet::Value(std::move(time))},
                             {"plugin", to_value({{"id", process.manifest.id}, {"version", process.manifest.version}})}};
  for (const auto& [key, value] : extra.items()) state.insert_or_assign(key, to_value(value));
  return facet::Value(std::move(state));
}

// --- Native plugins ------------------------------------------------------------

std::string NativeHost::get(std::string_view key) const {
  const Process* process = find_running(id_);
  if (process == nullptr) return {};
  const auto it = process->store.find(std::string(key));
  return it == process->store.end() || it->is_null() ? std::string() : it->dump();
}

void NativeHost::set(std::string_view key, std::string_view value) {
  Process* process = find_running(id_);
  if (process == nullptr) return;
  json parsed = json::parse(value, nullptr, false);
  if (parsed.is_discarded()) parsed = std::string(value);  // plain text is kept as text
  process->store[std::string(key)] = std::move(parsed);
  store_changed(*process);
}

std::string NativeHost::backend() const {
  const Process* process = find_running(id_);
  return process == nullptr ? "{}" : process->backend.dump();
}

void NativeHost::send(std::string_view value) {
  if (Process* process = find_running(id_)) {
    const json data = json::parse(value, nullptr, false);
    plugins::send(*process, {{"type", "action"}, {"data", data.is_discarded() ? json(std::string(value)) : data}});
  }
}

void NativeHost::refresh(int milliseconds) { request_refresh(milliseconds); }

bool NativeHost::run(std::string_view command) {
  Process* process = find_running(id_);
  if (process == nullptr || !process->manifest.has_permission("shell")) return false;
  run_command(*process, words_of(command));
  return true;
}

void NativeHost::notify(std::string_view summary, std::string_view body) {
  if (Process* process = find_running(id_)) plugins::notify(*process, std::string(summary), std::string(body));
}

void NativeHost::log(std::string_view text) {
  if (const Process* process = find_running(id_)) plugins::log(*process, text);
}

void load_native(Process& process) {
  process.native.reset();
  if (process.manifest.native.empty()) return;
  if (!process.manifest.has_permission("native")) {
    log(process, "has a native library but not the \"native\" permission; not loading it");
    return;
  }
  const std::string path = (process.manifest.directory / process.manifest.native).string();
  void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (library == nullptr) {
    log(process, std::format("can't load {}: {}", process.manifest.native, dlerror()));
    return;
  }
  using AbiFunction = const char* (*)();
  using CreateFunction = blxshell::Plugin* (*)();
  const auto abi = reinterpret_cast<AbiFunction>(dlsym(library, "blxshell_plugin_abi"));
  const auto create = reinterpret_cast<CreateFunction>(dlsym(library, "blxshell_plugin_create"));
  if (abi == nullptr || create == nullptr) {
    log(process, "its library doesn't end with BLXSHELL_PLUGIN(…)");
    dlclose(library);
    return;
  }
  // Built against another blxshell or compiler: its classes may not match ours.
  if (std::string_view(abi()) != BLXSHELL_PLUGIN_ABI) {
    log(process, std::format("its library was built for {}, this is {}; rebuild it", abi(), BLXSHELL_PLUGIN_ABI));
    dlclose(library);
    return;
  }
  auto native = std::make_unique<Native>();
  native->library = library;
  try {
    native->plugin.reset(create());
  } catch (const std::exception& error) {
    log(process, std::string("its library failed to start: ") + error.what());
    return;
  }
  native->host = std::make_unique<NativeHost>(process.manifest.id);
  process.native = std::move(native);
}

// --- Starting and stopping -------------------------------------------------------

void load_views(Process& process) {
  process.views.reset();
  if (process.manifest.facet.empty()) return;
  try {
    auto plugin = std::make_unique<facet::Plugin>(facet::Plugin::load(process.manifest.directory / process.manifest.facet));
    register_components(*plugin);
    plugin->on_error([id = process.manifest.id](const std::exception& error) {
      std::cerr << "blxshell: plugin " << id << ": " << error.what() << '\n';
    });
    process.views = std::move(plugin);
  } catch (const std::exception& error) {
    log(process, error.what());
  }
}

bool start(Process& process) {
  const packages::Manifest& m = process.manifest;
  store_watch();
  process.store = load_store(m.id);
  written()[m.id] = process.store.dump();
  if (!process.views) load_views(process);
  if (!process.native) load_native(process);
  if (m.exec.empty()) {
    process.state = "running";  // views only, no process
    return true;
  }
  int pair[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) {
    log(process, std::string("socketpair: ") + std::strerror(errno));
    return false;
  }
  const std::string program = (m.directory / m.exec[0]).string();
  std::vector<std::string> argv_storage{program};
  argv_storage.insert(argv_storage.end(), m.exec.begin() + 1, m.exec.end());
  std::vector<char*> argv;
  for (std::string& arg : argv_storage) argv.push_back(arg.data());
  argv.push_back(nullptr);

  std::vector<std::string> env_storage;
  for (char** entry = environ; *entry != nullptr; ++entry)
    if (!std::string_view(*entry).starts_with("BLXSHELL_PLUGIN_")) env_storage.emplace_back(*entry);
  env_storage.push_back("BLXSHELL_PLUGIN_ID=" + m.id);
  env_storage.push_back("BLXSHELL_PLUGIN_DIR=" + m.directory.string());
  env_storage.push_back(std::format("BLXSHELL_PLUGIN_API={}", packages::api_version));
  std::vector<char*> envp;
  for (std::string& entry : env_storage) envp.push_back(entry.data());
  envp.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pair[1], STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions, pair[1], STDOUT_FILENO);
  posix_spawn_file_actions_addchdir_np(&actions, m.directory.c_str());
  posix_spawnattr_t attributes;
  posix_spawnattr_init(&attributes);
  // Its own process group, so stopping it takes its children too; and the
  // default signal mask, not the shell's blocked set.
  sigset_t empty;
  sigemptyset(&empty);
  posix_spawnattr_setsigmask(&attributes, &empty);
  posix_spawnattr_setpgroup(&attributes, 0);
  posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK);
  pid_t pid = -1;
  const int error = posix_spawn(&pid, program.c_str(), &actions, &attributes, argv.data(), envp.data());
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attributes);
  ::close(pair[1]);
  if (error != 0) {
    ::close(pair[0]);
    log(process, std::format("can't run {}: {}", program, std::strerror(error)));
    process.state = "crashed";
    return false;
  }
  process.pid = pid;
  process.fd = pair[0];
  process.buffer.clear();
  process.state = "running";
  process.starts.push_back(Clock::now());
  epoll_event event{.events = EPOLLIN | EPOLLRDHUP, .data = {.ptr = &process}};
  epoll_ctl(epoll(), EPOLL_CTL_ADD, process.fd, &event);
  send(process, {{"type", "hello"},
                 {"api", packages::api_version},
                 {"shell", packages::shell_version()},
                 {"compositor", packages::compositor()},
                 {"plugin", {{"id", m.id}, {"version", m.version}, {"dir", m.directory.string()}}},
                 {"store", process.store},
                 {"colours", colour_roles()}});
  return true;
}

void stop(Process& process) {
  process.stopping = true;
  close_fd(process);  // EOF on its stdin: the polite way to ask
  if (process.pid > 0) {
    ::kill(-process.pid, SIGTERM);
    dying().push_back(process.pid);
    process.pid = -1;
  }
  process.desktop.clear();
  process.backend = json::object();
  process.state = "stopped";
}

// The process closed its end: it exited, or is about to.
void ended(Process& process) {
  close_fd(process);
  int status = 0;
  bool failed = true;
  if (process.pid > 0) {
    const pid_t result = waitpid(process.pid, &status, WNOHANG);
    if (result == process.pid) failed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    else {
      ::kill(-process.pid, SIGTERM);
      dying().push_back(process.pid);
    }
    process.pid = -1;
  }
  process.desktop.clear();
  if (process.stopping) return;
  const std::string& policy = process.manifest.restart;
  if (policy == "never" || (policy == "on-failure" && !failed)) {
    process.state = failed ? "crashed" : "exited";
    log(process, failed ? "crashed" : "exited");
    return;
  }
  const auto now = Clock::now();
  std::erase_if(process.starts, [&](Clock::time_point when) { return now - when > restart_window; });
  if (static_cast<int>(process.starts.size()) > max_restarts) {
    process.state = "gave up";
    log(process, "keeps exiting; not restarting it (blxshell plugin restart to try again)");
    return;
  }
  log(process, "exited; restarting");
  start(process);
}

// --- Messages from a process -------------------------------------------------------

bool handle_desktop(Process& process, const json& message) {
  // The whole area at once: items it leaves out are gone.
  const std::string id = message.value("id", "");
  if (id.empty() || id.size() > 64 || !std::ranges::all_of(id, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; })) {
    log(process, "sent a desktop message with a bad id (letters, digits, - and _)");
    return false;
  }
  if (!message.contains("items") || !message["items"].is_array()) {
    log(process, "sent a desktop message without an items array");
    return false;
  }
  const auto string = [](const json& object, const char* key, std::size_t limit) {
    return object.contains(key) && object[key].is_string() ? object[key].get<std::string>().substr(0, limit) : std::string();
  };
  // The look of every icon in the area; an item may set its own.
  const json style = message.contains("style") && message["style"].is_object() ? message["style"] : json::object();
  const auto shape_of = [&](const json& object, const std::string& fallback) {
    std::string shape = object.contains("shape") ? string(object, "shape", 32) : fallback;
    return shape == "none" ? std::string() : shape;
  };
  const auto tint_of = [&](const json& object, const std::string& fallback) {
    const std::string tint = object.contains("tint") ? string(object, "tint", 16) : fallback;
    return tint == "colour" || tint == "color" ? std::string("colour") : tint == "themed" ? tint : std::string();
  };
  const std::string area_shape = shape_of(style, "");
  const std::string area_tint = tint_of(style, "");
  std::vector<DesktopItem> items;
  for (const json& entry : message["items"]) {
    if (items.size() >= 500) break;
    if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_string()) continue;
    DesktopItem item{.id = string(entry, "id", 512), .label = string(entry, "label", 256), .image = string(entry, "image", 4096),
                     .glyph = string(entry, "glyph", 64)};
    if (entry.contains("icon")) {
      if (entry["icon"].is_string()) item.icons.push_back(entry["icon"].get<std::string>().substr(0, 4096));
      else if (entry["icon"].is_array())
        for (const json& name : entry["icon"])
          if (name.is_string() && item.icons.size() < 8) item.icons.push_back(name.get<std::string>().substr(0, 4096));
    }
    item.shape = shape_of(entry, area_shape);
    item.tint = tint_of(entry, area_tint);
    // Images must be absolute paths: the shell loads them itself.
    if (!item.image.empty() && !item.image.starts_with('/')) item.image.clear();
    if (entry.contains("actions") && entry["actions"].is_array())
      for (const json& action : entry["actions"]) {
        if (item.actions.size() >= 16 || !action.is_object() || !action.contains("id") || !action["id"].is_string()) continue;
        item.actions.push_back({.id = string(action, "id", 128), .label = string(action, "label", 128),
                                .glyph = string(action, "glyph", 64),
                                .danger = action.contains("danger") && action["danger"].is_boolean() && action["danger"].get<bool>()});
      }
    items.push_back(std::move(item));
  }
  if (process.desktop.size() >= 8 && !process.desktop.contains(id)) {
    log(process, "uses more than 8 desktop areas");
    return false;
  }
  process.desktop[id] = std::move(items);
  return true;
}

bool handle(Process& process, const std::string& line) {
  const json message = json::parse(line, nullptr, false);
  if (!message.is_object()) {
    log(process, "sent a line that isn't a JSON object");
    return false;
  }
  const std::string type = message.value("type", "");
  if (type == "state") {
    // Data for its views, as state.backend (Host::backend() for C++).
    if (!message.contains("state")) return false;
    process.backend = message["state"];
    return true;
  }
  if (type == "set") {
    // A value for its store, e.g. a setting it changed itself.
    if (!message.contains("key") || !message["key"].is_string()) return false;
    process.store[message["key"].get<std::string>()] = message.contains("value") ? message["value"] : json(nullptr);
    save_store(process.manifest.id, process.store);
    written()[process.manifest.id] = process.store.dump();
    return true;
  }
  if (type == "desktop") return handle_desktop(process, message);
  if (type == "shell") {
    std::vector<std::string> words;
    if (message.contains("command") && message["command"].is_array()) {
      for (const auto& word : message["command"])
        if (word.is_string()) words.push_back(word);
    } else if (message.contains("command") && message["command"].is_string()) {
      words = words_of(message["command"].get<std::string>());
    }
    run_command(process, std::move(words));
    return true;
  }
  if (type == "notify") {
    notify(process, message.value("summary", process.manifest.name), message.value("body", ""));
    return false;
  }
  if (type == "log") {
    log(process, message.value("message", ""));
    return false;
  }
  log(process, std::format("sent unknown message type \"{}\"", type));
  return false;
}

bool read_from(Process& process) {
  bool changed = false;
  char chunk[8192];
  for (;;) {
    const ssize_t got = ::recv(process.fd, chunk, sizeof chunk, MSG_DONTWAIT);
    if (got > 0) {
      process.buffer.append(chunk, static_cast<std::size_t>(got));
      if (process.buffer.size() > max_buffer) {
        log(process, "sent a line longer than 1 MiB; dropped");
        process.buffer.clear();
      }
      continue;
    }
    if (got == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      // Lines it finished before closing still count.
      for (std::size_t end; (end = process.buffer.find('\n')) != std::string::npos;) {
        changed |= handle(process, process.buffer.substr(0, std::min(end, max_line)));
        process.buffer.erase(0, end + 1);
      }
      ended(process);
      return true;
    }
    break;
  }
  for (std::size_t end; (end = process.buffer.find('\n')) != std::string::npos;) {
    if (end > max_line) log(process, "sent a line longer than 64 KiB; dropped");
    else changed |= handle(process, process.buffer.substr(0, end));
    process.buffer.erase(0, end + 1);
  }
  return changed;
}

std::pair<std::string, std::string> split(const std::string& bar_id) {
  const std::size_t colon = bar_id.rfind(':');
  if (colon == std::string::npos) return {};
  return {bar_id.substr(0, colon), bar_id.substr(colon + 1)};
}

// A plugin's view names: its Facet file's, then its library's.
std::vector<std::string> view_names(const Process& process) {
  std::vector<std::string> names;
  if (process.views) names = process.views->views();
  if (process.native) {
    try {
      for (std::string& name : process.native->plugin->views()) names.push_back(std::move(name));
    } catch (const std::exception&) {
    }
  }
  return names;
}

}  // namespace

// --- Shared with the settings app ---------------------------------------------------

std::filesystem::path store_path(const std::string& id) {
  const char* state = std::getenv("XDG_STATE_HOME");
  const char* home = std::getenv("HOME");
  const std::filesystem::path base = state != nullptr && *state ? std::filesystem::path(state)
                                                                 : std::filesystem::path(home != nullptr ? home : ".") / ".local/state";
  return base / "blxshell" / "plugins" / (id + ".json");
}

json load_store(const std::string& id) {
  std::ifstream input(store_path(id));
  json saved = json::parse(input, nullptr, false);
  return saved.is_object() ? saved : json::object();
}

void save_store(const std::string& id, const json& store) {
  const auto file = store_path(id);
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  {
    std::ofstream output(file.string() + ".tmp");
    output << store.dump(2) << '\n';
  }
  std::filesystem::rename(file.string() + ".tmp", file, error);
}

facet::Value to_value(const json& value) {
  if (value.is_boolean()) return facet::Value(value.get<bool>());
  if (value.is_number()) return facet::Value(value.get<double>());
  if (value.is_string()) return facet::Value(value.get<std::string>());
  if (value.is_array()) {
    facet::Value::List list;
    for (const json& item : value) list.push_back(to_value(item));
    return facet::Value(std::move(list));
  }
  if (value.is_object()) {
    facet::Value::Object object;
    for (const auto& [key, item] : value.items()) object.emplace(key, to_value(item));
    return facet::Value(std::move(object));
  }
  return facet::Value();
}

json to_json(const facet::Value& value) {
  if (value.is_bool()) return value.boolean();
  if (value.is_number()) return value.number();
  if (value.is_text()) return value.text();
  if (value.is_color()) return hex(value.color());
  if (value.is_list()) {
    json list = json::array();
    for (const auto& item : value.list()) list.push_back(to_json(item));
    return list;
  }
  if (value.is_object()) {
    json object = json::object();
    for (const auto& [key, item] : value.object()) object[key] = to_json(item);
    return object;
  }
  return nullptr;
}

facet::Value colour_values() {
  facet::Value::Object colours;
  for (const char* role : colour_role_names) colours.emplace(role, facet::Value(role_colour(role)));
  return facet::Value(std::move(colours));
}

facet::Value font_values() {
  return to_value({{"text", font::utility}, {"display", font::display}, {"icon", font::icon}});
}

namespace {

float number_or(const facet::Value& value, float fallback) {
  return value.is_number() ? static_cast<float>(value.number()) : fallback;
}

// A Facet handler as a vw handler, with the event as Facet passes it.
template <class Event, class Convert>
std::function<void(const Event&)> handler(const facet::Value& function, Convert convert) {
  if (!function.is_function()) return {};
  return [function, convert](const Event& event) {
    try {
      const facet::Value argument = convert(event);
      function(facet::Arguments(&argument, 1));
    } catch (const std::exception& error) {
      std::cerr << "blxshell: plugin handler: " << error.what() << '\n';
    }
  };
}

}  // namespace

void register_components(facet::Plugin& plugin) {
  // Native(name: Pill, color: …, on_click: …) { … }: children in a row on a
  // rounded background sized to them, the way bar modules look.
  plugin.component("Pill", [](vw::Ui& ui, const facet::Value& props, const facet::Children& children) {
    const auto click = [](const vw::ClickEvent& e) {
      return facet::Value(facet::Value::Object{{"x", e.x}, {"y", e.y}, {"button", e.button}, {"left", e.left()},
                                               {"right", e.right()}, {"middle", e.middle()}});
    };
    const auto scroll = [](const vw::ScrollEvent& e) {
      return facet::Value(facet::Value::Object{{"x", e.x}, {"y", e.y}, {"delta", e.delta}});
    };
    const vw::Color colour = props["color"].is_color() ? props["color"].color() : colours().surface_container;
    const float height = number_or(props["height"], 34);
    vw::Input input{.on_click = handler<vw::ClickEvent>(props["on_click"], click),
                    .on_right_click = handler<vw::ClickEvent>(props["on_right_click"], click),
                    .on_middle_click = handler<vw::ClickEvent>(props["on_middle_click"], click),
                    .on_scroll = handler<vw::ScrollEvent>(props["on_scroll"], scroll)};
    if (input.on_click || input.on_right_click || input.on_middle_click)
      input.hover = vw::Hover{.color = mix(colour, colours().text, 0.08F), .duration = 0.12F};
    const float radius = number_or(props["radius"], height / 2);
    const vw::Widget back = ui.rectangle({.width = 1, .height = 1, .color = colour, .input = std::move(input)});
    const std::vector<vw::Widget> items = children(ui);
    const vw::Widget content = ui.row(items, {.gap = number_or(props["gap"], 6), .padding = number_or(props["padding"], 6),
                                              .height = height});
    const vw::Widget pill = facet::behind(ui, back, content);
    // Its radius once it has its size: vw clamps it to half the 1x1 placeholder.
    ui.scene().rectangles[back.begin].radius = std::min(radius, std::min(content.width, content.height) / 2);
    return pill;
  });

  // Native(name: Parallax, …): full-screen image layers, back first, each
  // shifted by its depth as `shift` goes 0..1, gliding to its place.
  plugin.component("Parallax", [](vw::Ui& ui, const facet::Value& props, const facet::Children&) {
    const vw::Size size = vw::surface_size();
    const float width = number_or(props["width"], size.width > 0 ? static_cast<float>(size.width) : 1920.0F);
    const float height = number_or(props["height"], size.height > 0 ? static_cast<float>(size.height) : 1080.0F);
    const float shift = std::clamp(number_or(props["shift"], 0.5F), 0.0F, 1.0F);
    const float strength = std::clamp(number_or(props["strength"], 0.05F), 0.0F, 0.5F);
    const double glide = std::clamp(number_or(props["glide"], 700), 0.0F, 5000.0F) / 1000.0;
    const std::string key = props["id"].is_text() ? props["id"].text() : "parallax";
    const vw::Widget first = ui.hit_area({.width = 1, .height = 1});
    if (props["layers"].is_list()) {
      std::size_t index = 0;
      for (const facet::Value& layer : props["layers"].list()) {
        ++index;
        if (!layer.is_object() || !layer["path"].is_text()) continue;
        const float depth = std::max(0.0F, number_or(layer["depth"], 0));
        const float opacity = std::clamp(number_or(layer["opacity"], 1), 0.0F, 1.0F);
        if (opacity <= 0.001F) continue;
        // Wider than the screen by its travel, so its edges never show.
        const float travel = width * strength * depth;
        const float target = -travel * 2 * shift;
        const auto x = static_cast<float>(vw::animate(std::format("{}.{}", key, index), target,
                                                      {.duration = glide, .easing = vw::Easing::out_expo}));
        try {
          ui.image({.path = layer["path"].text(), .width = width + travel * 2, .height = height, .fit = vw::ImageFit::cover,
                    .tint = {1, 1, 1, opacity}, .place = {.x = x, .y = 0}});
        } catch (const std::exception&) {
        }
      }
    }
    const vw::Widget last = ui.hit_area({.width = 1, .height = 1});
    return vw::Widget{first.begin, last.end, width, height};
  });
  plugin.component("Icon", [](vw::Ui& ui, const facet::Value& props, const facet::Children&) {
    const float size = props["size"].is_number() ? static_cast<float>(props["size"].number()) : 20.0F;
    const vw::Color color = props["color"].is_color() ? props["color"].color() : colours().text;
    const std::string glyph = props["glyph"].is_text() ? props["glyph"].text() : props["value"].to_string();
    return draw::capture(ui, size, size, [&] { draw::glyph(ui, glyph, size / 2, size / 2, size, color); });
  });
  // Native(name: Shape, shape: "cookie9sided", size: 40, color: …, on_click: …):
  // a Material shape (see shapes.hpp); a new shape for the same id morphs.
  plugin.component("Shape", [](vw::Ui& ui, const facet::Value& props, const facet::Children&) {
    const float size = number_or(props["size"], 40);
    const vw::Color color = props["color"].is_color() ? props["color"].color() : colours().primary;
    const std::string shape = props["shape"].is_text() ? props["shape"].text() : std::string("circle");
    const std::string id = "facet." + (props["id"].is_text() ? props["id"].text() : shape);
    const auto click = [](const vw::ClickEvent& e) {
      return facet::Value(facet::Value::Object{{"x", e.x}, {"y", e.y}, {"button", e.button}});
    };
    auto on_click = handler<vw::ClickEvent>(props["on_click"], click);
    return draw::capture(ui, size, size, [&] {
      shapes::draw(ui, id, shape, 0, 0, size, color);
      if (on_click)
        ui.hit_area({.width = size, .height = size, .place = {.x = 0, .y = 0}, .input = {.on_click = std::move(on_click)}});
    });
  });
  plugin.component("Ring", [](vw::Ui& ui, const facet::Value& props, const facet::Children&) {
    const float size = props["size"].is_number() ? static_cast<float>(props["size"].number()) : 26.0F;
    const float stroke = props["stroke"].is_number() ? static_cast<float>(props["stroke"].number()) : 2.0F;
    const float progress = props["progress"].is_number() ? static_cast<float>(props["progress"].number())
                           : props["value"].is_number()  ? static_cast<float>(props["value"].number())
                                                         : 0.0F;
    const vw::Color color = props["color"].is_color() ? props["color"].color() : colours().primary;
    return draw::capture(ui, size, size, [&] { blob::ring(ui, {0, 0, size, size}, stroke, 0, progress, color); });
  });
}

// --- The running set --------------------------------------------------------------

int event_fd() {
  refresh_timer();
  store_watch();
  return epoll();
}

// What native plugins sent with Host::desktop, on the shell's thread now.
bool drain_native() {
  std::uint64_t count = 0;
  while (::read(native_event(), &count, sizeof count) > 0) {
  }
  std::vector<std::pair<std::string, std::string>> queue;
  {
    std::lock_guard lock(native_mutex());
    queue.swap(native_queue());
  }
  bool changed = false;
  for (const auto& [id, text] : queue) {
    Process* process = find_running(id);
    if (process == nullptr || !process->native) continue;
    try {
      changed |= handle_desktop(*process, json::parse(text));
    } catch (const std::exception& error) {
      log(*process, std::string("sent desktop items that aren't JSON: ") + error.what());
    }
  }
  return changed;
}

// A native plugin's start(), once it's in running() so its host can reach it.
void start_native(Process& process) {
  if (!process.native) return;
  try {
    process.native->plugin->start(*process.native->host);
  } catch (const std::exception& error) {
    log(process, std::string("its library failed to start: ") + error.what());
    process.native.reset();
  }
}

bool drain() {
  native_event();
  reap();
  bool changed = false;
  epoll_event events[16];
  for (;;) {
    const int count = epoll_wait(epoll(), events, 16, 0);
    if (count <= 0) break;
    for (int index = 0; index < count; ++index) {
      void* tag = events[index].data.ptr;
      if (tag == &refresh_tag) {  // a redraw a view asked for
        std::uint64_t expirations = 0;
        if (::read(refresh_timer(), &expirations, sizeof expirations) > 0) changed = true;
        next_refresh() = Clock::time_point::max();
        continue;
      }
      if (tag == &store_tag) {
        changed |= reload_stores();
        continue;
      }
      if (tag == &native_tag) {
        changed |= drain_native();
        continue;
      }
      auto* process = static_cast<Process*>(tag);
      // It may have been stopped by an earlier event in this batch.
      const bool alive = std::ranges::any_of(running(), [&](const auto& entry) { return entry.second.get() == process; });
      if (alive && process->fd >= 0) changed |= read_from(*process);
    }
    if (count < 16) break;
  }
  return changed;
}

void sync() {
  auto& processes = running();
  const auto& enabled = config().plugins_enabled;
  for (auto it = processes.begin(); it != processes.end();) {
    if (listed(enabled, it->first)) {
      ++it;
      continue;
    }
    stop(*it->second);
    it = processes.erase(it);
  }
  for (const std::string& id : enabled) {
    if (processes.contains(id)) continue;
    auto manifest = packages::find(id, packages::Kind::plugin);
    if (!manifest) {
      std::cerr << "blxshell: plugin " << id << " is enabled but not installed\n";
      continue;
    }
    if (const auto why = packages::incompatibility(*manifest)) {
      std::cerr << "blxshell: plugin " << id << " won't run here: " << *why << '\n';
      continue;
    }
    auto process = std::make_unique<Process>();
    process->manifest = std::move(*manifest);
    start(*process);
    Process& added = *processes.emplace(id, std::move(process)).first->second;
    start_native(added);
  }
}

void colours_changed() {
  const json message{{"type", "colours"}, {"colours", colour_roles()}};
  for (auto& [id, process] : running()) send(*process, message);
}

void stop_all() {
  for (auto& [id, process] : running()) stop(*process);
  running().clear();
  // Give them a moment to go, then make sure.
  for (int attempt = 0; attempt < 20 && !dying().empty(); ++attempt) {
    reap();
    if (!dying().empty()) usleep(10'000);
  }
  for (pid_t pid : dying()) ::kill(-pid, SIGKILL);
  reap();
}

bool is_plugin_module(const std::string& bar_id) { return bar_id.find(':') != std::string::npos; }

std::vector<std::string> bar_views() {
  std::vector<std::string> ids;
  for (const auto& [id, process] : running())
    for (const std::string& view : view_names(*process))
      if (view.starts_with("Bar")) ids.push_back(id + ":" + view);
  return ids;
}

bool has_view(const std::string& bar_id) {
  const auto [plugin, view] = split(bar_id);
  const Process* process = find_running(plugin);
  if (process == nullptr) return false;
  const auto names = view_names(*process);
  return std::ranges::find(names, view) != names.end();
}

std::optional<Material> lock_material() {
  for (const auto& [id, process] : running()) {
    const packages::Manifest& m = process->manifest;
    if (m.lock_material.empty()) continue;
    Material material{(m.directory / m.lock_material).string(), {}};
    for (std::size_t index = 0; index < m.lock_material_params.size() && index < 3; ++index) {
      const auto& [key, fallback] = m.lock_material_params[index];
      const auto it = process->store.find(key);
      material.params[index] = it != process->store.end() && it->is_number() ? it->get<float>() : fallback;
    }
    for (const auto& [key, fallback] : m.lock_material_data) {
      const auto it = process->store.find(key);
      material.data.push_back(it != process->store.end() && it->is_number() ? it->get<float>() : fallback);
    }
    // Passes: a number, or {"key", "default"} from the store; all of them
    // only while "passesWhen" {"key", "equals"} holds.
    const json given = json::parse(m.lock_material_json, nullptr, false);
    if (given.is_object() && given.contains("passes") && given["passes"].is_array()) {
      const auto value_of = [&](const json& entry) -> float {
        if (entry.is_number()) return entry.get<float>();
        if (entry.is_object() && entry.value("key", json()).is_string()) {
          const auto it = process->store.find(entry["key"].get<std::string>());
          if (it != process->store.end() && it->is_number()) return it->get<float>();
          return entry.value("default", 0.0F);
        }
        return 0;
      };
      bool applies = true;
      if (given.contains("passesWhen") && given["passesWhen"].is_object()) {
        const json& when = given["passesWhen"];
        applies = std::abs(value_of({{"key", when.value("key", "")}, {"default", when.value("default", 0.0F)}}) -
                           when.value("equals", 0.0F)) < 0.001F;
      }
      if (applies) {
        for (const json& pass : given["passes"]) {
          vw::ShaderPass added;
          added.shader = (m.directory / pass.value("shader", "")).string();
          added.output = pass.value("output", "");
          if (pass.contains("inputs") && pass["inputs"].is_array())
            for (const json& input : pass["inputs"])
              if (input.is_string()) added.inputs.push_back(input.get<std::string>());
          if (pass.contains("params") && pass["params"].is_array())
            for (std::size_t index = 0; index < 4 && index < pass["params"].size(); ++index)
              added.params[index] = value_of(pass["params"][index]);
          if (pass.contains("data") && pass["data"].is_array())
            for (const json& entry : pass["data"]) added.data.push_back(value_of(entry));
          material.passes.push_back(std::move(added));
        }
        if (given.contains("inputs") && given["inputs"].is_array())
          for (const json& input : given["inputs"])
            if (input.is_string()) material.inputs.push_back(input.get<std::string>());
        material.padding = given.value("padding", 0.0F);
      }
    }
    return material;
  }
  return std::nullopt;
}

std::vector<LockOption> lock_options(const std::string& view_id) {
  const auto [plugin, view] = split(view_id);
  const Process* process = find_running(plugin);
  if (process == nullptr) return {};
  const auto found = process->manifest.lock_options.find(view);
  if (found == process->manifest.lock_options.end()) return {};
  std::vector<LockOption> options;
  for (const json& given : json::parse(found->second, nullptr, false)) {
    if (!given.is_object()) continue;
    LockOption option;
    option.key = given.value("key", "");
    option.label = given.value("label", option.key);
    option.type = given.value("type", "");
    option.fallback = given.value("default", json());
    option.min = given.value("min", 0.0F);
    option.max = given.value("max", 1.0F);
    option.step = given.value("step", 0.0F);
    option.unit = given.value("unit", "");
    option.placeholder = given.value("placeholder", "");
    if (given.contains("choices") && given["choices"].is_array())
      for (const json& choice : given["choices"]) {
        if (choice.is_string()) option.choices.emplace_back(choice.get<std::string>(), choice.get<std::string>());
        else if (choice.is_object() && choice.value("value", json()).is_string())
          option.choices.emplace_back(choice["value"].get<std::string>(), choice.value("label", choice["value"].get<std::string>()));
      }
    if (option.fallback.is_null()) {
      // Something sensible to show before it is set.
      if (option.type == "toggle") option.fallback = false;
      else if (option.type == "slider") option.fallback = option.min;
      else if (option.type == "choice" && !option.choices.empty()) option.fallback = option.choices.front().first;
      else option.fallback = "";
    }
    options.push_back(std::move(option));
  }
  return options;
}

std::string plugin_name(const std::string& id) {
  const Process* process = find_running(id);
  return process == nullptr || process->manifest.name.empty() ? id : process->manifest.name;
}

std::vector<std::string> lock_views() {
  std::vector<std::string> ids;
  for (const auto& [id, process] : running())
    for (const std::string& view : view_names(*process))
      if (view.starts_with("Lock")) ids.push_back(id + ":" + view);
  return ids;
}

std::optional<vw::Widget> draw_view(vw::Ui& ui, const std::string& bar_id) { return draw_view(ui, bar_id, json::object()); }

std::optional<vw::Widget> draw_view(vw::Ui& ui, const std::string& bar_id, const json& extra) {
  const auto [plugin, view] = split(bar_id);
  Process* process = find_running(plugin);
  if (process == nullptr) return std::nullopt;
  const auto failed = [&](const std::exception& error) -> std::optional<vw::Widget> {
    if (process->last_error != error.what()) log(*process, process->last_error = error.what());
    return std::nullopt;
  };
  if (process->views) {
    const auto names = process->views->views();
    if (std::ranges::find(names, view) != names.end()) {
      // Edits to the .facet file show up within a second.
      if (Clock::now() - process->checked > std::chrono::seconds(1)) {
        process->checked = Clock::now();
        try {
          if (process->views->reload()) process->last_error.clear();
        } catch (const std::exception& error) {
          failed(error);
        }
      }
      try {
        const facet::Value arguments[] = {state_for(*process, extra), actions_for(plugin)};
        return process->views->view(ui, view, arguments);
      } catch (const std::exception& error) {
        return failed(error);
      }
    }
  }
  if (process->native) {
    try {
      process->native->host->set_context(extra.dump());
      const vw::Widget drawn = process->native->plugin->view(ui, view, *process->native->host);
      process->native->host->set_context("{}");
      return drawn;
    } catch (const std::exception& error) {
      return failed(error);
    }
  }
  return std::nullopt;
}

bool draw_wallpaper(vw::Ui& ui, float width, float height, int workspace, int workspaces) {
  for (auto& [id, process] : running()) {
    if (!process->views) continue;
    const auto names = process->views->views();
    if (std::ranges::find(names, "Wallpaper") == names.end()) continue;
    try {
      const json extra{{"screen", {{"width", width}, {"height", height}}},
                       {"workspace", {{"index", workspace}, {"count", workspaces}}}};
      const facet::Value arguments[] = {state_for(*process, extra), actions_for(id)};
      process->views->view(ui, "Wallpaper", arguments);
      return true;
    } catch (const std::exception& error) {
      if (process->last_error != error.what()) log(*process, process->last_error = error.what());
      return false;
    }
  }
  return false;
}

std::vector<Desktop> desktops() {
  std::vector<Desktop> result;
  for (const auto& [id, process] : running())
    for (const auto& [area, items] : process->desktop)
      if (!items.empty()) result.push_back({id + ":" + area, items});
  return result;
}

void desktop_action(const std::string& source, const std::string& item, const std::string& action) {
  const auto [plugin, area] = split(source);
  Process* process = find_running(plugin);
  if (process == nullptr) return;
  if (process->native) {
    try {
      process->native->plugin->desktop_action(*process->native->host, area, item, action);
    } catch (const std::exception& error) {
      log(*process, std::string("desktop_action: ") + error.what());
    }
  }
  send(*process, {{"type", "event"}, {"desktop", area}, {"item", item}, {"event", "action"}, {"action", action}});
}

std::string call(const std::string& id, const std::string& command, const std::vector<std::string>& args, bool& ok) {
  Process* process = find_running(id);
  ok = false;
  if (process == nullptr) return std::format("plugin {} isn't running", id);
  if (process->native) {
    try {
      if (process->native->plugin->command(*process->native->host, command, args)) {
        ok = true;
        return "ok";
      }
    } catch (const std::exception& error) {
      return std::format("plugin {}: {}", id, error.what());
    }
  }
  if (process->fd < 0) return std::format("plugin {} has no process to take commands", id);
  send(*process, {{"type", "command"}, {"name", command}, {"args", args}});
  ok = true;
  return "ok";
}

std::string restart(const std::string& id, bool& ok) {
  Process* process = find_running(id);
  ok = false;
  if (process == nullptr) return std::format("plugin {} isn't enabled", id);
  stop(*process);
  process->stopping = false;
  process->starts.clear();
  // Pick up an update installed since it started.
  if (auto manifest = packages::find(id, packages::Kind::plugin); manifest && manifest->valid()) process->manifest = std::move(*manifest);
  process->views.reset();
  process->native.reset();
  ok = start(*process);
  start_native(*process);
  return ok ? "ok" : std::format("plugin {} didn't start", id);
}

std::string status() {
  std::string text;
  for (const auto& [id, process] : running()) {
    std::string kinds;
    if (process->views) kinds += " facet";
    if (process->native) kinds += " native";
    if (process->pid > 0) kinds += std::format(" process (pid {})", process->pid);
    text += std::format("{:<36} {:<10} {}{}\n", id, process->manifest.version, process->state, kinds);
  }
  return text.empty() ? "no plugins enabled\n" : text;
}

std::string views() {
  std::string text;
  for (const auto& [id, process] : running())
    for (const std::string& view : view_names(*process)) text += id + ":" + view + '\n';
  return text;
}

}  // namespace blx::plugins
