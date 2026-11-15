#include "services.hpp"

#include <vw/mpris.hpp>
#include <vw/process.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <format>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.hpp"

namespace blx {
namespace {

using Clock = std::chrono::steady_clock;
using nlohmann::json;

int event() {
  static const int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  return fd;
}

void wake() {
  const std::uint64_t one = 1;
  const auto written = write(event(), &one, sizeof(one));
  (void)written;
}

// Runs argv and returns stdout and the exit status.
std::pair<std::string, int> run(const std::vector<std::string>& argv) {
  std::string command;
  for (const auto& argument : argv) {
    command += '\'';
    for (char c : argument) command += c == '\'' ? std::string("'\\''") : std::string(1, c);
    command += "' ";
  }
  command += "2>/dev/null";
  FILE* pipe = popen(command.c_str(), "re");
  if (pipe == nullptr) return {"", -1};
  std::string output;
  std::array<char, 4096> chunk{};
  for (std::size_t count; (count = std::fread(chunk.data(), 1, chunk.size(), pipe)) > 0;)
    output.append(chunk.data(), count);
  const int status = pclose(pipe);
  return {output, WIFEXITED(status) ? WEXITSTATUS(status) : -1};
}

std::string url_encode(const std::string& value) {
  std::string result;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
    else result += std::format("%{:02X}", c);
  }
  return result;
}

std::string home() {
  const char* value = std::getenv("HOME");
  return value != nullptr ? value : ".";
}

}  // namespace

int services_event_fd() { return event(); }
void wake_services() { wake(); }

bool drain_services() {
  std::uint64_t count = 0;
  bool any = false;
  while (read(event(), &count, sizeof(count)) > 0) any = true;
  return any;
}

// --- Weather (Services/Weather.qml) -----------------------------------------------

