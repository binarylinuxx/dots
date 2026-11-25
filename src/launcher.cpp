// The launcher, ported from ~/shell/blxshell/launcher: it rises out of the
// bottom frame edge with fuzzy-ranked results above a search field, `/`
// commands (wallpaper, calc, variant, scheme, emoji, outline, ...) and a
// wallpaper carousel. Results are ranked once per query and only the rows in
// view are built.
#include <vw/vw.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <random>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.hpp"
#include "draw.hpp"
#include "parts.hpp"
#include "state.hpp"
#include "surfaces.hpp"

namespace blx {
namespace {

using namespace props;

constexpr const char* search_id = "launcher-search";
constexpr const char* list_id = "launcher-results";
constexpr float row_spacing = 3;
constexpr float search_height = 48;

float row_height() { return static_cast<float>(std::clamp(config().launcher_row_height, 36, 90)); }
float bottom_extension(float reveal) { return (bar_height + launcher_radius) * reveal; }
float icon_size() { return static_cast<float>(std::clamp(config().launcher_icon_size, 16, 72)); }

constexpr float search_implicit = 40;  // TextField's implicit height

// --- Fuzzy matching (fuzzymatch.js) -------------------------------------------------

std::string lower(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::tolower(c); });
  return text;
}

std::string trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\n");
  if (first == std::string::npos) return {};
  return text.substr(first, text.find_last_not_of(" \t\n") - first + 1);
}

// Score of query as a subsequence of candidate, or nullopt.
std::optional<double> fuzzy(const std::string& query, const std::string& candidate) {
  const std::string needle = lower(trim(query));
  if (needle.empty()) return 0.0;
  const std::string folded = lower(candidate);
  double score = 0;
  std::size_t search_from = 0;
  long previous = -2;
  for (char character : needle) {
    const auto index = folded.find(character, search_from);
    if (index == std::string::npos) return std::nullopt;
    const bool consecutive = static_cast<long>(index) == previous + 1;
    const bool boundary = index == 0 || std::string_view(" \t_-./").find(candidate[index - 1]) != std::string_view::npos;
    score += 10 + (consecutive ? 14 : 0) + (boundary ? 8 : 0);
    score -= std::max(0L, static_cast<long>(index) - previous - 1) * 2.0;
    previous = static_cast<long>(index);
    search_from = index + 1;
  }
  return score - static_cast<double>(candidate.size()) * 0.15;
}

// Entries whose text matches, best first, ties in original order.
template <typename T>
std::vector<T> rank(const std::string& query, const std::vector<T>& entries,
                    const std::function<std::string(const T&)>& text) {
  std::vector<std::pair<double, std::size_t>> scored;
  for (std::size_t index = 0; index < entries.size(); ++index)
    if (const auto score = fuzzy(query, text(entries[index]))) scored.emplace_back(*score, index);
  std::ranges::stable_sort(scored, [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<T> result;
  for (const auto& [score, index] : scored) result.push_back(entries[index]);
  return result;
}

// --- Calculator (evaluateMath / calculate) -----------------------------------------

class Expression {
 public:
  Expression(std::string source, double x) : source_(std::move(source)), x_(x) {}

  double evaluate() {
    const double value = additive();
    skip();
    if (position_ != source_.size()) throw std::runtime_error("trailing input");
    return value;
  }

 private:
  void skip() {
    while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
  }
  bool accept(const std::string& token) {
    skip();
    if (source_.compare(position_, token.size(), token) != 0) return false;
    position_ += token.size();
    return true;
  }
  // A word operator: "mod" but not the start of "model".
  bool accept_word(const std::string& word) {
    skip();
    if (source_.compare(position_, word.size(), word) != 0) return false;
    const std::size_t end = position_ + word.size();
    if (end < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[end])) || source_[end] == '_')) return false;
    position_ = end;
    return true;
  }
  // The mathematical modulo: the result takes the divisor's sign, so -7 mod 3
  // is 2. `%` stays C's remainder, where it is -1.
  static double floor_mod(double a, double b) { return a - b * std::floor(a / b); }
  double additive() {
    double value = multiplicative();
    for (;;) {
      if (accept("+")) value += multiplicative();
      else if (accept("-")) value -= multiplicative();
      else return value;
    }
  }
  double multiplicative() {
    double value = unary();
    for (;;) {
      if (accept("*")) value *= unary();
      else if (accept("/")) value /= unary();
      else if (accept("%")) value = std::fmod(value, unary());
      else if (accept_word("mod")) value = floor_mod(value, unary());
      else return value;
    }
  }
  double unary() {
    if (accept("-")) return -unary();
    if (accept("+")) return unary();
    return power();
  }
  double power() {
    const double base = primary();
    if (accept("^")) return std::pow(base, unary());  // right associative, like **
    return base;
  }
  std::vector<double> arguments() {
    std::vector<double> values;
    if (!accept("(")) throw std::runtime_error("expected (");
    if (accept(")")) return values;
    do values.push_back(additive());
    while (accept(","));
    if (!accept(")")) throw std::runtime_error("expected )");
    return values;
  }
  double primary() {
    skip();
    if (accept("(")) {
      const double value = additive();
      if (!accept(")")) throw std::runtime_error("expected )");
      return value;
    }
    if (position_ < source_.size() && (std::isdigit(static_cast<unsigned char>(source_[position_])) || source_[position_] == '.')) {
      std::size_t used = 0;
      const double value = std::stod(source_.substr(position_), &used);
      position_ += used;
      return value;
    }
    std::string name;
    while (position_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_'))
      name += source_[position_++];
    if (name.empty()) throw std::runtime_error("unexpected input");
    if (name == "x") return x_;
    if (name == "pi") return std::numbers::pi;
    if (name == "e") return std::numbers::e;
    const auto args = arguments();
    const auto one = [&] {
      if (args.size() != 1) throw std::runtime_error("arity");
      return args[0];
    };
    const auto two = [&] {
      if (args.size() != 2) throw std::runtime_error("arity");
      return std::pair{args[0], args[1]};
    };
    if (name == "sin") return std::sin(one());
    if (name == "cos") return std::cos(one());
    if (name == "tan") return std::tan(one());
    if (name == "cot") return 1 / std::tan(one());
    if (name == "sec") return 1 / std::cos(one());
    if (name == "csc") return 1 / std::sin(one());
    if (name == "asin" || name == "arcsin") return std::asin(one());
    if (name == "acos" || name == "arccos") return std::acos(one());
    if (name == "atan" || name == "arctan") return std::atan(one());
    if (name == "acot" || name == "arccot") return std::atan2(1.0, one());
    if (name == "sinh") return std::sinh(one());
    if (name == "cosh") return std::cosh(one());
    if (name == "tanh") return std::tanh(one());
    if (name == "coth") return 1 / std::tanh(one());
    if (name == "asinh" || name == "arcsinh") return std::asinh(one());
    if (name == "acosh" || name == "arccosh") return std::acosh(one());
    if (name == "atanh" || name == "arctanh") return std::atanh(one());
    if (name == "acoth" || name == "arccoth") return std::atanh(1 / one());
    if (name == "mod") { const auto [a, b] = two(); return floor_mod(a, b); }
    if (name == "deg") return one() * 180 / std::numbers::pi;
    if (name == "rad") return one() * std::numbers::pi / 180;
    if (name == "sqrt") return std::sqrt(one());
    if (name == "abs") return std::abs(one());
    if (name == "sign") { const double v = one(); return v > 0 ? 1 : v < 0 ? -1 : v; }
    if (name == "exp") return std::exp(one());
    if (name == "floor") return std::floor(one());
    if (name == "ceil") return std::ceil(one());
    if (name == "round") return std::floor(one() + 0.5);
    if (name == "log" || name == "ln") return std::log(one());
    if (name == "log10") return std::log10(one());
    if (name == "pow") { const auto [a, b] = two(); return std::pow(a, b); }
    if (name == "min") { if (args.empty()) return INFINITY; return *std::ranges::min_element(args); }
    if (name == "max") { if (args.empty()) return -INFINITY; return *std::ranges::max_element(args); }
    if (name == "floormod" || name == "floormodulo") { const auto [a, b] = two(); return a - b * std::floor(a / b); }
    if (name == "ceilmod" || name == "ceilmodulo") { const auto [a, b] = two(); return a - b * std::ceil(a / b); }
    throw std::runtime_error("Unsupported function");
  }

  std::string source_;
  std::size_t position_ = 0;
  double x_;
};

