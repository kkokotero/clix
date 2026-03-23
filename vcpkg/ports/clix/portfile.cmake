vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kkokotero/clix
    REF v0.2.1
    SHA512 058ed587e9fa65ff6e26f4a6a6842e27ada8d47ab9a4337f3f0791f1dfb7169a84426ba344a9fd130337ea94089c567cf6c7c4589be972e2101e5b00f7a0a1f1
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
