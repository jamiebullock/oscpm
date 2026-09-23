# Dependencies

The library itself depends on nothing outside the C++17 standard library.

Everything below is fetched from source at configure time with CMake's
FetchContent, pinned to a full commit hash. No binaries or submodules are
committed.

| Name | Revision | Licence | Used for | Platforms |
| --- | --- | --- | --- | --- |
| [Catch2](https://github.com/catchorg/Catch2) | v3.16.0 (`317ac1ed4c0bb6e6b91eafc817e05c488feffcb3`) | BSL-1.0 | Tests only (`OSCPM_BUILD_TESTS`) | All |

The pinned revision is declared in `tests/CMakeLists.txt`.
