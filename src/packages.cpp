#include "packages.hpp"

#include <nlohmann/json.hpp>
#include <vw/hyprland.hpp>
#include <vw/zwwm.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <format>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <fcntl.h>

#include "config.hpp"
#include "halley.hpp"

#ifndef BLXSHELL_VERSION
#define BLXSHELL_VERSION "0.0.0"
#endif

extern char** environ;

namespace blx::packages {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

// Compositors a manifest may name. Others only warn, so a package made for a
// newer blxshell that knows more of them still installs.
constexpr std::array known_compositors{"zwwm", "hyprland", "halley"};
constexpr std::array known_permissions{"shell", "notifications", "network", "filesystem", "spawn"};
// A theme is one colour for each of these roles: no generated palette, no
// light/dark variants (a light theme is just another list).
constexpr std::array colour_roles{
    "primary", "primaryText", "primaryContainer", "secondary", "secondaryContainer", "tertiary",
    "tertiaryContainer", "error", "errorText", "errorContainer", "errorContainerText", "surface",
    "surfaceContainer", "surfaceContainerLow", "surfaceContainerHigh", "surfaceContainerHighest",
    "surfaceVariant", "text", "textMuted", "outline", "outlineVariant"};

std::string home() {
  const char* value = std::getenv("HOME");
  return value != nullptr ? value : ".";
}

fs::path data_home() {
  const char* value = std::getenv("XDG_DATA_HOME");
  return value != nullptr && *value ? fs::path(value) : fs::path(home()) / ".local/share";
}

std::vector<fs::path> data_dirs() {
  const char* value = std::getenv("XDG_DATA_DIRS");
  std::string list = value != nullptr && *value ? value : "/usr/local/share:/usr/share";
  std::vector<fs::path> result;
  std::stringstream stream(list);
  for (std::string entry; std::getline(stream, entry, ':');)
    if (!entry.empty()) result.emplace_back(entry);
  return result;
}

const char* folder(Kind kind) { return kind == Kind::theme ? "themes" : "plugins"; }

std::string slurp(const fs::path& file) {
  std::ifstream input(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// --- versions ---------------------------------------------------------------

using Version = std::array<int, 3>;

std::optional<Version> parse_version(std::string_view text) {
  if (text.starts_with('v')) text.remove_prefix(1);
  // Pre-release and build parts don't take part in range checks.
  text = text.substr(0, text.find_first_of("-+"));
  // One to three dot-separated numbers; missing parts are 0 ("0.2" = 0.2.0).
  Version version{0, 0, 0};
  std::size_t part = 0;
  std::size_t start = 0;
  for (;;) {
    if (part == 3) return std::nullopt;  // a fourth part
    const std::size_t end = std::min(text.find('.', start), text.size());
    const std::string_view number = text.substr(start, end - start);
    if (number.empty() || number.size() > 6 || !std::ranges::all_of(number, [](char c) { return c >= '0' && c <= '9'; }))
      return std::nullopt;
    version[part++] = std::stoi(std::string(number));
    if (end == text.size()) return version;
    start = end + 1;
  }
}

bool valid_semver(const std::string& text) {
  if (std::ranges::count(text, '.') < 2) return false;
  return parse_version(text).has_value();
}

// "*", "0.2.0", "=0.2.0", ">=0.1.0 <0.3.0", "^0.1.0", "~0.1.2", joined by
// spaces (all must hold) or "||" (any group may).
std::optional<bool> satisfies(const std::string& range, const Version& version) {
  if (range.empty() || range == "*") return true;
  bool any = false;
  std::size_t start = 0;
  while (start <= range.size()) {
    const std::size_t end = std::min(range.find("||", start), range.size());
    std::istringstream group(range.substr(start, end - start));
    bool all = true;
    for (std::string token; group >> token;) {
      if (token == "*") continue;
      std::string op;
      while (!token.empty() && std::string_view("<>=^~").find(token.front()) != std::string_view::npos) {
        op += token.front();
        token.erase(0, 1);
      }
      const auto bound = parse_version(token);
      if (!bound) return std::nullopt;
      const auto compare = version <=> *bound;
      bool ok = false;
      if (op.empty() || op == "=") ok = compare == 0;
      else if (op == ">=") ok = compare >= 0;
      else if (op == ">") ok = compare > 0;
      else if (op == "<=") ok = compare <= 0;
      else if (op == "<") ok = compare < 0;
      else if (op == "^") {
        // Same leftmost non-zero part, and not lower.
        Version upper = *bound;
        if (upper[0] > 0) upper = {upper[0] + 1, 0, 0};
        else if (upper[1] > 0) upper = {0, upper[1] + 1, 0};
        else upper = {0, 0, upper[2] + 1};
        ok = compare >= 0 && version < upper;
      } else if (op == "~") {
        ok = compare >= 0 && version < Version{(*bound)[0], (*bound)[1] + 1, 0};
      } else {
        return std::nullopt;
      }
      all = all && ok;
    }
    any = any || all;
    start = end + 2;
  }
  return any;
}

// --- manifest ---------------------------------------------------------------

bool valid_id(const std::string& id) {
  if (id.size() < 3 || id.size() > 128 || id.find('.') == std::string::npos) return false;
  if (!(id.front() >= 'a' && id.front() <= 'z')) return false;
  return std::ranges::all_of(id, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
  });
}

struct Reader {
  Manifest& m;
  const json& object;
  std::string where;

  template <typename T>
  bool get(const char* key, T& target, bool required = false) {
    if (!object.is_object() || !object.contains(key)) {
      if (required) m.errors.push_back(std::format("{}\"{}\" is missing", where, key));
      return false;
    }
    try {
      target = object.at(key).get<T>();
      return true;
    } catch (const std::exception&) {
      m.errors.push_back(std::format("{}\"{}\" has the wrong type", where, key));
      return false;
    }
  }

  void only(std::initializer_list<const char*> keys) {
    if (!object.is_object()) return;
    for (const auto& [key, value] : object.items())
      if (std::ranges::find_if(keys, [&](const char* known) { return key == known; }) == keys.end())
        m.warnings.push_back(std::format("{}unknown key \"{}\"", where, key));
  }
};

bool has_file(const std::vector<std::string>& files, const std::string& path) {
  return files.empty() || std::ranges::find(files, path) != files.end();
}

void check_colour(Manifest& m, const std::string& where, const json& value) {
  const auto ok = [](const std::string& text) {
    return (text.size() == 7 || text.size() == 9) && text[0] == '#' &&
           std::all_of(text.begin() + 1, text.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
  };
  if (!value.is_string() || !ok(value.get<std::string>()))
    m.errors.push_back(std::format("{} must be a colour like \"#6750a4\"", where));
}

void check_roles(Manifest& m, const json& roles, bool all) {
  if (!roles.is_object()) {
    m.errors.push_back("colours.roles must be an object of role: \"#rrggbb\"");
    return;
  }
  if (all)
    for (const char* role : colour_roles)
      if (!roles.contains(role)) m.errors.push_back(std::format("colours.roles: \"{}\" is missing (a static theme sets every role)", role));
  for (const auto& [role, value] : roles.items()) {
    if (std::ranges::find(colour_roles, role) == colour_roles.end()) {
      m.warnings.push_back(std::format("colours.roles: unknown role \"{}\"", role));
      continue;
    }
    check_colour(m, "colours.roles." + role, value);
  }
}

// "static": a fixed colour for every role. "generated": matugen settings
// (from the wallpaper or a seed), optionally with a few roles pinned.
void parse_colours(Manifest& m, const json& colours) {
  if (!colours.is_object()) {
    m.errors.push_back("\"colours\" must be an object");
    return;
  }
  Reader r{m, colours, "colours: "};
  std::string kind;
  if (!r.get("kind", kind, true)) return;
  if (kind == "static") {
    r.only({"kind", "roles"});
    if (!colours.contains("roles")) m.errors.push_back("a static theme needs colours.roles with every role");
    else check_roles(m, colours["roles"], true);
    return;
  }
  if (kind != "generated") {
    m.errors.push_back("colours.kind must be \"static\" or \"generated\"");
    return;
  }
  r.only({"kind", "source", "seed", "mode", "scheme", "contrast", "prefer", "preset", "roles"});
  std::string text;
  if (r.get("source", text) && text != "wallpaper" && text != "seed")
    m.errors.push_back("colours.source must be \"wallpaper\" or \"seed\"");
  if (text == "seed" && !colours.contains("seed")) m.errors.push_back("colours.source \"seed\" needs colours.seed");
  if (colours.contains("seed")) check_colour(m, "colours.seed", colours["seed"]);
  if (r.get("mode", text) && text != "dark" && text != "light") m.errors.push_back("colours.mode must be \"dark\" or \"light\"");
  static constexpr std::array schemes{"tonal-spot", "content", "expressive", "fidelity", "fruit-salad",
                                     "monochrome", "neutral", "rainbow", "vibrant"};
  if (r.get("scheme", text) && std::ranges::find(schemes, text) == schemes.end())
    m.errors.push_back(std::format("colours.scheme \"{}\" is not a matugen scheme", text));
  if (r.get("prefer", text) && text != "saturation" && text != "darkness" && text != "lightness")
    m.errors.push_back("colours.prefer must be saturation, darkness or lightness");
  if (r.get("preset", text) && text != "generated" && text != "neutral" && text != "amoled" && text != "mono")
    m.errors.push_back("colours.preset must be generated, neutral, amoled or mono");
  double contrast = 0;
  if (r.get("contrast", contrast) && (contrast < -1 || contrast > 1)) m.errors.push_back("colours.contrast must be -1..1");
  if (colours.contains("roles")) check_roles(m, colours["roles"], false);
}

void parse_plugin(Manifest& m, const json& plugin, const std::vector<std::string>& files) {
  if (!plugin.is_object()) {
    m.errors.push_back("\"plugin\" must be an object");
    return;
  }
  Reader r{m, plugin, "plugin: "};
  r.only({"api", "exec", "restart", "permissions", "barModules", "desktop", "commands", "settings"});
  if (r.get("api", m.api, true) && (m.api < 1 || m.api > api_version))
    m.errors.push_back(std::format("plugin.api {} is not supported (this blxshell speaks {})", m.api, api_version));
  if (r.get("exec", m.exec)) {
    if (m.exec.empty() || m.exec[0].empty()) m.errors.push_back("plugin.exec must name a program");
    else if (m.exec[0].starts_with('/') || m.exec[0].find("..") != std::string::npos)
      m.errors.push_back("plugin.exec[0] must be a path inside the package");
    else if (!has_file(files, m.exec[0]))
      m.errors.push_back(std::format("plugin.exec[0] \"{}\" is not in the package", m.exec[0]));
  }
  if (r.get("restart", m.restart) && m.restart != "never" && m.restart != "on-failure" && m.restart != "always")
    m.errors.push_back("plugin.restart must be never, on-failure or always");
  if (r.get("permissions", m.permissions))
    for (const std::string& permission : m.permissions)
      if (std::ranges::find(known_permissions, permission) == known_permissions.end())
        m.warnings.push_back(std::format("plugin.permissions: unknown permission \"{}\"", permission));
  r.get("commands", m.commands);
  if (plugin.contains("barModules")) {
    if (!plugin["barModules"].is_array()) m.errors.push_back("plugin.barModules must be an array");
    else
      for (const json& entry : plugin["barModules"]) {
        BarModule module;
        Reader mr{m, entry, "plugin.barModules[]: "};
        mr.only({"id", "name", "description"});
        mr.get("id", module.id, true);
        mr.get("name", module.name);
        mr.get("description", module.description);
        if (!module.id.empty() && !std::ranges::all_of(module.id, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; }))
          m.errors.push_back(std::format("bar module id \"{}\" may hold only letters, digits, - and _", module.id));
        m.bar_modules.push_back(std::move(module));
      }
  }
  if (plugin.contains("desktop")) {
    if (!plugin["desktop"].is_array()) m.errors.push_back("plugin.desktop must be an array");
    else
      for (const json& entry : plugin["desktop"]) {
        BarModule area;
        Reader ar{m, entry, "plugin.desktop[]: "};
        ar.only({"id", "name", "description"});
        ar.get("id", area.id, true);
        ar.get("name", area.name);
        ar.get("description", area.description);
        if (!area.id.empty() && !std::ranges::all_of(area.id, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; }))
          m.errors.push_back(std::format("desktop area id \"{}\" may hold only letters, digits, - and _", area.id));
        m.desktop_areas.push_back(std::move(area));
      }
  }
  if (!m.bar_modules.empty() && m.exec.empty()) m.errors.push_back("plugin.barModules needs plugin.exec to fill them");
  if (!m.desktop_areas.empty() && m.exec.empty()) m.errors.push_back("plugin.desktop needs plugin.exec to fill it");
  if (plugin.contains("settings")) {
    if (!plugin["settings"].is_array()) m.errors.push_back("plugin.settings must be an array");
    else
      for (const json& entry : plugin["settings"]) {
        Setting setting;
        Reader sr{m, entry, "plugin.settings[]: "};
        sr.only({"key", "type", "label", "description", "default", "choices"});
        sr.get("key", setting.key, true);
        sr.get("type", setting.type, true);
        sr.get("label", setting.label);
        sr.get("description", setting.description);
        sr.get("choices", setting.choices);
        if (entry.is_object() && entry.contains("default")) setting.default_json = entry["default"].dump();
        if (setting.type != "bool" && setting.type != "number" && setting.type != "string" && setting.type != "choice")
          m.errors.push_back(std::format("setting \"{}\": type must be bool, number, string or choice", setting.key));
        m.settings.push_back(std::move(setting));
      }
  }
}

}  // namespace