namespace weather {
namespace {

struct State {
  std::mutex mutex;
  std::optional<Report> report;
  std::string error;
  bool loading = false;
  Clock::time_point fetched{};
};

State& state() {
  static State instance;
  return instance;
}

std::string glyph_for(int code) {
  if (code == 0) return "sunny";
  if (code == 1 || code == 2) return "partly_cloudy_day";
  if (code == 3) return "cloud";
  if (code == 45 || code == 48) return "foggy";
  if (code >= 51 && code <= 67) return "rainy";
  if (code >= 71 && code <= 77) return "weather_snowy";
  if (code >= 80 && code <= 82) return "rainy";
  if (code >= 85 && code <= 86) return "weather_snowy";
  if (code >= 95) return "thunderstorm";
  return "cloud";
}

std::string description_for(int code) {
  if (code == 0) return "Clear sky";
  if (code == 1) return "Mainly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Foggy";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Rain showers";
  if (code >= 85 && code <= 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "Cloudy";
}

// "2026-10-28T06:12" -> "6:12 AM"
std::string clock_time(const std::string& timestamp) {
  const auto t = timestamp.find('T');
  if (t == std::string::npos || timestamp.size() < t + 6) return "--:--";
  int hour = std::stoi(timestamp.substr(t + 1, 2));
  const std::string minute = timestamp.substr(t + 4, 2);
  const char* suffix = hour >= 12 ? "PM" : "AM";
  hour %= 12;
  if (hour == 0) hour = 12;
  return std::format("{}:{} {}", hour, minute, suffix);
}

void fetch() {
  const char* env = std::getenv("BLXSHELL_WEATHER_LOCATION");
  std::string query = env != nullptr ? env : "";
  if (query.empty() && config().prefer_weather_location_from_config) query = config().weather_location;
  std::string latitude;
  std::string longitude;
  std::string place;
  static const std::regex coordinates(R"(^\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*$)");
  std::smatch match;
  try {
    if (std::regex_match(query, match, coordinates)) {
      latitude = match[1];
      longitude = match[2];
    } else if (!query.empty()) {
      const auto [body, status] = run({"curl", "--fail", "--silent", "--max-time", "10",
                                       "https://geocoding-api.open-meteo.com/v1/search?name=" +
                                           url_encode(query) + "&count=1&language=en&format=json"});
      const json result = json::parse(body).at("results").at(0);
      latitude = std::format("{}", result.at("latitude").get<double>());
      longitude = std::format("{}", result.at("longitude").get<double>());
      place = result.value("name", "");
    } else {
      const auto [body, status] = run({"curl", "--fail", "--silent", "--max-time", "8",
                                       "https://ipinfo.io/json"});
      const json data = json::parse(body);
      const std::string loc = data.at("loc").get<std::string>();
      const auto comma = loc.find(',');
      latitude = loc.substr(0, comma);
      longitude = loc.substr(comma + 1);
      place = data.value("city", "");
    }
  } catch (const std::exception&) {
    std::lock_guard lock(state().mutex);
    state().loading = false;
    if (!state().report) state().error = "Weather location could not be found";
    wake();
    return;
  }
  const auto [body, status] = run(
      {"curl", "--fail", "--silent", "--max-time", "10",
       "https://api.open-meteo.com/v1/forecast?latitude=" + url_encode(latitude) +
           "&longitude=" + url_encode(longitude) +
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,"
           "wind_speed_10m&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,"
           "sunset&timezone=auto&forecast_days=7" +
               std::string(config().weather_unit == "fahrenheit" ? "&temperature_unit=fahrenheit" : "")});
  std::lock_guard lock(state().mutex);
  state().loading = false;
  state().fetched = Clock::now();
  try {
    const json data = json::parse(body);
    const json& now = data.at("current");
    const json& daily = data.at("daily");
    Report report;
    const int code = now.at("weather_code").get<int>();
    const char* unit = config().weather_unit == "fahrenheit" ? "F" : "C";
    report.temperature = std::format("{}°{}", std::lround(now.at("temperature_2m").get<double>()), unit);
    report.feels_like = std::format("{}°{}", std::lround(now.at("apparent_temperature").get<double>()), unit);
    report.humidity = std::format("{}%", now.at("relative_humidity_2m").get<int>());
    report.wind = std::format("{} km/h", std::lround(now.at("wind_speed_10m").get<double>()));
    report.condition = description_for(code);
    report.glyph = glyph_for(code);
    report.location = place.empty() && state().report ? state().report->location : place;
    report.sunrise = clock_time(daily.at("sunrise").at(0).get<std::string>());
    report.sunset = clock_time(daily.at("sunset").at(0).get<std::string>());
    const auto& days = daily.at("time");
    for (std::size_t index = 0; index < days.size(); ++index)
      report.forecast.push_back(
          {days.at(index).get<std::string>(),
           std::format("{}°", std::lround(daily.at("temperature_2m_min").at(index).get<double>())),
           std::format("{}°", std::lround(daily.at("temperature_2m_max").at(index).get<double>())),
           glyph_for(daily.at("weather_code").at(index).get<int>())});
    report.updated = std::chrono::system_clock::now();
    state().report = std::move(report);
    state().error.clear();
  } catch (const std::exception&) {
    if (!state().report) state().error = status == 0 ? "Weather data could not be read"
                                                     : "Weather service unavailable";
  }
  wake();
}

void ensure_fresh() {
  State& s = state();
  if (s.loading) return;
  if (s.fetched != Clock::time_point{} && Clock::now() - s.fetched < std::chrono::minutes(30)) return;
  s.loading = true;
  s.fetched = Clock::now();
  std::thread(fetch).detach();
}

}  // namespace

std::optional<Report> current() {
  std::lock_guard lock(state().mutex);
  ensure_fresh();
  return state().report;
}

std::string error() {
  std::lock_guard lock(state().mutex);
  return state().error;
}

bool loading() {
  std::lock_guard lock(state().mutex);
  return state().loading;
}

void refresh() {
  std::lock_guard lock(state().mutex);
  state().fetched = {};
  ensure_fresh();
}

}  // namespace weather

// --- Recorder (Services/Recorder.qml) ---------------------------------------------

namespace recorder {
namespace {

struct State {
  std::mutex mutex;
  bool running = false;
  bool paused = false;
  int elapsed = 0;
  std::string error;
  std::vector<std::string> recordings;
  Clock::time_point checked{};
  Clock::time_point tick{};
  Clock::time_point expecting_until{};
  bool scanned = false;
};

State& state() {
  static State instance;
  return instance;
}

std::string directory() { return home() + "/Videos/Recordings"; }

void scan(State& s) {
  std::vector<std::filesystem::directory_entry> files;
  std::error_code error;
  std::filesystem::create_directories(directory(), error);
  for (const auto& entry : std::filesystem::directory_iterator(directory(), error))
    if (entry.path().extension() == ".mp4") files.push_back(entry);
  std::ranges::sort(files, [](const auto& a, const auto& b) {
    std::error_code ignored;
    return a.last_write_time(ignored) > b.last_write_time(ignored);
  });
  s.recordings.clear();
  for (const auto& file : files) s.recordings.push_back(file.path().string());
  s.scanned = true;
}

// Like `pidof gpu-screen-recorder`, without forking a shell and pidof on the
// UI thread: /proc/<pid>/comm holds the (15-character) process name.
bool recorder_alive() {
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator("/proc", error)) {
    const std::string name = entry.path().filename().string();
    if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;
    std::ifstream comm(entry.path() / "comm");
    std::string command;
    if (std::getline(comm, command) && command == "gpu-screen-reco") return true;
  }
  return false;
}

// Checks every two seconds (more often right after a start or stop request);
// the elapsed counter ticks once a second while recording unpaused.
void poll() {
  State& s = state();
  const auto now = Clock::now();
  if (!s.scanned) scan(s);
  if (s.running && !s.paused && now - s.tick >= std::chrono::seconds(1)) {
    s.elapsed += static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - s.tick).count());
    s.tick = now;
  }
  const bool expecting = now < s.expecting_until;
  if (now - s.checked < (expecting ? std::chrono::milliseconds(700) : std::chrono::seconds(2))) return;
  s.checked = now;
  const bool active = recorder_alive();
  if (active && !s.running) s.tick = now;
  if (!active && s.running) scan(s);
  if (!active && expecting) return;  // still starting (or picking a region)
  s.running = active;
  if (!active) s.paused = false;
}

}  // namespace

