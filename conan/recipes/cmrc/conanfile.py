# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

from conan import ConanFile
from conan.tools.files import copy, download
import os


class CmrcConan(ConanFile):
    """CMakeRC: a pure-CMake embedded resource generator.

    This is a build script, not a library, so the package exposes
    `CMakeRC.cmake` through Conan's `cmake_build_modules` property. Once
    `find_package(cmrc)` has run, `cmrc_add_resource_library()` is available
    and `cmrc.hpp` is generated inside the consumer's build directory.
    """

    name = "cmrc"
    version = "2.0.1"
    package_type = "build-scripts"
    license = "MIT"
    url = "https://github.com/vector-of-bool/cmrc"
    homepage = "https://github.com/vector-of-bool/cmrc"

    _base_url = (
        "https://raw.githubusercontent.com/vector-of-bool/cmrc/"
        "952ffddba731fc110bd50409e8d2b8a06abbd237"
    )

    def source(self):
        download(
            self,
            f"{self._base_url}/CMakeRC.cmake",
            "CMakeRC.cmake",
            sha256="c1c80edbea8d7f4c4541d46d9ca53354f396a4b1e3ff412662613e78864d62b8",
        )
        download(
            self,
            f"{self._base_url}/LICENSE.txt",
            "LICENSE.txt",
            sha256="1c82c1fff4d71c2728e422d36a456f8e8311a937457044ad7ddd567c828a45d0",
        )

    def package(self):
        copy(self, "CMakeRC.cmake", src=self.source_folder, dst=self.package_folder)
        copy(
            self,
            "LICENSE.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "cmrc")
        self.cpp_info.set_property("cmake_build_modules", ["CMakeRC.cmake"])
