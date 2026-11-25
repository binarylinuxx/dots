// Desktop items from plugins: icons on the wallpaper, in a grid from the top
// left, each with a popup of the plugin's actions.
#include <vw/vw.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "draw.hpp"
#include "parts.hpp"
#include "plugins.hpp"
#include "shapes.hpp"
#include "style.hpp"

namespace blx {
namespace {

using Clock = std::chrono::steady_clock;

constexpr float cell_width = 96;
constexpr float cell_height = 102;
constexpr float icon_size = 48;
constexpr float margin = 16;
constexpr float popup_width = 236;
constexpr float popup_row = 40;
constexpr float popup_header = 44;
constexpr float popup_padding = 6;
constexpr auto double_click = std::chrono::milliseconds(450);

// The item whose popup is open, and where its cell is.
struct Open {
  std::string source;
  std::string item;
  float x = 0;
  float y = 0;
  std::uint64_t serial = 0;  // each opening fades in afresh
};

std::uint64_t next_serial() {
  static std::uint64_t serial = 0;
  return ++serial;
}

std::optional<Open>& open_popup() {
  static std::optional<Open> open;
  return open;
}

struct LastClick {
  std::string source;
  std::string item;
  Clock::time_point when{};
};

LastClick& last_click() {
  static LastClick click;
  return click;
}

// --- Positions: grid cells the user dragged items to ---------------------------

using Cell = std::pair<int, int>;  // column, row

std::filesystem::path positions_file() {
  const char* state = std::getenv("XDG_STATE_HOME");
  const char* home = std::getenv("HOME");
  const std::filesystem::path base = state != nullptr && *state ? std::filesystem::path(state)
                                                                 : std::filesystem::path(home != nullptr ? home : ".") / ".local/state";
  return base / "blxshell" / "desktop.json";
}

// "<source>\n<item id>" -> its cell.
std::map<std::string, Cell>& positions() {
  static std::map<std::string, Cell> saved = [] {
    std::map<std::string, Cell> result;
    std::ifstream input(positions_file());
    const nlohmann::json root = nlohmann::json::parse(input, nullptr, false);
    if (root.is_object() && root.contains("positions") && root["positions"].is_object())
      for (const auto& [key, value] : root["positions"].items())
        if (value.is_array() && value.size() == 2 && value[0].is_number_integer() && value[1].is_number_integer())
          result[key] = {value[0].get<int>(), value[1].get<int>()};
    return result;
  }();
  return saved;
}

void save_positions() {
  nlohmann::json cells = nlohmann::json::object();
  for (const auto& [key, cell] : positions()) cells[key] = {cell.first, cell.second};
  const auto file = positions_file();
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  {
    std::ofstream output(file.string() + ".tmp");
    output << nlohmann::json{{"positions", cells}}.dump(2) << '\n';
  }
  std::filesystem::rename(file.string() + ".tmp", file, error);
}

std::string key_of(const std::string& source, const std::string& id) { return source + '\n' + id; }

struct Placed {
  std::string source;
  const plugins::DesktopItem* item;
  Cell cell;
};

// Items where the user put them; the rest fill free cells top to bottom, then
// left to right. Items past the last cell aren't shown.
std::vector<Placed> layout(const std::vector<plugins::Desktop>& areas, int columns, int rows) {
  std::vector<Placed> placed;
  std::set<Cell> taken;
  std::vector<std::pair<std::string, const plugins::DesktopItem*>> rest;
  for (const auto& area : areas)
    for (const auto& item : area.items) {
      const auto saved = positions().find(key_of(area.source, item.id));
      if (saved != positions().end() && saved->second.first >= 0 && saved->second.first < columns &&
          saved->second.second >= 0 && saved->second.second < rows && !taken.contains(saved->second)) {
        taken.insert(saved->second);
        placed.push_back({area.source, &item, saved->second});
      } else {
        rest.emplace_back(area.source, &item);
      }
    }
  int next = 0;
  for (const auto& [source, item] : rest) {
    while (next < columns * rows && taken.contains({next / rows, next % rows})) ++next;
    if (next >= columns * rows) break;
    const Cell cell{next / rows, next % rows};
    taken.insert(cell);
    placed.push_back({source, item, cell});
  }
  return placed;
}

// The item being dragged: where it was grabbed and where the pointer is.
struct Drag {
  bool active = false;
  std::string source;
  std::string id;
  float grab_x = 0;  // pointer offset inside the cell
  float grab_y = 0;
  float x = 0;  // the cell's top left, following the pointer
  float y = 0;
  Clock::time_point ended{};
};

Drag& dragging() {
  static Drag drag;
  return drag;
}

// The cells drawn last frame, for a drop to rearrange.
std::vector<std::pair<std::string, Cell>>& last_layout() {
  static std::vector<std::pair<std::string, Cell>> cells;
  return cells;
}

std::array<std::int32_t, 4> region(float x, float y, float width, float height) {
  return {static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(y)),
          static_cast<std::int32_t>(std::ceil(width)), static_cast<std::int32_t>(std::ceil(height))};
}

// An app or file icon's tint: "colour" leans it toward the accent keeping its
// own shading; "themed" makes it one colour, the accent, like Android's
// themed icons. Photos (item.image) are never tinted.
vw::ImageEffect tint_effect(const std::string& tint) {
  const Colours& c = colours();
  if (tint == "themed") return {.saturation = 0, .colorize = c.primary, .colorize_amount = 1};
  if (tint == "colour") return {.colorize = c.primary, .colorize_amount = 0.55F};
  return {};
}

// The item's picture: its image, else the first icon that loads, else its
// glyph. With a shape the icon sits on it, inset like an adaptive icon, and
// a photo fills it; changing the shape morphs to the new one.
void draw_picture(vw::Ui& ui, const plugins::DesktopItem& item, float x, float y) {
  const Colours& c = colours();
  const bool shaped = !item.shape.empty();
  const vw::ImageEffect effect = tint_effect(item.tint);
  if (!item.image.empty()) {
    const bool drawn = shaped ? draw::icon(ui, item.image, "", x, y, icon_size, std::nullopt, 0, vw::ImageFit::cover, {},
                                           shapes::mask(item.shape))
                              : draw::icon(ui, item.image, "", x, y, icon_size, std::nullopt, 10, vw::ImageFit::cover);
    if (drawn) return;
  }
  float inner = icon_size;
  float inset = 0;
  if (shaped) {
    const vw::Color backdrop = item.tint == "themed" ? c.primary_container : c.surface_container_highest;
    shapes::draw(ui, "desktop." + item.id, item.shape, x, y, icon_size, backdrop);
    inner = std::round(icon_size * 0.68F);
    inset = (icon_size - inner) / 2;
  }
  for (const std::string& icon : item.icons) {
    const bool file = icon.starts_with('/');
    if (draw::icon(ui, file ? icon : "", file ? "" : icon, x + inset, y + inset, inner, std::nullopt, 0,
                   vw::ImageFit::contain, effect))
      return;
  }
  if (!shaped) draw::rect(ui, x, y, icon_size, icon_size, 14, c.primary_container);
  draw::glyph(ui, item.glyph.empty() ? "draft" : item.glyph, x + icon_size / 2, y + icon_size / 2, shaped ? 24 : 28,
              c.primary);
}

// The icon and its name in a cell at (x, y).
void draw_item(vw::Ui& ui, const plugins::DesktopItem& item, float x, float y, bool selected) {
  const Colours& c = colours();
  if (selected) draw::rect(ui, x + 4, y + 2, cell_width - 8, cell_height - 4, 18, with_alpha(c.primary_container, 0.75F));
  draw_picture(ui, item, x + (cell_width - icon_size) / 2, y + 10);
  // The name on a pill of the surface colour, readable on any wallpaper.
  const float text_width = std::min(draw::measure(item.label, 12), cell_width - 20);
  draw::rect(ui, x + (cell_width - text_width) / 2 - 7, y + 66, text_width + 14, 24, 12,
             with_alpha(c.surface, selected ? 0.9F : 0.62F));
  draw::text_centered(ui, item.label, x + cell_width / 2, y + 78,
                      {.size = 12, .color = c.text, .weight = vw::weight::medium, .max_width = cell_width - 20});
}

void draw_popup(vw::Ui& ui, const Screen& screen, const Open& open, const plugins::DesktopItem& item,
                std::vector<std::array<std::int32_t, 4>>& input) {
  const Colours& c = colours();
  const float height = popup_header + static_cast<float>(item.actions.size()) * popup_row + popup_padding * 2;
  // Beside the icon, on whichever side has room; never under the bar.
  float x = open.x + cell_width + 4;
  if (x + popup_width > screen.width - props::frame_thickness - 8) x = open.x - popup_width - 4;
  x = std::max(x, props::frame_thickness + 8);
  const float y = std::clamp(open.y, props::frame_thickness + 8,
                             std::max(props::frame_thickness + 8, screen.height - props::bar_height - 8 - height));
  vw::AnimationOptions fade = motion::quick();
  fade.initial = 0;
  const float reveal = std::clamp(static_cast<float>(vw::animate("desktop.popup." + std::to_string(open.serial), 1, fade)), 0.0F, 1.0F);
  ui.rectangle({.width = popup_width, .height = height, .radius = 18, .color = with_alpha(c.surface_container_high, reveal),
                .border_width = 1, .border_color = with_alpha(c.outline_variant, reveal), .place = {.x = x, .y = y}});
  // Swallows clicks between the rows, so they don't close the popup.
  ui.hit_area({.width = popup_width, .height = height, .place = {.x = x, .y = y},
               .input = {.on_click = [](const vw::ClickEvent&) {}}});
  draw::text_middle(ui, item.label, x + 16, y + popup_padding + popup_header / 2,
                    {.size = 13, .color = c.text_muted, .weight = vw::weight::semibold, .max_width = popup_width - 32});
  draw::rect(ui, x + 12, y + popup_padding + popup_header - 1, popup_width - 24, 1, 0, c.outline_variant);
  float row_y = y + popup_padding + popup_header;
  for (const plugins::DesktopAction& action : item.actions) {
    const vw::Color tint = action.danger ? c.error : c.text;
    draw::glyph(ui, action.glyph.empty() ? "chevron_right" : action.glyph, x + 26, row_y + popup_row / 2, 20,
                action.danger ? c.error : c.primary);
    draw::text_middle(ui, action.label.empty() ? action.id : action.label, x + 48, row_y + popup_row / 2,
                      {.size = 14, .color = tint, .max_width = popup_width - 64});
    draw::state_layer(ui, x + popup_padding, row_y, popup_width - popup_padding * 2, popup_row, 12, tint,
                      {.on_click = [source = open.source, id = item.id, action = action.id](const vw::ClickEvent&) {
                         plugins::desktop_action(source, id, action);
                         open_popup().reset();
                       }});
    row_y += popup_row;
  }
  input.push_back(region(x, y, popup_width, height));
}

}  // namespace