bool running() {
  std::lock_guard lock(state().mutex);
  poll();
  return state().running;
}
bool paused() {
  std::lock_guard lock(state().mutex);
  return state().paused;
}
int elapsed() {
  std::lock_guard lock(state().mutex);
  poll();
  return state().elapsed;
}
std::string error() {
  std::lock_guard lock(state().mutex);
  return state().error;
}
std::vector<std::string> recordings() {
  std::lock_guard lock(state().mutex);
  poll();
  return state().recordings;
}

void start(const std::string& mode, bool audio, const std::string& monitor) {
  std::lock_guard lock(state().mutex);
  State& s = state();
  if (s.running) return;
  const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
  const std::string output = std::format("{}/recording_{:%Y%m%d_%H%M%S}.mp4", directory(),
                                         std::chrono::floor<std::chrono::seconds>(now.get_local_time()));
  s.error.clear();
  s.elapsed = 0;
  s.paused = false;
  s.expecting_until = Clock::now() + std::chrono::seconds(21);
  s.checked = Clock::now();
  vw::spawn({"sh", "-c",
             "mkdir -p \"$1\" || exit 1; "
             "if [ \"$3\" = region ]; then "
             "  geometry=$(slurp -f '%wx%h+%x+%y') || exit 2; [ -n \"$geometry\" ] || exit 2; "
             "  if [ \"$5\" = yes ]; then exec gpu-screen-recorder -w region -region \"$geometry\" -f 60 -a default_output -o \"$2\"; "
             "  else exec gpu-screen-recorder -w region -region \"$geometry\" -f 60 -o \"$2\"; fi; "
             "else "
             "  if [ \"$5\" = yes ]; then exec gpu-screen-recorder -w \"$4\" -f 60 -a default_output -o \"$2\"; "
             "  else exec gpu-screen-recorder -w \"$4\" -f 60 -o \"$2\"; fi; "
             "fi",
             "sh", directory(), output, mode, monitor.empty() ? "screen" : monitor,
             audio ? "yes" : "no"});
}

