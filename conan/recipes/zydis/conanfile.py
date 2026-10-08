# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import copy, get
import os


class ZydisConan(ConanFile):
    """Zydis x86-64 instruction decoder (static, decoder-only).

    Encoder, formatter and the optional ISA extensions are disabled; the
    matching public `ZYDIS_DISABLE_*` defines are propagated so consumers
    compile against the exact same configuration.
    """

    name = "zydis"
    version = "4.1.1"
    package_type = "static-library"
    license = "MIT"
    url = "https://github.com/zyantific/zydis"
    homepage = "https://github.com/zyantific/zydis"

    settings = "os", "arch", "compiler", "build_type"

    def source(self):
        get(
            self,
            url=(
                "https://github.com/zyantific/zydis/archive/"
                "a2278f1d254e492f6a6b39f6cb5d1f5d515659dc.tar.gz"
            ),
            sha256="efbca210596ef954241ef40732376d50be5b6b7ca7d63a3834ef0c71f3471f36",
            strip_root=True,
            destination="zydis",
        )
        get(
            self,
            url=(
                "https://github.com/zyantific/zycore-c/archive/"
                "74620eefd233bec20daeb66e78e744ff06e273b7.tar.gz"
            ),
            sha256="51717e66bc94966ecc8ae415b2d90aae5e624d8da9198332a1e81aa898f82a55",
            strip_root=True,
            destination="zycore",
        )

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"
        self.folders.generators = "build/generators"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["ZYAN_ZYCORE_PATH"] = os.path.join(
            self.source_folder, "zycore"
        )
        tc.cache_variables["ZYDIS_BUILD_SHARED_LIB"] = False
        tc.cache_variables["ZYDIS_BUILD_EXAMPLES"] = False
        tc.cache_variables["ZYDIS_BUILD_TOOLS"] = False
        tc.cache_variables["ZYDIS_BUILD_TESTS"] = False
        tc.cache_variables["ZYDIS_BUILD_DOXYGEN"] = False
        tc.cache_variables["ZYDIS_BUILD_MAN"] = False
        tc.cache_variables["ZYDIS_FEATURE_ENCODER"] = False
        tc.cache_variables["ZYDIS_FEATURE_FORMATTER"] = False
        tc.cache_variables["ZYDIS_FEATURE_AVX512"] = False
        tc.cache_variables["ZYDIS_FEATURE_KNC"] = False
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder="zydis")
        cmake.build()

    def package(self):
        copy(
            self,
            "LICENSE",
            src=os.path.join(self.source_folder, "zydis"),
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "LICENSE",
            src=os.path.join(self.source_folder, "zycore"),
            dst=os.path.join(self.package_folder, "licenses", "zycore"),
        )
        copy(
            self,
            "*.a",
            src=self.build_folder,
            dst=os.path.join(self.package_folder, "lib"),
            keep_path=False,
        )
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "zydis", "include"),
            dst=os.path.join(self.package_folder, "include"),
            keep_path=True,
        )
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "zycore", "include"),
            dst=os.path.join(self.package_folder, "include"),
            keep_path=True,
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Zydis")
        self.cpp_info.set_property("cmake_target_name", "Zydis::Zydis")
        # Static lib order matters: Zydis depends on Zycore.
        self.cpp_info.libs = ["Zydis", "Zycore"]
        self.cpp_info.defines = [
            "ZYDIS_STATIC_BUILD",
            "ZYDIS_DISABLE_ENCODER",
            "ZYDIS_DISABLE_FORMATTER",
            "ZYDIS_DISABLE_AVX512",
            "ZYDIS_DISABLE_KNC",
        ]
