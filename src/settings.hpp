#pragma once

namespace blx {

// `blxshell settings`: a window that edits config.json. The running shell
// watches that file and applies every change live. A second run just exits.
int run_settings();

}  // namespace blx