void stop() {
  std::lock_guard lock(state().mutex);
  if (!state().running) return;
  vw::spawn({"pkill", "-SIGINT", "-x", "gpu-screen-recorder"});
  state().paused = false;
  state().checked = Clock::now() - std::chrono::milliseconds(1500);
}

void toggle_pause() {
  std::lock_guard lock(state().mutex);
  if (!state().running) return;
  vw::spawn({"pkill", "-SIGUSR2", "-x", "gpu-screen-recorder"});
  state().paused = !state().paused;
  state().tick = Clock::now();
}

void open(const std::string& path) {
  if (!path.empty()) vw::spawn({"xdg-open", path});
}
void open_directory() { vw::spawn({"xdg-open", directory()}); }

void remove(const std::string& path) {
  std::lock_guard lock(state().mutex);
  std::error_code error;
  std::filesystem::remove(path, error);
  scan(state());
}

std::string display_name(const std::string& path) {
  std::string name = std::filesystem::path(path).stem().string();
  static const std::regex pattern(R"(^recording_(\d{4})(\d{2})(\d{2})_(\d{2})-?(\d{2})-?(\d{2})$)");
  std::smatch match;
  if (!std::regex_match(name, match, pattern)) return name;
  static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const int month = std::stoi(match[2]) - 1;
  int hour = std::stoi(match[4]);
  const char* suffix = hour >= 12 ? "PM" : "AM";
  hour %= 12;
  if (hour == 0) hour = 12;
  return std::format("Recording at {} {}, {}:{} {}", months[std::clamp(month, 0, 11)],
                     std::stoi(match[3]), hour, match[5].str(), suffix);
}

}  // namespace recorder

// --- Artwork -----------------------------------------------------------------------

namespace art {
namespace {

std::string percent_decode(const std::string& text) {
  std::string result;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 2 < text.size()) {
      result += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else {
      result += text[i];
    }
  }
  return result;
}

}  // namespace

std::string path(const std::string& url) {
  if (url.empty()) return {};
  if (url.starts_with("file://")) return percent_decode(url.substr(7));
  if (url.starts_with("/")) return url;
  if (!url.starts_with("http://") && !url.starts_with("https://")) return {};
  static std::mutex mutex;
  static std::map<std::string, std::string> ready;  // url -> file, "" while downloading
  std::lock_guard lock(mutex);
  if (const auto found = ready.find(url); found != ready.end()) return found->second;
  ready[url] = "";
  const std::string directory = home() + "/.cache/blxshell/art";
  const std::string file = std::format("{}/{:016x}", directory, std::hash<std::string>{}(url));
  std::thread([url, directory, file] {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const bool ok = std::filesystem::exists(file, error) ||
                    run({"curl", "--fail", "--silent", "--location", "--max-time", "10", "-o", file, url}).second == 0;
    std::lock_guard guard(mutex);
    ready[url] = ok ? file : "";
    wake();
  }).detach();
  return {};
}

}  // namespace art

// --- Lyrics (Services/Media.qml) --------------------------------------------------

