#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

// Services from ~/shell/Services that vw does not provide. Network and
// process work runs on worker threads; results land under a lock and
// services_event_fd() wakes the shell to redraw.
namespace blx {

int services_event_fd();
// Wakes the shell from a worker thread after it changed shared state.
void wake_services();
bool drain_services();

namespace weather {
struct Day {
  std::string date;  // YYYY-MM-DD
  std::string min;   // "12°"
  std::string max;
  std::string glyph;
};
struct Report {
  std::string temperature;  // "32°C"
  std::string condition;    // "Clear sky"
  std::string glyph;        // Material Symbols
  std::string location;
  std::string feels_like;
  std::string humidity;
  std::string wind;
  std::string sunrise;  // "6:12 AM"
  std::string sunset;
  std::vector<Day> forecast;
  std::chrono::system_clock::time_point updated{};
};
// Open-Meteo, located by BLXSHELL_WEATHER_LOCATION, the config's location
// (a place name or "lat,lon"), else by IP. Refreshed every 30 minutes.
std::optional<Report> current();
std::string error();
bool loading();
void refresh();
}  // namespace weather

namespace recorder {
// gpu-screen-recorder, polled every two seconds.
bool running();
bool paused();
int elapsed();
std::string error();
// Newest first.
std::vector<std::string> recordings();
void start(const std::string& mode, bool audio, const std::string& monitor);
void stop();
void toggle_pause();
void open(const std::string& path);
void open_directory();
void remove(const std::string& path);
// "Recording at Oct 28, 8:41 PM"
std::string display_name(const std::string& path);
}  // namespace recorder

namespace art {
// A local file for MPRIS artwork: file:// paths directly, http(s) downloaded
// once into ~/.cache/blxshell/art. Empty until available.
std::string path(const std::string& url);
}  // namespace art

namespace lyrics {
struct Line {
  double time;
  std::string text;
};
struct Lyrics {
  std::string plain;
  std::vector<Line> synced;
  std::string backend;  // "MPRIS", "LRCLIB", "Lyrics.ovh"
};
// For the active MPRIS track: embedded lyrics, else LRCLIB (exact, then
// search), else lyrics.ovh, cached per track. nullopt while loading.
std::optional<Lyrics> current();
bool loading();
std::string error();
// Index of the synced line playing at `position` (shown 0.65 s early).
int line_at(const Lyrics& lyrics, double position);
}  // namespace lyrics

namespace cava {
// Bar magnitudes 0..1 from `cava` (48 bars) while active() is called
// regularly; empty when cava is missing.
std::vector<float> values();
}  // namespace cava

namespace keep_awake {
// Holds a systemd-inhibit idle/sleep lock while enabled.
bool enabled();
void set(bool enabled);
std::chrono::system_clock::time_point since();
}  // namespace keep_awake

}  // namespace blx
