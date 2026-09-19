include(FetchContent)

# Catch2 defaults to C++14, which leaves out its std::string_view support.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Catch2's TEST_CASE expands to __COUNTER__, which clang 22 and later report
# under -Wpedantic as a C2y extension. That is Catch2's business, not the
# library's, so the test executables alone silence it, where the flag exists.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wc2y-extensions OSCPM_HAVE_WC2Y_EXTENSIONS)
function(oscpm_test_warnings target)
    oscpm_strict_warnings(${target})
    if(OSCPM_HAVE_WC2Y_EXTENSIONS)
        target_compile_options(${target} PRIVATE -Wno-c2y-extensions)
    endif()
endfunction()

add_executable(oscpm_tests
    address_space_test.cpp
    allocation_counter.cpp
    corpus.cpp
    corpus_test.cpp
    pattern_test.cpp
    version_test.cpp
    wildcard_lookup_test.cpp)

target_link_libraries(oscpm_tests PRIVATE oscpm::oscpm Catch2::Catch2WithMain)

target_compile_definitions(oscpm_tests PRIVATE
    OSCPM_CORPUS_PATH="${PROJECT_SOURCE_DIR}/corpus/matching.txt"
    OSCPM_CMAKE_PROJECT_VERSION="${PROJECT_VERSION}")

oscpm_test_warnings(oscpm_tests)
catch_discover_tests(oscpm_tests)

# The adapter's smoke test is a separate binary so that the core suite never
# includes oscpp; it is skipped when the adapter target is absent.
if(TARGET oscpm::oscpp)
    add_executable(oscpm_oscpp_tests
        allocation_counter.cpp
        oscpp_adapter_test.cpp)

    target_link_libraries(oscpm_oscpp_tests PRIVATE oscpm::oscpp Catch2::Catch2WithMain)

    oscpm_test_warnings(oscpm_oscpp_tests)
    catch_discover_tests(oscpm_oscpp_tests)
endif()

# The consumer project (tests/consumer) is configured and built as a separate
# CMake project, once against an install prefix and once through
# FetchContent, so that both documented routes deliver both targets. Each
# needs oscpp, so both are skipped when the adapter target is absent.
if(TARGET oscpm::oscpp)
    set(OSCPM_CONSUMER_ARGS
        -DSOURCE=${PROJECT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCONFIG=$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Debug>)
    # A multi-config generator such as Visual Studio or Xcode chooses its own
    # compiler; only a single-config generator is told which one to use.
    get_property(OSCPM_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT OSCPM_MULTI_CONFIG)
        list(APPEND OSCPM_CONSUMER_ARGS -DCXX=${CMAKE_CXX_COMPILER})
    endif()
    if(oscpp_SOURCE_DIR)
        list(APPEND OSCPM_CONSUMER_ARGS -DOSCPP_SOURCE=${oscpp_SOURCE_DIR})
    endif()
    foreach(route install fetch)
        add_test(NAME oscpm_consumer_${route}
            COMMAND ${CMAKE_COMMAND} -DROUTE=${route} ${OSCPM_CONSUMER_ARGS}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer/run.cmake)
        set_tests_properties(oscpm_consumer_${route} PROPERTIES LABELS consumer TIMEOUT 600)
    endforeach()
endif()
