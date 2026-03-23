vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kkokotero/clix
    REF v0.2.0
    SHA512 b09db83ffdf7593712faa18038e3c5eab9bbdf5ed36f3a30d59bc0b1023bf4ea41e3171ad45e96c84fc1a27711c3c3e420aab9408b70b8d42d5ed80ffc19a0f0
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCLIX_BUILD_EXAMPLES=OFF
        -DCLIX_BUILD_BENCHMARKS=OFF
        -DBUILD_TESTING=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/clix PACKAGE_NAME clix)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug"
    "${CURRENT_PACKAGES_DIR}/lib"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
