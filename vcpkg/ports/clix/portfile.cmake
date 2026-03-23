vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kkokotero/clix
    REF v0.3.1
    SHA512 7e2ef972a89a24cbb6a59e1366b753610987d3b76f29cb49f31d4b8386b0f2fa0402d39163f09018e82c4a51e1a9eabee957efe8774617d164e559f5a03f42cd
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
