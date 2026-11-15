#include "ipc.hpp"

#include <vw/ipc.hpp>
#include <vw/mpris.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "config.hpp"
#include "parts.hpp"
#include "state.hpp"
#include "style.hpp"

namespace blx::ipc {
namespace {

std::string socket_path() {
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  return (std::filesystem::path(runtime != nullptr ? runtime : "/tmp") / "blxshell.sock").string();
}

std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> result;
  std::istringstream stream{std::string(line)};
  for (std::string word; stream >> word;) result.push_back(std::move(word));
  return result;
}

// Flips toward `how`: "toggle", "open" or "close". False on anything else.
bool apply(bool& value, std::string_view how) {
  if (how == "toggle") value = !value;
  else if (how == "open") value = true;
  else if (how == "close") value = false;
  else return false;
  return true;
}

struct Reply {
  std::string text;
  bool changed = false;
};

Reply ok() { return {"ok\n", true}; }
Reply error(std::string message) { return {"error: " + std::move(message) + '\n', false}; }

Reply run(std::vector<std::string> args) {
  // Quickshell's form, `ipc call <target> <function>`, works too.
  if (args.size() >= 2 && args[0] == "ipc" && args[1] == "call") args.erase(args.begin(), args.begin() + 2);
  if (args.empty()) return error("no command; try `blxshell help`");
  const std::string& command = args[0];
  const std::string_view arg = args.size() > 1 ? std::string_view(args[1]) : std::string_view();
  Shell& shell = blx::shell();

  if (command == "ping") return {"ok\n", false};
  if (command == "help") return {usage(), false};

  if (command == "launcher") {
    if (!apply(shell.launcher_open, arg.empty() ? "toggle" : arg)) return error("launcher [toggle|open|close]");
    return ok();
  }
  if (command == "powermenu") {
    if (!apply(shell.power_pinned, arg.empty() ? "toggle" : arg)) return error("powermenu [toggle|open|close]");
    return ok();
  }
  if (command == "sidebar") {
    const auto flip = [](float reveal) { return reveal > 0.5F ? 0.0F : 1.0F; };
    const std::string_view part = arg.empty() ? "both" : arg;
    if (part == "both") {
      const float open = flip(std::max(shell.sidebar_reveal, shell.stack_reveal));
      shell.stack_from_corner = false;
      shell.sidebar_reveal = open;
      shell.stack_reveal = open;
    } else if (part == "top") {
      shell.sidebar_reveal = flip(shell.sidebar_reveal);
    } else if (part == "bottom" || part == "corner") {
      shell.stack_from_corner = part == "corner";
      shell.stack_reveal = flip(shell.stack_reveal);
    } else {
      return error("sidebar [both|top|bottom|corner]");
    }
    return ok();
  }
  if (command == "wallpapers") {
    open_wallpapers();
    return ok();
  }
  if (command == "lock" || (command == "lockscreen" && (arg.empty() || arg == "lock"))) {
    lock_session();
    return ok();
  }
  if (command == "reload" || ((command == "shell" || command == "widgets") && arg == "reload")) {
    reload_config();
    sync_style();
    reload_colours();
    return ok();
  }
  if (command == "media" || command == "mpris") {
    const auto player = vw::mpris::active();
    bool done = false;
    if (arg == "play-pause") done = vw::mpris::play_pause();
    else if (arg == "next") done = vw::mpris::next();
    else if (arg == "previous") done = vw::mpris::previous();
    else if (arg == "pause" || arg == "pauseAll")
      done = !player || player->playback_status != "Playing" || vw::mpris::play_pause();
    else return error("media [play-pause|pause|next|previous]");
    return done ? Reply{"ok\n", false} : error("no player can do that");
  }
  return error("unknown command `" + command + "`; try `blxshell help`");
}

}  // namespace

const char* usage() {
  return "usage: blxshell [command]\n"
         "\n"
         "  (none)                            run the shell\n"
         "  settings                          open the settings app\n"
         "\n"
         "Sent to the running shell:\n"
         "  launcher [toggle|open|close]\n"
         "  powermenu [toggle|open|close]\n"
         "  sidebar [both|top|bottom|corner]  bottom: along x; corner: out of the corner\n"
         "  wallpapers                        open the wallpaper picker\n"
         "  lock                              lock the session\n"
         "  reload                            re-read config and colours\n"
         "  media play-pause|pause|next|previous\n"
         "  ping                              ok when a shell is running\n"
         "\n"
         "Quickshell's `ipc call <target> <function>` form is accepted too\n"
         "(lockscreen lock, powermenu toggle, shell reload, mpris pauseAll).\n";
}

int listen() {
  const std::string path = socket_path();
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) return -1;
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

  vw::UniqueFd fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
  if (!fd) return -1;
  ::unlink(path.c_str());
  if (::bind(fd.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
      ::listen(fd.get(), 8) != 0) {
    std::cerr << "blxshell: can't listen on " << path << ": " << std::strerror(errno) << '\n';
    return -1;
  }
  return fd.release();
}

bool drain(int fd) {
  bool changed = false;
  for (;;) {
    vw::UniqueFd client(::accept4(fd, nullptr, nullptr, SOCK_CLOEXEC));
    if (!client) break;
    // A client that stalls can't hold the shell up for long.
    const timeval timeout{.tv_sec = 0, .tv_usec = 200'000};
    ::setsockopt(client.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(client.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    std::string request;
    char buffer[512];
    while (request.size() < 4096 && request.find('\n') == std::string::npos) {
      const ssize_t read = ::read(client.get(), buffer, sizeof(buffer));
      if (read <= 0) break;
      request.append(buffer, static_cast<std::size_t>(read));
    }
    request = request.substr(0, request.find('\n'));

    Reply reply = run(words(request));
    changed |= reply.changed;
    std::string_view rest = reply.text;
    while (!rest.empty()) {
      const ssize_t sent = ::send(client.get(), rest.data(), rest.size(), MSG_NOSIGNAL);
      if (sent <= 0) break;
      rest.remove_prefix(static_cast<std::size_t>(sent));
    }
  }
  return changed;
}

void close(int fd) {
  if (fd < 0) return;
  ::close(fd);
  ::unlink(socket_path().c_str());
}

int send(int argc, char** argv) {
  std::string line;
  for (int index = 1; index < argc; ++index) {
    if (index > 1) line += ' ';
    line += argv[index];
  }
  line += '\n';
  std::string reply;
  try {
    reply = vw::unix_request(socket_path(), line);
  } catch (const std::exception&) {
    std::cerr << "blxshell: no running shell\n";
    return EXIT_FAILURE;
  }
  const bool failed = reply.starts_with("error:");
  if (reply != "ok\n" || failed) (failed ? std::cerr : std::cout) << reply;
  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

}  // namespace blx::ipc
