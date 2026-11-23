#include "archive.hpp"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace blx::archive {
namespace {

namespace fs = std::filesystem;
constexpr std::size_t block = 512;

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }

// --- zstd -------------------------------------------------------------------

std::string decompress(const std::string& input) {
  std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), ZSTD_freeDCtx);
  if (!context) fail("out of memory");
  std::string output;
  std::array<char, 1 << 16> chunk{};
  ZSTD_inBuffer in{input.data(), input.size(), 0};
  std::size_t last = 1;
  while (in.pos < in.size) {
    ZSTD_outBuffer out{chunk.data(), chunk.size(), 0};
    last = ZSTD_decompressStream(context.get(), &out, &in);
    if (ZSTD_isError(last)) fail(std::string("not a .blx (zstd: ") + ZSTD_getErrorName(last) + ")");
    output.append(chunk.data(), out.pos);
    if (output.size() > max_unpacked + 2 * block * (max_entries + 1)) fail("package is larger than 64 MiB unpacked");
  }
  if (last != 0) fail("package is truncated");
  return output;
}

std::string compress(const std::string& input, int level) {
  std::string output(ZSTD_compressBound(input.size()), '\0');
  const std::size_t size = ZSTD_compress(output.data(), output.size(), input.data(), input.size(),
                                         std::clamp(level, 1, ZSTD_maxCLevel()));
  if (ZSTD_isError(size)) fail(std::string("zstd: ") + ZSTD_getErrorName(size));
  output.resize(size);
  return output;
}

// --- tar --------------------------------------------------------------------

std::string field(const char* data, std::size_t size) {
  std::size_t length = 0;
  while (length < size && data[length] != '\0') ++length;
  return {data, length};
}

std::uint64_t octal(const char* data, std::size_t size) {
  std::uint64_t value = 0;
  std::size_t index = 0;
  while (index < size && (data[index] == ' ' || data[index] == '\0')) ++index;
  for (; index < size && data[index] >= '0' && data[index] <= '7'; ++index) value = value * 8 + static_cast<std::uint64_t>(data[index] - '0');
  return value;
}

bool checksum_ok(const char* header) {
  std::uint64_t sum = 0;
  for (std::size_t index = 0; index < block; ++index)
    sum += (index >= 148 && index < 156) ? ' ' : static_cast<unsigned char>(header[index]);
  return sum == octal(header + 148, 8);
}

// "./a//b/" -> "a/b"; throws on anything that could leave the package folder.
std::string clean(std::string_view raw) {
  std::string result;
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t end = std::min(raw.find('/', start), raw.size());
    const std::string_view part = raw.substr(start, end - start);
    if (start == 0 && end == 0 && !raw.empty()) fail(std::format("absolute path `{}` in package", raw));
    if (part == "..") fail(std::format("path `{}` leaves the package", raw));
    if (!part.empty() && part != ".") {
      if (part.find('\0') != std::string_view::npos || part.find('\\') != std::string_view::npos)
        fail(std::format("bad path `{}` in package", raw));
      if (!result.empty()) result += '/';
      result += part;
    }
    start = end + 1;
  }
  return result;
}

// PAX extended header records: "<len> key=value\n".
std::string pax_path(std::string_view records) {
  std::string path;
  while (!records.empty()) {
    const std::size_t space = records.find(' ');
    if (space == std::string_view::npos) break;
    std::size_t length = 0;
    for (char c : records.substr(0, space)) length = length * 10 + static_cast<std::size_t>(c - '0');
    if (length == 0 || length > records.size()) break;
    const std::string_view record = records.substr(space + 1, length - space - 2);
    if (record.starts_with("path=")) path = std::string(record.substr(5));
    records.remove_prefix(length);
  }
  return path;
}

std::vector<Entry> parse(const std::string& tar) {
  std::vector<Entry> entries;
  std::set<std::string> seen;
  std::size_t total = 0;
  std::string long_name;
  std::size_t offset = 0;
  while (offset + block <= tar.size()) {
    const char* header = tar.data() + offset;
    if (std::all_of(header, header + block, [](char c) { return c == '\0'; })) break;
    if (!checksum_ok(header)) fail("package is corrupt (tar checksum)");
    const char type = header[156];
    const std::uint64_t size = octal(header + 124, 12);
    if (size > max_unpacked) fail("a file in the package is larger than 64 MiB");
    const std::size_t data_offset = offset + block;
    const std::size_t padded = (size + block - 1) / block * block;
    if (data_offset + size > tar.size()) fail("package is truncated");
    const std::string_view data(tar.data() + data_offset, size);
    offset = data_offset + padded;

    if (type == 'x') {  // PAX header for the next entry
      long_name = pax_path(data);
      continue;
    }
    if (type == 'g') continue;  // PAX global header: nothing we use
    if (type == 'L') {          // GNU long name for the next entry
      long_name = field(data.data(), data.size());
      continue;
    }

    std::string name = long_name;
    long_name.clear();
    if (name.empty()) {
      name = field(header, 100);
      if (std::string_view(header + 257, 5) == "ustar") {
        const std::string prefix = field(header + 345, 155);
        if (!prefix.empty()) name = prefix + '/' + name;
      }
    }
    const std::string path = clean(name);
    if (type != '0' && type != '\0' && type != '5' && type != '7')
      fail(std::format("`{}` is a link or special file; packages may hold only files and folders", name));
    if (path.empty()) continue;  // "./" itself
    if (!seen.insert(path).second) fail(std::format("`{}` is in the package twice", path));
    if (entries.size() >= max_entries) fail("package has too many files");
    total += size;
    if (total > max_unpacked) fail("package is larger than 64 MiB unpacked");
    Entry entry{.path = path, .directory = type == '5', .executable = (octal(header + 100, 8) & 0111) != 0};
    if (!entry.directory) entry.data.assign(data);
    entries.push_back(std::move(entry));
  }
  // A file and a folder can't share a path prefix the wrong way round.
  for (const Entry& entry : entries) {
    for (std::size_t slash = entry.path.find('/'); slash != std::string::npos; slash = entry.path.find('/', slash + 1)) {
      const auto parent = std::ranges::find(entries, entry.path.substr(0, slash), &Entry::path);
      if (parent != entries.end() && !parent->directory) fail(std::format("`{}` is inside a file", entry.path));
    }
  }
  return entries;
}

