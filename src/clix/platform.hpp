#pragma once

#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(_WIN32)
#define CLIX_PLATFORM_WINDOWS 1
#else
#define CLIX_PLATFORM_WINDOWS 0
#endif

#if defined(__ANDROID__)
#define CLIX_PLATFORM_ANDROID 1
#else
#define CLIX_PLATFORM_ANDROID 0
#endif

#if defined(__APPLE__) && ((defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || \
                           (defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR) || \
                           (defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST))
#define CLIX_PLATFORM_IOS 1
#else
#define CLIX_PLATFORM_IOS 0
#endif

#if defined(__APPLE__) && !CLIX_PLATFORM_IOS
#define CLIX_PLATFORM_MACOS 1
#else
#define CLIX_PLATFORM_MACOS 0
#endif

#if defined(__linux__) && !CLIX_PLATFORM_ANDROID
#define CLIX_PLATFORM_LINUX 1
#else
#define CLIX_PLATFORM_LINUX 0
#endif

#if defined(__FreeBSD__)
#define CLIX_PLATFORM_FREEBSD 1
#else
#define CLIX_PLATFORM_FREEBSD 0
#endif

#if defined(__OpenBSD__)
#define CLIX_PLATFORM_OPENBSD 1
#else
#define CLIX_PLATFORM_OPENBSD 0
#endif

#if defined(__NetBSD__)
#define CLIX_PLATFORM_NETBSD 1
#else
#define CLIX_PLATFORM_NETBSD 0
#endif

#if defined(__DragonFly__)
#define CLIX_PLATFORM_DRAGONFLYBSD 1
#else
#define CLIX_PLATFORM_DRAGONFLYBSD 0
#endif

#if CLIX_PLATFORM_FREEBSD || CLIX_PLATFORM_OPENBSD || CLIX_PLATFORM_NETBSD || CLIX_PLATFORM_DRAGONFLYBSD
#define CLIX_PLATFORM_BSD 1
#else
#define CLIX_PLATFORM_BSD 0
#endif

#if CLIX_PLATFORM_MACOS || CLIX_PLATFORM_IOS
#define CLIX_PLATFORM_APPLE 1
#else
#define CLIX_PLATFORM_APPLE 0
#endif

#if !CLIX_PLATFORM_WINDOWS && (defined(__unix__) || defined(_POSIX_VERSION) || CLIX_PLATFORM_APPLE || \
                               CLIX_PLATFORM_ANDROID || CLIX_PLATFORM_LINUX || CLIX_PLATFORM_BSD)
#define CLIX_PLATFORM_POSIX 1
#else
#define CLIX_PLATFORM_POSIX 0
#endif

#if CLIX_PLATFORM_IOS || CLIX_PLATFORM_ANDROID
#define CLIX_PLATFORM_MOBILE 1
#else
#define CLIX_PLATFORM_MOBILE 0
#endif

