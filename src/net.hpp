#pragma once

#include <string_view>
#include <vector>

#include <vw/vw.hpp>

// Networking for the shell: NetworkManager through vw::network where it runs,
// and wpa_supplicant directly on runit systems such as Runix, which have no
// NetworkManager (runix-network sets up wpa_supplicant, and dhcpcd gets the
// address once a network is joined). Same types and calls as vw::network.
namespace blx::net {

/// No NetworkManager, but runit and a wpa_supplicant control socket.
bool runix();

vw::network::Status status();
std::vector<vw::network::AccessPoint> access_points();
bool set_wifi_enabled(bool enabled);
bool scan();
bool connect(std::string_view ssid, std::string_view password = {});
bool disconnect_wifi();
int event_fd();
bool drain_events();

}  // namespace blx::net
