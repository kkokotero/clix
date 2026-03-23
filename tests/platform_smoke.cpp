#include <clix/clix.hpp>
#include <clix/platform.hpp>

int main() {
    constexpr auto info = clix::platform::current_info();
    static_assert(info.kind != clix::platform::Kind::Unknown, "CLIX should detect the current target.");
    static_assert(info.has_filesystem, "CLIX expects std::filesystem support on supported targets.");
    static_assert(info.has_process_environment, "CLIX expects process environment access on supported targets.");

    clix::CLI cli("portable", "0.0.0");
    cli.description("Platform smoke fixture.");

    if (clix::platform::is_mobile()) {
        cli.command("sync").description("Mobile-friendly sync command.");
    } else {
        cli.command("shell").description("Desktop shell command.");
    }

    return 0;
}
