# Dependencies

The library itself depends on nothing outside the C++17 standard library.

Everything below is fetched from source at configure time with CMake's
FetchContent, pinned to a full commit hash. No binaries or submodules are
committed.

| Name | Revision | Licence | Used for | Platforms |
| --- | --- | --- | --- | --- |
| [doctest](https://github.com/doctest/doctest) | v2.5.3 (`2d0a9359a60c51affe2a9bebb1be1dca47868151`) | MIT | Tests only (`OSCPM_BUILD_TESTS`) | All |
| [oscpp](https://github.com/kaoskorobase/oscpp) | 1.0.0 (`a62fe7690ce3563c997d7c9915d8cb54ff8f79b0`) | BSL-1.0 | Example only (`OSCPM_BUILD_EXAMPLES`) | All |

The pinned revisions are declared in `tests/CMakeLists.txt` and
`examples/CMakeLists.txt`.
