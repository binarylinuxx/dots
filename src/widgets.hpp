#pragma once

#include <vw/ui.hpp>

#include <functional>
#include <string>
#include <vector>

#include "style.hpp"

// Controls from ~/shell's blxshell/components, osd and dashboard folders.
// Vector parts (arcs, waves, graphs, Material shapes) are drawn with vw's
// canvas; flat parts are vw rectangles. Every control takes an `id` that
// keys its canvas cache and animation state.
namespace blx::widgets {

// CSS colour for canvas code.
std::string css(vw::Color color);

struct CircularOptions {
  double value = 0;
  vw::Color accent{1, 1, 1, 1};
  vw::Color track{1, 1, 1, 0.2F};
  float stroke = 4;
  float start_angle = -90;   // degrees
  float sweep_angle = 360;
  double critical_threshold = 1;
  vw::Color critical{1, 0.4F, 0.4F, 1};
  bool active = true;
  bool wavy = false;
  // Spins a full turn every 1.3 s.
  bool indeterminate = false;
  bool show_end_indicator = true;
  // A glyph, or the percentage when empty and show_label is set.
  std::string icon;
  bool show_label = true;
};
// CircularProgress.qml in a size x size square at (x, y).
void circular_progress(vw::Ui& ui, const std::string& id, float x, float y, float size,
                       const CircularOptions& options);

// ProgressBar.qml: accent segment, a gap, the track, and an end dot.
void progress_bar(vw::Ui& ui, float x, float y, float width, float height, double value,
                  vw::Color accent, vw::Color track);

// UsageGraph.qml: a square grid with two polylines.
void usage_graph(vw::Ui& ui, const std::string& id, float x, float y, float width, float height,
                 const std::vector<double>& primary, const std::vector<double>& secondary,
                 vw::Color primary_colour, vw::Color secondary_colour, vw::Color grid);

// RoundedPolygon.qml's Material 3 shapes: "sunny", "cloud", "gem", "pill",
// "cookie4sided".."cookie12sided", "clover4leaf", "clover8leaf", "burst",
// "softburst", "flower", "puffy", "circle".
void shape(vw::Ui& ui, const std::string& id, const std::string& type, float x, float y,
           float size, vw::Color fill, float stroke_width = 0, vw::Color stroke = {});

struct SliderOptions {
  double value = 0;
  float track_thickness = 6;
  float handle_diameter = 18;
  float handle_width = 2;
  float handle_progress_gap = 4;
  float endpoint_padding = 4;
  bool wave = false;
  float wave_amplitude = 2;
  float wave_length = 24;
  vw::Color track{1, 1, 1, 0.2F};
  vw::Color fill{1, 1, 1, 1};
  bool interactive = true;
  // While dragging (moved) and on release.
  std::function<void(double)> moved;
  std::function<void(double)> released;
};
// MaterialSlider.qml, horizontal.
void material_slider(vw::Ui& ui, const std::string& id, float x, float y, float width,
                     float height, const SliderOptions& options);

struct HandleSliderOptions {
  double value = 0;
  float handle_diameter = 32;
  float endpoint_padding = 4;
  vw::Color track{1, 1, 1, 0.2F};
  vw::Color fill{1, 1, 1, 1};
  vw::Color handle{1, 1, 1, 1};
  std::string icon;
  std::string value_text;
  std::function<void(double)> moved;
};
// CircleHandleSlider.qml, vertical: a thick rounded track with a round
// handle that shows its icon, or the value for a moment after a change.
void circle_handle_slider(vw::Ui& ui, const std::string& id, float x, float y, float height,
                          const HandleSliderOptions& options);

// Cava bars arranged radially around a circle of `radius` at the centre of
// a size x size square.
void radial_bars(vw::Ui& ui, const std::string& id, float x, float y, float size, float radius,
                 const std::vector<float>& values, vw::Color colour, float alpha);

// MarqueeText.qml: elided-free text that scrolls back and forth when wider
// than `width`.
void marquee(vw::Ui& ui, const std::string& id, const std::string& text, float x, float y,
             float width, float height, float size, vw::Color colour, int weight,
             const char* family);

}  // namespace blx::widgets
