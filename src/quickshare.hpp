#pragma once

#include <cstdint>
#include <string>
#include <vector>

// LocalSend v2, ported from ~/shell/quickshare (QuickShareManager): UDP
// multicast discovery on 224.0.0.167:53317, an HTTP server on the same port
// for incoming transfers (held for approval, saved to Downloads), and
// outgoing uploads. Runs on its own thread; services_event_fd() fires on
// every change.
namespace blx::quickshare {

struct Device {
  std::string id;  // fingerprint
  std::string alias;
  std::string address;
  std::string model;
  std::string type;  // "mobile", "desktop", ...
};

struct Transfer {
  std::string id;
  std::string name;
  std::string state;  // preparing, uploading, complete, error
  std::string error;
  double progress = 0;
};

struct Incoming {
  std::string id;
  std::string sender;
  std::string address;
  std::size_t files = 0;
  std::uint64_t bytes = 0;
};

// Starts on first use.
bool running();
std::string error();
std::vector<Device> devices();
std::vector<Transfer> transfers();
std::vector<Incoming> incoming();

void refresh();
void send_file(const std::string& device, const std::string& path);
// Opens a file chooser (zenity, else kdialog) and sends the picks.
void pick_and_send(const std::string& device);
void approve(const std::string& id);
void reject(const std::string& id);

}  // namespace blx::quickshare
