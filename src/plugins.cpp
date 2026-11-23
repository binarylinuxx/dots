#include "plugins.hpp"

#include <nlohmann/json.hpp>
#include <vw/vw.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <memory>
#include <spawn.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.hpp"
#include "ipc.hpp"
#include "packages.hpp"
#include "style.hpp"

extern char** environ;

namespace blx::plugins {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

constexpr std::size_t max_line = 64 * 1024;
constexpr std::size_t max_buffer = 1 << 20;
// More restarts than this in the window and the plugin stays stopped.
constexpr int max_restarts = 5;
constexpr auto restart_window = std::chrono::seconds(60);

struct Process {
  packages::Manifest manifest;
  pid_t pid = -1;
  int fd = -1;  // our end of the socketpair: its stdin and stdout
  std::string buffer;
  std::string settings;  // the settings JSON last sent
  std::map<std::string, Module> modules;
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

int epoll() {
  static const int fd = epoll_create1(EPOLL_CLOEXEC);
  return fd;
}

void log(const Process& process, std::string_view message) {
  std::cerr << "blxshell: plugin " << process.manifest.id << ": " << message << '\n';
}

std::string hex(vw::Color colour) {
  const auto byte = [](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255)); };
  return std::format("#{:02x}{:02x}{:02x}", byte(colour[0]), byte(colour[1]), byte(colour[2]));
}

json colour_roles() {
  json roles = json::object();
  for (const char* role : {"primary", "primaryText", "primaryContainer", "secondary", "secondaryContainer", "tertiary",
                           "tertiaryContainer", "error", "errorText", "errorContainer", "errorContainerText", "surface",
                           "surfaceContainer", "surfaceContainerLow", "surfaceContainerHigh", "surfaceContainerHighest",
                           "surfaceVariant", "text", "textMuted", "outline", "outlineVariant"})
    roles[role] = hex(role_colour(role));
  return roles;
}

