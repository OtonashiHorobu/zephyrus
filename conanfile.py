# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.scm import Git


class ZephyrusConan(ConanFile):
    name = "zephyrus"
    version = "0.1.0-alpha"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "lto": [True, False],
        "with_rmlui_lua": [True, False],
    }
    default_options = {
        "lto": False,
        "with_rmlui_lua": False,
        "*:shared": False,
    }

    def requirements(self):
        self.requires("spdlog/1.17.0")
        self.requires("fmt/12.1.0")
        self.requires("boost-ext-ut/2.3.1")
        # Not on ConanCenter; exported from conan/recipes (see scripts/conan-export.sh).
        self.requires("boost-ext-di/1.3.2")
        self.requires("rmlui/6.2")
        self.requires("cmrc/2.0.1")
        self.requires("zydis/4.1.1")
        self.requires("tomlplusplus/3.4.0")

    def configure(self):
        # Share one fmt instance between zephyrus and spdlog.
        self.options["spdlog"].fmt_external = True
        self.options["rmlui"].font_interface = "freetype"
        self.options["rmlui"].with_lua_bindings = self.options.with_rmlui_lua

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_INTERPROCEDURAL_OPTIMIZATION"] = (
            "ON" if self.options.lto else "OFF"
        )

        git = Git(self)
        try:
            branch = git.run(["symbolic-ref", "--short", "-q", "HEAD"]) or ""
            commit = git.run(["log", "-1", "--pretty=format:%h"]) or ""
        except ConanException:
            branch = ""
            commit = ""

        tc.variables["RELEASE_VERSION"] = self.version
        tc.variables["BRANCH_NAME"] = branch
        tc.variables["COMMIT_HASH"] = commit
        tc.generate()

        CMakeDeps(self).generate()