const char* kind_name(Kind kind) { return kind == Kind::theme ? "theme" : "plugin"; }

bool Manifest::has_permission(const std::string& permission) const {
  return std::ranges::find(permissions, permission) != permissions.end();
}

const std::string& compositor() {
  static const std::string name = [] {
    try {
      if (halley::available()) return std::string("halley");
      if (vw::zwwm::available()) return std::string("zwwm");
      if (vw::hyprland::available()) return std::string("hyprland");
    } catch (const std::exception&) {
    }
    return std::string();
  }();
  return name;
}

const char* shell_version() { return BLXSHELL_VERSION; }

std::optional<std::string> incompatibility(const Manifest& manifest) {
  if (!manifest.valid()) return "it has errors";
  if (const auto version = parse_version(shell_version()); version) {
    const auto ok = satisfies(manifest.requires_shell, *version);
    if (!ok) return std::format("its blxshell range \"{}\" can't be read", manifest.requires_shell);
    if (!*ok) return std::format("it needs blxshell {} (this is {})", manifest.requires_shell, shell_version());
  }
  if (std::ranges::find(manifest.compositors, "any") != manifest.compositors.end()) return std::nullopt;
  const std::string& running = compositor();
  if (running.empty()) return std::nullopt;  // no compositor to check against (e.g. a TTY)
  if (std::ranges::find(manifest.compositors, running) == manifest.compositors.end()) {
    std::string list;
    for (const std::string& name : manifest.compositors) list += (list.empty() ? "" : ", ") + name;
    return std::format("it supports {}, not {}", list, running);
  }
  return std::nullopt;
}

