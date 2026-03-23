from __future__ import annotations

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class ClixTestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain"
    test_type = "explicit"

    def requirements(self) -> None:
        self.requires(self.tested_reference_str)

    def layout(self) -> None:
        cmake_layout(self)

    def build(self) -> None:
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self) -> None:
        if can_run(self):
            executable = os.path.join(self.cpp.build.bindir, "clix_test_package")
            self.run(f"\"{executable}\" echo smoke", env="conanrun")
