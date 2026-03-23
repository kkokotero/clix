#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "command.hpp"
#include "detail/strings.hpp"

namespace clix {

/**
 * Small composable router used to register command trees across multiple
 * translation units without forcing a specific application architecture.
 *
 * A router can:
 * - configure the current command with `root(...)`
 * - register nested commands with `command("path/to/command", ...)`
 * - compose child routers with `use(...)`
 */
class Router {
public:
    using Configurator = std::function<void(Command&)>;

    Router& root(Configurator configurator) {
        root_configurators_.push_back(std::move(configurator));
        return *this;
    }

    Router& command(std::string path, Configurator configurator) {
        routes_.push_back(RouteDefinition{parse_path(path), std::move(configurator)});
        return *this;
    }

    Router& use(const Router& child) {
        return use({}, child);
    }

    Router& use(std::string path, const Router& child) {
        const auto prefix = path.empty() ? std::vector<std::string>{} : parse_path(path);

        if (prefix.empty()) {
            root_configurators_.insert(root_configurators_.end(),
                                       child.root_configurators_.begin(),
                                       child.root_configurators_.end());
        } else {
            for (const auto& configurator : child.root_configurators_) {
                routes_.push_back(RouteDefinition{prefix, configurator});
            }
        }

        for (const auto& route : child.routes_) {
            auto full_path = prefix;
            full_path.insert(full_path.end(), route.path.begin(), route.path.end());
            routes_.push_back(RouteDefinition{std::move(full_path), route.configurator});
        }

        return *this;
    }

    void mount(Command& command) const {
        for (const auto& configurator : root_configurators_) {
            configurator(command);
        }

        for (const auto& route : routes_) {
            auto* current = &command;
            for (const auto& segment : route.path) {
                current = &current->ensure_command(segment);
            }
            route.configurator(*current);
        }
    }

private:
    struct RouteDefinition {
        std::vector<std::string> path;
        Configurator configurator;
    };

    [[nodiscard]] static std::vector<std::string> parse_path(std::string_view path) {
        const auto normalized = detail::trim_copy(path);
        if (normalized.empty()) {
            throw CommandError("Router command path cannot be empty");
        }

        const auto raw_segments = detail::split(normalized, '/');
        std::vector<std::string> segments;
        segments.reserve(raw_segments.size());

        for (const auto& raw_segment : raw_segments) {
            const auto segment = detail::trim_copy(raw_segment);
            if (segment.empty()) {
                throw CommandError(
                    "Router command path contains an empty segment",
                    CommandErrorOptions{"",
                                        "",
                                        "",
                                        {},
                                        {std::string(path)},
                                        {},
                                        "Use canonical paths such as \"project/create\"."});
            }

            segments.push_back(segment);
        }

        return segments;
    }

    std::vector<Configurator> root_configurators_;
    std::vector<RouteDefinition> routes_;
};

}  // namespace clix
