#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace examples::file_editor {

class FileService {
public:
    static std::string read_text(const std::filesystem::path& path);
    static void write_text(const std::filesystem::path& path, const std::string& content);
    static void append_text(const std::filesystem::path& path, const std::string& content);
    static std::size_t replace_text(const std::filesystem::path& path,
                                    const std::string& from,
                                    const std::string& to);
};

}  // namespace examples::file_editor
