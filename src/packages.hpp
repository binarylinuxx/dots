#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "archive.hpp"

// Themes and plugins ship as .blx packages: zstd-compressed tar archives with
// a manifest.json at the root whose "type" is "theme" or "plugin". Installing
// one unpacks it to
//
//   ~/.local/share/blxshell/{themes,plugins}/<id>/      the user's, first
//   $XDG_DATA_DIRS/blxshell/{themes,plugins}/<id>/       system-wide
//
// The format is in docs/packages.md; schemas/manifest.schema.json is what the
// store validates uploads against.
namespace blx::packages {

// The manifest format this build reads. A higher "schema" is refused rather
// than half understood.
inline constexpr int schema_version = 1;
// The plugin protocol this build speaks (manifest "plugin.api").
inline constexpr int api_version = 1;
inline constexpr const char* manifest_name = "manifest.json";
inline constexpr const char* extension = ".blx";

enum class Kind { theme, plugin };
const char* kind_name(Kind kind);

struct Author {
  std::string name;
  std::string email;
  std::string url;
};

// A plugin's bar module: placed in the bar as "<plugin id>:<module id>".
struct BarModule {
  std::string id;
  std::string name;
  std::string description;
};

// A setting a plugin exposes; its value reaches the plugin in the hello
// message. type: "bool", "number", "string" or "choice".
struct Setting {
  std::string key;
  std::string type;
  std::string label;
  std::string description;
  std::string default_json;  // the default, as JSON text
  std::vector<std::string> choices;
};

struct Manifest {
  Kind kind = Kind::theme;
  std::filesystem::path directory;  // the installed folder; empty for a .blx not yet installed
  int schema = 1;
  std::string id;  // reverse-DNS, e.g. "io.github.someone.glass"
  std::string name;
  std::string version;  // semver
  std::string description;
  std::vector<Author> authors;
  std::string license;  // SPDX
  std::string homepage;
  std::string repository;
  std::vector<std::string> tags;
  std::vector<std::string> previews;  // image paths inside the package
  std::string icon;
  // Which blxshell versions it works with, e.g. ">=0.1.0 <0.3.0"; empty is any.
  std::string requires_shell;
  // Compositors it works under ("zwwm", "hyprland"), or "any".
  std::vector<std::string> compositors;

  // Plugins only.
  int api = 0;
  std::vector<std::string> exec;  // argv; [0] is relative to the package
  std::string restart = "on-failure";  // "never", "on-failure", "always"
  std::vector<std::string> permissions;
  std::vector<BarModule> bar_modules;
  std::vector<std::string> commands;
  std::vector<Setting> settings;

  // The whole manifest, for themes to be applied from.
  std::string json;

  // Problems found while reading it; a package with any is not used.
  std::vector<std::string> errors;
  // Things that don't stop it from working (unknown keys, unknown compositors).
  std::vector<std::string> warnings;

  bool valid() const { return errors.empty(); }
  bool has_permission(const std::string& permission) const;
};

// The running compositor: "zwwm", "hyprland", or "" when neither is found.
const std::string& compositor();
// This blxshell's version, e.g. "0.1.0".
const char* shell_version();

// Why a valid package can't run here, or nullopt when it can.
std::optional<std::string> incompatibility(const Manifest& manifest);

// Parses manifest.json text; `files` (the package's paths) lets it check that
// referenced files exist. Never throws: problems land in Manifest::errors.
Manifest parse(const std::string& json, const std::vector<std::string>& files = {});
// An installed package's folder.
Manifest read_directory(const std::filesystem::path& directory);
// A .blx file, checked in full (archive safety, manifest, referenced files).
Manifest read_package(const std::filesystem::path& file);

// Every installed package of a kind, the user's shadowing the system's.
std::vector<Manifest> installed(Kind kind);
// An installed package by id (either kind when kind is empty).
std::optional<Manifest> find(const std::string& id, std::optional<Kind> kind = std::nullopt);

// ~/.local/share/blxshell/<themes|plugins>.
std::filesystem::path user_directory(Kind kind);

// Unpacks a .blx into the user directory, replacing an older version of the
// same id. False with a message on failure.
bool install(const std::filesystem::path& file, std::string& message);
bool remove(const std::string& id, std::string& message);

// --- Repositories -----------------------------------------------------------
//
// A repository publishes prebuilt packages and an index.json describing them
// (see dots-plugins' tools/build.py). The official one comes first; users add
// more in config.json's plugins.repositories.

inline constexpr const char* official_repository = "binarylinuxx/dots-plugins";

// One package a repository offers, from its index.json.
struct Remote {
  std::string repository;
  std::string id;
  std::string type;  // "plugin" or "theme"
  std::string name;
  std::string version;
  std::string description;
  std::string requires_shell;
  std::vector<std::string> compositors;
  std::vector<std::string> permissions;
  std::string url;
  std::string sha256;
  std::uint64_t size = 0;
};

// The official repository, then the user's.
std::vector<std::string> repositories();
// Where a repository's index.json is: "owner/name" means its GitHub release
// called "packages"; a URL is used as it is.
std::string index_url(const std::string& repository);
// Every package the repositories offer, the first repository winning a
// duplicate id. Repositories that can't be read land in `problems`.
std::vector<Remote> available(std::vector<std::string>& problems);
// Downloads a package and checks its size and sha256 against the index;
// the verified .blx, or nullopt with a message.
std::optional<std::filesystem::path> download(const Remote& remote, std::string& message);
// True when `candidate` is a newer version than `installed`.
bool newer(const std::string& candidate, const std::string& installed);

// A theme is a fixed colour for every role. Applying one pins them all in
// ~/.config/blxshell/config.json (previous file kept as config.json.bak);
// the running shell picks it up live. Resetting returns to the colours
// generated from the wallpaper.
bool apply_theme(const Manifest& theme, std::string& message);
bool reset_theme(std::string& message);

// `blxshell pack|install|remove|inspect|theme|plugin …`, which work on files.
// nullopt when the arguments are for the running shell instead.
std::optional<int> command(int argc, char** argv);
const char* usage();

}  // namespace blx::packages
