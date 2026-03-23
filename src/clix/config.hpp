#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "detail/strings.hpp"
#include "exceptions/parsing_error/index.hpp"
#include "values/path.hpp"

namespace clix {

/**
 * Runtime settings for the built-in config file loader.
 */
struct ConfigFileSettings {
    bool enabled = false;
    std::string option_name = "config";
    std::vector<std::string> aliases = {"C"};
    std::string description = "Load values from an INI-like configuration file.";
    bool strict = true;
};

/**
 * Minimal INI-like config document used by the CLI runtime.
 *
 * Supported syntax:
 * - `key = value`
 * - `[section]`
 * - `#` / `;` comments
 */
class ConfigDocument {
public:
    using section_map = std::map<std::string, std::string>;
    using storage_type = std::map<std::string, section_map>;

    [[nodiscard]] const storage_type& sections() const noexcept { return sections_; }

    [[nodiscard]] const section_map* section(std::string_view name) const {
        const auto iterator = sections_.find(std::string(name));
        if (iterator == sections_.end()) {
            return nullptr;
        }
        return &iterator->second;
    }

    [[nodiscard]] std::vector<section_map> resolve_command_sections(
        const std::vector<std::string>& command_path) const {
        std::vector<section_map> sections;

        if (const auto* root = section(""); root != nullptr) {
            sections.push_back(*root);
        }

        std::string current;
        for (std::size_t index = 1; index < command_path.size(); ++index) {
            if (!current.empty()) {
                current += '.';
            }
            current += command_path[index];

            if (const auto* value = section(current); value != nullptr) {
                sections.push_back(*value);
            }
        }

        return sections;
    }

    static ConfigDocument parse(std::string_view text) {
        ConfigDocument document;
        auto current_section = std::string();
        document.sections_[current_section] = {};

        const auto lines = detail::split(text, '\n');

        for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
            auto line = detail::trim_copy(lines[line_index]);

            if (line.empty() || detail::starts_with(line, "#") || detail::starts_with(line, ";")) {
                continue;
            }

            if (detail::starts_with(line, "[") && detail::ends_with(line, "]")) {
                current_section = detail::trim_copy(std::string_view(line).substr(1, line.size() - 2));
                document.sections_[current_section];
                continue;
            }

            const auto separator = line.find('=');
            if (separator == std::string::npos) {
                throw ParsingError(
                    "Invalid config file line",
                    ParsingErrorOptions{{"key = value"},
                                        {"line " + std::to_string(line_index + 1) + ": " + line},
                                        {},
                                        "Config lines must use the form key = value."});
            }

            const auto key = detail::trim_copy(std::string_view(line).substr(0, separator));
            const auto value = detail::trim_copy(std::string_view(line).substr(separator + 1));

            if (key.empty()) {
                throw ParsingError(
                    "Config keys cannot be empty",
                    ParsingErrorOptions{{"key = value"},
                                        {"line " + std::to_string(line_index + 1) + ": " + line},
                                        {},
                                        "Provide a non-empty key before the '=' sign."});
            }

            document.sections_[current_section][key] = value;
        }

        return document;
    }

    static ConfigDocument parse_file(const Path& path) {
        return parse(path.read());
    }

private:
    storage_type sections_;
};

}  // namespace clix
