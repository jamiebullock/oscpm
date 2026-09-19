# 08: CI, install and export, 0.1.0 tag

**What to build:** Every push is verified on Apple Clang, GCC and MSVC at strict warning levels with warnings as errors, plus a one-minute fuzz pass under sanitizers. A consumer can `find_package(oscpm)` after a CMake install, or pull it with FetchContent, and get both the core and the adapter targets. The library reports its version through a macro, and 0.1.0 is tagged.

**Blocked by:** 06 oscpp adapter, example, README; 07 Fuzzing under sanitizers

**Status:** in-progress

- [x] GitHub Actions workflow with the three-compiler matrix, `-Wall -Wextra -Wpedantic -Werror` and `/W4 /WX`, running the full test suite and building the example
- [x] Workflow job running the fuzz target for one minute under ASan and UBSan on a supporting compiler
- [x] CMake install and export produce a config package; a separate consumer test project uses `find_package` and links both targets
- [x] `OSCPM_VERSION` macro and CMake project version agree; semantic versioning documented
- [ ] Repository has a GitHub remote, CI is green, and tag `0.1.0` is pushed
