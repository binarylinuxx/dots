#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "style.hpp"

// What the shell is showing. Only touched on the event-loop thread.
namespace blx {

using Clock = std::chrono::steady_clock;

// A hover-opened popout: open while its trigger or the popout itself is
// hovered, or while pinned by a click; closes 180 ms after the pointer
// leaves both, like ~/shell's close timers.
struct HoverPopout {
  std::string kind;      // which trigger opened it; empty when closed
  std::string pinned;    // kind pinned by a click
  float anchor = 0;      // trigger centre x, for placing the popout
  bool trigger_hovered = false;
  bool popout_hovered = false;
  Clock::time_point left{};

  void enter(const std::string& which, float x) {
    kind = which;
    anchor = x;
    trigger_hovered = true;
  }
  void leave() {
    trigger_hovered = false;
    left = Clock::now();
  }
  void leave_popout() {
    popout_hovered = false;
    left = Clock::now();
  }
  // The kind on show this frame; also expires a stale hover.
  const std::string& shown() {
    if (!pinned.empty()) return pinned;
    if (!kind.empty() && !trigger_hovered && !popout_hovered &&
        Clock::now() - left > std::chrono::milliseconds(props::hover_linger_ms))
      kind.clear();
    return kind;
  }
  // Still deciding whether to close: keep frames coming.
  bool pending() const { return !kind.empty() && !trigger_hovered && !popout_hovered; }
};

struct Shell {
  bool locked = false;
  bool launcher_open = false;
  std::string query;
  std::size_t selected = 0;

  bool dashboard_hovered = false;
  int dashboard_page = 0;
  int calendar_offset = 0;

  bool power_pinned = false;
  bool power_hovered = false;

  HoverPopout status;
  HoverPopout tray;

  // 0 hidden .. 1 out; follows the pointer while the sidebar is dragged.
  float sidebar_reveal = 0;
  bool sidebar_dragging = false;
  // The utility stack: its own value, so each section moves on its own.
  float stack_reveal = 0;
  bool stack_dragging = false;
  // Pulled from the bottom right: it grows out of the corner, not along x.
  bool stack_from_corner = false;
  bool quick_share_open = false;

  bool audio_hovered = false;  // over the open OSD: keeps it open
  Clock::time_point audio_until{};
  // Tugged out of the edge: follows the pointer, then stays until this time.
  float audio_tug = 0;
  float audio_tug_start = 0;
  float audio_shown = 0;
  bool audio_dragging = false;
  Clock::time_point audio_tug_until{};
};

inline Shell& shell() {
  static Shell instance;
  return instance;
}

}  // namespace blx
