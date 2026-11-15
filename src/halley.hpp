#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The Halley compositor's IPC, ported from ~/shell/halley (HalleyManager):
// length-prefixed binary frames on /run/user/$UID/halley/halley.sock, one
// request per connection. A worker polls nodes every 500 ms and the whole
// state every 2 s, waking services_event_fd() on changes.
namespace blx::halley {

struct Node {
  std::uint64_t id = 0;
  std::string title;
  std::string app;
  std::string output;
  bool visible = false;
  bool focused = false;
};

struct Cluster {
  std::uint64_t id = 0;
  std::optional<int> slot;
  std::string name;
  std::string output;
  bool active = false;
  bool focused = false;
  std::vector<Node> members;  // from the per-cluster inspect
};

bool available();
std::vector<Cluster> clusters();
std::optional<Node> active_node();

void focus_cluster_slot(int slot);
void focus_node(std::uint64_t id);

}  // namespace blx::halley
