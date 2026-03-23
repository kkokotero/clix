#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "detail/strings.hpp"

namespace clix {

/**
 * Supported shells for generated completion scripts.
 */
enum class CompletionShell {
    bash,
    zsh,
    fish,
    powershell,
};

/**
 * A single completion candidate that may optionally include a user-facing
 * description for shells that support richer completion UIs.
 */
struct CompletionSuggestion {
    std::string value;
    std::string description;

    [[nodiscard]] friend bool operator==(const CompletionSuggestion& left,
                                         const CompletionSuggestion& right) {
        return left.value == right.value && left.description == right.description;
    }
};

using CompletionProvider = std::function<std::vector<CompletionSuggestion>(std::string_view prefix)>;

/**
 * Runtime settings for shell completion support.
 */
struct CompletionSettings {
    bool enabled = false;
    std::string generate_option = "generate-completion";
    std::string hidden_command = "__clix_complete";
    std::string description = "Generate shell completion scripts.";
};

[[nodiscard]] inline std::string to_string(CompletionShell shell) {
    switch (shell) {
        case CompletionShell::bash:
            return "bash";
        case CompletionShell::zsh:
            return "zsh";
        case CompletionShell::fish:
            return "fish";
        case CompletionShell::powershell:
            return "powershell";
    }

    return "bash";
}

[[nodiscard]] inline std::optional<CompletionShell> completion_shell_from_string(std::string_view value) {
    const auto normalized = detail::to_lower_copy(value);

    if (normalized == "bash") {
        return CompletionShell::bash;
    }
    if (normalized == "zsh") {
        return CompletionShell::zsh;
    }
    if (normalized == "fish") {
        return CompletionShell::fish;
    }
    if (normalized == "powershell" || normalized == "pwsh" || normalized == "ps") {
        return CompletionShell::powershell;
    }

    return std::nullopt;
}

inline void filter_completion_suggestions(std::vector<CompletionSuggestion>& suggestions,
                                          std::string_view prefix) {
    const auto normalized_prefix = std::string(prefix);

    suggestions.erase(
        std::remove_if(suggestions.begin(),
                       suggestions.end(),
                       [&normalized_prefix](const CompletionSuggestion& suggestion) {
                           return !normalized_prefix.empty() &&
                                  !detail::starts_with(suggestion.value, normalized_prefix);
                       }),
        suggestions.end());

    std::vector<CompletionSuggestion> unique;
    unique.reserve(suggestions.size());

    for (const auto& suggestion : suggestions) {
        const auto duplicate = std::find_if(unique.begin(),
                                            unique.end(),
                                            [&suggestion](const CompletionSuggestion& current) {
                                                return current.value == suggestion.value;
                                            });

        if (duplicate == unique.end()) {
            unique.push_back(suggestion);
        }
    }

    suggestions = std::move(unique);
}

}  // namespace clix
