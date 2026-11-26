#pragma once

// GTK 3/4 and Qt apps in the shell's colours and Material 3 shapes
// (Settings → Theme → Apps). Turning it off restores what was there.
namespace blx::apptheme {

// Call at startup and after colour or config changes; the shell's process only.
void apply();

}  // namespace blx::apptheme
