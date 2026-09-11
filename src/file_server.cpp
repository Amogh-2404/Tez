#include "file_server.hpp"
#include "diagnostics.hpp"
#include "middleware.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
class FileDescriptor {
  public:
    explicit FileDescriptor(int value = -1) : value_(value) {}
    ~FileDescriptor() {
        if (value_ >= 0)
            ::close(value_);
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    FileDescriptor(FileDescriptor &&other) noexcept : value_(std::exchange(other.value_, -1)) {}
    FileDescriptor &operator=(FileDescriptor &&other) noexcept {
        if (this != &other) {
            if (value_ >= 0)
                ::close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    int get() const {
        return value_;
    }

  private:
    int value_;
};

std::shared_ptr<const FileDescriptor> static_root;

const std::unordered_map<std::string, std::string> mime_types = {
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".txt", "text/plain; charset=utf-8"},
    {".csv", "text/csv; charset=utf-8"},
    {".js", "text/javascript; charset=utf-8"},
    {".mjs", "text/javascript; charset=utf-8"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".pdf", "application/pdf"},
    {".wasm", "application/wasm"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
    {".avif", "image/avif"},
    {".ico", "image/vnd.microsoft.icon"},
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".bz2", "application/x-bzip2"},
    {".7z", "application/x-7z-compressed"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".eot", "application/vnd.ms-fontobject"},
    {".otf", "font/otf"}};

Response error_response(const std::string &status, const std::string &message) {
    Response response;
    response.status = status;
    response.content_type = "text/plain; charset=utf-8";
    response.body = message + "\n";
    return response;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool decode_components(std::string_view path, std::vector<std::string> &components) {
    std::string component;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            // Dotfiles are private by default (including .git and .env).
            if (component.empty() || component.front() == '.')
                return false;
            components.push_back(std::move(component));
            component.clear();
            continue;
        }
        auto c = static_cast<unsigned char>(path[i]);
        if (c == '%') {
            if (i + 2 >= path.size())
                return false;
            const int high = hex_value(path[i + 1]);
            const int low = hex_value(path[i + 2]);
            if (high < 0 || low < 0)
                return false;
            c = static_cast<unsigned char>(high * 16 + low);
            if (c == '/')
                return false;
            i += 2;
        }
        if (c < 0x20 || c == 0x7f || c == '\\' || c == ':')
            return false;
        component += static_cast<char>(c);
    }
    return !components.empty();
}

std::string mime_type(const std::string &filename) {
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::string extension = filename.substr(dot);
    for (auto &c : extension) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    const auto found = mime_types.find(extension);
    return found == mime_types.end() ? "application/octet-stream" : found->second;
}

std::string file_version(const struct stat &info) {
#ifdef __APPLE__
    const auto modified = info.st_mtimespec;
    const auto changed = info.st_ctimespec;
#else
    const auto modified = info.st_mtim;
    const auto changed = info.st_ctim;
#endif
    return std::to_string(info.st_dev) + ":" + std::to_string(info.st_ino) + ":" +
           std::to_string(info.st_size) + ":" + std::to_string(modified.tv_sec) + ":" +
           std::to_string(modified.tv_nsec) + ":" + std::to_string(changed.tv_sec) + ":" +
           std::to_string(changed.tv_nsec);
}

Response open_error(int error) {
    if (error == ENOENT || error == ENAMETOOLONG) {
        return error_response("404 Not Found", "File not found.");
    }
    if (error == ELOOP || error == EACCES || error == EPERM || error == ENOTDIR) {
        return error_response("403 Forbidden", "Access denied.");
    }
    return error_response("503 Service Unavailable", "File temporarily unavailable.");
}
} // namespace

std::string configure_static_root(const std::string &path) {
    auto selected = path.empty() ? std::string("static") : path;
    int fd = ::open(selected.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (path.empty() && fd < 0 && errno == ENOENT) {
        selected = "../static";
        fd = ::open(selected.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    }
    if (fd < 0 && (!path.empty() || errno != ENOENT)) {
        const auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Cannot open static directory " + quote_diagnostic(selected));
    }
    FileDescriptor opened_root(fd);
    const auto resolved = fd < 0 ? std::string() : std::filesystem::absolute(selected).string();
    const auto root = std::make_shared<const FileDescriptor>(std::move(opened_root));
    std::atomic_store(&static_root, root);
    return resolved;
}

Response serve_file(const std::string &path) {
    const std::string_view pathname = std::string_view(path).substr(0, path.find('?'));
    if (pathname.substr(0, 8) != "/static/") {
        return error_response("400 Bad Request", "Not a static file request.");
    }
    std::vector<std::string> components;
    if (!decode_components(pathname.substr(8), components)) {
        return error_response("403 Forbidden", "Access denied: invalid file path.");
    }
    auto root = std::atomic_load(&static_root);
    if (!root) {
        configure_static_root();
        root = std::atomic_load(&static_root);
    }
    if (root->get() < 0)
        return error_response("404 Not Found", "File not found.");

    int parent = root->get();
    FileDescriptor opened;
    for (std::size_t i = 0; i < components.size(); ++i) {
        // Each component is resolved relative to an already-open directory.
        // O_NOFOLLOW alone on a complete path protects only its final component.
        int flags = O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY;
        if (i + 1 != components.size())
            flags |= O_DIRECTORY;
        const int fd = ::openat(parent, components[i].c_str(), flags);
        if (fd < 0)
            return open_error(errno);
        opened = FileDescriptor(fd);
        parent = opened.get();
    }

    struct stat before {};
    if (::fstat(opened.get(), &before) < 0)
        return open_error(errno);
    if (!S_ISREG(before.st_mode))
        return error_response("403 Forbidden", "Only regular files are served.");
    if (before.st_size < 0 || static_cast<std::uintmax_t>(before.st_size) > MAX_STATIC_FILE_BYTES) {
        return error_response("413 Content Too Large", "File exceeds the static file size limit.");
    }

    const auto version = file_version(before);
    const auto type = mime_type(components.back());
    const auto key = version + ":" + type;
    auto cached = get_cached_file(key);
    if (!cached.status.empty())
        return cached;

    Response response;
    response.status = "200 OK";
    response.content_type = type;
    response.body.resize(static_cast<std::size_t>(before.st_size));
    std::size_t offset = 0;
    while (offset < response.body.size()) {
        const auto count =
            ::read(opened.get(), response.body.data() + offset, response.body.size() - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            return open_error(errno);
        if (count == 0)
            break;
        offset += static_cast<std::size_t>(count);
    }
    struct stat after {};
    if (::fstat(opened.get(), &after) < 0)
        return open_error(errno);
    if (offset != response.body.size() || file_version(after) != version) {
        return error_response("503 Service Unavailable",
                              "File changed while being read; retry the request.");
    }
    cache_file(key, response);
    return response;
}
