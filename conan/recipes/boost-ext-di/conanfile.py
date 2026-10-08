# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

from conan import ConanFile
from conan.tools.files import copy, get
import os


class BoostDiConan(ConanFile):
    """boost-ext/di: header-only C++14 dependency injection library."""

    name = "boost-ext-di"
    version = "1.3.2"
    package_type = "header-library"
    license = "BSL-1.0"
    url = "https://github.com/boost-ext/di"
    homepage = "https://github.com/boost-ext/di"

    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def source(self):
        get(
            self,
            url=(
                "https://github.com/boost-ext/di/archive/"
                "629454327c37cb1ed86c7b1b33e955c942bbf537.tar.gz"
            ),
            sha256="41d0e1bc1bd7c50aadb3a4e75f58ad8aacd3debed7dc4a3fbc836431a327ee53",
            strip_root=True,
        )

    def package(self):
        copy(
            self,
            "di.hpp",
            src=os.path.join(self.source_folder, "include", "boost"),
            dst=os.path.join(self.package_folder, "include", "boost"),
        )
        copy(
            self,
            "LICENSE*",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_id(self):
        self.info.clear()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "boost-ext-di")
        self.cpp_info.set_property("cmake_target_name", "boost-ext-di::boost-ext-di")