double evaluate(const std::string& expression, double x) {
  std::string source = lower(expression);
  for (auto at = source.find("π"); at != std::string::npos; at = source.find("π")) source.replace(at, std::string("π").size(), "pi");
  return Expression(source, x).evaluate();
}

std::string number(double value) {
  // Floating point leaves sin(pi) at 1.2e-16 and 0.1 + 0.2 at 0.30000000000000004.
  if (std::abs(value) < 1e-12) value = 0;
  if (value == std::floor(value) && std::abs(value) < 1e15) return std::format("{}", static_cast<long long>(value));
  const double rounded = std::stod(std::format("{:.12g}", value));
  return rounded == std::floor(rounded) && std::abs(rounded) < 1e15 ? std::format("{}", static_cast<long long>(rounded))
                                                                     : std::format("{}", rounded);
}

std::string calculate(const std::string& expression) {
  if (expression.empty()) return "Type an expression";
  static const std::regex allowed(R"(^[0-9A-Za-z_+*/%().,=\s^-]+$)");
  std::string checked = expression;
  for (auto at = checked.find("π"); at != std::string::npos; at = checked.find("π")) checked.replace(at, std::string("π").size(), "p");
  if (!std::regex_match(checked, allowed)) return "Unsupported expression";
  try {
    const auto equals = expression.find('=');
    if (equals == std::string::npos) {
      const double value = evaluate(expression, 0);
      return std::isfinite(value) ? number(value) : "Invalid expression";
    }
    if (expression.find('=', equals + 1) != std::string::npos || expression.find('x') == std::string::npos)
      return "Use one equation with x";
    const std::string left = expression.substr(0, equals);
    const std::string right = expression.substr(equals + 1);
    const auto difference = [&](double x) { return evaluate(left, x) - evaluate(right, x); };
    double previous_x = -100;
    double previous_y = difference(previous_x);
    for (double x = -99.75; x <= 100; x += 0.25) {
      const double current_y = difference(x);
      if (std::isfinite(previous_y) && std::isfinite(current_y) && previous_y * current_y <= 0) {
        double low = previous_x;
        double high = x;
        for (int iteration = 0; iteration < 48; ++iteration) {
          const double middle = (low + high) / 2;
          if (difference(low) * difference(middle) <= 0) high = middle;
          else low = middle;
        }
        return std::format("x = {:.8f}", (low + high) / 2);
      }
      previous_x = x;
      previous_y = current_y;
    }
    return "No real root found";
  } catch (const std::exception&) {
    return "Invalid expression";
  }
}

// --- Entries -------------------------------------------------------------------------

struct Entry {
  std::string id;
  std::string name;
  std::string comment;
  std::string icon;  // app icon file, or the emoji
  bool action = false;
  std::string action_icon;
  std::string command;
  std::string value;
};