namespace lyrics {
namespace {

struct State {
  std::mutex mutex;
  std::map<std::string, Lyrics> cache;
  std::string requested;  // track key being fetched
  std::string error;
  // Tracks whose lookup failed on the service's side, and when to try again.
  std::map<std::string, std::chrono::steady_clock::time_point> retry;
};

State& state() {
  static State instance;
  return instance;
}

std::string lower(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::tolower(c); });
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

std::vector<Line> parse_lrc(const std::string& source) {
  std::vector<Line> result;
  static const std::regex stamp(R"(\[(\d+):(\d+(?:\.\d+)?)\])");
  std::size_t start = 0;
  while (start <= source.size()) {
    auto end = source.find('\n', start);
    if (end == std::string::npos) end = source.size();
    std::string line = source.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<double> times;
    for (std::sregex_iterator it(line.begin(), line.end(), stamp), last; it != last; ++it)
      times.push_back(std::stod((*it)[1]) * 60 + std::stod((*it)[2]));
    std::string text = std::regex_replace(line, std::regex(R"(\[[^\]]+\])"), "");
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(0, 1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
    if (!text.empty())
      for (double time : times) result.push_back({time, text});
    start = end + 1;
  }
  std::ranges::sort(result, {}, &Line::time);
  return result;
}

Lyrics from_result(const json& result, const std::string& backend) {
  const auto field = [&result](const char* key) {
    return result.is_object() && result.contains(key) && result[key].is_string()
               ? result[key].get<std::string>() : std::string();
  };
  Lyrics lyrics;
  lyrics.backend = backend;
  lyrics.synced = parse_lrc(field("syncedLyrics"));
  lyrics.plain = field("plainLyrics");
  if (lyrics.plain.empty())
    for (const auto& line : lyrics.synced) lyrics.plain += line.text + "\n";
  return lyrics;
}

bool usable(const Lyrics& lyrics) {
  return !lyrics.synced.empty() || lyrics.plain.find_first_not_of(" \n") != std::string::npos;
}

std::string squeeze(const std::string& text) {
  std::string result = std::regex_replace(text, std::regex(R"(\s+)"), " ");
  const auto first = result.find_first_not_of(" \"'-");
  if (first == std::string::npos) return {};
  return result.substr(first, result.find_last_not_of(" \"'-") - first + 1);
}

struct Query {
  std::string artist;
  std::string title;
};

// Browsers report a video's title, not a track's: `Channel - Song (Official
// Video) - YouTube`. The likeliest readings of it, best first, after the title
// as reported.
std::vector<Query> queries(const std::string& artist, const std::string& title) {
  using std::regex;
  const auto flags = regex::ECMAScript | regex::icase;
  std::vector<Query> result{{artist, title}};
  const auto add = [&result](const std::string& who, const std::string& what) {
    const Query query{squeeze(who), squeeze(what)};
    if (query.title.empty()) return;
    for (const Query& existing : result)
      if (lower(existing.artist) == lower(query.artist) && lower(existing.title) == lower(query.title)) return;
    result.push_back(query);
  };
  const std::string channel = std::regex_replace(artist, regex(R"(\s*-\s*Topic$|VEVO$)", flags), "");
  const std::string page = std::regex_replace(title, regex(R"(\s*[-|\xE2\x80\x93\xE2\x80\x94]+\s*YouTube( Music)?\s*$)", flags), "");

  // A quoted name is the song far more often than not.
  std::smatch quoted;
  if (std::regex_search(page, quoted, regex("[\"\\xE2\\x80\\x9C]([^\"\\xE2\\x80\\x9D]{2,})[\"\\xE2\\x80\\x9D]")))
    add(channel, quoted[1].str());

  std::string plain = std::regex_replace(page, regex(R"(\[[^\]]*\]|\([^)]*\))"), " ");
  plain = std::regex_replace(plain, regex(R"(\b(official\s+)?((music|lyrics?)\s+)?(video|audio|visuali[sz]er)\b|\blyrics?\b|\b(hd|hq|4k)\b)", flags), " ");
  plain = std::regex_replace(plain, regex(R"(\b(feat\.?|ft\.?)\s.*$)", flags), " ");
  if (std::smatch split; std::regex_search(plain, split, regex(R"(^(.+?)\s+(?:-|\xE2\x80\x93|\xE2\x80\x94|\|)\s+(.+)$)"))) {
    add(channel, split[2].str());
    add(split[1].str(), split[2].str());
  }
  add(channel, plain);
  return result;
}

// The body and HTTP status; status 0 when the service could not be reached.
// curl itself retries a busy server (LRCLIB answers 503 under load).
std::pair<std::string, int> request(const std::string& url) {
  auto [output, exit_code] = run({"curl", "--silent", "--retry", "2", "--retry-delay", "1", "--max-time", "10",
                                  "--write-out", "\n%{http_code}", url});
  const auto newline = output.rfind('\n');
  if (exit_code != 0 || newline == std::string::npos) return {"", 0};
  int status = 0;
  try {
    status = std::stoi(output.substr(newline + 1));
  } catch (const std::exception&) {
  }
  return {output.substr(0, newline), status};
}

void fetch(std::string key, std::string artist, std::string title, std::string album, double length) {
  std::optional<Lyrics> found;
  bool service_failed = false;
  const auto ask = [&service_failed](const std::string& url) -> json {
    auto [body, status] = request(url);
    if (status == 0 || status >= 500) service_failed = true;
    return status == 200 ? json::parse(body, nullptr, false) : json();
  };
  const auto accept = [&found](const Lyrics& lyrics) {
    if (usable(lyrics)) found = lyrics;
    return found.has_value();
  };
  // Synced lyrics from the first result that has them, else the first with any.
  const auto best_of = [&](const json& results) {
    if (!results.is_array()) return false;
    for (const auto& result : results)
      if (result.is_object() && !result.value("syncedLyrics", json()).is_null() && accept(from_result(result, "LRCLIB")))
        return true;
    for (const auto& result : results)
      if (accept(from_result(result, "LRCLIB"))) return true;
    return false;
  };
  try {
    std::string url = "https://lrclib.net/api/get?artist_name=" + url_encode(artist) + "&track_name=" + url_encode(title);
    if (!album.empty()) url += "&album_name=" + url_encode(album);
    if (length > 0) url += std::format("&duration={}", std::lround(length));
    if (const json result = ask(url); result.is_object()) accept(from_result(result, "LRCLIB"));

    const auto readings = queries(artist, title);
    for (const Query& query : readings) {
      if (found) break;
      if (query.artist.empty()) best_of(ask("https://lrclib.net/api/search?q=" + url_encode(query.title)));
      else best_of(ask("https://lrclib.net/api/search?artist_name=" + url_encode(query.artist) +
                       "&track_name=" + url_encode(query.title)));
    }
    for (const Query& query : readings) {
      if (found) break;
      if (query.artist.empty()) continue;
      const json data = ask("https://api.lyrics.ovh/v1/" + url_encode(query.artist) + "/" + url_encode(query.title));
      if (!data.is_object() || !data.contains("lyrics") || !data["lyrics"].is_string()) continue;
      Lyrics lyrics;
      lyrics.plain = data["lyrics"].get<std::string>();
      lyrics.backend = "Lyrics.ovh";
      accept(lyrics);
    }
  } catch (const std::exception&) {
    service_failed = true;
  }
  std::lock_guard lock(state().mutex);
  if (found) {
    state().cache[key] = *found;
    state().error.clear();
  } else if (service_failed) {
    // Not an answer: ask again rather than remember "none" for good.
    state().retry[key] = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    state().error = "Lyrics service unavailable";
  } else {
    state().cache[key] = Lyrics{};
    state().error = "No lyrics found";
  }
  if (state().requested == key) state().requested.clear();
  wake();
}

}  // namespace

std::optional<Lyrics> current() {
  const auto player = vw::mpris::active();
  if (!player || player->title.empty()) return std::nullopt;
  const std::string key = lower(player->artist) + '\x1f' + lower(player->title) + '\x1f' +
                          lower(player->album) + '\x1f' + std::to_string(std::lround(player->length));
  std::lock_guard lock(state().mutex);
  if (!player->lyrics.empty() && !state().cache.contains(key)) {
    Lyrics embedded;
    embedded.synced = parse_lrc(player->lyrics);
    embedded.plain = player->lyrics;
    embedded.backend = "MPRIS";
    state().cache[key] = embedded;
  }
  if (const auto found = state().cache.find(key); found != state().cache.end()) {
    if (found->second.synced.empty() && found->second.plain.empty()) return std::nullopt;
    return found->second;
  }
  if (const auto retry = state().retry.find(key); retry != state().retry.end()) {
    if (std::chrono::steady_clock::now() < retry->second) return std::nullopt;
    state().retry.erase(retry);
  }
  if (state().requested.empty()) {
    state().requested = key;
    state().error.clear();
    std::thread(fetch, key, player->artist, player->title, player->album, player->length).detach();
  }
  return std::nullopt;
}

bool loading() {
  std::lock_guard lock(state().mutex);
  return !state().requested.empty();
}

std::string error() {
  std::lock_guard lock(state().mutex);
  return state().error;
}

int line_at(const Lyrics& lyrics, double position) {
  int current = -1;
  for (std::size_t index = 0; index < lyrics.synced.size(); ++index) {
    if (lyrics.synced[index].time > position + 0.65) break;
    current = static_cast<int>(index);
  }
  return current;
}

}  // namespace lyrics

