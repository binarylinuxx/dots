#pragma once

#include <string_view>

namespace blx {

// `blxshell settings [page]`: a window that edits config.json. The running
// shell watches that file and applies every change live. `page` opens on a
// page by its title ("plugins", "bar", ...). A second run replaces the
// open window.
int run_settings(std::string_view page = {});

}  // namespace blx