Manifest parse(const std::string& text, const std::vector<std::string>& files) {
  Manifest m;
  m.json = text;
  json root;
  try {
    root = json::parse(text);
  } catch (const std::exception& error) {
    m.errors.push_back(std::format("manifest.json is not valid JSON: {}", error.what()));
    return m;
  }
  if (!root.is_object()) {
    m.errors.push_back("manifest.json must be an object");
    return m;
  }
  Reader r{m, root, ""};
  r.only({"$schema", "schema", "type", "id", "name", "version", "description", "authors", "license", "homepage",
          "repository", "tags", "previews", "icon", "blxshell", "compositors", "colours", "plugin"});
  if (r.get("schema", m.schema, true) && (m.schema < 1 || m.schema > schema_version))
    m.errors.push_back(std::format("manifest schema {} is newer than this blxshell reads ({}); update blxshell", m.schema, schema_version));
  std::string type;
  if (r.get("type", type, true)) {
    if (type == "theme") m.kind = Kind::theme;
    else if (type == "plugin") m.kind = Kind::plugin;
    else m.errors.push_back("\"type\" must be \"theme\" or \"plugin\"");
  }
  if (r.get("id", m.id, true) && !valid_id(m.id))
    m.errors.push_back(std::format("id \"{}\" must be reverse-DNS: lowercase letters, digits, '.', '-', '_' (e.g. io.github.you.name)", m.id));
  r.get("name", m.name, true);
  if (r.get("version", m.version, true) && !valid_semver(m.version))
    m.errors.push_back(std::format("version \"{}\" is not semver (major.minor.patch)", m.version));
  r.get("description", m.description);
  r.get("license", m.license);
  r.get("homepage", m.homepage);
  r.get("repository", m.repository);
  r.get("tags", m.tags);
  r.get("icon", m.icon);
  if (!m.icon.empty() && !has_file(files, m.icon)) m.errors.push_back(std::format("icon \"{}\" is not in the package", m.icon));
  if (r.get("previews", m.previews))
    for (const std::string& preview : m.previews)
      if (!has_file(files, preview)) m.errors.push_back(std::format("preview \"{}\" is not in the package", preview));
  if (root.contains("authors")) {
    if (!root["authors"].is_array()) m.errors.push_back("\"authors\" must be an array");
    else
      for (const json& entry : root["authors"]) {
        Author author;
        if (entry.is_string()) author.name = entry.get<std::string>();
        else {
          Reader ar{m, entry, "authors[]: "};
          ar.get("name", author.name, true);
          ar.get("email", author.email);
          ar.get("url", author.url);
        }
        m.authors.push_back(std::move(author));
      }
  }
  if (r.get("blxshell", m.requires_shell) && !satisfies(m.requires_shell, {0, 0, 0}).has_value())
    m.errors.push_back(std::format("blxshell range \"{}\" can't be read (try \">=0.1.0\")", m.requires_shell));
  if (r.get("compositors", m.compositors, true)) {
    if (m.compositors.empty()) m.errors.push_back("\"compositors\" must name at least one (or \"any\")");
    for (const std::string& name : m.compositors)
      if (name != "any" && std::ranges::find(known_compositors, name) == known_compositors.end())
        m.warnings.push_back(std::format("unknown compositor \"{}\"", name));
  }
  if (m.kind == Kind::theme) {
    if (!root.contains("colours")) m.errors.push_back("a theme needs a \"colours\" object");
    else parse_colours(m, root["colours"]);
    if (root.contains("plugin")) m.errors.push_back("a theme can't have a \"plugin\" object");
  } else {
    if (!root.contains("plugin")) m.errors.push_back("a plugin needs a \"plugin\" object");
    else parse_plugin(m, root["plugin"], files);
    if (root.contains("colours")) m.errors.push_back("a plugin can't have a \"colours\" object");
  }
  return m;
}

