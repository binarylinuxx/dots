// The lock and login screen layout editor: the page full screen, as it
// will look, with every item movable and resizable in place. Opened with
// `blxshell lockscreen edit [greeter]` or from settings.
//
//   drag an item          move it (snaps to the centre, the margins and
//                         other items; hold Shift to move freely)
//   drag its corner, or   resize it
//   scroll over it
//   arrows, Shift         nudge by 1 px, or 10
//   + / -                 bigger, smaller
//   Delete                hide it (text and plugin items go)
//   Tab                   the next item
//   G, [ and ]            the grid on or off; fewer or more cells. On the
//                         grid items move, nudge and resize by whole cells
//                         (Alt+arrows still nudge by a pixel)
//   A, P                  the Add menu; preview with the sheet up
//   Ctrl+Z, Ctrl+Y        undo, redo
//   Ctrl+S                save
//   Escape                deselect, close the menu, then leave
#include <vw/vw.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "lockpage.hpp"
#include "parts.hpp"
#include "plugins.hpp"
#include "style.hpp"
#include "system.hpp"

namespace blx {
namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* text_field_id = "lockeditor.text";
constexpr float snap_distance = 10;
constexpr float margin = 24;
constexpr float minimum_scale = 0.3F;
constexpr float maximum_scale = 4;

struct Editor {
  bool open = false;
  int target = 0;  // 0 the lock screen, 1 the login screen
  std::array<std::vector<LockItem>, 2> drafts;
  std::array<bool, 2> touched{};
  std::vector<std::vector<LockItem>> undo;
  std::vector<std::vector<LockItem>> redo;
  int selected = -1;
  bool sheet = false;  // previewing with the sheet up
  bool snap = true;
  bool adding = false;
  // Moving or resizing an item.
  int moving = -1;
  int resizing = -1;
  float grab_x = 0;
  float grab_y = 0;
  float start_scale = 1;
  float start_distance = 1;
  bool free = false;  // Shift held: no snapping
  std::optional<float> guide_x;
  std::optional<float> guide_y;
  Clock::time_point gesture_ended{};
  Clock::time_point last_scroll{};
  Clock::time_point discard_asked{};
  std::vector<PlacedItem> boxes;  // where items landed last frame
  float start_width = 1;          // the resized item's width when grabbed
  float width = 1920;
  float height = 1080;
};

Editor& editor() {
  static Editor state;
  return state;
}

std::vector<LockItem>& items() { return editor().drafts[static_cast<std::size_t>(editor().target)]; }
bool greeter() { return editor().target == 1; }

LockItem* selected_item() {
  Editor& e = editor();
  if (e.selected < 0 || static_cast<std::size_t>(e.selected) >= items().size()) return nullptr;
  return &items()[static_cast<std::size_t>(e.selected)];
}

const PlacedItem* box_of(int index) {
  for (const PlacedItem& box : editor().boxes)
    if (static_cast<int>(box.index) == index) return &box;
  return nullptr;
}

// --- History -----------------------------------------------------------------------

void checkpoint() {
  Editor& e = editor();
  e.undo.push_back(items());
  if (e.undo.size() > 200) e.undo.erase(e.undo.begin());
  e.redo.clear();
  e.touched[static_cast<std::size_t>(e.target)] = true;
}

void step_history(std::vector<std::vector<LockItem>>& from, std::vector<std::vector<LockItem>>& to) {
  if (from.empty()) return;
  to.push_back(items());
  items() = std::move(from.back());
  from.pop_back();
  editor().touched[static_cast<std::size_t>(editor().target)] = true;
  if (editor().selected >= static_cast<int>(items().size())) editor().selected = -1;
  vw::reset_text_field(text_field_id);
}

void undo() { step_history(editor().undo, editor().redo); }
void redo() { step_history(editor().redo, editor().undo); }

// --- Loading and saving ---------------------------------------------------------------

// What a screen draws now, as items: its saved layout, else the classic one,
// with any built-in item it lacks added hidden.
std::vector<LockItem> starting_layout(bool login) {
  const std::vector<LockItem>* saved = lock_layout_for(login);
  std::vector<LockItem> result;
  if (saved != nullptr)
    for (const LockItem& item : *saved)
      if (lock_item_allowed(item.id, login)) result.push_back(item);
  for (LockItem fallback : default_lock_layout(login)) {
    if (std::ranges::any_of(result, [&](const LockItem& item) { return item.id == fallback.id; })) continue;
    if (saved != nullptr) fallback.visible = false;
    result.push_back(std::move(fallback));
  }
  return result;
}

void switch_to(int target) {
  Editor& e = editor();
  if (e.target == target) return;
  e.target = target;
  e.selected = -1;
  e.adding = false;
  e.undo.clear();
  e.redo.clear();
  vw::reset_text_field(text_field_id);
}

void close_editor() {
  Editor& e = editor();
  e.open = false;
  e.adding = false;
  e.selected = -1;
  vw::reset_text_field(text_field_id);
}

void save() {
  Editor& e = editor();
  Config& c = config();
  if (e.touched[0]) {
    c.lock_layout = e.drafts[0];
    c.lock_layout_custom = true;
  }
  if (e.touched[1]) {
    c.greeter_layout = e.drafts[1];
    c.greeter_layout_custom = true;
  }
  if (e.touched[0] || e.touched[1]) save_config();
  close_editor();
}

bool unsaved() { return editor().touched[0] || editor().touched[1]; }

void leave() {
  Editor& e = editor();
  // Changes go only on a second Escape (or Cancel) within a few seconds.
  if (unsaved() && Clock::now() - e.discard_asked > std::chrono::seconds(4)) {
    e.discard_asked = Clock::now();
    return;
  }
  close_editor();
}

// --- Editing -------------------------------------------------------------------------

// Where an item's anchor goes for a box at (x, y): the side of the screen
// its centre is on decides which edge it grows away from.
void place(LockItem& item, float x, float y, float width, float height) {
  const Editor& e = editor();
  const float centre_x = (x + width / 2) / e.width;
  const float centre_y = (y + height / 2) / e.height;
  item.ax = centre_x < 1.0F / 3 ? 0.0F : centre_x > 2.0F / 3 ? 1.0F : 0.5F;
  item.ay = centre_y < 1.0F / 3 ? 0.0F : centre_y > 2.0F / 3 ? 1.0F : 0.5F;
  item.x = std::clamp((x + item.ax * width) / e.width, 0.0F, 1.0F);
  item.y = std::clamp((y + item.ay * height) / e.height, 0.0F, 1.0F);
}

void set_scale(LockItem& item, float scale) { item.scale = std::clamp(scale, minimum_scale, maximum_scale); }

// --- The grid ---------------------------------------------------------------------------

constexpr int grid_steps[] = {6, 8, 12, 16, 24, 32, 48};

int grid_columns() { return std::clamp(config().lock_editor_grid, 0, 96); }

// Square cells: the columns decide their size, the rows follow the screen.
struct Grid {
  int columns = 0;
  int rows = 0;
  float cell_width = 0;
  float cell_height = 0;
};

Grid grid() {
  const Editor& e = editor();
  Grid g;
  g.columns = grid_columns();
  if (g.columns <= 0) return g;
  g.cell_width = e.width / static_cast<float>(g.columns);
  g.rows = std::max(1, static_cast<int>(std::lround(e.height / g.cell_width)));
  g.cell_height = e.height / static_cast<float>(g.rows);
  return g;
}

void set_grid(int columns) {
  config().lock_editor_grid = columns;
  save_config();  // a preference of the editor's, kept whatever happens to the layout
}

void toggle_grid() { set_grid(grid_columns() > 0 ? 0 : 12); }

void step_grid(int by) {
  const int now = grid_columns() > 0 ? grid_columns() : 12;
  const auto* found = std::ranges::find(grid_steps, now);
  int index = found == std::end(grid_steps) ? 2 : static_cast<int>(found - std::begin(grid_steps));
  index = std::clamp(index + by, 0, static_cast<int>(std::size(grid_steps)) - 1);
  set_grid(grid_steps[index]);
}

// The box's start, centre or end on the nearest grid line, whichever is
// closest: small items centre in cells, big ones line up with them.
float on_grid(float position, float size, float cell, float limit) {
  float best = position;
  float distance = 1e9F;
  for (const float offset : {0.0F, size / 2, size}) {
    const float point = position + offset;
    // Centres sit mid-cell as well as on lines.
    for (const float line : {std::round(point / cell) * cell,
                             offset == size / 2 ? (std::floor(point / cell) + 0.5F) * cell : -1e9F}) {
      if (std::abs(line - point) < distance) {
        distance = std::abs(line - point);
        best = line - offset;
      }
    }
  }
  return std::clamp(best, 0.0F, std::max(0.0F, limit - size));
}

// A scale whose width spans whole cells, from a box `width` wide at `scale`.
float scale_on_grid(float scale, float width) {
  const Grid g = grid();
  if (g.columns <= 0 || width < 1) return scale;
  const float cells = std::max(1.0F, std::round(width / g.cell_width));
  return scale * cells * g.cell_width / width;
}

// By pixels, or on the grid by cells (landing on it).
void nudge(float dx, float dy, bool cells) {
  LockItem* item = selected_item();
  const PlacedItem* box = box_of(editor().selected);
  if (item == nullptr || box == nullptr) return;
  checkpoint();
  float x = std::clamp(box->x + dx, 0.0F, editor().width - box->width);
  float y = std::clamp(box->y + dy, 0.0F, editor().height - box->height);
  if (const Grid g = grid(); cells && g.columns > 0) {
    x = on_grid(box->x + dx * g.cell_width, box->width, g.cell_width, editor().width);
    y = on_grid(box->y + dy * g.cell_height, box->height, g.cell_height, editor().height);
  }
  place(*item, x, y, box->width, box->height);
}

void centre_selected(bool horizontal) {
  LockItem* item = selected_item();
  if (item == nullptr) return;
  checkpoint();
  if (horizontal) {
    item->ax = 0.5F;
    item->x = 0.5F;
  } else {
    item->ay = 0.5F;
    item->y = 0.5F;
  }
}

bool removable(const LockItem& item) { return item.id.starts_with("text:") || item.id.starts_with("plugin:"); }

void hide_selected() {
  Editor& e = editor();
  LockItem* item = selected_item();
  if (item == nullptr) return;
  checkpoint();
  if (removable(*item)) items().erase(items().begin() + e.selected);
  else item->visible = false;
  e.selected = -1;
  vw::reset_text_field(text_field_id);
}

void restack(int by) {
  Editor& e = editor();
  const int to = e.selected + by;
  if (selected_item() == nullptr || to < 0 || to >= static_cast<int>(items().size())) return;
  checkpoint();
  std::swap(items()[static_cast<std::size_t>(e.selected)], items()[static_cast<std::size_t>(to)]);
  e.selected = to;
}

void select_next() {
  Editor& e = editor();
  const int count = static_cast<int>(items().size());
  for (int step = 1; step <= count; ++step) {
    const int index = (e.selected + step + count) % count;
    if (items()[static_cast<std::size_t>(index)].visible) {
      e.selected = index;
      vw::reset_text_field(text_field_id);
      return;
    }
  }
}

void add(LockItem item) {
  Editor& e = editor();
  checkpoint();
  items().push_back(std::move(item));
  e.selected = static_cast<int>(items().size()) - 1;
  e.adding = false;
  vw::reset_text_field(text_field_id);
}

void add_text() {
  int next = 1;
  for (const LockItem& item : items())
    if (item.id.starts_with("text:")) {
      try {
        next = std::max(next, std::stoi(item.id.substr(5)) + 1);
      } catch (const std::exception&) {}
    }
  add({.id = std::format("text:{}", next), .x = 0.5F, .y = 0.5F, .text = "Hello", .colour = "text"});
}

void show(int index) {
  checkpoint();
  items()[static_cast<std::size_t>(index)].visible = true;
  editor().selected = index;
  editor().adding = false;
}

void reset_layout() {
  checkpoint();
  items() = default_lock_layout(greeter());
  editor().selected = -1;
  vw::reset_text_field(text_field_id);
}

// Snaps a box at (x, y) to the screen's centre and margins and the other
// items' centres and edges; sets the guides it snapped to.
std::pair<float, float> snapped(float x, float y, float width, float height, int self) {
  Editor& e = editor();
  e.guide_x.reset();
  e.guide_y.reset();
  if (e.free) return {x, y};
  if (const Grid g = grid(); g.columns > 0)
    return {on_grid(x, width, g.cell_width, e.width), on_grid(y, height, g.cell_height, e.height)};
  if (!e.snap) return {x, y};
  // (offset of the box's point from x, the line it snaps to)
  std::vector<std::pair<float, float>> lines_x{{width / 2, e.width / 2}, {0, margin}, {width, e.width - margin}};
  std::vector<std::pair<float, float>> lines_y{{height / 2, e.height / 2}, {0, margin}, {height, e.height - margin}};
  for (const PlacedItem& other : e.boxes) {
    if (static_cast<int>(other.index) == self) continue;
    lines_x.push_back({width / 2, other.x + other.width / 2});
    lines_x.push_back({0, other.x});
    lines_x.push_back({width, other.x + other.width});
    lines_y.push_back({height / 2, other.y + other.height / 2});
    lines_y.push_back({0, other.y + other.height + 16});  // just under it
    lines_y.push_back({height, other.y - 16});            // just over it
  }
  const auto best = [](float position, const std::vector<std::pair<float, float>>& lines, std::optional<float>& guide) {
    float distance = snap_distance;
    float result = position;
    for (const auto& [offset, line] : lines) {
      if (std::abs(position + offset - line) < distance) {
        distance = std::abs(position + offset - line);
        result = line - offset;
        guide = line;
      }
    }
    return result;
  };
  return {best(x, lines_x, e.guide_x), best(y, lines_y, e.guide_y)};
}

// --- Drawing -------------------------------------------------------------------------

float button_width(const std::string& label) {
  return 16 + 20 + (label.empty() ? 0 : 8 + draw::measure(label, 14, vw::weight::medium, font::utility)) + 16;
}

// A toolbar button: tonal when `active`, filled for the main action.
float button(vw::Ui& ui, float x, float y, const std::string& glyph, const std::string& label, bool active, bool enabled,
             std::function<void()> run, bool filled = false) {
  const Colours& c = colours();
  constexpr float height = 40;
  const float width = button_width(label);
  const vw::Color fill = filled ? c.primary : active ? c.secondary_container : with_alpha(c.surface, 0);
  const vw::Color ink = !enabled ? c.outline_variant : filled ? c.primary_text : c.text;
  draw::rect(ui, x, y, width, height, height / 2, fill);
  draw::glyph(ui, glyph, x + 26, y + height / 2, 20, ink);
  if (!label.empty())
    draw::text_middle(ui, label, x + 44, y + height / 2,
                      {.size = 14, .color = ink, .weight = vw::weight::medium, .family = font::utility});
  if (enabled)
    draw::state_layer(ui, x, y, width, height, height / 2, ink,
                      {.on_click = [run = std::move(run)](const vw::ClickEvent&) { run(); }});
  return width;
}

struct Tool {
  std::string glyph;
  std::string label;
  bool active = false;
  bool enabled = true;
  std::function<void()> run;
  bool filled = false;
  bool gap = false;  // a divider before it
};

void draw_toolbar(vw::Ui& ui, float& add_x) {
  Editor& e = editor();
  const Colours& c = colours();
  const std::vector<Tool> tools{
      {"lock", "Lock screen", e.target == 0, true, [] { switch_to(0); }},
      {"login", "Login screen", e.target == 1, true, [] { switch_to(1); }},
      {"add", "Add", e.adding, true, [] { editor().adding = !editor().adding; }, false, true},
      {"vertical_align_top", "Sheet", e.sheet, true, [] {
         editor().sheet = !editor().sheet;
         editor().selected = -1;
       }},
      {"align_justify_center", "Snap", e.snap && grid_columns() == 0, grid_columns() == 0, [] { editor().snap = !editor().snap; }},
      {"grid_4x4", grid_columns() > 0 ? std::format("Grid {}", grid_columns()) : std::string("Grid"), grid_columns() > 0, true,
       [] { toggle_grid(); }},
      {"remove", "", false, grid_columns() > 0, [] { step_grid(-1); }},
      {"add", "", false, grid_columns() > 0, [] { step_grid(1); }},
      {"undo", "", false, !e.undo.empty(), [] { undo(); }},
      {"redo", "", false, !e.redo.empty(), [] { redo(); }},
      {"restart_alt", "Reset", false, true, [] { reset_layout(); }},
      {"close", "Cancel", false, true, [] { close_editor(); }, false, true},
      {"check", "Save", false, true, [] { save(); }, true},
  };
  float width = 12;
  for (const Tool& tool : tools) width += button_width(tool.label) + 4 + (tool.gap ? 13 : 0);
  width += 8;
  float x = (e.width - width) / 2;
  constexpr float y = 20;
  draw::rect(ui, x, y, width, 56, 28, with_alpha(c.surface_container, 0.96F));
  x += 12;
  for (const Tool& tool : tools) {
    if (tool.gap) {
      draw::rect(ui, x + 4, y + 16, 1, 24, 0, c.outline_variant);
      x += 13;
    }
    if (tool.label == "Add") add_x = x;
    x += button(ui, x, y + 8, tool.glyph, tool.label, tool.active, tool.enabled, tool.run, tool.filled) + 4;
  }
}

void draw_add_menu(vw::Ui& ui, float x) {
  Editor& e = editor();
  const Colours& c = colours();
  struct Entry {
    std::string glyph;
    std::string label;
    std::function<void()> run;
  };
  std::vector<Entry> entries;
  for (std::size_t index = 0; index < items().size(); ++index) {
    const LockItem& item = items()[index];
    if (!item.visible && !removable(item))
      entries.push_back({"visibility", lock_item_label(item), [index] { show(static_cast<int>(index)); }});
  }
  entries.push_back({"title", "Text", [] { add_text(); }});
  if (!greeter()) {
    for (const std::string& view : plugins::lock_views()) {
      const std::string id = "plugin:" + view;
      if (std::ranges::any_of(items(), [&](const LockItem& item) { return item.id == id; })) continue;
      LockItem item{.id = id, .x = 0.5F, .y = 0.5F};
      entries.push_back({"extension", lock_item_label(item), [item] { add(item); }});
    }
  }
  constexpr float row = 44;
  float width = 240;
  for (const Entry& entry : entries) width = std::max(width, 76 + draw::measure(entry.label, 14, vw::weight::medium, font::utility));
  const float height = 16 + row * static_cast<float>(entries.size()) + (greeter() ? 30 : 0);
  x = std::min(x, e.width - width - 16);
  const float y = 84;
  draw::rect(ui, x, y, width, height, 24, c.surface_container_high);
  // Clicks on the card's padding stay on the card.
  ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, 0),
                .place = {.x = x, .y = y}, .input = {.on_click = [](const vw::ClickEvent&) {}}});
  float row_y = y + 8;
  for (const Entry& entry : entries) {
    draw::glyph(ui, entry.glyph, x + 30, row_y + row / 2, 20, c.primary);
    draw::text_middle(ui, entry.label, x + 52, row_y + row / 2,
                      {.size = 14, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
    draw::state_layer(ui, x + 6, row_y, width - 12, row, 16, c.text,
                      {.on_click = [run = entry.run](const vw::ClickEvent&) { run(); }});
    row_y += row;
  }
  if (greeter())
    draw::text(ui, "Plugins run in your session, not at login", x + 20, row_y + 8,
               {.size = 12, .color = c.text_muted, .family = font::utility});
}

// A slider for a value in [minimum, maximum]: drag or click along it.
void slider(vw::Ui& ui, float x, float y, float width, float value, float minimum, float maximum,
            const std::function<void(float)>& set) {
  const Colours& c = colours();
  const float t = std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
  draw::rect(ui, x, y + 13, width, 6, 3, c.surface_variant);
  draw::rect(ui, x, y + 13, width * t, 6, 3, c.primary);
  draw::rect(ui, x + width * t - 9, y + 7, 18, 18, 9, c.primary);
  const auto at = [x, width, minimum, maximum](double pointer) {
    return minimum + std::clamp(static_cast<float>(pointer) - x, 0.0F, width) / width * (maximum - minimum);
  };
  ui.rectangle({.width = width + 18, .height = 32, .color = with_alpha(c.surface, 0),
                .place = {.x = x - 9, .y = y}, .input = {.on_click = [set, at](const vw::ClickEvent& click) {
                            checkpoint();
                            set(at(click.x));
                          },
                          .on_drag_start = [set, at](const vw::DragEvent& drag) {
                            checkpoint();
                            set(at(drag.start_x));
                          },
                          .on_drag = [set, at](const vw::DragEvent& drag) { set(at(drag.x)); }}});
}

void icon_button(vw::Ui& ui, float x, float y, const std::string& glyph, const std::function<void()>& run, bool danger = false) {
  const Colours& c = colours();
  draw::rect(ui, x, y, 40, 40, 20, c.surface_container_highest);
  draw::glyph(ui, glyph, x + 20, y + 20, 20, danger ? c.error : c.text);
  draw::state_layer(ui, x, y, 40, 40, 20, c.text, {.on_click = [run](const vw::ClickEvent&) { run(); }});
}

// The selected item's settings, in a card on the side away from it.
void draw_inspector(vw::Ui& ui) {
  Editor& e = editor();
  LockItem* item = selected_item();
  const PlacedItem* box = box_of(e.selected);
  if (item == nullptr) return;
  const Colours& c = colours();
  const bool text = item->id.starts_with("text:");
  constexpr float width = 320;
  const float height = 318 + (text ? 132 : 0);
  const bool left = box != nullptr && box->x + box->width / 2 > e.width / 2;
  const float x = left ? margin : e.width - margin - width;
  const float y = 132;
  draw::rect(ui, x, y, width, height, 28, with_alpha(c.surface_container, 0.97F));
  ui.rectangle({.width = width, .height = height, .color = with_alpha(c.surface, 0),
                .place = {.x = x, .y = y}, .input = {.on_click = [](const vw::ClickEvent&) {}}});
  float cursor = y + 20;
  draw::text_middle(ui, lock_item_label(*item), x + 20, cursor + 14,
                    {.size = 17, .color = c.text, .weight = vw::weight::semibold, .family = font::display,
                     .max_width = width - 80});
  draw::glyph(ui, "close", x + width - 34, cursor + 14, 20, c.text_muted);
  draw::state_layer(ui, x + width - 54, cursor - 6, 40, 40, 20, c.text,
                    {.on_click = [](const vw::ClickEvent&) { editor().selected = -1; }});
  cursor += 40;
  draw::text(ui, std::format("x {}%  ·  y {}%", std::lround(item->x * 100), std::lround(item->y * 100)), x + 20, cursor,
             {.size = 12, .color = c.text_muted, .family = font::utility});
  cursor += 24;

  const auto labelled = [&](const std::string& label, const std::string& value) {
    draw::text(ui, label, x + 20, cursor, {.size = 13, .color = c.text, .weight = vw::weight::medium, .family = font::utility});
    const float value_width = draw::measure(value, 13, vw::weight::medium, font::utility);
    draw::text(ui, value, x + width - 20 - value_width, cursor,
               {.size = 13, .color = c.text_muted, .weight = vw::weight::medium, .family = font::utility});
    cursor += 12;
  };
  labelled("Size", std::format("{}%", std::lround(item->scale * 100)));
  slider(ui, x + 24, cursor, width - 48, item->scale, minimum_scale, maximum_scale,
         [](float value) { if (LockItem* chosen = selected_item()) set_scale(*chosen, std::round(value * 20) / 20); });
  cursor += 44;
  labelled("Opacity", std::format("{}%", std::lround(item->opacity * 100)));
  slider(ui, x + 24, cursor, width - 48, item->opacity, 0.1F, 1,
         [](float value) { if (LockItem* chosen = selected_item()) chosen->opacity = std::round(value * 20) / 20; });
  cursor += 44;
  // The backdrop behind it: the theme's see-through one until set; 100% is solid.
  labelled("Background", item->background < 0 ? std::string("Automatic") : std::format("{}%", std::lround(item->background * 100)));
  slider(ui, x + 24, cursor, width - 48, item->background < 0 ? 0.8F : item->background, 0, 1,
         [](float value) { if (LockItem* chosen = selected_item()) chosen->background = std::round(value * 20) / 20; });
  cursor += 44;

  if (text) {
    ui.text_field({.id = text_field_id, .value = item->text, .placeholder = "Text, or strftime like %A",
                   .width = width - 40, .height = 44, .radius = 16, .border_width = 1,
                   .color = c.surface_container_highest, .border_color = c.surface_container_highest,
                   .focus_color = c.primary, .text_color = c.text, .placeholder_color = c.text_muted,
                   .caret_color = c.primary, .font = font::utility, .size = 14,
                   .on_change = [](const std::string& value) {
                     if (LockItem* chosen = selected_item(); chosen != nullptr && chosen->text != value) {
                       // One undo step per editing burst, not per key.
                       if (Clock::now() - editor().last_scroll > std::chrono::seconds(2)) checkpoint();
                       editor().last_scroll = Clock::now();
                       chosen->text = value;
                     }
                   },
                   .place = {.x = x + 20, .y = cursor}});
    cursor += 58;
    float chip_x = x + 20;
    for (const auto& [role, colour] : std::initializer_list<std::pair<const char*, vw::Color>>{
             {"text", c.text}, {"primary", c.primary}, {"secondary", c.secondary}, {"tertiary", c.tertiary},
             {"muted", c.text_muted}}) {
      const bool chosen = item->colour == role || (item->colour.empty() && std::string_view(role) == "text");
      if (chosen) draw::rect(ui, chip_x - 3, cursor - 3, 42, 42, 21, c.primary);
      draw::rect(ui, chip_x, cursor, 36, 36, 18, colour);
      draw::state_layer(ui, chip_x, cursor, 36, 36, 18, c.text,
                        {.on_click = [role](const vw::ClickEvent&) {
                          if (LockItem* target = selected_item()) {
                            checkpoint();
                            target->colour = role;
                          }
                        }});
      chip_x += 48;
    }
    cursor += 60;
  }

  float button_x = x + 20;
  for (const auto& [glyph, run] : std::initializer_list<std::pair<const char*, std::function<void()>>>{
           {"align_horizontal_center", [] { centre_selected(true); }},
           {"align_vertical_center", [] { centre_selected(false); }},
           {"flip_to_front", [] { restack(1); }},
           {"flip_to_back", [] { restack(-1); }},
           {"format_color_reset", [] {
              if (LockItem* chosen = selected_item(); chosen != nullptr && chosen->background >= 0) {
                checkpoint();
                chosen->background = -1;  // the automatic backdrop again
              }
            }}}) {
    icon_button(ui, button_x, cursor, glyph, run);
    button_x += 48;
  }
  icon_button(ui, x + width - 60, cursor, removable(*item) ? "delete" : "visibility_off", [] { hide_selected(); },
              removable(*item));
}

// A rectangle's edges only.
void outline(vw::Ui& ui, float x, float y, float width, float height, float thickness, vw::Color colour) {
  draw::rect(ui, x, y, width, thickness, 0, colour);
  draw::rect(ui, x, y + height - thickness, width, thickness, 0, colour);
  draw::rect(ui, x, y + thickness, thickness, height - thickness * 2, 0, colour);
  draw::rect(ui, x + width - thickness, y + thickness, thickness, height - thickness * 2, 0, colour);
}

// Hit areas, outlines and the resize handle over each item.
void draw_handles(vw::Ui& ui) {
  Editor& e = editor();
  const Colours& c = colours();
  const vw::Color clear = with_alpha(c.surface, 0);
  if (const Grid g = grid(); g.columns > 0) {
    // The cells under what is being moved light up.
    if (const PlacedItem* box = box_of(e.moving >= 0 ? e.moving : e.resizing)) {
      const int left = static_cast<int>(std::floor(box->x / g.cell_width));
      const int right = static_cast<int>(std::ceil((box->x + box->width) / g.cell_width));
      const int top = static_cast<int>(std::floor(box->y / g.cell_height));
      const int bottom = static_cast<int>(std::ceil((box->y + box->height) / g.cell_height));
      draw::rect(ui, static_cast<float>(left) * g.cell_width, static_cast<float>(top) * g.cell_height,
                 static_cast<float>(right - left) * g.cell_width, static_cast<float>(bottom - top) * g.cell_height, 0,
                 with_alpha(c.primary, 0.14F));
    }
    const vw::Color line = with_alpha(c.outline, 0.28F);
    const vw::Color middle = with_alpha(c.primary, 0.45F);
    for (int column = 1; column < g.columns; ++column)
      draw::rect(ui, static_cast<float>(column) * g.cell_width - 0.5F, 0, 1, e.height, 0,
                 column * 2 == g.columns ? middle : line);
    for (int row = 1; row < g.rows; ++row)
      draw::rect(ui, 0, static_cast<float>(row) * g.cell_height - 0.5F, e.width, 1, 0, row * 2 == g.rows ? middle : line);
  }
  for (const PlacedItem& box : e.boxes) {
    const int index = static_cast<int>(box.index);
    const bool chosen = index == e.selected;
    outline(ui, box.x - 6, box.y - 6, box.width + 12, box.height + 12, chosen ? 2.0F : 1.0F,
            chosen ? c.primary : with_alpha(c.outline, 0.6F));
    ui.rectangle({.width = box.width + 12, .height = box.height + 12, .radius = 14, .color = clear,
                  .place = {.x = box.x - 6, .y = box.y - 6},
                  .input = {.on_click = [index](const vw::ClickEvent&) {
                              Editor& state = editor();
                              if (Clock::now() - state.gesture_ended < std::chrono::milliseconds(200)) return;
                              if (state.selected != index) vw::reset_text_field(text_field_id);
                              state.selected = index;
                              state.adding = false;
                            },
                            .on_drag_start = [index](const vw::DragEvent& drag) {
                              Editor& state = editor();
                              const PlacedItem* start = box_of(index);
                              if (start == nullptr) return;
                              checkpoint();
                              if (state.selected != index) vw::reset_text_field(text_field_id);
                              state.selected = index;
                              state.adding = false;
                              state.moving = index;
                              state.grab_x = static_cast<float>(drag.start_x) - start->x;
                              state.grab_y = static_cast<float>(drag.start_y) - start->y;
                            },
                            .on_drag = [index](const vw::DragEvent& drag) {
                              Editor& state = editor();
                              const PlacedItem* now = box_of(index);
                              if (state.moving != index || now == nullptr) return;
                              const float x = std::clamp(static_cast<float>(drag.x) - state.grab_x, 0.0F, state.width - now->width);
                              const float y = std::clamp(static_cast<float>(drag.y) - state.grab_y, 0.0F, state.height - now->height);
                              const auto [snapped_x, snapped_y] = snapped(x, y, now->width, now->height, index);
                              place(items()[static_cast<std::size_t>(index)], snapped_x, snapped_y, now->width, now->height);
                            },
                            .on_drag_end = [](const vw::DragEvent&) {
                              Editor& state = editor();
                              state.moving = -1;
                              state.free = false;
                              state.guide_x.reset();
                              state.guide_y.reset();
                              state.gesture_ended = Clock::now();
                            },
                            .on_scroll = [index](const vw::ScrollEvent& scroll) {
                              Editor& state = editor();
                              if (Clock::now() - state.last_scroll > std::chrono::milliseconds(700)) checkpoint();
                              state.last_scroll = Clock::now();
                              LockItem& item = items()[static_cast<std::size_t>(index)];
                              set_scale(item, item.scale * std::exp(-static_cast<float>(scroll.delta) * 0.002F));
                              state.selected = index;
                            }}});
    if (!chosen) continue;
    // The resize handle, bottom right: scales about the item's anchor.
    constexpr float handle = 22;
    const float handle_x = box.x + box.width + 6 - handle / 2;
    const float handle_y = box.y + box.height + 6 - handle / 2;
    draw::rect(ui, handle_x, handle_y, handle, handle, handle / 2, c.primary);
    draw::glyph(ui, "open_in_full", handle_x + handle / 2, handle_y + handle / 2, 14, c.primary_text);
    const float centre_x = box.x + box.width / 2;
    const float centre_y = box.y + box.height / 2;
    ui.rectangle({.width = handle + 12, .height = handle + 12, .radius = (handle + 12) / 2, .color = clear,
                  .place = {.x = handle_x - 6, .y = handle_y - 6},
                  .input = {.on_drag_start = [index, centre_x, centre_y, width = box.width](const vw::DragEvent& drag) {
                              Editor& state = editor();
                              checkpoint();
                              state.resizing = index;
                              state.start_width = width;
                              state.start_scale = items()[static_cast<std::size_t>(index)].scale;
                              state.start_distance = std::max(8.0F, std::hypot(static_cast<float>(drag.start_x) - centre_x,
                                                                               static_cast<float>(drag.start_y) - centre_y));
                            },
                            .on_drag = [index, centre_x, centre_y](const vw::DragEvent& drag) {
                              Editor& state = editor();
                              if (state.resizing != index) return;
                              const float distance = std::hypot(static_cast<float>(drag.x) - centre_x,
                                                                static_cast<float>(drag.y) - centre_y);
                              float scale = state.start_scale * distance / state.start_distance;
                              if (!state.free)
                                scale = scale_on_grid(scale, state.start_width * scale / state.start_scale);
                              set_scale(items()[static_cast<std::size_t>(index)], scale);
                            },
                            .on_drag_end = [](const vw::DragEvent&) {
                              editor().resizing = -1;
                              editor().free = false;
                              editor().gesture_ended = Clock::now();
                            }}});
  }
  // Where it snapped.
  if (e.guide_x) draw::rect(ui, *e.guide_x - 0.5F, 0, 1, e.height, 0, c.tertiary);
  if (e.guide_y) draw::rect(ui, 0, *e.guide_y - 0.5F, e.width, 1, 0, c.tertiary);
}

void draw_footer(vw::Ui& ui) {
  const Editor& e = editor();
  const Colours& c = colours();
  const bool asking = Clock::now() - e.discard_asked < std::chrono::seconds(4);
  const std::string line =
      asking ? "Unsaved changes: Escape again to throw them away, Ctrl+S to save"
      : e.sheet ? "Previewing with the sheet up: items shrink into the room above it. Sheet again to edit"
                : grid_columns() > 0
                ? "On the grid: drag, arrows and +/- go by cells · Shift drags freely · [ ] cell size · G grid off · Ctrl+S save"
                : "Drag to move · corner or scroll to resize · arrows nudge · Del hides · Tab next · A add · G grid · Ctrl+S save";
  const float width = draw::measure(line, 13, vw::weight::medium, font::utility) + 40;
  const float y = 84;
  draw::rect(ui, (e.width - width) / 2, y, width, 36, 18, asking ? c.error_container : with_alpha(c.surface_container, 0.92F));
  draw::text_centered(ui, line, e.width / 2, y + 18,
                      {.size = 13, .color = asking ? c.error : c.text, .weight = vw::weight::medium, .family = font::utility});
}

bool on_key(const vw::KeyEvent& key) {
  Editor& e = editor();
  if (key.name == "Escape") {
    if (e.adding) e.adding = false;
    else if (e.selected >= 0) e.selected = -1;
    else leave();
    return true;
  }
  if (key.ctrl) {
    if (key.name == "s") save();
    else if (key.name == "z" && !key.shift) undo();
    else if (key.name == "y" || key.name == "Z" || (key.name == "z" && key.shift)) redo();
    else return false;
    return true;
  }
  // Only presses arrive: Shift on its own starts free dragging, which lasts
  // until the drag ends or a key comes without it.
  e.free = key.shift || key.name.starts_with("Shift");
  // A text item's field takes the typing.
  if (const LockItem* item = selected_item(); item != nullptr && item->id.starts_with("text:")) return false;
  const bool cells = grid_columns() > 0 && !key.alt;
  const float step = cells ? 1.0F : key.shift ? 10.0F : 1.0F;
  if (key.name == "Left") nudge(-step, 0, cells);
  else if (key.name == "Right") nudge(step, 0, cells);
  else if (key.name == "Up") nudge(0, -step, cells);
  else if (key.name == "Down") nudge(0, step, cells);
  else if (key.name == "Tab" || key.name == "ISO_Left_Tab") select_next();
  else if (key.name == "a") e.adding = !e.adding;
  else if (key.name == "p") {
    e.sheet = !e.sheet;
    e.selected = -1;
  } else if (key.name == "g") toggle_grid();
  else if (key.name == "bracketleft") step_grid(-1);
  else if (key.name == "bracketright") step_grid(1);
  else if (key.name == "Delete" || key.name == "BackSpace") hide_selected();
  else if (key.text == "+" || key.text == "=" || key.text == "-") {
    if (LockItem* item = selected_item()) {
      checkpoint();
      const PlacedItem* box = box_of(e.selected);
      if (const Grid g = grid(); g.columns > 0 && box != nullptr && box->width > 1) {
        // One cell wider or narrower.
        const float cells = std::max(1.0F, std::round(box->width / g.cell_width) + (key.text == "-" ? -1.0F : 1.0F));
        set_scale(*item, item->scale * cells * g.cell_width / box->width);
      } else {
        set_scale(*item, item->scale + (key.text == "-" ? -0.05F : 0.05F));
      }
    }
  } else {
    return false;
  }
  return true;
}

}  // namespace

