// Notification popups, after ~/shell's blxshell/notifications: one card per
// app (its newest notification, with a count badge), stacked under the top
// right corner. The stack fans out while hovered; cards swipe away sideways.
#include <vw/vw.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "notify.hpp"
#include "parts.hpp"
#include "state.hpp"

namespace blx {
namespace {

float panel_width = 390;
constexpr float card_height = 76;
constexpr float collapsed_spacing = -68;
constexpr float expanded_spacing = 8;
constexpr float body_line = 14;

struct CardState {
  bool expanded = false;
  float swipe = 0;
  bool dragging = false;
};

std::map<std::string, CardState>& cards() {
  static std::map<std::string, CardState> instance;
  return instance;
}

bool& stack_hovered() {
  static bool hovered = false;
  return hovered;
}

int body_lines(const std::string& body, float width) {
  if (body.empty()) return 0;
  const float total = draw::measure(body, 11, vw::weight::regular, font::display);
  return std::max(1, static_cast<int>(std::ceil(total / std::max(width, 1.0F))));
}

float text_width() { return panel_width - 10 - 24 - 54 - 10 - 8 - 24; }

float card_target_height(const notify::Group& group) {
  const CardState& state = cards()[group.key];
  if (!stack_hovered() || !state.expanded) return card_height;
  const auto& newest = group.entries.front();
  const float body = static_cast<float>(body_lines(notify::plain(newest.body), text_width())) * body_line;
  return std::max(card_height, 16.0F + 20.0F + body + 24);
}

std::vector<notify::Group> visible_groups() {
  panel_width = static_cast<float>(std::clamp(config().notification_width, 280, 640));
  if (shell().sidebar_reveal > 0 || !config().notification_popups) return {};
  auto groups = notify::popups();
  const auto maximum = static_cast<std::size_t>(std::clamp(config().notification_max, 1, 12));
  if (groups.size() > maximum) groups.resize(maximum);
  return groups;
}

}  // namespace

Panel notifications_panel(const Screen& screen) {
  const auto groups = visible_groups();
  float content = 0;
  const float spacing = stack_hovered() ? expanded_spacing : collapsed_spacing;
  for (std::size_t index = 0; index < groups.size(); ++index) {
    const float height = static_cast<float>(vw::animate(
        "notification.height." + groups[index].key, card_target_height(groups[index]),
        {.duration = 0.18, .easing = vw::Easing::out_cubic}));
    content += height + (index > 0 ? spacing : 0);
  }
  const float target = groups.empty() ? 0 : content + 20;
  const float height = static_cast<float>(vw::animate("notifications.height", target, motion::bounce()));
  if (height < 1) {
    stack_hovered() = false;
    return {};
  }
  Panel panel;
  panel.reveal = target > 0 ? std::min(1.0F, height / target) : 0;
  blob::Radii radii = blob::Radii::all(props::island_radius);
  const bool left = config().notification_side == "left";
  (left ? radii.bottom_left : radii.bottom_right) = 0;
  panel.box = {left ? props::frame_thickness - 2 : screen.width - props::frame_thickness - panel_width + 2, props::frame_thickness - 10,
               panel_width, height, radii};
  return panel;
}

void draw_notifications(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible()) return;
  track_hover(ui, panel.box, [](bool inside) { stack_hovered() = inside; });
  const auto groups = visible_groups();
  if (groups.empty()) return;
  const Colours& c = colours();
  const bool expanded = stack_hovered();
  const float spacing = static_cast<float>(vw::animate(
      "notifications.spacing", expanded ? expanded_spacing : collapsed_spacing,
      {.duration = 0.28, .easing = vw::Easing::out_cubic}));
  const float stack_x = panel.box.x + 10;
  const float stack_width = panel.box.width - 10;
  const float clip_top = panel.box.y + 10;
  const float clip_bottom = panel.box.y + panel.box.height - 10;

  // Positions first; cards are drawn last-to-first so the newest is on top.
  struct Placed {
    const notify::Group* group;
    float y;
    float height;
    std::size_t index;
  };
  std::vector<Placed> placed;
  float y = clip_top;
  for (std::size_t index = 0; index < groups.size(); ++index) {
    const float height = static_cast<float>(vw::animate(
        "notification.height." + groups[index].key, card_target_height(groups[index]),
        {.duration = 0.18, .easing = vw::Easing::out_cubic}));
    placed.push_back({&groups[index], y, height, index});
    y += height + spacing;
  }

  for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
    const notify::Group& group = *it->group;
    const auto& newest = group.entries.front();
    CardState& state = cards()[group.key];
    const float index = static_cast<float>(it->index);
    const float width = static_cast<float>(vw::animate(
        "notification.width." + group.key, stack_width - (expanded ? 0 : index * 6),
        {.duration = 0.28, .easing = vw::Easing::out_cubic}));
    // Slide in from the right edge the first time a group appears.
    const float entry = static_cast<float>(vw::animate(
        "notification.entry." + group.key, 0,
        {.duration = 0.24, .initial = static_cast<double>(stack_width),
         .easing = vw::Easing::out_cubic}));
    const float swipe = state.dragging
                            ? state.swipe
                            : static_cast<float>(vw::animate("notification.swipe." + group.key,
                                                             state.swipe, {.duration = 0.18}));
    const float x = stack_x + entry + swipe + (expanded ? 0 : index * 3);
    if (it->y + it->height > clip_bottom + 1 || x >= panel.box.x + panel.box.width) continue;
    const float visible_width = std::min(width, panel.box.x + panel.box.width - x);
    if (visible_width < 8 || x < 0) continue;
    const bool critical = newest.urgency > 1;
    const vw::Clip clip{.x = panel.box.x, .y = clip_top, .width = panel.box.width,
                        .height = std::max(1.0F, clip_bottom - clip_top)};