Manifest read_directory(const fs::path& directory) {
  std::vector<std::string> files;
  std::error_code error;
  for (auto it = fs::recursive_directory_iterator(directory, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
    if (it->is_regular_file()) files.push_back(fs::relative(it->path(), directory).generic_string());
  const fs::path file = directory / manifest_name;
  Manifest m;
  if (!fs::exists(file)) m.errors.push_back("no manifest.json");
  else m = parse(slurp(file), files.empty() ? std::vector<std::string>{std::string(manifest_name)} : files);
  m.directory = directory;
  return m;
}

Manifest read_package(const fs::path& file) {
  std::vector<archive::Entry> entries;
  try {
    entries = archive::read(file);
  } catch (const std::exception& error) {
    Manifest m;
    m.errors.push_back(error.what());
    return m;
  }
  const archive::Entry* manifest = archive::find(entries, manifest_name);
  if (manifest == nullptr || manifest->directory) {
    Manifest m;
    m.errors.push_back("no manifest.json at the root of the package");
    return m;
  }
  std::vector<std::string> files;
  for (const auto& entry : entries)
    if (!entry.directory) files.push_back(entry.path);
  if (!file.string().ends_with(extension)) {
    Manifest m = parse(manifest->data, files);
    m.warnings.push_back(std::format("package files should end in {}", extension));
    return m;
  }
  return parse(manifest->data, files);
}

fs::path user_directory(Kind kind) { return data_home() / "blxshell" / folder(kind); }

std::vector<Manifest> installed(Kind kind) {
  std::vector<fs::path> roots{user_directory(kind)};
  for (const fs::path& dir : data_dirs()) roots.push_back(dir / "blxshell" / folder(kind));
  // The built-in packages next to the binary (<prefix>/share/blxshell), or
  // in the source tree when running from the build folder.
  std::error_code self_error;
  const fs::path self = fs::read_symlink("/proc/self/exe", self_error);
  if (!self_error) roots.push_back(self.parent_path().parent_path() / "share/blxshell" / folder(kind));
#ifdef BLXSHELL_SOURCE_PACKAGE_DIR
  roots.push_back(fs::path(BLXSHELL_SOURCE_PACKAGE_DIR) / folder(kind));
#endif
  std::vector<Manifest> result;
  std::set<std::string> seen;
  for (const fs::path& root : roots) {
    std::error_code error;
    std::vector<fs::path> folders;
    for (const auto& entry : fs::directory_iterator(root, error))
      // Folders without a manifest.json aren't packages (e.g. old Quickshell
      // plugins sharing the folder): skipped, not reported as broken.
      if (entry.is_directory() && !entry.path().filename().string().starts_with('.') &&
          fs::exists(entry.path() / manifest_name, error))
        folders.push_back(entry.path());
    std::ranges::sort(folders);
    for (const fs::path& directory : folders) {
      Manifest m = read_directory(directory);
      if (!m.id.empty() && directory.filename() != m.id)
        m.warnings.push_back(std::format("installed as {} but its id is {}", directory.filename().string(), m.id));
      const std::string key = m.id.empty() ? directory.string() : m.id;
      if (!seen.insert(key).second) continue;
      if (m.valid() && m.kind != kind) m.errors.push_back(std::format("a {} in the {} folder", kind_name(m.kind), folder(kind)));
      result.push_back(std::move(m));
    }
  }
  return result;
}

std::optional<Manifest> find(const std::string& id, std::optional<Kind> kind) {
  for (const Kind k : {Kind::theme, Kind::plugin}) {
    if (kind && *kind != k) continue;
    for (Manifest& m : installed(k))
      if (m.id == id || m.directory.filename() == id) return std::move(m);
  }
  return std::nullopt;
}

bool install(const fs::path& file, std::string& message) {
  std::vector<archive::Entry> entries;
  try {
    entries = archive::read(file);
  } catch (const std::exception& error) {
    message = error.what();
    return false;
  }
  const Manifest m = read_package(file);
  if (!m.valid()) {
    message = "the package has errors:";
    for (const std::string& error : m.errors) message += "\n  " + error;
    return false;
  }
  const fs::path root = user_directory(m.kind);
  const fs::path target = root / m.id;
  const fs::path staging = root / ("." + m.id + ".new");
  const fs::path old = root / ("." + m.id + ".old");
  try {
    fs::create_directories(root);
    fs::remove_all(staging);
    fs::remove_all(old);
    archive::extract(entries, staging);
    // Swap in place, so a running plugin's folder is never half written.
    if (fs::exists(target)) fs::rename(target, old);
    fs::rename(staging, target);
    fs::remove_all(old);
  } catch (const std::exception& error) {
    std::error_code ignored;
    fs::remove_all(staging, ignored);
    if (!fs::exists(target, ignored) && fs::exists(old, ignored)) fs::rename(old, target, ignored);
    message = error.what();
    return false;
  }
  message = std::format("installed {} {} {} to {}", kind_name(m.kind), m.id, m.version, target.string());
  if (const auto why = incompatibility(m)) message += std::format("\nnote: it won't run here: {}", *why);
  return true;
}

// --- Repositories -------------------------------------------------------------

namespace {

// SHA-256 (FIPS 180-4), to check downloads against their index entry.
std::string sha256(const std::string& data) {
  static constexpr std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::string message = data;
  const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;
  message += static_cast<char>(0x80);
  while (message.size() % 64 != 56) message += '\0';
  for (int shift = 56; shift >= 0; shift -= 8) message += static_cast<char>((bits >> shift) & 0xff);
  const auto rotate = [](std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      const auto* b = reinterpret_cast<const unsigned char*>(message.data() + chunk + i * 4);
      w[i] = (std::uint32_t{b[0]} << 24) | (std::uint32_t{b[1]} << 16) | (std::uint32_t{b[2]} << 8) | b[3];
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotate(w[i - 15], 7) ^ rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotate(w[i - 2], 17) ^ rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t t1 = hh + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      const std::uint32_t t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::string hex;
  for (std::uint32_t word : h) hex += std::format("{:08x}", word);
  return hex;
}

fs::path cache_directory() {
  const char* value = std::getenv("XDG_CACHE_HOME");
  return (value != nullptr && *value ? fs::path(value) : fs::path(home()) / ".cache") / "blxshell" / "packages";
}

// A repository on this machine (file://): its files may be file:// too.
bool local_repository(const std::string& repository) { return repository.starts_with("file://"); }

// curl url -o file, run directly (no shell). Empty on success, else why not.
// Only https, plus file:// for a local repository.
std::string fetch(const std::string& url, const fs::path& file, bool allow_file = false) {
  std::vector<std::string> storage{"curl", "--fail", "--silent", "--location", "--proto", allow_file ? "=https,file" : "=https",
                                   "--proto-redir", "=https",
                                   "--connect-timeout", "15", "--max-time", "300",
                                   "--max-filesize", std::to_string(archive::max_unpacked),
                                   "-o", file.string(), url};
  std::vector<char*> argv;
  for (std::string& argument : storage) argv.push_back(argument.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  pid_t pid = -1;
  const int error = posix_spawnp(&pid, "curl", &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (error != 0) return "curl isn't installed";
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  switch (code) {
    case 0: return "";
    case 6: return "can't resolve the host (offline?)";
    case 7: return "can't connect";
    case 22: return "not found (HTTP error)";
    case 28: return "timed out";
    case 63: return "larger than 64 MiB";
    default: return std::format("download failed (curl exit {})", code);
  }
}

}  // namespace

std::vector<std::string> repositories() {
  std::vector<std::string> list{official_repository};
  for (const std::string& repository : config().plugin_repositories)
    if (!repository.empty() && std::ranges::find(list, repository) == list.end()) list.push_back(repository);
  return list;
}

std::string index_url(const std::string& repository) {
  if (repository.starts_with("https://") || local_repository(repository))
    return repository.ends_with(".json") ? repository : repository + "/index.json";
  return std::format("https://github.com/{}/releases/download/packages/index.json", repository);
}

std::vector<Remote> available(std::vector<std::string>& problems) {
  std::vector<Remote> result;
  std::error_code error;
  fs::create_directories(cache_directory(), error);
  for (const std::string& repository : repositories()) {
    const fs::path file = cache_directory() / std::format("index-{}.json", sha256(repository).substr(0, 16));
    if (const std::string why = fetch(index_url(repository), file, local_repository(repository)); !why.empty()) {
      problems.push_back(std::format("{}: {}", repository, why));
      continue;
    }
    json index;
    try {
      index = json::parse(slurp(file));
    } catch (const std::exception&) {
      problems.push_back(repository + ": its index.json isn't valid JSON");
      continue;
    }
    if (!index.is_object() || index.value("schema", 0) != 1 || !index["packages"].is_array()) {
      problems.push_back(repository + ": its index.json has an unknown format");
      continue;
    }
    for (const json& entry : index["packages"]) {
      if (!entry.is_object()) continue;
      Remote remote;
      remote.repository = repository;
      try {
        remote.id = entry.at("id").get<std::string>();
        remote.type = entry.value("type", "plugin");
        remote.name = entry.value("name", remote.id);
        remote.version = entry.at("version").get<std::string>();
        remote.description = entry.value("description", "");
        remote.requires_shell = entry.value("blxshell", "");
        remote.compositors = entry.value("compositors", std::vector<std::string>{});
        remote.permissions = entry.value("permissions", std::vector<std::string>{});
        remote.url = entry.at("url").get<std::string>();
        remote.sha256 = entry.at("sha256").get<std::string>();
        remote.size = entry.value("size", std::uint64_t{0});
      } catch (const std::exception&) {
        problems.push_back(std::format("{}: skipping an entry without id, version, url or sha256", repository));
        continue;
      }
      const bool fetchable = remote.url.starts_with("https://") || (local_repository(repository) && remote.url.starts_with("file://"));
      if (!valid_id(remote.id) || !fetchable || remote.sha256.size() != 64) {
        problems.push_back(std::format("{}: skipping {}: bad id, url or sha256", repository, remote.id));
        continue;
      }
      if (std::ranges::none_of(result, [&](const Remote& other) { return other.id == remote.id; }))
        result.push_back(std::move(remote));
    }
  }
  return result;
}

std::optional<fs::path> download(const Remote& remote, std::string& message) {
  std::error_code error;
  fs::create_directories(cache_directory(), error);
  const fs::path file = cache_directory() / std::format("{}-{}{}", remote.id, remote.version, extension);
  const fs::path partial = file.string() + ".part";
  if (const std::string why = fetch(remote.url, partial, local_repository(remote.repository)); !why.empty()) {
    fs::remove(partial, error);
    message = std::format("can't download {}: {}", remote.id, why);
    return std::nullopt;
  }
  const std::string data = slurp(partial);
  // The index is the authority: a file that doesn't match it isn't installed.
  if ((remote.size != 0 && data.size() != remote.size) || sha256(data) != remote.sha256) {
    fs::remove(partial, error);
    message = std::format("{} doesn't match its checksum in {}'s index; not installing it", remote.id, remote.repository);
    return std::nullopt;
  }
  fs::rename(partial, file, error);
  if (error) {
    message = error.message();
    return std::nullopt;
  }
  return file;
}

bool newer(const std::string& candidate, const std::string& installed) {
  const auto a = parse_version(candidate);
  const auto b = parse_version(installed);
  return a && b && *a > *b;
}

bool remove(const std::string& id, std::string& message) {
  for (const Kind kind : {Kind::theme, Kind::plugin}) {
    const fs::path target = user_directory(kind) / id;
    if (!valid_id(id) || !fs::is_directory(target)) continue;
    std::error_code error;
    fs::remove_all(target, error);
    if (error) {
      message = error.message();
      return false;
    }
    if (kind == Kind::plugin && listed(config().plugins_enabled, id)) {
      toggle_listed(config().plugins_enabled, id);
      save_config();
    }
    message = std::format("removed {} {}", kind_name(kind), id);
    return true;
  }
  message = std::format("no user-installed package {}", id);
  return false;
}

namespace {

// The config.json keys a theme sets.
constexpr std::array<std::pair<const char*, const char*>, 8> colour_keys{{
    {"appearance", "colourOverrides"}, {"appearance", "colourPreset"}, {"appearance", "themeSource"},
    {"appearance", "themeSeed"}, {"matugen", "mode"}, {"matugen", "scheme"}, {"matugen", "contrast"},
    {"matugen", "prefer"}}};

// What the colours were before the first theme, for reset_theme().
fs::path snapshot_path() { return fs::path(config_path()).parent_path() / "colours-before-theme.json"; }

// Edits config.json in place, keeping the previous file as config.json.bak.
// The running shell reloads it live (and regenerates the scheme when the
// matugen settings changed).
bool edit_config(const std::function<void(json&)>& change, std::string& message) {
  const fs::path path = config_path();
  json current = json::object();
  try {
    if (fs::exists(path)) current = json::parse(slurp(path));
  } catch (const std::exception&) {
    message = path.string() + " is not valid JSON; fix or remove it first";
    return false;
  }
  if (!current.is_object()) current = json::object();
  for (const char* section : {"appearance", "matugen"})
    if (!current.contains(section) || !current[section].is_object()) current[section] = json::object();
  std::error_code error;
  fs::create_directories(fs::path(path).parent_path(), error);
  if (fs::exists(path)) fs::copy_file(path, path.string() + ".bak", fs::copy_options::overwrite_existing, error);
  change(current);
  const fs::path temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary);
    output << current.dump(2) << '\n';
    if (!output) {
      message = "can't write " + temporary.string();
      return false;
    }
  }
  fs::rename(temporary, path, error);
  if (error) {
    message = error.message();
    return false;
  }
  return true;
}

}  // namespace

bool apply_theme(const Manifest& theme, std::string& message) {
  if (theme.kind != Kind::theme) {
    message = theme.id + " is not a theme";
    return false;
  }
  if (!theme.valid()) {
    message = theme.id + " has errors";
    return false;
  }
  const json colours = json::parse(theme.json)["colours"];
  const bool ok = edit_config([&](json& config) {
    json& appearance = config["appearance"];
    json& matugen = config["matugen"];
    // The first theme remembers the user's own colour settings.
    const bool themed = appearance.contains("theme") && appearance["theme"].is_string() &&
                        !appearance["theme"].get<std::string>().empty();
    if (!themed || !fs::exists(snapshot_path())) {
      json snapshot = json::object();
      for (const auto& [section, key] : colour_keys)
        if (config[section].contains(key)) snapshot[section][key] = config[section][key];
      std::ofstream(snapshot_path()) << snapshot.dump(2) << '\n';
    }
    if (colours["kind"] == "static") {
      // Every role pinned: the wallpaper no longer changes anything.
      appearance["colourOverrides"] = colours["roles"];
    } else {
      // A generation preset: the theme's matugen settings, regenerated from
      // the wallpaper or its seed. Unset ones go back to the defaults.
      const std::string source = colours.value("source", colours.contains("seed") ? "seed" : "wallpaper");
      appearance["themeSource"] = source == "seed" ? "colour" : "wallpaper";
      if (colours.contains("seed")) appearance["themeSeed"] = colours["seed"];
      appearance["colourPreset"] = colours.value("preset", "generated");
      appearance["colourOverrides"] = colours.contains("roles") ? colours["roles"] : json::object();
      matugen["mode"] = colours.value("mode", "dark");
      matugen["scheme"] = "scheme-" + colours.value("scheme", std::string("tonal-spot"));
      matugen["contrast"] = colours.value("contrast", 0.0);
      matugen["prefer"] = colours.value("prefer", "saturation");
    }
    appearance["theme"] = theme.id;
  }, message);
  if (!ok) return false;
  message = std::format("applied {} {} ({} colours)", theme.id, theme.version, colours["kind"].get<std::string>());
  return true;
}

bool reset_theme(std::string& message) {
  json snapshot = json::object();
  try {
    if (fs::exists(snapshot_path())) snapshot = json::parse(slurp(snapshot_path()));
  } catch (const std::exception&) {
  }
  const bool ok = edit_config([&](json& config) {
    // Back to exactly what was there before the first theme; keys that
    // weren't set then are removed so their defaults return.
    for (const auto& [section, key] : colour_keys) {
      if (snapshot.contains(section) && snapshot[section].contains(key)) config[section][key] = snapshot[section][key];
      else config[section].erase(key);
    }
    config["appearance"].erase("theme");
  }, message);
  if (!ok) return false;
  std::error_code error;
  fs::remove(snapshot_path(), error);
  message = "colours are back to what they were before the first theme";
  return true;
}

// --- CLI ----------------------------------------------------------------------

namespace {

void print_problems(const Manifest& m) {
  for (const std::string& error : m.errors) std::cerr << "  error: " << error << '\n';
  for (const std::string& warning : m.warnings) std::cerr << "  warning: " << warning << '\n';
}

void print_list(Kind kind) {
  const auto list = installed(kind);
  if (list.empty()) {
    std::cout << "no " << folder(kind) << " installed (" << user_directory(kind).string() << ")\n";
    return;
  }
  for (const Manifest& m : list) {
    std::string state;
    if (!m.valid()) state = "broken";
    else if (const auto why = incompatibility(m)) state = "incompatible: " + *why;
    else if (kind == Kind::plugin) state = listed(config().plugins_enabled, m.id) ? "enabled" : "disabled";
    else state = config().theme == m.id ? "applied" : "";
    std::cout << std::format("{:<36} {:<10} {}{}\n", m.id.empty() ? m.directory.filename().string() : m.id, m.version, m.name,
                             state.empty() ? "" : "  [" + state + "]");
  }
}

void print_info(const Manifest& m) {
  std::cout << std::format("{} {} {}\n", kind_name(m.kind), m.id, m.version);
  if (!m.name.empty()) std::cout << "  name:        " << m.name << '\n';
  if (!m.description.empty()) std::cout << "  description: " << m.description << '\n';
  for (const Author& author : m.authors) std::cout << "  author:      " << author.name << '\n';
  if (!m.license.empty()) std::cout << "  license:     " << m.license << '\n';
  std::string list;
  for (const auto& name : m.compositors) list += (list.empty() ? "" : ", ") + name;
  std::cout << "  compositors: " << list << '\n';
  std::cout << "  blxshell:    " << (m.requires_shell.empty() ? "any" : m.requires_shell) << '\n';
  if (m.kind == Kind::plugin) {
    std::string permissions;
    for (const auto& permission : m.permissions) permissions += (permissions.empty() ? "" : ", ") + permission;
    std::cout << "  permissions: " << (permissions.empty() ? "none" : permissions) << '\n';
    for (const BarModule& module : m.bar_modules) std::cout << "  bar module:  " << m.id << ':' << module.id << '\n';
    for (const std::string& name : m.commands) std::cout << "  command:     " << name << '\n';
  }
  if (!m.directory.empty()) std::cout << "  folder:      " << m.directory.string() << '\n';
  if (const auto why = incompatibility(m)) std::cout << "  won't run here: " << *why << '\n';
  print_problems(m);
}

// A .blx, an unpacked folder, or an installed id.
std::optional<Manifest> resolve(const std::string& what, std::optional<Kind> kind) {
  std::error_code error;
  if (fs::is_regular_file(what, error)) return read_package(what);
  if (fs::is_directory(what, error)) return read_directory(what);
  return find(what, kind);
}

int set_enabled(const std::string& id, bool enable) {
  const auto m = find(id, Kind::plugin);
  if (!m) {
    std::cerr << "blxshell: no plugin " << id << " installed\n";
    return EXIT_FAILURE;
  }
  if (enable) {
    if (const auto why = incompatibility(*m)) {
      std::cerr << "blxshell: " << id << " can't run here: " << *why << '\n';
      print_problems(*m);
      return EXIT_FAILURE;
    }
  }
  if (listed(config().plugins_enabled, m->id) != enable) {
    toggle_listed(config().plugins_enabled, m->id);
    save_config();
  }
  std::cout << (enable ? "enabled " : "disabled ") << m->id << '\n';
  if (enable)
    for (const BarModule& module : m->bar_modules)
      std::cout << "  add \"" << m->id << ':' << module.id << "\" to bar.left, bar.center or bar.right to show it\n";
  return EXIT_SUCCESS;
}

}  // namespace

namespace {

std::string joined(const std::vector<std::string>& list) {
  std::string text;
  for (const std::string& item : list) text += (text.empty() ? "" : ", ") + item;
  return text;
}

// Why a repository package won't run here, from its index entry alone (so
// nothing is downloaded for it).
std::optional<std::string> remote_incompatibility(const Remote& remote) {
  Manifest m;
  m.compositors = remote.compositors;
  m.requires_shell = remote.requires_shell;
  return incompatibility(m);
}

// Runs this blxshell again with `arguments`, quietly; for asking the running
// shell to restart an updated plugin.
void run_self(std::vector<std::string> arguments) {
  std::error_code error;
  std::string self = fs::read_symlink("/proc/self/exe", error).string();
  if (error) return;
  if (self.ends_with(" (deleted)")) self.erase(self.size() - 10);
  arguments.insert(arguments.begin(), self);
  std::vector<char*> argv;
  for (std::string& argument : arguments) argv.push_back(argument.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  pid_t pid = -1;
  if (posix_spawn(&pid, self.c_str(), &actions, nullptr, argv.data(), environ) == 0) {
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  }
  posix_spawn_file_actions_destroy(&actions);
}

// Downloads, checks and installs a repository package. An enabled plugin is
// restarted in the running shell so the new version takes over.
bool install_remote(const Remote& remote, bool force) {
  if (const auto why = remote_incompatibility(remote); why && !force) {
    std::cerr << "blxshell: " << remote.id << " won't run here: " << *why << " (--force to install anyway)\n";
    return false;
  }
  std::cout << std::format("{} {} {} from {}", remote.type, remote.id, remote.version, remote.repository);
  if (remote.type == "plugin") std::cout << "  [" << (remote.permissions.empty() ? "no permissions" : "permissions: " + joined(remote.permissions)) << "]";
  std::cout << '\n';
  std::string message;
  const auto file = download(remote, message);
  if (!file) {
    std::cerr << "blxshell: " << message << '\n';
    return false;
  }
  if (!install(*file, message)) {
    std::cerr << "blxshell: " << message << '\n';
    return false;
  }
  std::cout << message << '\n';
  if (remote.type == "plugin" && listed(config().plugins_enabled, remote.id)) run_self({"plugin", "restart", remote.id});
  return true;
}

void print_problems(const std::vector<std::string>& problems) {
  for (const std::string& problem : problems) std::cerr << "warning: " << problem << '\n';
}

}  // namespace

const char* usage() {
  return "Packages (.blx: zstd-compressed tar with manifest.json at the root):\n"
         "  pack <folder> [-o file.blx]       make a package from a folder\n"
         "  inspect <file.blx|folder|id>      show a package and check it\n"
         "  install <file.blx|id> [--force]   install a package file, or one by id from the repositories\n"
         "  search [words]                    what the repositories offer\n"
         "  update [id…]                      install newer versions from the repositories\n"
         "  repo list|add <owner/name|url|folder>|remove <…>\n"
         "  remove <id>                       uninstall\n"
         "  theme list\n"
         "  theme apply <id|file.blx> [--force]\n"
         "  theme reset                       back to colours from the wallpaper\n"
         "  plugin list\n"
         "  plugin enable|disable <id>\n"
         "  plugin call <id> <command> [args]  (sent to the running shell)\n"
         "  plugin restart <id>               (sent to the running shell)\n";
}

std::optional<int> command(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) return std::nullopt;
  // `plugin install x`, `theme update`, …: the same as without the noun.
  if ((args[0] == "plugin" || args[0] == "plugins" || args[0] == "theme" || args[0] == "themes") && args.size() > 1) {
    static constexpr std::string_view shared[] = {"install", "remove", "uninstall", "search", "update", "upgrade", "inspect"};
    if (std::ranges::find(shared, args[1]) != std::end(shared)) {
      std::vector<char*> rest{argv[0]};
      for (int index = 2; index < argc; ++index) rest.push_back(argv[index]);
      return command(static_cast<int>(rest.size()), rest.data());
    }
  }
  const std::string& verb = args[0];
  const auto need = [&](std::size_t count, const char* help) {
    if (args.size() >= count) return true;
    std::cerr << "usage: blxshell " << help << '\n';
    return false;
  };

  if (verb == "pack") {
    if (!need(2, "pack <folder> [-o file.blx]")) return EXIT_FAILURE;
    const fs::path source = args[1];
    const Manifest m = read_directory(source);
    if (!m.valid()) {
      std::cerr << "blxshell: " << source.string() << " isn't a valid package:\n";
      print_problems(m);
      return EXIT_FAILURE;
    }
    print_problems(m);
    fs::path output = m.id + "-" + m.version + extension;
    for (std::size_t index = 2; index + 1 < args.size(); ++index)
      if (args[index] == "-o") output = args[index + 1];
    try {
      archive::pack(source, output);
    } catch (const std::exception& error) {
      std::cerr << "blxshell: " << error.what() << '\n';
      return EXIT_FAILURE;
    }
    // Read it back the way an installer would.
    const Manifest check = read_package(output);
    if (!check.valid()) {
      std::cerr << "blxshell: the package doesn't read back:\n";
      print_problems(check);
      return EXIT_FAILURE;
    }
    std::cout << "packed " << output.string() << " (" << fs::file_size(output) << " bytes)\n";
    return EXIT_SUCCESS;
  }
  if (verb == "inspect") {
    if (!need(2, "inspect <file.blx|folder|id>")) return EXIT_FAILURE;
    const auto m = resolve(args[1], std::nullopt);
    if (!m) {
      std::cerr << "blxshell: no package " << args[1] << '\n';
      return EXIT_FAILURE;
    }
    print_info(*m);
    return m->valid() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (verb == "install") {
    if (!need(2, "install <file.blx|id> [--force]")) return EXIT_FAILURE;
    const bool force = std::ranges::find(args, "--force") != args.end();
    std::error_code file_error;
    if (!fs::is_regular_file(args[1], file_error)) {
      // An id: from the repositories, prebuilt.
      std::vector<std::string> problems;
      const auto offered = available(problems);
      print_problems(problems);
      const auto it = std::ranges::find(offered, args[1], &Remote::id);
      if (it == offered.end()) {
        std::cerr << "blxshell: no package " << args[1] << " in " << joined(repositories())
                  << " (blxshell search to see what there is)\n";
        return EXIT_FAILURE;
      }
      return install_remote(*it, force) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    std::string message;
    const bool ok = install(args[1], message);
    (ok ? std::cout : std::cerr) << (ok ? "" : "blxshell: ") << message << '\n';
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (verb == "search") {
    std::vector<std::string> problems;
    const auto offered = available(problems);
    print_problems(problems);
    std::vector<std::string> words(args.begin() + 1, args.end());
    const auto lower = [](std::string text) {
      std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::tolower(c); });
      return text;
    };
    int shown = 0;
    for (const Remote& remote : offered) {
      const std::string haystack = lower(remote.id + ' ' + remote.name + ' ' + remote.description);
      if (std::ranges::any_of(words, [&](const std::string& word) { return haystack.find(lower(word)) == std::string::npos; })) continue;
      std::string state;
      if (const auto local = find(remote.id)) state = newer(remote.version, local->version) ? "update: " + local->version + " installed" : "installed";
      else if (const auto why = remote_incompatibility(remote)) state = "won't run here";
      std::cout << std::format("{:<28} {:<8} {:<7} {}{}\n", remote.id, remote.version, remote.type, remote.name,
                               state.empty() ? "" : "  [" + state + "]");
      if (!remote.description.empty()) std::cout << "    " << remote.description << '\n';
      ++shown;
    }
    if (shown == 0) std::cout << (offered.empty() ? "nothing available\n" : "nothing matches\n");
    return offered.empty() && !problems.empty() ? EXIT_FAILURE : EXIT_SUCCESS;
  }
  if (verb == "update" || verb == "upgrade") {
    std::vector<std::string> problems;
    const auto offered = available(problems);
    print_problems(problems);
    const std::vector<std::string> only(args.begin() + 1, args.end());
    int updated = 0;
    int failed = 0;
    for (const Kind kind : {Kind::plugin, Kind::theme})
      for (const Manifest& local : installed(kind)) {
        if (!only.empty() && std::ranges::find(only, local.id) == only.end()) continue;
        // Only the user's own installs: system packages are the distro's to update.
        if (local.directory.parent_path() != user_directory(kind)) continue;
        const auto it = std::ranges::find(offered, local.id, &Remote::id);
        if (it == offered.end() || !newer(it->version, local.version)) continue;
        std::cout << local.id << ": " << local.version << " -> " << it->version << '\n';
        if (install_remote(*it, false)) ++updated;
        else ++failed;
      }
    if (updated == 0 && failed == 0) std::cout << "everything is up to date\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (verb == "repo" || verb == "repos") {
    const std::string sub = args.size() > 1 ? args[1] : "list";
    if (sub == "list") {
      for (const std::string& repository : repositories())
        std::cout << repository << (repository == official_repository ? "  (official, reviewed)" : "  (not reviewed by blxshell)") << '\n'
                  << "    " << index_url(repository) << '\n';
      return EXIT_SUCCESS;
    }
    if ((sub == "add" || sub == "remove") && args.size() > 2) {
      std::string repository = args[2];
      while (repository.ends_with('/')) repository.pop_back();
      if (repository.starts_with("https://github.com/") && !repository.ends_with(".json"))
        repository = repository.substr(19);  // github.com/owner/name -> owner/name
      if (repository.ends_with(".git")) repository.erase(repository.size() - 4);
      // A folder or index.json on this machine, e.g. a repository's build/.
      if (repository.starts_with("/") || repository.starts_with("./") || repository.starts_with("~/")) {
        std::error_code path_error;
        const fs::path path = repository.starts_with("~/") ? fs::path(home()) / repository.substr(2) : fs::path(repository);
        repository = "file://" + fs::weakly_canonical(fs::absolute(path), path_error).string();
      }
      const bool url = repository.starts_with("https://") || repository.starts_with("file://");
      const bool slug = !url && std::ranges::count(repository, '/') == 1 && repository.front() != '/' && repository.back() != '/';
      if (!url && !slug) {
        std::cerr << "blxshell: a repository is owner/name on GitHub, an https:// URL of an index.json, or a local folder\n";
        return EXIT_FAILURE;
      }
      if (repository == official_repository) {
        std::cerr << "blxshell: the official repository is always there\n";
        return EXIT_FAILURE;
      }
      auto& list = config().plugin_repositories;
      const bool present = listed(list, repository);
      if (sub == "add") {
        if (present) {
          std::cout << repository << " is already added\n";
          return EXIT_SUCCESS;
        }
        list.push_back(repository);
        save_config();
        std::cout << "added " << repository << "\nPackages from it aren't reviewed by blxshell: only add sources you trust.\n";
        return EXIT_SUCCESS;
      }
      if (!present) {
        std::cerr << "blxshell: " << repository << " isn't added\n";
        return EXIT_FAILURE;
      }
      std::erase(list, repository);
      save_config();
      std::cout << "removed " << repository << " (packages installed from it stay installed)\n";
      return EXIT_SUCCESS;
    }
    std::cerr << "usage: blxshell repo list|add <owner/name|url>|remove <owner/name|url>\n";
    return EXIT_FAILURE;
  }
  if (verb == "remove" || verb == "uninstall") {
    if (!need(2, "remove <id>")) return EXIT_FAILURE;
    std::string message;
    const bool ok = remove(args[1], message);
    (ok ? std::cout : std::cerr) << (ok ? "" : "blxshell: ") << message << '\n';
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (verb == "theme" || verb == "themes") {
    const std::string sub = args.size() > 1 ? args[1] : "list";
    if (sub == "list") {
      print_list(Kind::theme);
      return EXIT_SUCCESS;
    }
    if (sub == "reset") {
      std::string message;
      const bool ok = reset_theme(message);
      (ok ? std::cout : std::cerr) << (ok ? "" : "blxshell: ") << message << '\n';
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (sub == "apply") {
      if (!need(3, "theme apply <id|file.blx> [--force]")) return EXIT_FAILURE;
      const bool force = std::ranges::find(args, "--force") != args.end();
      std::optional<Manifest> m;
      std::string message;
      if (fs::is_regular_file(args[2])) {
        // Installing first gives the theme a folder for its wallpaper to live in.
        if (!install(args[2], message)) {
          std::cerr << "blxshell: " << message << '\n';
          return EXIT_FAILURE;
        }
        m = find(read_package(args[2]).id, Kind::theme);
      } else {
        m = find(args[2], Kind::theme);
      }
      if (!m) {
        std::cerr << "blxshell: no theme " << args[2] << " installed\n";
        return EXIT_FAILURE;
      }
      if (const auto why = incompatibility(*m); why && !force) {
        std::cerr << "blxshell: " << m->id << " can't be applied here: " << *why << " (--force to apply anyway)\n";
        return EXIT_FAILURE;
      }
      const bool ok = apply_theme(*m, message);
      (ok ? std::cout : std::cerr) << (ok ? "" : "blxshell: ") << message << '\n';
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    std::cerr << "usage: blxshell theme list|apply <id>|reset\n";
    return EXIT_FAILURE;
  }
  if (verb == "plugin" || verb == "plugins") {
    const std::string sub = args.size() > 1 ? args[1] : "list";
    if (sub == "list") {
      print_list(Kind::plugin);
      return EXIT_SUCCESS;
    }
    if (sub == "enable" || sub == "disable") {
      if (!need(3, "plugin enable|disable <id>")) return EXIT_FAILURE;
      return set_enabled(args[2], sub == "enable");
    }
    return std::nullopt;  // call / restart / status: the running shell's
  }
  return std::nullopt;
}

}  // namespace blx::packages
