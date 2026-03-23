#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "detail/strings.hpp"
#include "exceptions/parsing_error/index.hpp"
#include "parser/value/types.hpp"
#include "parser/value/utils/parse_json.hpp"

namespace clix {

/**
 * Runtime settings for the built-in config file loader.
 *
 * Config files are discovered in the following order:
 * 1. the explicit `--config` option
 * 2. the first non-empty environment variable listed here
 * 3. the first existing file listed in `default_filenames`
 *
 * Only `.toml` and `.json` files are supported.
 */
struct ConfigFileSettings {
    bool enabled = false;
    std::string option_name = "config";
    std::vector<std::string> aliases = {"C"};
    std::string description = "Load values from a JSON or TOML configuration file.";
    bool strict = true;
    std::vector<std::string> environment_variables;
    std::vector<std::string> default_filenames;
    std::vector<std::string> allowed_extensions = {".toml", ".json"};
};

/**
 * Minimal config document used by the CLI runtime.
 *
 * Supported formats:
 * - TOML tables and scalar values
 * - JSON objects with nested command sections
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

    static ConfigDocument parse_file(const Path& path, const ConfigFileSettings& settings = {}) {
        const auto extension = detail::to_lower_copy(path.value().extension().string());
        if (!extension_allowed(extension, settings.allowed_extensions)) {
            throw ParsingError(
                "Unsupported config file extension",
                ParsingErrorOptions{settings.allowed_extensions,
                                    {path.to_string()},
                                    {},
                                    "Use a .toml or .json config file."});
        }

        if (extension == ".json") {
            return parse_json(path.read());
        }

        return parse_toml(path.read());
    }

    static ConfigDocument parse_toml(std::string_view text) {
        ConfigDocument document;
        auto current_section = std::string();
        document.sections_[current_section] = {};

        const auto lines = detail::split(text, '\n');
        for (std::size_t line_index = 0; line_index < lines.size(); ++line_index) {
            auto line = strip_toml_comment(lines[line_index]);
            if (line.empty()) {
                continue;
            }

            if (detail::starts_with(line, "[") && detail::ends_with(line, "]")) {
                current_section = detail::trim_copy(std::string_view(line).substr(1, line.size() - 2));
                if (current_section.empty()) {
                    throw ParsingError(
                        "Config section name cannot be empty",
                        ParsingErrorOptions{{},
                                            {"line " + std::to_string(line_index + 1) + ": " + line},
                                            {},
                                            "Use a section name such as [project.create]."});
                }

                document.sections_[current_section];
                continue;
            }

            const auto separator = line.find('=');
            if (separator == std::string::npos) {
                throw ParsingError(
                    "Invalid TOML config line",
                    ParsingErrorOptions{{"key = value"},
                                        {"line " + std::to_string(line_index + 1) + ": " + line},
                                        {},
                                        "Config lines must use the form key = value."});
            }

            const auto key = detail::trim_copy(std::string_view(line).substr(0, separator));
            if (key.empty()) {
                throw ParsingError(
                    "Config keys cannot be empty",
                    ParsingErrorOptions{{"key = value"},
                                        {"line " + std::to_string(line_index + 1) + ": " + line},
                                        {},
                                        "Provide a non-empty key before the '=' sign."});
            }

            const auto value = normalize_toml_value(std::string_view(line).substr(separator + 1));
            document.sections_[current_section][key] = value;
        }

        return document;
    }

    static ConfigDocument parse_json(std::string_view text) {
        const auto root = detail::JsonParser(text).parse();
        if (!root.is_object()) {
            throw ParsingError(
                "Config JSON must contain an object at the top level",
                ParsingErrorOptions{{"{\"key\": \"value\"}"},
                                    {},
                                    {},
                                    "Wrap config keys inside a top-level JSON object."});
        }

        ConfigDocument document;
        document.sections_[""] = {};
        append_json_object(root.as_object(), "", document);
        return document;
    }

private:
    [[nodiscard]] static bool extension_allowed(std::string_view extension,
                                                const std::vector<std::string>& allowed_extensions) {
        if (extension.empty()) {
            return false;
        }

        for (const auto& allowed : allowed_extensions) {
            if (detail::to_lower_copy(allowed) == extension) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] static std::string strip_toml_comment(std::string_view line) {
        bool in_single_quote = false;
        bool in_double_quote = false;
        bool escaped = false;

        for (std::size_t index = 0; index < line.size(); ++index) {
            const auto current = line[index];

            if (in_double_quote && !escaped && current == '\\') {
                escaped = true;
                continue;
            }

            if (!escaped && current == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (current == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (!in_single_quote && !in_double_quote && current == '#') {
                return detail::trim_copy(line.substr(0, index));
            }

            escaped = false;
        }

        return detail::trim_copy(line);
    }

    [[nodiscard]] static std::string normalize_toml_value(std::string_view value) {
        const auto trimmed = detail::trim_copy(value);
        if (trimmed.empty()) {
            throw ParsingError(
                "Config values cannot be empty",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Provide a TOML scalar, string, or array value."});
        }

        if ((trimmed.front() == '"' && trimmed.back() == '"') ||
            (trimmed.front() == '\'' && trimmed.back() == '\'')) {
            return parse_toml_string(trimmed);
        }

        if (trimmed.front() == '[') {
            return parse_toml_array(trimmed);
        }

        return trimmed;
    }

    [[nodiscard]] static std::string parse_toml_string(std::string_view value) {
        if (value.size() < 2) {
            throw ParsingError(
                "Invalid TOML string literal",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Wrap TOML strings in matching quotes."});
        }

        const auto quote = value.front();
        if (value.back() != quote) {
            throw ParsingError(
                "Invalid TOML string literal",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Wrap TOML strings in matching quotes."});
        }

        const auto body = std::string_view(value).substr(1, value.size() - 2);
        if (quote == '\'') {
            return std::string(body);
        }

        std::string result;
        result.reserve(body.size());

        for (std::size_t index = 0; index < body.size(); ++index) {
            const auto current = body[index];
            if (current != '\\') {
                result.push_back(current);
                continue;
            }

            if (index + 1 >= body.size()) {
                throw ParsingError(
                    "Invalid TOML escape sequence",
                    ParsingErrorOptions{{},
                                        {std::string(value)},
                                        {},
                                        "Use a valid TOML string escape sequence."});
            }

            const auto escaped = body[++index];
            switch (escaped) {
                case '"':
                case '\\':
                    result.push_back(escaped);
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    throw ParsingError(
                        "Invalid TOML escape sequence",
                        ParsingErrorOptions{{},
                                            {std::string("\\") + escaped},
                                            {},
                                            "Use one of the supported escape sequences: \\\\, \\\", \\n, \\r, \\t."});
            }
        }

        return result;
    }

    [[nodiscard]] static std::string parse_toml_array(std::string_view value) {
        const auto trimmed = detail::trim_copy(value);
        if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
            throw ParsingError(
                "Invalid TOML array literal",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Wrap TOML arrays in square brackets."});
        }

        const auto inner = std::string_view(trimmed).substr(1, trimmed.size() - 2);
        const auto items = split_toml_array_items(inner);
        if (items.empty()) {
            throw ParsingError(
                "Empty TOML arrays are not supported",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Provide at least one value inside the array."});
        }

        std::vector<std::string> normalized;
        normalized.reserve(items.size());
        for (const auto& item : items) {
            const auto normalized_value = normalize_toml_value(item);
            if (!normalized_value.empty()) {
                normalized.push_back(normalized_value);
            }
        }

        if (normalized.empty()) {
            throw ParsingError(
                "Empty TOML arrays are not supported",
                ParsingErrorOptions{{},
                                    {std::string(value)},
                                    {},
                                    "Provide at least one value inside the array."});
        }

        return detail::join_strings(normalized, ", ");
    }

    [[nodiscard]] static std::vector<std::string> split_toml_array_items(std::string_view value) {
        std::vector<std::string> items;
        std::string current;
        bool in_single_quote = false;
        bool in_double_quote = false;
        bool escaped = false;
        int bracket_depth = 0;

        for (const auto character : value) {
            if (in_double_quote && !escaped && character == '\\') {
                escaped = true;
                current.push_back(character);
                continue;
            }

            if (!escaped && character == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                current.push_back(character);
                continue;
            }

            if (character == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                current.push_back(character);
                continue;
            }

            if (!in_single_quote && !in_double_quote) {
                if (character == '[') {
                    ++bracket_depth;
                } else if (character == ']') {
                    --bracket_depth;
                } else if (character == ',' && bracket_depth == 0) {
                    items.push_back(detail::trim_copy(current));
                    current.clear();
                    continue;
                }
            }

            current.push_back(character);
            escaped = false;
        }

        const auto tail = detail::trim_copy(current);
        if (!tail.empty()) {
            items.push_back(tail);
        }

        return items;
    }

    [[nodiscard]] static std::string json_value_to_string(const JsonValue& value) {
        if (std::holds_alternative<std::nullptr_t>(value.storage)) {
            throw ParsingError(
                "Null config values are not supported",
                ParsingErrorOptions{{},
                                    {"null"},
                                    {},
                                    "Use a concrete string, number, boolean, or array value."});
        }

        if (const auto* boolean = std::get_if<bool>(&value.storage)) {
            return *boolean ? "true" : "false";
        }
        if (const auto* number = std::get_if<double>(&value.storage)) {
            return detail::number_to_string(*number);
        }
        if (const auto* string = std::get_if<std::string>(&value.storage)) {
            return *string;
        }
        if (const auto* array = std::get_if<JsonValue::array_ptr>(&value.storage)) {
            std::vector<std::string> items;
            items.reserve((*array)->size());
            for (const auto& item : **array) {
                if (item.is_object() || item.is_array()) {
                    throw ParsingError(
                        "Nested JSON arrays or objects are not supported inside config arrays",
                        ParsingErrorOptions{{},
                                            {item.dump()},
                                            {},
                                            "Use arrays of scalar values only."});
                }

                items.push_back(json_value_to_string(item));
            }

            if (items.empty()) {
                throw ParsingError(
                    "Empty JSON arrays are not supported",
                    ParsingErrorOptions{{},
                                        {"[]"},
                                        {},
                                        "Provide at least one value inside the array."});
            }

            return detail::join_strings(items, ", ");
        }

        throw ParsingError(
            "Config values must be scalars or arrays",
            ParsingErrorOptions{{},
                                {value.dump()},
                                {},
                                "Use nested objects only to represent command sections."});
    }

    static void append_json_object(const JsonObject& object,
                                   const std::string& section_name,
                                   ConfigDocument& document) {
        auto& section = document.sections_[section_name];
        for (const auto& [key, value] : object) {
            if (value.is_object()) {
                const auto child_section =
                    section_name.empty() ? key : section_name + "." + key;
                append_json_object(value.as_object(), child_section, document);
                continue;
            }

            section[key] = json_value_to_string(value);
        }
    }

    storage_type sections_;
};

}  // namespace clix
