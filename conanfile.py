from __future__ import annotations

import os
import re
from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class ClixConan(ConanFile):
    name = "clix"
    license = "MIT"
    url = "https://github.com/kkokotero/clix"
    homepage = "https://github.com/kkokotero/clix"
    description = (
        "A modern header-only C++ CLI toolkit with nested commands, validation, "
        "config files, env support, and shell completion."
    )
    topics = ("cli", "command-line", "cpp", "header-only", "parser")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def set_version(self) -> None:
        content = Path(self.recipe_folder, "CMakeLists.txt").read_text(encoding="utf-8")
        match = re.search(r"project\(clix VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)", content)
        if match is None:
            raise ConanInvalidConfiguration("Unable to resolve the CLIX version from CMakeLists.txt.")
        self.version = match.group(1)

    def export_sources(self) -> None:
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        copy(self, "LICENSE", self.recipe_folder, self.export_sources_folder)
        copy(self, "*", os.path.join(self.recipe_folder, "cmake"), os.path.join(self.export_sources_folder, "cmake"))
        copy(self, "*", os.path.join(self.recipe_folder, "src"), os.path.join(self.export_sources_folder, "src"))

    def layout(self) -> None:
        cmake_layout(self)

    def validate(self) -> None:
        cppstd = self.settings.get_safe("compiler.cppstd")
        if cppstd:
            check_min_cppstd(self, "17")

    def generate(self) -> None:
        toolchain = CMakeToolchain(self)
        toolchain.variables["CLIX_BUILD_EXAMPLES"] = False
        toolchain.variables["CLIX_BUILD_BENCHMARKS"] = False
        toolchain.variables["BUILD_TESTING"] = False
        toolchain.generate()

    def build(self) -> None:
        cmake = CMake(self)
        cmake.configure()

    def package(self) -> None:
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))

    def package_id(self) -> None:
        self.info.clear()

    def package_info(self) -> None:
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.builddirs = [os.path.join("lib", "cmake", "clix")]
        self.cpp_info.set_property("cmake_file_name", "clix")
        self.cpp_info.set_property("cmake_target_name", "clix::clix")
        self.cpp_info.set_property("pkg_config_name", "clix")
