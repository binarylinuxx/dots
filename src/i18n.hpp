// The shell's own text in the user's language. Catalogs are JSON files in
// share/blxshell/translations, named after the language (ru.json):
//
//   { "language": "Русский", "strings": { "Settings": "Настройки" } }
//
// Keys are the English text as the shell draws it; anything missing stays
// English. The language is config().language: "auto" follows LC_ALL,
// LC_MESSAGES, then LANG.
#pragma once

#include <string>

namespace blx::i18n {

const std::string& tr(const std::string& english);

}  // namespace blx::i18n
