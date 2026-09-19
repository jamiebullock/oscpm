# The example feeds a hard-coded packet through oscpp and the adapter, so it
# needs the adapter target and nothing else: no sockets, no networking. Like
# the adapter target, it is silently absent when oscpp is not located.
if(NOT TARGET oscpm::oscpp)
    return()
endif()

add_executable(oscpp_dispatch oscpp_dispatch.cpp)
target_link_libraries(oscpp_dispatch PRIVATE oscpm::oscpp)
oscpm_strict_warnings(oscpp_dispatch)

# The example runs to completion with no input, so it doubles as a test.
if(OSCPM_BUILD_TESTS)
    add_test(NAME oscpm_example_oscpp_dispatch COMMAND oscpp_dispatch)
endif()
