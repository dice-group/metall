import re
import os

from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import load, rmdir, copy


class DiceCopperrConan(ConanFile):
    license = "MIT/Apache"
    author = "DICE Group <info@dice-research.org>"
    homepage = "https://github.com/dice-group/metall"
    url = homepage
    topics = "persistent memory", "allocator"
    settings = "build_type", "compiler", "os", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_test_deps": [True, False],
        "build_ffi": [True, False],
        "with_default_logger": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_ffi": False,
        "with_test_deps": False,
        "with_default_logger": True,
    }

    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "libs/*", "CMakeLists.txt", "cmake/*", "LICENSE*", "COPYRIGHT", "NOTICE"

    def requirements(self):
        self.requires("boost/1.83.0", transitive_headers=True)

        if self.options.with_test_deps:
            self.requires("gtest/1.14.0")

    def set_name(self):
        if not hasattr(self, 'name') or self.version is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.name = re.search(r"project\(\s*([A-Za-z\-]+)\s+VERSION", cmake_file).group(1)

    def set_version(self):
        if not hasattr(self, 'version') or self.version is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.version = re.search(r"project\([^)]*VERSION\s+(\d+\.\d+.\d+)[^)]*\)", cmake_file).group(1)
        if not hasattr(self, 'description') or self.description is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.description = re.search(r"project\([^)]*DESCRIPTION\s+\"([^\"]+)\"[^)]*\)", cmake_file).group(1)

    _cmake = None

    def _configure_cmake(self):
        if self._cmake is None:
            self._cmake = CMake(self)
            self._cmake.configure(variables={"USE_CONAN": False, "BUILD_FFI": self.options.build_ffi,
                                             "WITH_DEFAULT_LOGGER": self.options.with_default_logger})

        return self._cmake

    def build(self):
        self._configure_cmake().build()

    def package(self):
        self._configure_cmake().install()

        for dir in ("cmake", "share"):
            rmdir(self, os.path.join(self.package_folder, dir))

        copy(self, pattern="LICENSE*", dst="licenses", src=self.folders.source_folder)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", self.name)

        self.cpp_info.components["global"].set_property("cmake_find_mode", "both")
        self.cpp_info.components["global"].set_property("cmake_file_name", self.name)
        self.cpp_info.components["global"].set_property("cmake_target_name", f"{self.name}::{self.name}")
        self.cpp_info.components["global"].includedirs = [f"include/{self.name}/{self.name}"]
        self.cpp_info.components["global"].libdirs = []
        self.cpp_info.components["global"].bindirs = []
        self.cpp_info.components["global"].requires = ["boost::headers"]

        if self.options.build_ffi:
            self.cpp_info.components["ffi"].set_property("cmake_find_mode", "both")
            self.cpp_info.components["ffi"].set_property("cmake_file_name", self.name)
            self.cpp_info.components["ffi"].set_property("cmake_target_name", f"{self.name}::ffi")
            self.cpp_info.components["ffi"].includedirs = [f"include/{self.name}/ffi"]
            self.cpp_info.components["ffi"].libdirs = [f"lib/{self.name}/ffi"]
            self.cpp_info.components["ffi"].libs = [f"{self.name}-ffi"]
            self.cpp_info.components["ffi"].requires = ["global"]

        if self.options.with_default_logger:
            self.cpp_info.components["default-logger"].set_property("cmake_find_mode", "both")
            self.cpp_info.components["default-logger"].set_property("cmake_file_name", self.name)
            self.cpp_info.components["default-logger"].set_property("cmake_target_name", f"{self.name}::default-logger")
            self.cpp_info.components["default-logger"].includedirs = []
            self.cpp_info.components["default-logger"].libdirs = [f"lib/{self.name}/default-logger"]
            self.cpp_info.components["default-logger"].libs = [f"{self.name}-default-logger"]
            self.cpp_info.components["default-logger"].requires = []

            self.cpp_info.components["global"].requires += ["default-logger"]
