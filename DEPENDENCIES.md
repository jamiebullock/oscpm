# Dependencies

The library itself depends on nothing outside the C++17 standard library.

The components below are fetched from source at configure time with CMake's FetchContent, each pinned to a full commit hash. The repository contains none of them, and no binaries or submodules are committed. None of them is covered by `LICENSE`.

## Fetched at configure time

### doctest

- Source: [doctest/doctest](https://github.com/doctest/doctest), declared in `tests/CMakeLists.txt`
- Version: 2.5.3
- Revision: tag `v2.5.3`, commit `2d0a9359a60c51affe2a9bebb1be1dca47868151`
- Licence: MIT
- Copyright: Copyright (c) 2016-2023 Viktor Kirilov
- Used for: the tests, built when `BUILD_TESTING` is on
- Platforms: all

### Google Benchmark

- Source: [google/benchmark](https://github.com/google/benchmark), declared in `bench/CMakeLists.txt`
- Version: 1.9.5
- Revision: tag `v1.9.5`, commit `192ef10025eb2c4cdd392bc502f0c852196baa48`
- Licence: Apache-2.0
- Copyright: the authors listed in the upstream `AUTHORS` file. The source files say Copyright 2015 Google Inc.
- Used for: the benchmarks, built when `OSCPM_BUILD_BENCHMARKS` is on
- Platforms: all

### oscpp

- Source: [kaoskorobase/oscpp](https://github.com/kaoskorobase/oscpp), declared in `examples/CMakeLists.txt`
- Version: 1.0.0
- Revision: tag `1.0.0`, commit `a62fe7690ce3563c997d7c9915d8cb54ff8f79b0`
- Licence: BSL-1.0
- Copyright: Copyright (c) 2004-2018 Stefan Kersten
- Used for: the example, built when `OSCPM_BUILD_EXAMPLES` is on
- Platforms: all

## Obligations

- **The library.** None. The headers under `include/` use none of these components, and an installed copy of oscpm contains none of them.
- **doctest (MIT) and Google Benchmark (Apache-2.0).** A test or benchmark executable contains the component it links. A copy of that executable given to someone else carries the component's licence and copyright notice.
- **oscpp (BSL-1.0).** The licence asks for its notice in copies of the source and exempts compiled object code.
