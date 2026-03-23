#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include "../exceptions/parsing_error/index.hpp"

namespace clix {

/**
 * Rich filesystem path wrapper used by the CLI parser.
 *
 * The class keeps both the original user input and the normalized absolute
 * path so command handlers can access a predictable representation without
 * losing the original context.
 */
class Path {
public:
    explicit Path(std::string input)
        : raw_(std::move(input)) {
        if (trim(raw_).empty()) {
            throw ParsingError(
                "Path must be a non-empty string",
                ParsingErrorOptions{{}, {}, {}, "Provide a valid file or directory path."});
        }

        if (raw_.find('\0') != std::string::npos) {
            throw ParsingError(
                "Path contains an embedded null byte",
                ParsingErrorOptions{{}, {raw_}, {}, "Embedded null bytes are not valid in filesystem paths."});
        }

        std::error_code error;
        const auto absolute = std::filesystem::absolute(raw_, error);
        if (error) {
            throw ParsingError(
                "Failed to normalize the provided path",
                ParsingErrorOptions{{}, {raw_}, {}, error.message()});
        }

        value_ = absolute.lexically_normal();
    }

    [[nodiscard]] const std::string& raw() const noexcept { return raw_; }
    [[nodiscard]] const std::filesystem::path& value() const noexcept { return value_; }

    [[nodiscard]] bool exists() const {
        std::error_code error;
        return std::filesystem::exists(value_, error) && !error;
    }

    [[nodiscard]] bool is_file() const {
        std::error_code error;
        return std::filesystem::is_regular_file(value_, error) && !error;
    }

    [[nodiscard]] bool is_directory() const {
        std::error_code error;
        return std::filesystem::is_directory(value_, error) && !error;
    }

    [[nodiscard]] std::string basename() const { return value_.filename().string(); }
    [[nodiscard]] std::string dirname() const { return value_.parent_path().string(); }
    [[nodiscard]] std::string extname() const { return value_.extension().string(); }
    [[nodiscard]] std::string normalized() const { return value_.string(); }
    [[nodiscard]] std::string absolute() const { return value_.string(); }

    /**
     * Reads the file contents using UTF-8 text mode.
     */
    [[nodiscard]] std::string read(std::string_view encoding = "utf-8") const {
        if (encoding != "utf-8" && encoding != "utf8") {
            throw ParsingError(
                "Unsupported file encoding",
                ParsingErrorOptions{{},
                                    {std::string(encoding)},
                                    {},
                                    "Only UTF-8 text reading is supported by the header-only build."});
        }

        if (!exists()) {
            throw ParsingError(
                "File does not exist",
                ParsingErrorOptions{{},
                                    {value_.string()},
                                    {},
                                    "Check that the path is correct and the file exists."});
        }

        if (!is_file()) {
            throw ParsingError(
                "Path does not point to a regular file",
                ParsingErrorOptions{{}, {value_.string()}, {}, "Provide a path to a readable file."});
        }

        std::ifstream stream(value_, std::ios::binary);
        if (!stream.is_open()) {
            throw ParsingError(
                "Failed to open file",
                ParsingErrorOptions{{},
                                    {value_.string()},
                                    {},
                                    "Check the file permissions and that the path is readable."});
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    [[nodiscard]] std::string string() const { return value_.string(); }
    [[nodiscard]] std::string to_string() const { return value_.string(); }

    [[nodiscard]] friend bool operator==(const Path& left, const Path& right) {
        return left.raw_ == right.raw_ && left.value_ == right.value_;
    }

    static Path parse(std::string_view input) { return Path(std::string(input)); }

    [[nodiscard]] static bool is_valid(std::string_view input) {
        if (trim(input).empty() || input.find('\0') != std::string_view::npos) {
            return false;
        }

        std::error_code error;
        const auto path = std::filesystem::path(input);
        const auto absolute = std::filesystem::absolute(path, error);
        (void)absolute;
        return !error;
    }

private:
    [[nodiscard]] static std::string trim(std::string_view value) {
        const auto begin = value.find_first_not_of(" \t\n\r\f\v");
        if (begin == std::string_view::npos) {
            return {};
        }

        const auto end = value.find_last_not_of(" \t\n\r\f\v");
        return std::string(value.substr(begin, end - begin + 1));
    }

    std::string raw_;
    std::filesystem::path value_;
};

}  // namespace clix
