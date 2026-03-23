vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kkokotero/clix
    REF f4d90e24f428d6950cc777e14740223fd69a3393
    SHA512 9422390160ea5e269936e0087be1f9ea66acc99471eea42b0f2a6dfe04932b23091f9c0d4466af5bbced26cee625db0308dfa6e521dcb7c00877031c91157107
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