    ui.rectangle({.width = visible_width, .height = it->height,
                  .radius = props::notification_radius,
                  .color = critical ? c.error_container : c.surface_container,
                  .place = {.x = x, .y = it->y, .clip = clip},
                  .input = {.on_click = [key = group.key, id = newest.id](const vw::ClickEvent&) {
                              // The default action ("default", else the only one).
                              if (std::abs(cards()[key].swipe) > 4) return;
                              vw::notifications::activate(id);
                            },
                            .on_drag_start = [key = group.key](const vw::DragEvent&) {
                              cards()[key].dragging = true;
                            },
                            .on_drag = [key = group.key](const vw::DragEvent& drag) {
                              cards()[key].swipe = static_cast<float>(drag.x - drag.start_x);
                            },
                            .on_drag_end = [key = group.key, width = visible_width,
                                            ids = group.ids()](const vw::DragEvent&) {
                              CardState& card = cards()[key];
                              card.dragging = false;
                              if (std::abs(card.swipe) > width * 0.32F) {
                                card.swipe = card.swipe < 0 ? -panel_width : panel_width;
                                for (auto id : ids) vw::notifications::dismiss(id);
                              } else {
                                card.swipe = 0;
                              }
                            }}});

    const float inner_x = x + 12;
    const float centre_y = it->y + it->height / 2;
    // The notification's image filling a disc, else the app's icon on it, else
    // a bell, with a count badge.
    if (inner_x + 54 < panel.box.x + panel.box.width) {
      draw::rect(ui, inner_x, centre_y - 27, 54, 54, 27, critical ? c.error : c.primary_container);
      if (!newest.image.empty() && config().notification_images)
        draw::icon(ui, newest.image, "", inner_x, centre_y - 27, 54, std::nullopt, 27, vw::ImageFit::cover);
      else if (!newest.icon.empty())
        draw::icon(ui, newest.icon, "", inner_x + 8, centre_y - 19, 38);
      else
        draw::glyph(ui, critical ? "notification_important" : "notifications", inner_x + 27, centre_y, 34,
                    critical ? c.error_text : c.primary);
      if (group.entries.size() > 1) {
        draw::rect(ui, inner_x + 36, centre_y - 27, 18, 18, 9, c.primary);
        draw::text_centered(ui, std::to_string(group.entries.size()), inner_x + 45, centre_y - 18,
                            {.size = 9, .color = c.primary_text, .weight = vw::weight::black,
                             .family = font::display});
      }
    }

    const float text_x = inner_x + 54 + 10;
    const float text_max = std::max(10.0F, visible_width - 12 - 54 - 10 - 8 - 24);
    const std::string body = notify::plain(newest.body);
    const int lines = body_lines(body, text_width());
    const bool open = expanded && state.expanded;
    const int shown_lines = body.empty() ? 0 : open ? lines : std::min(lines, 2);
    const float text_height = 13 + 2 + 18 + (shown_lines > 0 ? 2 + body_line * shown_lines : 0);
    float text_y = centre_y - text_height / 2;
    const vw::Color muted = critical ? with_alpha(c.error_container_text, 0.8F) : c.text_muted;
    draw::text(ui, newest.app_name, text_x, text_y,
               {.size = 9, .color = muted, .weight = vw::weight::semibold, .family = font::display,
                .max_width = text_max});
    text_y += 15;
    draw::text(ui, newest.summary, text_x, text_y,
               {.size = 14, .color = critical ? c.error_container_text : c.text,
                .weight = vw::weight::black, .family = font::display, .max_width = text_max});
    text_y += 20;
    if (shown_lines > 0)
      draw::text(ui, body, text_x, text_y,
                 {.size = 11, .color = muted, .family = font::display, .max_width = text_max,
                  .max_lines = shown_lines});

    if (lines > 2 || state.expanded) {
      const float bx = x + visible_width - 12 - 24;
      const float by = it->y + it->height - 12 - 24;
      if (bx > text_x) {
        ui.rectangle({.width = 20, .height = 20, .radius = 10, .color = c.primary_container,
                      .place = {.x = bx + 2, .y = by + 2},
                      .input = {.on_click = [key = group.key](const vw::ClickEvent&) {
                                  cards()[key].expanded = !cards()[key].expanded;
                                },
                                .hover = vw::Hover{.color = c.surface_container_high,
                                                   .duration = 0.1F}}});
        draw::glyph(ui, open ? "expand_less" : "expand_more", bx + 12, by + 12, 17, c.primary);
      }
    }
  }
}

}  // namespace blx
