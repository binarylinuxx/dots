// The Quick Share panel, ported from
// ~/shell/blxshell/components/QuickSharePanel.qml: nearby LocalSend devices,
// a send button, outgoing transfers and incoming requests to accept.
#include <vw/vw.hpp>

#include <algorithm>
#include <format>
#include <string>

#include "draw.hpp"
#include "parts.hpp"
#include "quickshare.hpp"
#include "state.hpp"

namespace blx {
namespace {

constexpr float panel_width = 360;
constexpr float panel_height = 470;

std::string& selected_device() {
  static std::string id;
  return id;
}

}  // namespace

Panel share_panel(const Screen& screen) {
  const bool open = shell().quick_share_open;
  const float reveal = static_cast<float>(vw::animate("share.reveal", open ? 1 : 0, motion::quick()));
  const float slide = static_cast<float>(vw::animate("share.slide", open ? 0 : 36, motion::quick()));
  if (reveal < 0.002F) return {};
  const float x = screen.width - props::frame_thickness - panel_width + props::popout_frame_overlap + slide;
  Panel panel;
  panel.reveal = reveal;
  panel.box = {x + panel_width * (1 - reveal), middle(screen) - panel_height / 2, panel_width * reveal,
               panel_height * reveal, blob::Radii::all(props::island_radius)};
  return panel;
}

void draw_share(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible() || panel.reveal < 0.95F) return;
  const Colours& c = colours();
  const float x = panel.box.x + panel.box.width - panel_width;
  const float y = panel.box.y;
  const bool running = quickshare::running();
  const auto devices = quickshare::devices();
  std::string& selected = selected_device();
  if (!selected.empty() && std::ranges::none_of(devices, [&](const auto& d) { return d.id == selected; })) selected.clear();

  // Header.
  draw::text(ui, "QUICK SHARE", x + 18, y + 16,
             {.size = 15, .color = c.text, .weight = vw::weight::black, .family = font::display});
  const std::string error = quickshare::error();
  draw::text(ui, running ? std::format("{} nearby LocalSend device{}", devices.size(), devices.size() == 1 ? "" : "s")
                         : (error.empty() ? "Starting LocalSend" : error),
             x + 18, y + 38, {.size = 10, .color = running ? c.text_muted : c.error, .family = font::display,
                              .max_width = 240});
  const float title_centre = y + 16 + 10;
  draw::glyph(ui, "close", x + panel_width - 16 - 10, title_centre, 20, c.text_muted);
  draw::state_layer(ui, x + panel_width - 16 - 22, title_centre - 12, 24, 24, 12, c.text,
                    {.on_click = [](const vw::ClickEvent&) { shell().quick_share_open = false; }});
  draw::glyph(ui, "refresh", x + panel_width - 46 - 10, title_centre, 20, c.primary);
  draw::state_layer(ui, x + panel_width - 46 - 22, title_centre - 12, 24, 24, 12, c.primary,
                    {.on_click = [](const vw::ClickEvent&) { quickshare::refresh(); }});
  draw::rect(ui, x + panel_width - 78 - 8, title_centre - 4, 8, 8, 4, running ? c.primary : c.error);
  const float divider_y = y + 62;
  draw::rect(ui, x + 18, divider_y, panel_width - 36, 1, 0.5F, with_alpha(c.surface_container_high, 0.72F));
  draw::text(ui, "NEARBY", x + 18, divider_y + 13, {.size = 9, .color = c.text_muted, .weight = vw::weight::black,
                                                    .family = font::display});

