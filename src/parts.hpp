#pragma once

#include <vw/ui.hpp>
#include <vw/zwwm.hpp>

#include <array>
#include <cstdint>
#include <vector>

#include "blob.hpp"
#include "style.hpp"

// The pieces of the shell surface, after ~/shell's shell.qml. Each panel
// reports its current (animated) blob box, which joins the surface's blob
// group, then draws its contents on top.
namespace blx {

struct Screen {
  float width = 1920;
  float height = 1080;
};

struct Panel {
  blob::Box box;
  float reveal = 0;
  bool visible() const { return !box.empty(); }
};

// The vertical middle of the area above the bar.
inline float middle(const Screen& screen) { return (props::frame_thickness + screen.height - props::bar_height) / 2; }

// Bottom bar: OS icon and workspaces on the left, the active window centred,
// tray, clock, status pill and power button on the right.
void draw_bar(vw::Ui& ui, const Screen& screen);

// Plugins' desktop items on the wallpaper surface, and their popups; adds
// their input regions.
void draw_desktop(vw::Ui& ui, const Screen& screen, std::vector<std::array<std::int32_t, 4>>& input);

Panel launcher_panel(const Screen& screen);
void draw_launcher(vw::Ui& ui, const Panel& panel);
// Opens the launcher on its wallpaper carousel.
void open_wallpapers();

// The lock screen: drawn over everything, once per output, while locked.
void lock_session();
void lock_surface(vw::Ui& ui);

// Top-centre dashboard, opened by hovering the top frame edge.
Panel dashboard_panel(const Screen& screen);
// The hover region that opens and keeps the dashboard open.
void dashboard_trigger(vw::Ui& ui, const Screen& screen, const Panel& panel);
void draw_dashboard(vw::Ui& ui, const Panel& panel);

Panel notifications_panel(const Screen& screen);
void draw_notifications(vw::Ui& ui, const Panel& panel);

Panel power_panel(const Screen& screen);
void draw_power(vw::Ui& ui, const Panel& panel);

Panel status_panel(const Screen& screen);
void draw_status(vw::Ui& ui, const Panel& panel);

Panel tray_panel(const Screen& screen);
void draw_tray_actions(vw::Ui& ui, const Panel& panel);

// `edge`: the left edge of the sidebar when it is out, else the screen's width.
Panel audio_panel(const Screen& screen, const Panel& power, float edge);
// The edge strip the OSD is tugged out of.
blob::Box audio_tug_area(const Screen& screen, float edge);
void audio_trigger(vw::Ui& ui, const Screen& screen, const Panel& panel, float edge);
void draw_audio(vw::Ui& ui, const Panel& panel);

// Right-hand sidebar (notification centre) and the utility stack under it
// (quick toggles, screen recorder, keep awake), dragged out of the top
// right corner; plus the recorder's mode menu.
struct Sidebar {
  Panel centre;
  Panel stack;
  Panel menu;
};
Sidebar sidebar_panels(const Screen& screen);
// The 64 px corners you drag out of: top right for the whole sidebar, bottom
// right for the utility stack alone.
void sidebar_tug(vw::Ui& ui, const Screen& screen);
// Touch swipes from zwwm: in from the right edge pulls the sidebar out under
// the finger, up from the bottom edge opens the launcher. True on a change.
bool apply_edge_swipes(const std::vector<vw::zwwm::EdgeSwipe>& swipes);
void draw_sidebar(vw::Ui& ui, const Screen& screen, const Sidebar& sidebar);

// The Quick Share panel on the right edge.
Panel share_panel(const Screen& screen);
void draw_share(vw::Ui& ui, const Panel& panel);

// The media island under the top edge (hidden while the dashboard is out).
Panel island_panel(const Screen& screen, bool suppressed);
void draw_island(vw::Ui& ui, const Panel& panel);

// Hover tracking over a panel: a passive hit area under its contents.
void track_hover(vw::Ui& ui, const blob::Box& box, vw::HoverHandler handler);

}  // namespace blx
