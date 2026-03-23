vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kkokotero/clix
    REF v0.3.0
    SHA512 24588ba7a922634d21051e269c356eb35b1272e6f3e3ed7ebaebcbe6037a516566fc800ba84e44eb502baa50e3dee1179b4eaf1fec6fa87d565def1e5d558270
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