  // Devices.
  const float list_x = x + 12;
  const float list_y = divider_y + 32;
  const float list_width = panel_width - 24;
  if (devices.empty() && running) {
    draw::text(ui, "No nearby devices yet\nTap refresh after opening LocalSend on your phone", list_x, list_y + 46,
               {.size = 10, .color = c.text_muted, .family = font::display, .max_width = list_width, .max_lines = 2,
                .align = "center"});
  } else {
    ui.listview({.id = "share-devices", .width = list_width, .height = 126, .item_height = 48, .gap = 5,
                 .count = devices.size(),
                 .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                   const auto& device = devices[index];
                   return draw::capture(ui, list_width, 48, [&] {
                     ui.rectangle({.width = list_width, .height = 48, .radius = 8,
                                   .color = selected == device.id ? c.primary_container : c.surface_container,
                                   .input = {.on_click = [id = device.id](const vw::ClickEvent&) { selected_device() = id; }}});
                     draw::glyph(ui, device.type == "mobile" ? "smartphone" : "desktop_windows", 12 + 10.5F, 24, 21, c.primary);
                     draw::text(ui, device.alias, 43, 9, {.size = 12, .color = c.text, .weight = vw::weight::semibold,
                                                          .family = font::display, .max_width = list_width - 55});
                     draw::text(ui, device.model.empty() ? device.address : device.model, 43, 27,
                                {.size = 9, .color = c.text_muted, .family = font::display, .max_width = list_width - 55});
                   });
                 },
                 .place = {.x = list_x, .y = list_y}});
  }

  // Send.
  const float send_y = list_y + 126 + 12;
  const bool can_send = !selected.empty();
  const float alpha = can_send ? 1 : 0.45F;
  ui.rectangle({.width = list_width, .height = 40, .radius = props::popout_radius,
                .color = with_alpha(can_send ? c.primary : c.surface_variant, alpha), .place = {.x = list_x, .y = send_y},
                .input = {.on_click = [can_send](const vw::ClickEvent&) {
                  if (can_send) quickshare::pick_and_send(selected_device());
                }}});
  draw::glyph(ui, "upload", list_x + 14 + 9, send_y + 20, 18, with_alpha(c.primary_text, alpha));
  draw::text_centered(ui, "SEND FILES", list_x + list_width / 2, send_y + 20,
                      {.size = 11, .color = with_alpha(c.primary_text, alpha), .weight = vw::weight::black,
                       .family = font::display});

  // Outgoing.
  const float outgoing_y = list_y + 126 + 66;
  draw::text(ui, "OUTGOING", x + 18, outgoing_y, {.size = 9, .color = c.text_muted, .weight = vw::weight::semibold,
                                                  .family = font::display});
  const auto transfers = quickshare::transfers();
  ui.listview({.id = "share-transfers", .width = list_width, .height = 60, .item_height = 30, .gap = 4,
               .count = transfers.size(),
               .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                 const auto& transfer = transfers[transfers.size() - 1 - index];  // newest first
                 return draw::capture(ui, list_width, 30, [&] {
                   draw::rect(ui, 0, 0, list_width, 30, 8, c.surface_container);
                   draw::text_middle(ui, transfer.name, 9, 15, {.size = 10, .color = c.text, .family = font::display,
                                                                .max_width = list_width - 88});
                   std::string state = transfer.state;
                   std::ranges::transform(state, state.begin(), [](unsigned char ch) { return std::toupper(ch); });
                   const std::string label = transfer.state == "uploading"
                                                 ? std::format("{}%", std::lround(transfer.progress * 100)) : state;
                   const float label_width = draw::measure(label, 9, vw::weight::semibold, font::display);
                   draw::text_middle(ui, label, list_width - 9 - label_width, 15,
                                     {.size = 9, .color = transfer.state == "error" ? c.error : c.primary,
                                      .weight = vw::weight::semibold, .family = font::display});
                 });
               },
               .place = {.x = list_x, .y = outgoing_y + 18}});

  // Incoming requests at the bottom.
  const auto requests = quickshare::incoming();
  ui.listview({.id = "share-incoming", .width = list_width, .height = 88, .item_height = 84, .gap = 4,
               .count = requests.size(),
               .render = [&](std::size_t index) -> std::optional<vw::Widget> {
                 const auto& request = requests[index];
                 return draw::capture(ui, list_width, 84, [&] {
                   draw::rect(ui, 0, 0, list_width, 84, 8, c.surface_container_high);
                   draw::text(ui, "INCOMING  " + request.sender, 10, 9,
                              {.size = 10, .color = c.text, .weight = vw::weight::black, .family = font::display,
                               .max_width = list_width - 20});
                   draw::text(ui, std::format("{} file{} ready to receive", request.files, request.files == 1 ? "" : "s"),
                              10, 28, {.size = 9, .color = c.text_muted, .family = font::display});
                   const auto action = [&](const char* name, float ax, vw::Color colour, bool accept) {
                     const float width = draw::measure(name, 9, vw::weight::black, font::display);
                     draw::text(ui, name, ax, 84 - 9 - 12, {.size = 9, .color = colour, .weight = vw::weight::black,
                                                            .family = font::display});
                     ui.hit_area({.width = width, .height = 16, .place = {.x = ax, .y = 84 - 9 - 14},
                                  .input = {.on_click = [id = request.id, accept](const vw::ClickEvent&) {
                                    if (accept) quickshare::approve(id);
                                    else quickshare::reject(id);
                                  }}});
                   };
                   action("REJECT", 10, c.error, false);
                   action("ACCEPT", list_width - 10 - draw::measure("ACCEPT", 9, vw::weight::black, font::display),
                          c.primary, true);
                 });
               },
               .place = {.x = list_x, .y = y + panel_height - 12 - 88}});
}

}  // namespace blx