// --- Cava (cava plugin) ------------------------------------------------------------

namespace cava {
namespace {

struct State {
  std::mutex mutex;
  std::vector<float> values;
  Clock::time_point wanted{};
  bool running = false;
};

State& state() {
  static State instance;
  return instance;
}

// Raw ASCII output: one line of `bars` values (0..1000) per frame. Stops
// when nobody has asked for values for two seconds.
void reader() {
  const std::string config_path = std::format("/tmp/blxshell-cava-{}.conf", getpid());
  {
    std::ofstream config(config_path);
    config << "[general]\nbars = 48\nframerate = 30\n[output]\nmethod = raw\nraw_target = /dev/stdout\n"
              "data_format = ascii\nascii_max_range = 1000\nbar_delimiter = 59\n";
  }
  FILE* pipe = popen(("exec cava -p '" + config_path + "' 2>/dev/null").c_str(), "re");
  if (pipe == nullptr) return;
  std::array<char, 4096> line{};
  while (std::fgets(line.data(), line.size(), pipe) != nullptr) {
    std::vector<float> values;
    const char* cursor = line.data();
    while (*cursor != '\0' && *cursor != '\n') {
      char* end = nullptr;
      const long value = std::strtol(cursor, &end, 10);
      if (end == cursor) break;
      values.push_back(std::clamp(static_cast<float>(value) / 1000.0F, 0.0F, 1.0F));
      cursor = *end == ';' ? end + 1 : end;
    }
    std::lock_guard lock(state().mutex);
    state().values = std::move(values);
    if (Clock::now() - state().wanted > std::chrono::seconds(2)) break;
  }
  pclose(pipe);
  std::filesystem::remove(config_path);
  std::lock_guard lock(state().mutex);
  state().running = false;
  state().values.clear();
}

}  // namespace

std::vector<float> values() {
  std::lock_guard lock(state().mutex);
  state().wanted = Clock::now();
  if (!state().running) {
    state().running = true;
    std::thread(reader).detach();
  }
  return state().values;
}

}  // namespace cava

// --- Keep awake (Services/KeepAwake.qml) -------------------------------------------

namespace keep_awake {
namespace {

pid_t inhibitor = 0;
std::chrono::system_clock::time_point started{};

}  // namespace

bool enabled() {
  if (inhibitor > 0 && waitpid(inhibitor, nullptr, WNOHANG) != 0) inhibitor = 0;
  return inhibitor > 0;
}

void set(bool on) {
  if (on == enabled()) return;
  if (on) {
    try {
      inhibitor = vw::spawn({"systemd-inhibit", "--what=idle:sleep", "--who=blxshell",
                             "--why=Keep Awake", "sleep", "infinity"});
      started = std::chrono::system_clock::now();
    } catch (const std::exception&) {
      inhibitor = 0;
    }
  } else {
    // vw::spawn starts a new session: end the whole group, sleep included.
    kill(-inhibitor, SIGTERM);
    waitpid(inhibitor, nullptr, 0);
    inhibitor = 0;
  }
}

std::chrono::system_clock::time_point since() { return started; }

}  // namespace keep_awake

}  // namespace blx