void open_lock_editor(bool login) {
  Editor& e = editor();
  if (!e.open) {
    e.drafts = {starting_layout(false), starting_layout(true)};
    e.touched = {};
    e.undo.clear();
    e.redo.clear();
    e.sheet = false;
  }
  e.open = true;
  e.target = -1;
  switch_to(login ? 1 : 0);
}

bool lock_editor_open() { return editor().open; }

void lock_editor_surface(vw::Ui& ui) {
  Editor& e = editor();
  if (!e.open) return;
  const vw::Output output = vw::current_output();
  e.width = output.width > 0 ? static_cast<float>(output.width) : 1920;
  e.height = output.height > 0 ? static_cast<float>(output.height) : 1080;
  ui.keyboard(on_key, vw::Keyboard::exclusive);

  const Colours& c = colours();
  const bool login = greeter();
  const char* user = std::getenv("USER");
  std::vector<PlacedItem> boxes;
  const float reveal = std::clamp(static_cast<float>(vw::animate("lockeditor.sheet", e.sheet ? 1 : 0, motion::bounce())), 0.0F, 1.0F);
  draw_lock_page(ui, {.width = e.width, .height = e.height, .reveal = reveal,
                      .hint = login ? (config().greeter_show_hint ? config().greeter_hint : "") : "Press any key to unlock",
                      .hint_glyph = login ? "login" : "lock", .session_widgets = !login,
                      .on_click = [] {
                        editor().selected = -1;
                        editor().adding = false;
                      },
                      .look = login ? greeter_look() : lock_look(), .layout = &items(), .editing = true, .greeter = login,
                      .user_name = user != nullptr ? user : "", .user_picture = system::avatar(),
                      .avatar_shape = login ? config().greeter_avatar_shape : "cookie9sided", .placed = &boxes});
  e.boxes = std::move(boxes);
  if (reveal > 0.01F) {
    const SheetCard card = draw_sheet_card(ui, e.width, e.height, login ? 380 : 318, reveal);
    draw::text_centered(ui, login ? "The login sheet" : "The password sheet", card.centre, card.y + card.height / 2,
                        {.size = 16, .color = c.text_muted, .weight = vw::weight::medium, .family = font::utility});
  }
  if (!e.sheet && reveal < 0.01F) draw_handles(ui);
  float add_x = 0;
  draw_toolbar(ui, add_x);
  draw_footer(ui);
  if (!e.sheet) draw_inspector(ui);
  if (e.adding) draw_add_menu(ui, add_x);

  ui.surface({.width = 0, .height = 0, .anchor = vw::anchor::fill, .layer = vw::Layer::overlay, .exclusive_zone = -1,
              .surface_namespace = "blxshell-lock-editor", .refresh_interval = 1});
}

}  // namespace blx