struct Command {
  const char* command;
  const char* name;
  const char* description;
  const char* icon;
};

std::vector<Command> commands() {
  const Config& c = config();
  return {
      {"wallpaper", "Wallpaper", "Browse and set a wallpaper", "wallpaper"},
      {"random-wallpaper", "Random", "Switch to a random wallpaper", "casino"},
      {"calc", "Calculator", "Evaluate an expression", "calculate"},
      {"variant", "MD3 variant", "Choose a Matugen palette type", "palette"},
      {"scheme", "Color mode", "Choose dark or light Matugen mode", "palette"},
      {"set-light", "Light", "Change the scheme to light mode", "light_mode"},
      {"set-dark", "Dark", "Change the scheme to dark mode", "dark_mode"},
      {"toggle-icon-tint", c.launcher_tint_app_icons ? "Disable app icon tint" : "Tint app icons",
       "Color application icons with the MD3 primary color", "format_paint"},
      {"toggle-tray-tint", c.bar_tint_tray_icons ? "Disable tray icon tint" : "Tint tray icons",
       "Color bar tray icons with the MD3 primary color", "format_paint"},
      {"outline", "Shell outline", "Choose an MD3 color for the SDF outline", "border_outer"},
      {"emoji", "Emoji picker", "Search and copy an emoji", "mood"},
      {"quickshare", "Quick Share", "Send files to nearby LocalSend devices", "nearby_share"},
      {"lock", "Lock", "Lock the current session", "lock"},
      {"suspend-then-hibernate", "Sleep", "Suspend then hibernate", "bedtime"},
  };
}

struct Named {
  const char* name;
  const char* value;
  const char* icon;
};

constexpr Named variants[] = {
    {"Dynamic Hard", "dynamic-hard", "contrast"},         {"Dynamic Soft", "dynamic-soft", "blur_on"},
    {"Tonal Spot", "scheme-tonal-spot", "android"},       {"Vibrant", "scheme-vibrant", "sentiment_very_dissatisfied"},
    {"Expressive", "scheme-expressive", "compare_arrows"}, {"Fidelity", "scheme-fidelity", "compare"},
    {"Content", "scheme-content", "sentiment_calm"},       {"Fruit Salad", "scheme-fruit-salad", "nutrition"},
    {"Rainbow", "scheme-rainbow", "looks"},                {"Neutral", "scheme-neutral", "contrast"},
    {"Monochrome", "scheme-monochrome", "filter_b_and_w"},
};

constexpr Named emojis[] = {
    {"grinning face", "😀", ""}, {"face with tears of joy", "😂", ""}, {"smiling face with tear", "🥲", ""},
    {"heart eyes", "😍", ""},    {"loudly crying face", "😭", ""},     {"thumbs up", "👍", ""},
    {"thumbs down", "👎", ""},   {"clapping hands", "👏", ""},         {"folded hands", "🙏", ""},
    {"handshake", "🤝", ""},     {"red heart", "❤️", ""},              {"blue heart", "💙", ""},
    {"sparkles", "✨", ""},      {"fire", "🔥", ""},                   {"party popper", "🎉", ""},
    {"check mark", "✅", ""},    {"cross mark", "❌", ""},             {"warning", "⚠️", ""},
    {"rocket", "🚀", ""},        {"light bulb", "💡", ""},             {"music note", "🎵", ""},
    {"pushpin", "📌", ""},       {"paperclip", "📎", ""},              {"laptop", "💻", ""},
    {"penguin", "🐧", ""},       {"moon", "🌙", ""},                   {"sun", "☀️", ""},
    {"rainbow", "🌈", ""},       {"four leaf clover", "🍀", ""},       {"star", "⭐", ""},
};

constexpr Named outline_colours[] = {
    {"Primary", "primary", ""},           {"On primary", "primaryText", ""},
    {"Primary container", "primaryContainer", ""}, {"Secondary", "secondary", ""},
    {"Tertiary", "tertiary", ""},         {"Error", "error", ""},
    {"Surface", "surface", ""},           {"Surface container", "surfaceContainer", ""},
    {"High surface", "surfaceContainerHigh", ""}, {"Surface variant", "surfaceVariant", ""},
    {"On surface", "text", ""},           {"On surface variant", "textMuted", ""},
    {"Outline", "outline", ""},
};


struct Wallpaper {
  std::string name;
  std::string path;
};

// What the launcher shows. Apps and wallpapers are loaded when it opens;
// results are recomputed only when the query (or a config label) changes.
struct Model {
  std::vector<Entry> apps;
  std::vector<Wallpaper> wallpapers;
  bool loaded = false;
  std::string ranked_query = "\x01";
  std::string ranked_labels;
  std::vector<Entry> results;
  std::size_t wallpaper_index = 0;
};

Model& model() {
  static Model instance;
  return instance;
}

void load() {
  Model& m = model();
  m.apps.clear();
  for (const auto& app : vw::applications::search("", 0))
    m.apps.push_back({app.id, app.display_name, app.description, app.icon_path, false, "", "app", ""});
  std::ranges::stable_sort(m.apps, [](const Entry& a, const Entry& b) { return lower(a.name) < lower(b.name); });
  m.wallpapers.clear();
  const char* home = std::getenv("HOME");
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(std::string(home ? home : ".") + "/.local/wallpapers", error)) {
    if (!entry.is_regular_file(error)) continue;
    const std::string extension = lower(entry.path().extension().string());
    if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp")
      m.wallpapers.push_back({entry.path().filename().string(), entry.path().string()});
  }
  std::ranges::sort(m.wallpapers, {}, &Wallpaper::path);
  // Start the carousel on the current wallpaper.
  const auto current = std::ranges::find(m.wallpapers, config().wallpaper_path, &Wallpaper::path);
  m.wallpaper_index = current == m.wallpapers.end() ? 0 : static_cast<std::size_t>(current - m.wallpapers.begin());
  m.loaded = true;
  m.ranked_query = "\x01";
}

}  // namespace

