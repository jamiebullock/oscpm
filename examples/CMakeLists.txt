add_executable(oscpm_example_match match.cpp)
target_link_libraries(oscpm_example_match PRIVATE oscpm::oscpm)

if(TARGET oscpp::oscpp)
    add_executable(oscpm_example_dispatch dispatch.cpp)
    target_link_libraries(oscpm_example_dispatch PRIVATE oscpm::oscpm oscpp::oscpp)
endif()