void draw_desktop(vw::Ui& ui, const Screen& screen, std::vector<std::array<std::int32_t, 4>>& input) {
  const std::vector<plugins::Desktop> areas = plugins::desktops();
  auto& open = open_popup();
  if (areas.empty()) {
    open.reset();
    return;
  }
  const Colours& c = colours();
  const float left = props::frame_thickness + margin;
  const float top = props::frame_thickness + margin;
  const float bottom = screen.height - props::bar_height - margin;
  const int rows = std::max(1, static_cast<int>((bottom - top) / cell_height));

  // While a popup is open, a click anywhere else on the desktop closes it.
  const plugins::DesktopItem* open_item = nullptr;
  if (open) {
    for (const auto& area : areas)
      if (area.source == open->source)
        for (const auto& item : area.items)
          if (item.id == open->item) open_item = &item;
    if (open_item == nullptr || open_item->actions.empty()) open.reset();
  }
  if (open) {
    ui.hit_area({.width = screen.width, .height = screen.height, .place = {.x = 0, .y = 0},
                 .input = {.on_click = [](const vw::ClickEvent&) { open_popup().reset(); }}});
    input.push_back(region(0, 0, screen.width, screen.height));
  }

  const int columns = std::max(1, static_cast<int>((screen.width - props::frame_thickness - left) / cell_width));
  const std::vector<Placed> placed = layout(areas, columns, rows);
  auto& cells = last_layout();
  cells.clear();
  for (const Placed& entry : placed) cells.emplace_back(key_of(entry.source, entry.item->id), entry.cell);

  Drag& drag = dragging();
  const auto cell_of = [left, top, columns, rows](float x, float y) {
    return Cell{std::clamp(static_cast<int>(std::lround((x - left) / cell_width)), 0, columns - 1),
                std::clamp(static_cast<int>(std::lround((y - top) / cell_height)), 0, rows - 1)};
  };
  if (drag.active) {
    // The pointer must keep reaching us wherever it goes.
    input.push_back(region(0, 0, screen.width, screen.height));
    const Cell target = cell_of(drag.x, drag.y);
    ui.rectangle({.width = cell_width - 8, .height = cell_height - 4, .radius = 18, .color = with_alpha(c.primary, 0.12F),
                  .border_width = 2, .border_color = with_alpha(c.primary, 0.7F),
                  .place = {.x = left + static_cast<float>(target.first) * cell_width + 4,
                            .y = top + static_cast<float>(target.second) * cell_height + 2}});
  }

  const plugins::DesktopItem* dragged = nullptr;
  for (const Placed& entry : placed) {
    const plugins::DesktopItem& item = *entry.item;
    const float x = left + static_cast<float>(entry.cell.first) * cell_width;
    const float y = top + static_cast<float>(entry.cell.second) * cell_height;
    const bool is_dragged = drag.active && drag.source == entry.source && drag.id == item.id;
    if (is_dragged) dragged = &item;  // drawn last, over the others
    else draw_item(ui, item, x, y, open && open->source == entry.source && open->item == item.id);
    vw::Input handlers{
        .on_click = [source = entry.source, id = item.id, first = item.actions.empty() ? std::string() : item.actions.front().id,
                     x, y](const vw::ClickEvent& click) {
          const auto now = Clock::now();
          if (now - dragging().ended < std::chrono::milliseconds(250)) return;  // the end of a drag
          LastClick& last = last_click();
          // A second left click on the same item soon after: its first action.
          if (click.button != 273 && last.source == source && last.item == id && now - last.when < double_click) {
            last = {};
            open_popup().reset();
            if (!first.empty()) plugins::desktop_action(source, id, first);
            return;
          }
          last = {source, id, now};
          open_popup() = Open{source, id, x, y, next_serial()};
        },
        .on_drag_start = [source = entry.source, id = item.id, x, y](const vw::DragEvent& event) {
          open_popup().reset();
          dragging() = {true, source, id, static_cast<float>(event.start_x) - x, static_cast<float>(event.start_y) - y, x, y};
        },
        .on_drag = [](const vw::DragEvent& event) {
          Drag& active = dragging();
          active.x = static_cast<float>(event.x) - active.grab_x;
          active.y = static_cast<float>(event.y) - active.grab_y;
        },
        .on_drag_end = [cell_of, from = entry.cell](const vw::DragEvent&) {
          Drag& active = dragging();
          if (!active.active) return;
          const Cell to = cell_of(active.x, active.y);
          const std::string moved = key_of(active.source, active.id);
          // Whatever was on the target cell trades places; everything shown
          // keeps its cell from now on, so new items don't shuffle them.
          for (const auto& [key, cell] : last_layout()) {
            if (key == moved) positions()[key] = to;
            else if (cell == to) positions()[key] = from;
            else positions()[key] = cell;
          }
          save_positions();
          active.active = false;
          active.ended = Clock::now();
        }};
    draw::state_layer(ui, x + 4, y + 2, cell_width - 8, cell_height - 4, 18, c.text, std::move(handlers));
    input.push_back(region(x + 4, y + 2, cell_width - 8, cell_height - 4));
  }
  if (dragged != nullptr) draw_item(ui, *dragged, drag.x, drag.y, true);
  if (open && open_item != nullptr) draw_popup(ui, screen, *open, *open_item, input);
}

}  // namespace blx