void open_wallpapers() {
  shell().query = "/wallpaper ";
  shell().selected = 0;
  shell().launcher_open = true;
  vw::reset_text_field(search_id);
}

namespace {

bool showing_wallpapers() { return shell().query.starts_with("/wallpaper "); }

// The apps to offer for a search: hidden ones gone, custom names applied.
// An empty search lists them in the configured order; otherwise the best
// match wins, with preferred apps then favourites pulled ahead of the rest.
std::vector<Entry> ranked_apps(const std::string& input) {
  const Config& c = config();
  std::vector<Entry> apps;
  apps.reserve(model().apps.size());
  for (const Entry& app : model().apps) {
    if (listed(c.launcher_hidden, app.id)) continue;
    Entry entry = app;
    if (const auto name = c.launcher_names.find(app.id); name != c.launcher_names.end() && !name->second.empty())
      entry.name = name->second;
    apps.push_back(std::move(entry));
  }
  const auto favorite = [&c](const Entry& e) { return listed(c.launcher_favorites, e.id); };
  const auto preferred = [&c](const Entry& e) { return listed(c.launcher_preferred, e.id); };
  if (trim(input).empty()) {
    const auto usage = [&c](const Entry& e) {
      const auto found = c.launcher_usage.find(e.id);
      return found == c.launcher_usage.end() ? AppUsage{} : found->second;
    };
    if (c.launcher_sort == "used")
      std::ranges::stable_sort(apps, [&](const Entry& a, const Entry& b) { return usage(a).count > usage(b).count; });
    else if (c.launcher_sort == "recent")
      std::ranges::stable_sort(apps, [&](const Entry& a, const Entry& b) { return usage(a).last > usage(b).last; });
    if (c.launcher_favorites_first) {
      // In the order they were favourited.
      const auto position = [&c](const Entry& e) { return std::ranges::find(c.launcher_favorites, e.id) - c.launcher_favorites.begin(); };
      const auto split = std::ranges::stable_partition(apps, favorite).begin();
      std::stable_sort(apps.begin(), split, [&](const Entry& a, const Entry& b) { return position(a) < position(b); });
    }
    return apps;
  }
  const bool comments = c.launcher_search_comments;
  std::vector<Entry> matches = rank<Entry>(input, apps, [comments](const Entry& e) {
    return comments ? e.name + " " + e.comment + " " + e.id : e.name;
  });
  if (c.launcher_boost_favorites) std::ranges::stable_partition(matches, favorite);
  std::ranges::stable_partition(matches, preferred);
  return matches;
}

const std::vector<Entry>& results() {
  Model& m = model();
  const std::string& input = shell().query;
  const std::string labels = std::format("{}", config_generation());
  if (input == m.ranked_query && labels == m.ranked_labels) return m.results;
  m.ranked_query = input;
  m.ranked_labels = labels;
  std::vector<Entry>& out = m.results;
  out.clear();
  const std::string query = trim(input);
  const auto subset = [&](const char* prefix, auto&& emit) {
    if (!input.starts_with(prefix)) return false;
    emit(input.substr(std::string_view(prefix).size()));
    return true;
  };
  if (query.starts_with("/")) {
    const bool handled =
        subset("/variant ", [&](const std::string& filter) {
          std::vector<Named> list(std::begin(variants), std::end(variants));
          for (const auto& v : rank<Named>(filter, list, [](const Named& n) { return std::string(n.name); }))
            out.push_back({std::string("variant-") + v.value, v.name, "Set MD3 palette variant", "", true, v.icon,
                           "set-variant", v.value});
        }) ||
        subset("/scheme ", [&](const std::string& filter) {
          const std::vector<Named> modes{{"Dark", "dark", "dark_mode"}, {"Light", "light", "light_mode"}};
          for (const auto& v : rank<Named>(filter, modes, [](const Named& n) { return std::string(n.name); }))
            out.push_back({std::string("mode-") + v.value, v.name, "Set Matugen color mode", "", true, v.icon,
                           "set-mode", v.value});
        }) ||
        subset("/calc ", [&](const std::string& rest) {
          const std::string expression = trim(rest);
          out.push_back({"calc", calculate(expression),
                         expression.empty() ? "Use numbers and + - * / %" : expression, "", true, "calculate",
                         "calc", ""});
        }) ||
        subset("/emoji ", [&](const std::string& filter) {
          std::vector<Named> list(std::begin(emojis), std::end(emojis));
          for (const auto& v : rank<Named>(filter, list, [](const Named& n) { return std::string(n.name); }))
            out.push_back({std::string("emoji-") + v.name, v.name, "Copy to clipboard", v.value, true, "",
                           "copy-emoji", v.value});
        }) ||
        subset("/outline ", [&](const std::string& filter) {
          std::vector<Named> list(std::begin(outline_colours), std::end(outline_colours));
          for (const auto& v : rank<Named>(filter, list, [](const Named& n) { return std::string(n.name); }))
            out.push_back({std::string("outline-") + v.value, v.name, "Set shell outline color", "", true,
                           "border_outer", "set-outline", v.value});
        });
    if (!handled) {
      for (const auto& command : rank<Command>(query.substr(1), commands(), [](const Command& c) {
             return std::string(c.name) + " " + c.command;
           }))
        out.push_back({std::string("action-") + command.command, command.name, command.description, "", true,
                       command.icon, command.command, ""});
    }
    return out;
  }
  out = ranked_apps(input);
  return out;
}

void close() { shell().launcher_open = false; }

void set_query(const std::string& text) {
  shell().query = text;
  shell().selected = 0;
  vw::reset_text_field(search_id);
}

void choose_wallpaper(const Wallpaper& wallpaper) {
  set_wallpaper(wallpaper.path);
  close();
}

void launch(std::size_t index) {
  if (showing_wallpapers()) {
    const Model& m = model();
    if (m.wallpaper_index < m.wallpapers.size()) choose_wallpaper(m.wallpapers[m.wallpaper_index]);
    return;
  }
  const auto& list = results();
  if (index >= list.size()) return;
  const Entry entry = list[index];
  Config& c = config();
  if (!entry.action) {
    try {
      vw::applications::launch(entry.id);
      if (c.launcher_record_usage) {
        AppUsage& usage = c.launcher_usage[entry.id];
        ++usage.count;
        usage.last = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        save_config();
      }
    } catch (const std::exception&) {
    }
    if (c.launcher_close_on_launch) close();
    return;
  }
  const std::string& command = entry.command;
  if (command == "wallpaper" || command == "calc" || command == "variant" || command == "scheme" ||
      command == "emoji" || command == "outline") {
    set_query("/" + command + " ");
  } else if (command == "set-variant") {
    c.matugen_scheme = entry.value;
    save_config();
    regenerate_colours();
    close();
  } else if (command == "set-mode") {
    c.matugen_mode = entry.value;
    save_config();
    regenerate_colours();
    close();
  } else if (command == "random-wallpaper") {
    const auto& walls = model().wallpapers;
    if (!walls.empty()) {
      static std::mt19937 random{std::random_device{}()};
      set_wallpaper(walls[std::uniform_int_distribution<std::size_t>(0, walls.size() - 1)(random)].path);
    }
    close();
  } else if (command == "set-light" || command == "set-dark") {
    c.matugen_mode = command == "set-light" ? "light" : "dark";
    save_config();
    regenerate_colours();
    close();
  } else if (command == "toggle-icon-tint") {
    c.launcher_tint_app_icons = !c.launcher_tint_app_icons;
    save_config();
  } else if (command == "toggle-tray-tint") {
    c.bar_tint_tray_icons = !c.bar_tint_tray_icons;
    save_config();
  } else if (command == "set-outline") {
    c.sdf_outline_color_role = entry.value;
    c.sdf_outline_enabled = true;
    save_config();
    close();
  } else if (command == "copy-emoji") {
    vw::set_clipboard(entry.value);
    close();
  } else if (command == "quickshare") {
    shell().quick_share_open = true;
    close();
  } else if (command == "lock") {
    vw::spawn({"loginctl", "lock-session"});
    close();
  } else if (command == "suspend-then-hibernate") {
    vw::spawn({"systemctl", "suspend-then-hibernate"});
    close();
  }
}

float wallpaper_width(const Screen& screen) {
  const Config& c = config();
  return std::min(static_cast<float>(c.wallpaper_launcher_max_width),
                  std::max(static_cast<float>(c.launcher_width), screen.width * c.wallpaper_launcher_width_ratio));
}

void move_wallpaper(int step) {
  Model& m = model();
  if (m.wallpapers.empty()) return;
  const auto count = static_cast<long>(m.wallpapers.size());
  m.wallpaper_index = static_cast<std::size_t>(((static_cast<long>(m.wallpaper_index) + step) % count + count) % count);
}

// --- Drawing -----------------------------------------------------------------------

void draw_row(vw::Ui& ui, const Entry& entry, std::size_t index, float width, std::vector<vw::Widget>& parts) {
  const Colours& c = colours();
  const float y = 0;
  const float centre = row_height() / 2;
  // Hover state layer (the highlight itself is drawn under the list).
  parts.push_back(ui.rectangle({.width = width, .height = row_height(), .radius = row_height() / 2,
                                .color = with_alpha(c.surface_container_high, 0),
                                .input = {.on_click = [index, id = entry.id, app = !entry.action](const vw::ClickEvent& click) {
                                            if (app && click.button == 273) {  // right click
                                              toggle_listed(config().launcher_favorites, id);
                                              save_config();
                                              return;
                                            }
                                            shell().selected = index;
                                            launch(index);
                                          },
                                          .hover = vw::Hover{.color = c.surface_container_high, .duration = 0.1F}}}));
  if (!entry.action) {
    if (!entry.icon.empty()) {
      try {
        parts.push_back(ui.image({.path = entry.icon, .width = icon_size(), .height = icon_size(), .fit = vw::ImageFit::contain,
                                  .effect = config().launcher_tint_app_icons ? draw::icon_tint() : vw::ImageEffect{},
                                  .place = {.x = 10, .y = centre - icon_size() / 2}}));
      } catch (const std::exception&) {
      }
    }
  } else if (entry.command == "set-outline") {
    parts.push_back(ui.rectangle({.width = 24, .height = 24, .radius = 7, .color = role_colour(entry.value),
                                  .border_width = 1, .border_color = c.outline, .place = {.x = 14, .y = centre - 12}}));
  } else if (entry.command == "copy-emoji") {
    const auto widget = ui.text({.text = entry.icon, .font = "Noto Color Emoji", .size = 28, .color = c.primary,
                                 .place = {.x = 14, .y = 0}});
    ui.move(widget, 0, centre - widget.height / 2);
    parts.push_back(widget);
  } else {
    const auto widget = ui.text({.text = entry.action_icon, .font = font::icon, .size = 26, .color = c.primary,
                                 .place = {.x = 14, .y = 0}});
    ui.move(widget, 0, centre - widget.height / 2);
    parts.push_back(widget);
  }
  const bool app = !entry.action;
  const float text_x = app ? 10 + icon_size() + 12 : 52;
  const bool starred = app && config().launcher_show_favorite_star && listed(config().launcher_favorites, entry.id);
  const float text_width = width - text_x - (starred ? 44 : 12);
  if (starred) {
    const auto star = ui.text({.text = "star", .font = font::icon, .size = 18, .color = c.primary, .place = {.x = width - 34, .y = 0}});
    ui.move(star, 0, centre - star.height / 2);
    parts.push_back(star);
  }
  const auto name = ui.text({.text = entry.name, .font = font::display, .size = 14 * props::font_scale, .weight = vw::weight::semibold,
                             .color = c.text, .max_width = text_width, .place = {.x = text_x, .y = 0}});
  if (entry.comment.empty() || (app && !config().launcher_show_comments)) {
    ui.move(name, 0, centre - name.height / 2);
    parts.push_back(name);
  } else {
    const auto comment = ui.text({.text = entry.comment, .font = font::display, .size = 10 * props::font_scale, .color = c.text_muted,
                                  .max_width = text_width, .place = {.x = text_x, .y = 0}});
    const float block = name.height + 1 + comment.height;
    ui.move(name, 0, centre - block / 2);
    ui.move(comment, 0, centre - block / 2 + name.height + 1);
    parts.push_back(name);
    parts.push_back(comment);
  }
  (void)y;
}

void draw_results(vw::Ui& ui, const blob::Box& box, float list_x, float list_y, float list_width, float list_height,
                  float reveal) {
  const Colours& c = colours();
  const auto& list = results();
  Shell& state = shell();
  if (state.selected >= list.size()) state.selected = list.empty() ? 0 : list.size() - 1;
  const float lift = (1 - reveal) * 18;  // translate y like the QML list
  const float top = list_y + lift;
  // Scroll the selection into view when it changes (the wheel scrolls freely
  // otherwise); the highlight glides between rows (230 ms OutCubic).
  const float stride = row_height() + row_spacing;
  const float content = list.empty() ? 0 : static_cast<float>(list.size()) * stride - row_spacing;
  static std::size_t kept = static_cast<std::size_t>(-1);
  float offset = std::clamp(vw::scroll_offset(list_id), 0.0F, std::max(0.0F, content - list_height));
  if (state.selected != kept && !list.empty()) {
    kept = state.selected;
    const float row = static_cast<float>(state.selected) * stride;
    if (row < offset) offset = row;
    else if (row + row_height() > offset + list_height) offset = row + row_height() - list_height;
  }
  vw::set_scroll_offset(list_id, offset);
  if (!list.empty()) {
    const float y = static_cast<float>(vw::animate("launcher.highlight", static_cast<float>(state.selected) * stride,
                                                   {.duration = 0.23, .easing = vw::Easing::out_cubic}));
    ui.rectangle({.width = list_width, .height = row_height(), .radius = row_height() / 2, .color = c.primary_container,
                  .place = {.x = list_x, .y = top + y - offset,
                            .clip = vw::Clip{.x = list_x, .y = list_y, .width = list_width, .height = list_height}}});
  }
  // A wheel notch (15 px of axis) moves about one row.
  ui.listview({.id = list_id, .width = list_width, .height = list_height, .item_height = row_height(),
               .gap = row_spacing, .scroll_step = 150, .count = list.size(),
               .render = [&ui, &list, list_width](std::size_t index) -> std::optional<vw::Widget> {
                 std::vector<vw::Widget> parts;
                 draw_row(ui, list[index], index, list_width, parts);
                 if (parts.empty()) return std::nullopt;
                 vw::Widget all = parts.front();
                 for (const auto& part : parts) {
                   all.begin = std::min(all.begin, part.begin);
                   all.end = std::max(all.end, part.end);
                 }
                 all.width = list_width;
                 all.height = row_height();
                 return all;
               },
               .place = {.x = list_x, .y = top}});
  (void)box;
}

// The carousel's cards: the chosen one large, the rest small, side by side.
constexpr float card_big_width = 320;
constexpr float card_big_height = 200;
constexpr float card_small_width = 176;
constexpr float card_small_height = 110;
constexpr float card_label = 26;

float carousel_height() {
  return std::max(static_cast<float>(config().wallpaper_carousel_height), card_big_height + card_label + 16);
}

// Where the card `relative` places from the chosen one sits: its centre,
// from the carousel's, and its size from big (0) to small (1 and beyond).
struct CardPlace {
  float offset;
  float grow;  // 1 big, 0 small
};

CardPlace card_place(float relative) {
  const float gap = static_cast<float>(std::clamp(config().wallpaper_item_gap, 0, 120));
  const float first = card_big_width / 2 + gap + card_small_width / 2;  // centre to the next centre
  const float pitch = card_small_width + gap;
  const float distance = std::abs(relative);
  const float offset = distance <= 1 ? distance * first : first + (distance - 1) * pitch;
  return {relative < 0 ? -offset : offset, 1 - std::min(1.0F, distance)};
}

struct CarouselDrag {
  bool active = false;
  bool moved = false;
  float pixels = 0;          // dragged so far, to the right
  double from = 0;           // the index the drag started on
  float scroll = 0;          // wheel and touchpad travel not yet a step
};

CarouselDrag& carousel_drag() {
  static CarouselDrag drag;
  return drag;
}

// A click `here` pixels from the carousel's centre: a side card comes to
// the middle, the middle one becomes the wallpaper.
void carousel_click(float here, long count) {
  long best = 0;
  float nearest = 1e9F;
  for (long k = -count; k <= count; ++k) {
    const CardPlace place = card_place(static_cast<float>(k));
    const float half = (place.grow > 0.5F ? card_big_width : card_small_width) / 2;
    const float distance = std::abs(here - place.offset);
    if (distance <= half && distance < nearest) {
      nearest = distance;
      best = k;
    }
  }
  if (nearest > 1e8F) return;
  Model& m = model();
  const auto index = static_cast<std::size_t>(((static_cast<long>(m.wallpaper_index) + best) % count + count) % count);
  m.wallpaper_index = index;
  if (best == 0) choose_wallpaper(m.wallpapers[index]);
}

void draw_carousel(vw::Ui& ui, float x, float y, float width, float height) {
  const Colours& c = colours();
  Model& m = model();
  const auto count = static_cast<long>(m.wallpapers.size());
  if (count == 0) return;
  CarouselDrag& drag = carousel_drag();
  const float gap = static_cast<float>(std::clamp(config().wallpaper_item_gap, 0, 120));
  const float pitch = card_small_width + gap;

  // The position glides to the chosen index the short way round; a drag
  // moves it directly, by a card per pitch dragged.
  static double shown = static_cast<double>(m.wallpaper_index);
  double target = static_cast<double>(m.wallpaper_index);
  if (drag.active) {
    target = drag.from - drag.pixels / pitch;
  } else {
    double delta = std::fmod(target - shown, static_cast<double>(count));
    if (delta > count / 2.0) delta -= count;
    if (delta < -count / 2.0) delta += count;
    target = shown + delta;
  }
  const double position = vw::animate("launcher.carousel", target,
                                      {.duration = drag.active ? 0.0 : 0.3, .easing = vw::Easing::out_cubic});
  if (!drag.active && std::abs(position - target) < 1e-3) shown = static_cast<double>(m.wallpaper_index);

  const float centre_x = x + width / 2;
  const float image_bottom = y + (height - card_label) / 2 + card_big_height / 2;
  const vw::Clip clip{.x = x, .y = y, .width = width, .height = height};
  const vw::Widget first = ui.hit_area({.width = 1, .height = 1, .place = {.x = x, .y = y}});

  // Behind the cards: dragging, the wheel and clicks all land here.
  ui.hit_area({.width = width, .height = height, .place = {.x = x, .y = y},
               .input = {.on_click = [centre_x, count](const vw::ClickEvent& click) {
                           carousel_click(static_cast<float>(click.x) - centre_x, count);
                         },
                         .on_drag_start = [](const vw::DragEvent&) {
                           CarouselDrag& state = carousel_drag();
                           state.active = true;
                           state.moved = false;
                           state.pixels = 0;
                           state.from = static_cast<double>(model().wallpaper_index);
                         },
                         .on_drag = [](const vw::DragEvent& event) {
                           CarouselDrag& state = carousel_drag();
                           state.pixels = static_cast<float>(event.dx);
                           if (std::abs(event.dx) > 6) state.moved = true;
                         },
                         .on_drag_end = [pitch, count, centre_x](const vw::DragEvent& event) {
                           CarouselDrag& state = carousel_drag();
                           state.active = false;
                           // Barely moved: a left click, which the drag took.
                           if (!state.moved) {
                             state.pixels = 0;
                             carousel_click(static_cast<float>(event.start_x) - centre_x, count);
                             return;
                           }
                           state.moved = false;
                           // Land on the nearest card.
                           const auto steps = std::lround(-state.pixels / pitch);
                           Model& model_now = model();
                           model_now.wallpaper_index = static_cast<std::size_t>(
                               ((static_cast<long>(state.from) + steps) % count + count) % count);
                           state.pixels = 0;
                         },
                         .on_scroll = [](const vw::ScrollEvent& event) {
                           CarouselDrag& state = carousel_drag();
                           state.scroll += static_cast<float>(event.delta);
                           while (std::abs(state.scroll) >= 40) {
                             move_wallpaper(state.scroll > 0 ? 1 : -1);
                             state.scroll -= state.scroll > 0 ? 40 : -40;
                           }
                         },
                         .on_scroll_x = [](const vw::ScrollEvent& event) {
                           CarouselDrag& state = carousel_drag();
                           state.scroll += static_cast<float>(event.delta);
                           while (std::abs(state.scroll) >= 40) {
                             move_wallpaper(state.scroll > 0 ? 1 : -1);
                             state.scroll -= state.scroll > 0 ? 40 : -40;
                           }
                         }}});

  // Only the cards that can show are built, the small ones first so the
  // big one draws over its neighbours as it grows.
  const long base = static_cast<long>(std::floor(position));
  const long reach = static_cast<long>(width / 2 / pitch) + 2;
  std::vector<std::pair<float, long>> order;
  for (long k = -reach; k <= reach + 1; ++k) order.emplace_back(std::abs(static_cast<float>(static_cast<double>(base + k) - position)), k);
  std::ranges::sort(order, std::greater<>{}, &std::pair<float, long>::first);
  for (const auto& [distance, k] : order) {
    const long index = ((base + k) % count + count) % count;
    const float relative = static_cast<float>(static_cast<double>(base + k) - position);
    const CardPlace place = card_place(relative);
    const float image_width = card_small_width + (card_big_width - card_small_width) * place.grow;
    const float image_height = card_small_height + (card_big_height - card_small_height) * place.grow;
    const float cx = centre_x + place.offset;
    if (cx + image_width / 2 < x || cx - image_width / 2 > x + width) continue;
    const Wallpaper& wallpaper = m.wallpapers[static_cast<std::size_t>(index)];
    const float left = cx - image_width / 2;
    const float top = image_bottom - image_height;
    const float radius = 12 + 6 * place.grow;
    draw::rect(ui, left, top, image_width, image_height, radius, c.surface_container);
    try {
      ui.image({.path = wallpaper.path, .width = image_width, .height = image_height, .fit = vw::ImageFit::cover,
                .radius = radius, .place = {.x = left, .y = top}});
    } catch (const std::exception&) {
    }
    ui.text({.text = wallpaper.name, .font = font::display, .size = 11 + 2 * place.grow,
             .color = place.grow > 0.5F ? c.text : c.text_muted, .max_width = image_width, .align = "center",
             .place = {.x = left, .y = image_bottom + 8}});
  }
  const vw::Widget last = ui.hit_area({.width = 1, .height = 1, .place = {.x = x, .y = y}});
  // Everything, images and input alike, stays inside the launcher.
  ui.clip(vw::Widget{first.begin, last.end, width, height}, clip);
}

}  // namespace

