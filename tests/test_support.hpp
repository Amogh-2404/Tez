#ifndef TEZ_TEST_SUPPORT_HPP
#define TEZ_TEST_SUPPORT_HPP

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto pattern = (std::filesystem::temp_directory_path() / "tez-test-XXXXXX").string();
        std::vector<char> writable(pattern.begin(), pattern.end());
        writable.push_back('\0');
        const auto created = ::mkdtemp(writable.data());
        if (!created)
            throw std::runtime_error("Cannot create test directory");
        path = created;
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    void write(const std::string &name, const std::string &content) const {
        const auto file_path = path / name;
        std::filesystem::create_directories(file_path.parent_path());
        std::ofstream file(file_path, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!file)
            throw std::runtime_error("Cannot write test fixture");
    }

    std::filesystem::path path;
};

#endif
