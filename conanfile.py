import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy, save
from conan.tools.layout import basic_layout

required_conan_version = ">=2.0"

# Raises the standard of every consumer target that links Metall::Metall. The
# engine headers are C++23, and a consumer may pin a lower standard on its own
# target (metall-ffi pins 20), which wins over the toolchain default. A compile
# feature on the imported target is what CMake takes the maximum of.
CXX_STD_MODULE = """\
if(TARGET Metall::Metall)
    set_property(TARGET Metall::Metall APPEND PROPERTY INTERFACE_COMPILE_FEATURES cxx_std_23)
endif()
"""


class MetallConan(ConanFile):
    """The metall fork, packaged so consumers can select the segment backend.

    Same package name, version, cmake file name and target name as the
    upstream recipe, so a consumer graph switches backends through
    [replace_requires] in a profile without touching any recipe.
    """

    name = "metall"
    version = "0.32"
    homepage = "https://github.com/dice-group/metall"
    description = "Meta allocator for persistent memory, with the privateer segment backend"
    license = "MIT", "Apache-2.0"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"use_privateer": [True, False]}
    default_options = {"use_privateer": True}
    exports_sources = "include/*", "LICENSE*", "COPYRIGHT"
    no_copy_source = True

    def layout(self):
        basic_layout(self)

    def requirements(self):
        self.requires("boost/[>=1.81 <2]", transitive_headers=True)
        if self.options.use_privateer:
            # The adapter is a header of this package and includes engine
            # headers, so consumers compile against them and link the engine.
            self.requires("privateer/0.2.0@dice-group/rewrite",
                          transitive_headers=True, transitive_libs=True)

    def validate(self):
        check_min_cppstd(self, 23 if self.options.use_privateer else 17)

    def package(self):
        copy(self, "*", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))
        copy(self, "LICENSE*", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYRIGHT", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        if self.options.use_privateer:
            save(self, os.path.join(self.package_folder, "lib", "cmake", "metall_cxx_std.cmake"),
                 CXX_STD_MODULE)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Metall")
        self.cpp_info.set_property("cmake_target_name", "Metall::Metall")

        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.requires = ["boost::headers"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("pthread")

        if self.options.use_privateer:
            # metall/metall.hpp includes metall/ext/privateer.hpp under this
            # define, and the adapter is what aliases metall::manager to
            # manager_privateer. Every consumer in one binary must see it, or
            # two translation units name two different types under one symbol.
            self.cpp_info.defines.append("METALL_USE_PRIVATEER")
            self.cpp_info.requires.append("privateer::privateer")
            self.cpp_info.set_property(
                "cmake_build_modules", [os.path.join("lib", "cmake", "metall_cxx_std.cmake")])