Panel launcher_panel(const Screen& screen) {
  Shell& state = shell();
  static bool was_open = false;
  if (state.launcher_open && !was_open) load();  // fresh apps and wallpapers per opening
  was_open = state.launcher_open;
  const float reveal = static_cast<float>(vw::animate("launcher.reveal", state.launcher_open ? 1 : 0,
                                                      {.duration = 0.26, .easing = vw::Easing::out_cubic}));
  if (!state.launcher_open && reveal < 0.002F) {
    if (!state.query.empty()) set_query("");
    state.selected = 0;
    vw::set_scroll_offset(list_id, 0);
    return {};
  }
  if (!model().loaded) load();
  const bool walls = showing_wallpapers();
  const float target_width = walls ? wallpaper_width(screen) : static_cast<float>(config().launcher_width);
  float content = carousel_height();
  if (!walls) {
    const auto count = static_cast<float>(results().size());
    content = count > 0 ? count * row_height() + (count - 1) * row_spacing : 0;
  }
  const float target_height = std::min(static_cast<float>(std::clamp(config().launcher_max_height, 200, 1000)), search_implicit + content + 55);
  const float width = static_cast<float>(vw::animate("launcher.width", target_width, motion::bounce()));
  const float height = static_cast<float>(vw::animate("launcher.height", target_height, motion::bounce()));
  Panel panel;
  panel.reveal = reveal;
  // Sinks into the bar, and runs on behind the screen edge so its bottom
  // corners never show: it joins the bar as one surface.
  const auto sink = static_cast<float>(std::clamp(config().launcher_sink, 0, 80));
  panel.box = {(screen.width - width) / 2, screen.height - bar_height + (sink - height) * reveal, width,
               height + bottom_extension(reveal), {30, 30, launcher_radius, launcher_radius}};
  return panel;
}

