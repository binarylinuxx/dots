#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// .blx packages: a tar archive compressed with zstd (what `tar --zstd -cf`
// makes), with manifest.json at its root. Only regular files and folders
// are allowed in one: anything else (links, devices, absolute paths, `..`)
// makes the whole package invalid, since packages come from the internet.
namespace blx::archive {

struct Entry {
  std::string path;  // relative, '/'-separated, no leading "./"
  bool directory = false;
  bool executable = false;
  std::string data;
};

// Hard limits for one package.
inline constexpr std::size_t max_unpacked = 64u << 20;  // 64 MiB
inline constexpr std::size_t max_entries = 4096;

// Decompresses and reads a .blx. Throws std::runtime_error with a readable
// message on anything malformed or unsafe.
std::vector<Entry> read(const std::filesystem::path& file);
std::vector<Entry> read_bytes(const std::string& compressed);

// Writes every file and folder under `directory` (sorted, so the same input
// gives the same bytes) as a .blx. Throws on links and special files.
void pack(const std::filesystem::path& directory, const std::filesystem::path& output, int level = 19);

// Writes entries under `destination`, which must not exist yet.
void extract(const std::vector<Entry>& entries, const std::filesystem::path& destination);

// The entry at `path`, or nullptr.
const Entry* find(const std::vector<Entry>& entries, const std::string& path);

}  // namespace blx::archive
