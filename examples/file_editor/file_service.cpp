#include "file_service.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace examples::file_editor {

std::string FileService::read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void FileService::write_text(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    stream << content;
}

void FileService::append_text(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary | std::ios::app);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open file for appending: " + path.string());
    }
    stream << content;
}

std::size_t FileService::replace_text(const std::filesystem::path& path,
                                      const std::string& from,
                                      const std::string& to) {
    if (from.empty()) {
        throw std::runtime_error("The replacement source must not be empty.");
    }

    auto content = read_text(path);
    std::size_t count = 0;
    std::size_t offset = 0;

    while ((offset = content.find(from, offset)) != std::string::npos) {
        content.replace(offset, from.size(), to);
        offset += to.size();
        ++count;
    }

    write_text(path, content);
    return count;
}

}  // namespace examples::file_editor