// The manifest's defaults with the user's values on top.
json settings_for(const packages::Manifest& manifest) {
  json values = json::object();
  for (const packages::Setting& setting : manifest.settings)
    if (!setting.default_json.empty()) values[setting.key] = json::parse(setting.default_json, nullptr, false);
  const auto& saved = config().plugin_settings;
  if (const auto it = saved.find(manifest.id); it != saved.end()) {
    const json user = json::parse(it->second, nullptr, false);
    if (user.is_object())
      for (const auto& [key, value] : user.items()) values[key] = value;
  }
  return values;
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

bool start(Process& process) {
  const packages::Manifest& m = process.manifest;
  if (m.exec.empty()) {
    process.state = "running";  // nothing to run: a declaration-only plugin
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

  json modules = json::array();
  for (const auto& module : m.bar_modules) modules.push_back(module.id);
  const json settings = settings_for(m);
  process.settings = settings.dump();
  send(process, {{"type", "hello"},
                 {"api", packages::api_version},
                 {"shell", packages::shell_version()},
                 {"compositor", packages::compositor()},
                 {"plugin", {{"id", m.id}, {"version", m.version}, {"dir", m.directory.string()}}},
                 {"modules", modules},
                 {"settings", settings},
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
  process.modules.clear();
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
  process.modules.clear();
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

bool handle(Process& process, const std::string& line) {
  const json message = json::parse(line, nullptr, false);
  if (!message.is_object()) {
    log(process, "sent a line that isn't a JSON object");
    return false;
  }
  const std::string type = message.value("type", "");
  if (type == "module") {
    const std::string id = message.value("id", "");
    const bool declared = std::ranges::any_of(process.manifest.bar_modules, [&](const auto& module) { return module.id == id; });
    if (!declared) {
      log(process, std::format("sent module \"{}\", which its manifest doesn't declare", id));
      return false;
    }
    Module& module = process.modules[id];
    const Module before = module;
    // Fields that are present update; the rest keep their value.
    if (message.contains("text") && message["text"].is_string()) module.text = message["text"].get<std::string>().substr(0, 256);
    if (message.contains("icon") && message["icon"].is_string()) module.icon = message["icon"];
    if (message.contains("tone") && message["tone"].is_string()) module.tone = message["tone"];
    if (message.contains("visible") && message["visible"].is_boolean()) module.visible = message["visible"];
    if (message.contains("progress")) {
      if (message["progress"].is_number()) module.progress = std::clamp(message["progress"].get<float>(), 0.0F, 1.0F);
      else module.progress.reset();
    }
    return module.text != before.text || module.icon != before.icon || module.tone != before.tone ||
           module.visible != before.visible || module.progress != before.progress;
  }
  if (type == "shell") {
    if (!process.manifest.has_permission("shell")) {
      log(process, "asked for a shell command without the \"shell\" permission");
      return false;
    }
    std::vector<std::string> args;
    if (message.contains("command") && message["command"].is_array()) {
      for (const auto& word : message["command"])
        if (word.is_string()) args.push_back(word);
    } else if (message.contains("command") && message["command"].is_string()) {
      std::istringstream words(message["command"].get<std::string>());
      for (std::string word; words >> word;) args.push_back(word);
    }
    // A plugin can't drive the package manager or other plugins through this.
    if (args.empty() || args[0] == "plugin" || args[0] == "plugins") return false;
    bool changed = false;
    bool ok = true;
    const std::string reply = ipc::execute(args, ok, changed);
    if (!ok) log(process, reply);
    return changed;
  }
  if (type == "notify") {
    if (!process.manifest.has_permission("notifications")) {
      log(process, "sent a notification without the \"notifications\" permission");
      return false;
    }
    vw::spawn({"notify-send", "--app-name", process.manifest.name.empty() ? process.manifest.id : process.manifest.name,
               message.value("summary", process.manifest.name), message.value("body", "")});
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

}  // namespace

int event_fd() { return epoll(); }

bool drain() {
  reap();
  bool changed = false;
  epoll_event events[16];
  for (;;) {
    const int count = epoll_wait(epoll(), events, 16, 0);
    if (count <= 0) break;
    for (int index = 0; index < count; ++index) {
      auto* process = static_cast<Process*>(events[index].data.ptr);
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
  bool changed = false;
  for (auto it = processes.begin(); it != processes.end();) {
    if (listed(enabled, it->first)) {
      ++it;
      continue;
    }
    stop(*it->second);
    it = processes.erase(it);
    changed = true;
  }
  for (const std::string& id : enabled) {
    if (const auto it = processes.find(id); it != processes.end()) {
      // Settings edited in config.json reach the running plugin.
      Process& process = *it->second;
      const json settings = settings_for(process.manifest);
      if (settings.dump() != process.settings) {
        process.settings = settings.dump();
        send(process, {{"type", "settings"}, {"settings", settings}});
      }
      continue;
    }
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
    processes.emplace(id, std::move(process));
    changed = true;
  }
  (void)changed;
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

const Module* module(const std::string& bar_id) {
  const auto [plugin, id] = split(bar_id);
  const auto it = running().find(plugin);
  if (it == running().end()) return nullptr;
  const auto found = it->second->modules.find(id);
  return found == it->second->modules.end() ? nullptr : &found->second;
}

void click(const std::string& bar_id, const std::string& button) {
  const auto [plugin, id] = split(bar_id);
  if (const auto it = running().find(plugin); it != running().end())
    send(*it->second, {{"type", "event"}, {"module", id}, {"event", "click"}, {"button", button}});
}

void scroll(const std::string& bar_id, double delta) {
  const auto [plugin, id] = split(bar_id);
  if (const auto it = running().find(plugin); it != running().end())
    send(*it->second, {{"type", "event"}, {"module", id}, {"event", "scroll"}, {"delta", delta}});
}

std::string call(const std::string& id, const std::string& command, const std::vector<std::string>& args, bool& ok) {
  const auto it = running().find(id);
  ok = false;
  if (it == running().end()) return std::format("plugin {} isn't running", id);
  Process& process = *it->second;
  if (std::ranges::find(process.manifest.commands, command) == process.manifest.commands.end())
    return std::format("plugin {} has no command {}", id, command);
  send(process, {{"type", "command"}, {"name", command}, {"args", args}});
  ok = true;
  return "ok";
}

std::string restart(const std::string& id, bool& ok) {
  const auto it = running().find(id);
  ok = false;
  if (it == running().end()) return std::format("plugin {} isn't enabled", id);
  Process& process = *it->second;
  stop(process);
  process.stopping = false;
  process.starts.clear();
  // Pick up an update installed since it started.
  if (auto manifest = packages::find(id, packages::Kind::plugin); manifest && manifest->valid()) process.manifest = std::move(*manifest);
  ok = start(process);
  return ok ? "ok" : std::format("plugin {} didn't start", id);
}

std::string status() {
  std::string text;
  for (const auto& [id, process] : running())
    text += std::format("{:<36} {:<10} {}{}\n", id, process->manifest.version, process->state,
                        process->pid > 0 ? std::format(" (pid {})", process->pid) : "");
  return text.empty() ? "no plugins enabled\n" : text;
}

}  // namespace blx::plugins
