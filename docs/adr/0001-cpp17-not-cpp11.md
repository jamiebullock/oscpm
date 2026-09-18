# Target C++17 rather than oscpp's C++11

oscpm complements oscpp, which is header-only C++11, so a reader would expect the same standard. We target C++17 instead: matching slices patterns into parts, alternatives and classes constantly, and doing that without `std::string_view` means either copying on the hot path (which breaks the no-allocation guarantee) or hand-rolling a view type. C++17 has been the baseline for JUCE and mainstream plugin toolchains for years; C++20 was rejected because it adds nothing essential and excludes some embedded targets.

## Consequences

- oscpm can be used alongside oscpp, but a project pinned to C++11 cannot use oscpm.
- The optional oscpp adapter header must not raise oscpp's own requirement; it only wraps `const char*` in a `string_view`.
