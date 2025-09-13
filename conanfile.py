from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import rmdir
from conan.tools.scm import Version
import os, sys, re

required_conan_version = ">=1.53.0"


class OpioOpioConan(ConanFile):
    def set_version(self):
        version_file_path = os.path.join(
            self.recipe_folder,
            "opio/include/opio/version.hpp"
        )
        with open(version_file_path, 'r') as file:
            content = file.read()
            major_match = re.search(r'VERSION_MAJOR\s+(\d+)ull', content)
            minor_match = re.search(r'VERSION_MINOR\s+(\d+)ull', content)
            patch_match = re.search(r'VERSION_PATCH\s+(\d+)ull', content)

            if major_match and minor_match and patch_match:
                major = int(major_match.group(1))
                minor = int(minor_match.group(1))
                patch = int(patch_match.group(1))

                self.version = f"{major}.{minor}.{patch}"
            else:
                raise ValueError(f"cannot detect version from {version_file_path}")

    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    name = "opio"

    license = "BSD-3-Clause"
    author = "Nicolai Grodzitski"
    url = "https://github.com/ngrodzitski/opio"
    homepage = "https://github.com/ngrodzitski/opio"
    description = "opio - Overengineered Protobuf IO"

    topics = ("opio", "asio", "network", "client-server",  "protobuf",)

    settings = "os", "compiler", "build_type", "arch"

    exports_sources = [
        "CMakeLists.txt",
        "opio/*",
        "net/*",
        "proto_entry/*",
        "cmake-scripts/*"
    ]
    no_copy_source = False
    build_policy = "missing"

    options = {"asio": ["boost", "standalone"]}
    default_options = {"asio": "standalone"}

    def _compiler_support_lut(self):
        return {
            "gcc": "11",
            "clang": "13",
            "Visual Studio": "22",
            "msvc": "193"
        }

    # This hint tells that this conanfile acts as
    # a conanfile for a package, which implies
    # it is responsible only for library itself.
    # Used to eliminate tests-related stuff (gtest, building tests)
    ACT_AS_PACKAGE_ONLY_CONANFILE = False

    def _is_package_only(self):
        return (
            self.ACT_AS_PACKAGE_ONLY_CONANFILE
            # The environment variable below can be used
            # to run conan create localy (used for debugging issues).
            or os.environ.get("OPIO_CONAN_PACKAGING") == "ON"
        )

    def _detect_asio_ref(self):
        if self.options.asio == "standalone":
            return "asio/1.36.0"
        else:
            asio_ref = "boost/1.83.0"

        return asio_ref

    def requirements(self):
        self.requires(self._detect_asio_ref())
        self.requires("fmt/11.2.0")
        self.requires("protobuf/6.30.1")
        self.requires("expected-lite/0.9.0")

        self.requires("json_dto/0.3.1")
        self.requires("logr/0.8.0")
        self.requires("rapidjson/cci.20230929", override=True)

    def configure(self):
        self.options["logr"].backend = "spdlog"

    def build_requirements(self):
        if not self._is_package_only():
            self.requires("cli11/2.5.0")
            self.test_requires("gtest/1.17.0")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def validate(self):
        minimal_cpp_standard = "17"
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, minimal_cpp_standard)
        minimal_version = self._compiler_support_lut()

        compiler = str(self.settings.compiler)
        if compiler not in minimal_version:
            self.output.warning(
                "%s recipe lacks information about the %s compiler standard version support" % (self.name, compiler))
            self.output.warning(
                "%s requires a compiler that supports at least C++%s" % (self.name, minimal_cpp_standard))
            return

        version = Version(self.settings.compiler.version)
        if version < minimal_version[compiler]:
            raise ConanInvalidConfiguration("%s requires a compiler that supports at least C++%s" % (self.name, minimal_cpp_standard))

    def layout(self):
        cmake_layout(self, src_folder=".", build_folder=".")
        self.folders.generators = ""

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["OPIO_INSTALL"] = True
        tc.variables[
            "OPIO_BUILD_TESTS"
        ] = not self._is_package_only()

        tc.generate()

        cmake_deps = CMakeDeps(self)
        cmake_deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.source_folder)
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", self.name)

        component_name = "_opio"
        self.cpp_info.components[component_name].set_property("cmake_target_name", f"{self.name}::opio")
        # TODO: consider adding alloaces
        # self.cpp_info.components[component_name].set_property("cmake_target_aliases", [f"{self.name}::{self.name}"])
        self.cpp_info.components[component_name].set_property("pkg_config_name", self.name)
        self.cpp_info.components[component_name].libs = [self.name]
        self.cpp_info.components[component_name].requires = [
            # TODO: add dependencies here.
            "fmt::fmt"
        ]

        # OS dependent settings
        # Here is an example:
        # if self.settings.os in ["Linux", "FreeBSD"]:
        #     self.cpp_info.components[component_name].system_libs.append("m")
        #     self.cpp_info.components[component_name].system_libs.append("pthread")
