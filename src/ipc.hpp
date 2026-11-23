#pragma once

// blxshell's command socket, $XDG_RUNTIME_DIR/blxshell.sock. A client writes
// one line of words ("launcher toggle"), closes its write side and reads one
// reply: "ok", "error: …", or the text a command asks for.
//
//   blxshell launcher            # the CLI is a client of the socket
//   echo lock | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/blxshell.sock
#include <string>
#include <vector>

namespace blx::ipc {

// Binds the socket (replacing a stale one) and returns its listening,
// non-blocking fd, or -1 when it can't; for vw::App::on_event.
int listen();
// Answers every waiting client; true when a command changed the shell.
bool drain(int fd);
// Removes the socket file on exit.
void close(int fd);

// Runs one command inside the shell, as if it came over the socket (for
// plugins with the "shell" permission). Returns the reply text.
std::string execute(const std::vector<std::string>& args, bool& ok, bool& changed);

// The CLI side: sends argv[1..] to the running shell and prints its reply.
int send(int argc, char** argv);
// The command list, for `blxshell help`.
const char* usage();

}  // namespace blx::ipc