void octal_field(char* out, std::size_t size, std::uint64_t value) {
  const std::string text = std::format("{:0{}o}", value, size - 1);
  if (text.size() > size - 1) fail("value too large for tar");
  std::memcpy(out, text.data(), text.size());
}

void write_header(std::string& tar, const std::string& path, std::uint64_t size, char type, unsigned mode) {
  std::array<char, block> header{};
  std::string name = path;
  std::string prefix;
  if (name.size() > 100) {
    // ustar splits at a '/' into prefix (155) and name (100).
    const std::size_t split = name.rfind('/', 155);
    if (split == std::string::npos || name.size() - split - 1 > 100) fail(std::format("path `{}` is too long", path));
    prefix = name.substr(0, split);
    name = name.substr(split + 1);
  }
  std::memcpy(header.data(), name.data(), name.size());
  octal_field(header.data() + 100, 8, mode);
  octal_field(header.data() + 108, 8, 0);
  octal_field(header.data() + 116, 8, 0);
  octal_field(header.data() + 124, 12, size);
  octal_field(header.data() + 136, 12, 0);  // mtime 0: reproducible packages
  header[156] = type;
  std::memcpy(header.data() + 257, "ustar", 6);
  std::memcpy(header.data() + 263, "00", 2);
  std::memcpy(header.data() + 345, prefix.data(), prefix.size());
  std::memset(header.data() + 148, ' ', 8);
  std::uint64_t sum = 0;
  for (char c : header) sum += static_cast<unsigned char>(c);
  const std::string checksum = std::format("{:06o}", sum);
  std::memcpy(header.data() + 148, checksum.data(), 6);
  header[154] = '\0';
  header[155] = ' ';
  tar.append(header.data(), header.size());
}

std::string slurp(const fs::path& file) {
  std::ifstream input(file, std::ios::binary);
  if (!input) fail(std::format("can't read {}", file.string()));
  return {std::istreambuf_iterator<char>(input), {}};
}

}  // namespace

std::vector<Entry> read_bytes(const std::string& compressed) { return parse(decompress(compressed)); }

std::vector<Entry> read(const fs::path& file) {
  std::error_code error;
  const auto size = fs::file_size(file, error);
  if (error) fail(std::format("can't read {}: {}", file.string(), error.message()));
  if (size > max_unpacked) fail("package is larger than 64 MiB");
  return read_bytes(slurp(file));
}

void pack(const fs::path& directory, const fs::path& output, int level) {
  std::vector<fs::path> paths;
  std::error_code error;
  for (auto it = fs::recursive_directory_iterator(directory, error); !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
    const fs::file_status status = it->symlink_status();
    if (fs::is_symlink(status)) fail(std::format("{} is a symlink; packages may hold only files and folders", it->path().string()));
    if (!fs::is_regular_file(status) && !fs::is_directory(status)) fail(std::format("{} is a special file", it->path().string()));
    paths.push_back(it->path());
  }
  if (error) fail(std::format("can't read {}: {}", directory.string(), error.message()));
  std::ranges::sort(paths);
  std::string tar;
  std::size_t total = 0;
  for (const fs::path& path : paths) {
    const std::string relative = clean(fs::relative(path, directory).generic_string());
    if (fs::is_directory(path)) {
      write_header(tar, relative + '/', 0, '5', 0755);
      continue;
    }
    const std::string data = slurp(path);
    total += data.size();
    if (total > max_unpacked) fail("package would be larger than 64 MiB");
    const bool executable = (fs::status(path).permissions() & fs::perms::owner_exec) != fs::perms::none;
    write_header(tar, relative, data.size(), '0', executable ? 0755 : 0644);
    tar += data;
    tar.append((block - data.size() % block) % block, '\0');
  }
  tar.append(block * 2, '\0');
  const std::string compressed = compress(tar, level);
  const fs::path temporary = output.string() + ".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(compressed.data(), static_cast<std::streamsize>(compressed.size())))
      fail(std::format("can't write {}", output.string()));
  }
  fs::rename(temporary, output);
}

void extract(const std::vector<Entry>& entries, const fs::path& destination) {
  if (fs::exists(destination)) fail(std::format("{} already exists", destination.string()));
  fs::create_directories(destination);
  for (const Entry& entry : entries) {
    const fs::path target = destination / fs::path(entry.path);
    if (entry.directory) {
      fs::create_directories(target);
      continue;
    }
    fs::create_directories(target.parent_path());
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(entry.data.data(), static_cast<std::streamsize>(entry.data.size())))
      fail(std::format("can't write {}", target.string()));
    file.close();
    fs::permissions(target, entry.executable ? fs::perms(0755) : fs::perms(0644), fs::perm_options::replace);
  }
}

const Entry* find(const std::vector<Entry>& entries, const std::string& path) {
  const auto it = std::ranges::find(entries, path, &Entry::path);
  return it == entries.end() ? nullptr : &*it;
}

}  // namespace blx::archive