namespace clix::platform {

/**
 * Normalized target platform identifiers exposed by CLIX.
 */
enum class Kind {
    Windows,
    Macos,
    Ios,
    Linux,
    Android,
    Freebsd,
    Openbsd,
    Netbsd,
    Dragonflybsd,
    Unknown,
};

/**
 * Coarser platform families useful for feature gates and docs.
 */
enum class Family {
    Windows,
    Apple,
    Linux,
    Android,
    Bsd,
    Unix,
    Unknown,
};

/**
 * Snapshot of the current target platform capabilities as seen by CLIX.
 */
struct Info {
    Kind kind = Kind::Unknown;
    Family family = Family::Unknown;
    bool is_apple = false;
    bool is_bsd = false;
    bool is_mobile = false;
    bool is_posix = false;
    bool is_desktop = false;
    bool has_process_environment = false;
    bool has_filesystem = false;
};

/**
 * Returns the normalized platform identifier for the current target.
 */
[[nodiscard]] constexpr Kind current() noexcept {
#if CLIX_PLATFORM_WINDOWS
    return Kind::Windows;
#elif CLIX_PLATFORM_ANDROID
    return Kind::Android;
#elif CLIX_PLATFORM_IOS
    return Kind::Ios;
#elif CLIX_PLATFORM_MACOS
    return Kind::Macos;
#elif CLIX_PLATFORM_LINUX
    return Kind::Linux;
#elif CLIX_PLATFORM_FREEBSD
    return Kind::Freebsd;
#elif CLIX_PLATFORM_OPENBSD
    return Kind::Openbsd;
#elif CLIX_PLATFORM_NETBSD
    return Kind::Netbsd;
#elif CLIX_PLATFORM_DRAGONFLYBSD
    return Kind::Dragonflybsd;
#else
    return Kind::Unknown;
#endif
}

/**
 * Returns the broader family for the current target.
 */
[[nodiscard]] constexpr Family family() noexcept {
#if CLIX_PLATFORM_WINDOWS
    return Family::Windows;
#elif CLIX_PLATFORM_APPLE
    return Family::Apple;
#elif CLIX_PLATFORM_ANDROID
    return Family::Android;
#elif CLIX_PLATFORM_LINUX
    return Family::Linux;
#elif CLIX_PLATFORM_BSD
    return Family::Bsd;
#elif CLIX_PLATFORM_POSIX
    return Family::Unix;
#else
    return Family::Unknown;
#endif
}

/**
 * Converts a normalized platform identifier into a stable lowercase name.
 */
[[nodiscard]] constexpr std::string_view name(Kind kind) noexcept {
    switch (kind) {
        case Kind::Windows:
            return "windows";
        case Kind::Macos:
            return "macos";
        case Kind::Ios:
            return "ios";
        case Kind::Linux:
            return "linux";
        case Kind::Android:
            return "android";
        case Kind::Freebsd:
            return "freebsd";
        case Kind::Openbsd:
            return "openbsd";
        case Kind::Netbsd:
            return "netbsd";
        case Kind::Dragonflybsd:
            return "dragonflybsd";
        case Kind::Unknown:
            return "unknown";
    }
    return "unknown";
}

/**
 * Converts a platform family into a stable lowercase name.
 */
[[nodiscard]] constexpr std::string_view family_name(Family value) noexcept {
    switch (value) {
        case Family::Windows:
            return "windows";
        case Family::Apple:
            return "apple";
        case Family::Linux:
            return "linux";
        case Family::Android:
            return "android";
        case Family::Bsd:
            return "bsd";
        case Family::Unix:
            return "unix";
        case Family::Unknown:
            return "unknown";
    }
    return "unknown";
}

/**
 * Returns the lowercase name for the current target.
 */
[[nodiscard]] constexpr std::string_view name() noexcept {
    return name(current());
}

/**
 * Returns whether the current target belongs to Apple's platform family.
 */
[[nodiscard]] constexpr bool is_apple() noexcept {
    return CLIX_PLATFORM_APPLE == 1;
}

/**
 * Returns whether the current target is one of the BSD variants.
 */
[[nodiscard]] constexpr bool is_bsd() noexcept {
    return CLIX_PLATFORM_BSD == 1;
}

/**
 * Returns whether the current target is a mobile platform.
 */
[[nodiscard]] constexpr bool is_mobile() noexcept {
    return CLIX_PLATFORM_MOBILE == 1;
}

/**
 * Returns whether the current target behaves like a POSIX environment.
 */
[[nodiscard]] constexpr bool is_posix() noexcept {
    return CLIX_PLATFORM_POSIX == 1;
}

/**
 * Returns whether the current target is treated as a desktop-style platform.
 */
[[nodiscard]] constexpr bool is_desktop() noexcept {
    return !is_mobile();
}

/**
 * Returns whether process environment access is expected to be available.
 */
[[nodiscard]] constexpr bool has_process_environment() noexcept {
    return current() != Kind::Unknown;
}

/**
 * Returns whether std::filesystem is expected to be available.
 */
[[nodiscard]] constexpr bool has_filesystem() noexcept {
    return current() != Kind::Unknown;
}

/**
 * Returns a single aggregate view of the current target platform.
 */
[[nodiscard]] constexpr Info current_info() noexcept {
    return Info{
        current(),
        family(),
        is_apple(),
        is_bsd(),
        is_mobile(),
        is_posix(),
        is_desktop(),
        has_process_environment(),
        has_filesystem(),
    };
}

}  // namespace clix::platform
