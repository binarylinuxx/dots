#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
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
inline constexpr int api_version = 2;
inline constexpr const char* manifest_name = "manifest.json";
inline constexpr const char* extension = ".blx";

enum class Kind { theme, plugin };
const char* kind_name(Kind kind);

struct Author {
  std::string name;
  std::string email;
  std::string url;
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
  // The project it's for ("blxshell" when it doesn't say); a build only takes
  // its own project's packages.
  std::string project = "blxshell";
  // Which versions of that project it works with, e.g. ">=0.1.0 <0.3.0";
  // empty is any.
  std::string requires_shell;
  // Compositors it works under ("zwwm", "hyprland"), or "any".
  std::vector<std::string> compositors;

  // Themes only: an image inside the package that applying the theme sets as
  // the wallpaper; empty for a colours-only theme.
  std::string wallpaper;

  // Plugins only (api 2). Any of the three entry points, at least one:
  int api = 0;
  // Its views, in Facet (a .facet file in the package), drawn by the shell.
  std::string facet;
  // A process in any language behind it: argv, [0] relative to the package.
  std::vector<std::string> exec;
  // A C++ library (needs the "native" permission), see <blxshell/plugin.hpp>.
  std::string native;
  std::string restart = "on-failure";  // "never", "on-failure", "always"
  // A material for the lock screen: every backdrop there (pills, cards, the
  // sheet) is drawn by this rectangle shader (.vws, or GLSL compiled at
  // runtime), which may sample the wallpaper behind it. Up to three
  // parameters come from the plugin's store (key, default); see
  // docs/packages.md.
  std::string lock_material;
  std::vector<std::pair<std::string, float>> lock_material_params;
  // Options of its Lock views the layout editor shows, per view name: the
  // view's array from plugin.lockOptions as JSON text (see docs/packages.md).
  std::map<std::string, std::string> lock_options;
  std::vector<std::string> permissions;

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
// The project this build takes packages for ("blxshell" unless built with
// another BLXSHELL_PROJECT_ID, for a shell reusing the plugin system).
const char* project_id();

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
// (see dots-plugins' tools/build.py). The official ones come first; users add
// more in config.json's plugins.repositories.

inline constexpr std::array<const char*, 2> official_repositories{"binarylinuxx/dots-plugins",
                                                                  "binarylinuxx/dots-themes"};
inline bool official(const std::string& repository) {
  return std::ranges::find(official_repositories, repository) != official_repositories.end();
}

// One package a repository offers, from its index.json.
struct Remote {
  std::string repository;
  std::string id;
  std::string type;  // "plugin" or "theme"
  bool wallpaper = false;  // a theme that comes with a wallpaper
  std::string name;
  std::string version;
  std::string description;
  std::string project = "blxshell";
  std::string requires_shell;
  std::vector<std::string> compositors;
  std::vector<std::string> permissions;
  std::vector<std::string> tags;
  std::vector<std::string> previews;  // image URLs
  std::string readme;                 // its README.md's URL, when it has one
  std::string license;
  std::vector<std::string> authors;
  std::string url;
  std::string sha256;
  std::uint64_t size = 0;
};

// The official repositories, then the user's.
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
// A file from an https URL (a preview, a README) in the cache, downloaded
// once, or again when `fresh`; nullopt when it can't be fetched.
std::optional<std::filesystem::path> cached_file(const std::string& url, bool fresh = false);
// Why a repository package won't run here, from its index entry alone.
std::optional<std::string> remote_incompatibility(const Remote& remote);
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
