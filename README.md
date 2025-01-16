# opio

Yet another overengineered protobuf IO via tcp.


# Build

## Linux (verified on Ubuntu)

```bash

# Debug
conan install -pr:a ./conan_profiles/ubu-gcc-11 -s:a build_type=Debug --build missing -of _build .
( source ./_build/conanbuild.sh && cmake -B_build . -DCMAKE_TOOLCHAIN_FILE=_build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug )
cmake --build _build -j 6


# RelWithDebInfo
conan install -pr:a ./conan_profiles/ubu-gcc-11 -s:a build_type=RelWithDebInfo --build missing -of _build_release .
( source ./_build_release/conanbuild.sh && cmake -B_build_release . -DCMAKE_TOOLCHAIN_FILE=_build_release/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo )
cmake --build _build_release -j 6


# ASAN:
conan install -pr:a ./conan_profiles/ubu-gcc-11-asan --build missing -of _build_asan .
( source ./_build_asan/conanbuild.sh && cmake -B_build_asan . -DCMAKE_PREFIX_PATH=/home/ngrodzitski/qt673ASAN -DCMAKE_TOOLCHAIN_FILE=_build_asan/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo )
cmake --build _build_asan -j 6
```

