cmake_minimum_required(VERSION 3.23)
project(clix VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(CLIX_BUILD_EXAMPLES "Build clix example programs" OFF)
option(CLIX_BUILD_BENCHMARKS "Build clix benchmark programs" OFF)

file(GLOB_RECURSE CLIX_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

add_library(clix INTERFACE)
add_library(clix::clix ALIAS clix)

target_include_directories(clix INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(clix INTERFACE cxx_std_17)

target_sources(clix INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
    FILES ${CLIX_HEADERS}
)

install(TARGETS clix EXPORT clixTargets
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT clixTargets
    FILE clixTargets.cmake
    NAMESPACE clix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clix
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/clixConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/clixConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clix
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/clixConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/clixConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/clixConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clix
)

if(CLIX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CLIX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