void draw_launcher(vw::Ui& ui, const Panel& panel) {
  if (!panel.visible() || panel.reveal < 0.02F) return;
  Shell& state = shell();
  const Colours& c = colours();
  const blob::Box& box = panel.box;
  const float reveal = panel.reveal;
  const bool walls = showing_wallpapers();

  // Search surface at the bottom.
  const float search_x = box.x + 6;
  const float search_y = box.y + box.height - bottom_extension(reveal) - 6 - search_height + (1 - reveal) * 16;
  const float search_width = box.width - 12;
  draw::rect(ui, search_x, search_y, search_width, search_height, search_height / 2, c.surface_container);
  ui.text_field({.id = search_id, .value = state.query, .placeholder = "Search applications",
                 .width = search_width - 20, .height = search_height, .radius = 0, .border_width = 0, .padding = 4,
                 .color = with_alpha(c.surface_container, 0), .focus_color = with_alpha(c.primary, 0),
                 .text_color = c.text, .placeholder_color = c.outline, .caret_color = c.text,
                 .font = font::display, .size = 16, .autofocus = true,
                 .on_change = [](const std::string& value) {
                   shell().query = value;
                   shell().selected = 0;
                   vw::set_scroll_offset(list_id, 0);
                 },
                 .place = {.x = search_x + 10, .y = search_y}});

  // Results or the wallpaper carousel above it.
  const float list_x = box.x + 6;
  const float list_y = box.y + 6;
  const float list_width = box.width - 12;
  const float list_height = search_y - 4 - list_y;
  if (list_height > 4) {
    if (walls) draw_carousel(ui, list_x, list_y, list_width, list_height);
    else draw_results(ui, box, list_x, list_y, list_width, list_height, reveal);
  }

  ui.keyboard([](const vw::KeyEvent& key) {
    Shell& current = shell();
    const bool carousel = showing_wallpapers();
    if (key.name == "Escape") {
      close();
    } else if (key.name == "Up") {
      if (carousel) move_wallpaper(-1);
      else if (current.selected > 0) --current.selected;
    } else if (key.name == "Down") {
      if (carousel) move_wallpaper(1);
      else if (current.selected + 1 < results().size()) ++current.selected;
    } else if (carousel && key.name == "Left") {
      move_wallpaper(-1);
    } else if (carousel && key.name == "Right") {
      move_wallpaper(1);
    } else if (key.name == "Return" || key.name == "KP_Enter") {
      const std::string root = trim(current.query);
      if (root == "/wallpaper" || root == "/calc" || root == "/variant" || root == "/scheme" || root == "/emoji" ||
          root == "/outline")
        set_query(root + " ");
      else
        launch(current.selected);
    } else {
      return false;  // typing goes to the search field
    }
    return true;
  }, vw::Keyboard::exclusive);
}

}  // namespace blx
