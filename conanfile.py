from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy
from conan.tools.layout import basic_layout
from conan.tools.scm import Version
import os

required_conan_version = ">=2.0"


class MetallConan(ConanFile):
    # Header-only recipe for this branch. It follows the conan-center recipe for metall,
    # but packages the checked-out include/ tree instead of a release tarball.
    name = "metall"
    version = "0.36-pre"
    url = "https://github.com/dice-group/metall"
    homepage = "https://github.com/LLNL/metall"
    description = "Meta allocator for persistent memory"
    license = "MIT", "Apache-2.0"
    topics = "cpp", "allocator", "memory-allocator", "persistent-memory", "ecp", "exascale-computing"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True
    exports_sources = "include/*", "LICENSE*", "COPYRIGHT", "NOTICE"

    def layout(self):
        basic_layout(self)

    def requirements(self):
        self.requires("boost/[>=1.81 <2]")

    def package_id(self):
        self.info.clear()

    def validate(self):
        check_min_cppstd(self, 17)

        if self.settings.os not in ["Linux", "Macos"]:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires some POSIX functionalities like mmap.")

    def package(self):
        copy(self, "*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYRIGHT", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "Metall")
        self.cpp_info.set_property("cmake_target_name", "Metall::Metall")

        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("pthread")

        if self.settings.compiler == "gcc" or (self.settings.os == "Linux" and self.settings.compiler == "clang"):
            if Version(self.settings.compiler.version) < "9":
                self.cpp_info.system_libs += ["stdc++fs"]

        self.cpp_info.requires = ["boost::headers"]
