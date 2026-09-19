#include <oscpm/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("the version macros agree with each other and with CMake", "[version]")
{
    const std::string fromParts = std::to_string(OSCPM_VERSION_MAJOR) + "." +
                                  std::to_string(OSCPM_VERSION_MINOR) + "." +
                                  std::to_string(OSCPM_VERSION_PATCH);
    CHECK(fromParts == OSCPM_VERSION);
    CHECK(std::string(OSCPM_VERSION) == OSCPM_CMAKE_PROJECT_VERSION);
}
