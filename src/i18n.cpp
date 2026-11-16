#include "i18n.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "config.hpp"

namespace blx::i18n {
namespace {

namespace fs = std::filesystem;

std::string wanted_language() {
  std::string language = config().language;
  if (language.empty() || language == "auto") {
    language.clear();
    for (const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
      if (const char* value = std::getenv(variable); value != nullptr && *value != '\0') {
        language = value;
        break;
      }
  }
  // ru_RU.UTF-8 -> ru
  const auto end = language.find_first_of("_.@");
  return end == std::string::npos ? language : language.substr(0, end);
}

std::vector<fs::path> catalog_directories() {
  std::vector<fs::path> directories;
  std::error_code error;
  const auto executable = fs::read_symlink("/proc/self/exe", error);
  if (!error) directories.push_back(executable.parent_path().parent_path() / "share" / "blxshell" / "translations");
#ifdef BLXSHELL_SOURCE_TRANSLATION_DIR
  directories.emplace_back(BLXSHELL_SOURCE_TRANSLATION_DIR);
#endif
  return directories;
}

std::unordered_map<std::string, std::string> load(const std::string& language) {
  std::unordered_map<std::string, std::string> strings;
  if (language.empty() || language == "en" || language == "C" || language == "POSIX") return strings;
  std::error_code error;
  for (const auto& directory : catalog_directories()) {
    const fs::path file = directory / (language + ".json");
    if (!fs::exists(file, error)) continue;
    try {
      std::ifstream input(file);
      const auto catalog = nlohmann::json::parse(input);
      for (const auto& [english, translated] : catalog.value("strings", nlohmann::json::object()).items())
        if (translated.is_string() && !translated.get<std::string>().empty())
          strings[english] = translated.get<std::string>();
    } catch (const std::exception&) {
    }
    break;
  }
  return strings;
}

}  // namespace

const std::string& tr(const std::string& english) {
  static std::string language = "\x01";  // nothing loaded yet
  static std::unordered_map<std::string, std::string> strings;
  if (std::string wanted = wanted_language(); wanted != language) {
    language = std::move(wanted);
    strings = load(language);
  }
  if (strings.empty()) return english;
  const auto found = strings.find(english);
  return found == strings.end() ? english : found->second;
}

}  // namespace blx::i18n
