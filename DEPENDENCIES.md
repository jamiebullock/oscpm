# Dependencies

The library itself depends on nothing outside the C++17 standard library.

Everything below is fetched from source at configure time with CMake's
FetchContent, pinned to a full commit hash. No binaries or submodules are
committed.

| Name | Revision | Licence | Used for | Platforms |
| --- | --- | --- | --- | --- |
| [Catch2](https://github.com/catchorg/Catch2) | v3.16.0 (`317ac1ed4c0bb6e6b91eafc817e05c488feffcb3`) | BSL-1.0 | Tests only (`OSCPM_BUILD_TESTS`) | All |
| [oscpp](https://github.com/kaoskorobase/oscpp) | 1.0.0 (`a62fe7690ce3563c997d7c9915d8cb54ff8f79b0`) | BSL-1.0 | Example only (`OSCPM_BUILD_EXAMPLES`) | All |

The pinned revisions are declared in `tests/CMakeLists.txt` and
`examples/CMakeLists.txt`.
