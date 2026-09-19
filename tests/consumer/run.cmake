# Drives tests/consumer from ctest. Run as
#
#   cmake -DROUTE=<install|fetch> -DSOURCE=<oscpm tree> -DWORK=<scratch dir>
#         -DGENERATOR=<generator> -DCONFIG=<config> -DCXX=<compiler>
#         [-DOSCPP_SOURCE=<oscpp checkout>] -P run.cmake
#
# The install route builds oscpm afresh with tests and examples off, installs
# it to a prefix under WORK, then configures and builds the consumer against
# that prefix. The fetch route points the consumer's FetchContent at SOURCE.
# Both then run the two consumer programs.

foreach(variable ROUTE SOURCE WORK GENERATOR CONFIG)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "run.cmake needs -D${variable}=...")
    endif()
endforeach()

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status COMMAND_ECHO STDOUT)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "failed with status ${status}")
    endif()
endfunction()

set(common -G "${GENERATOR}" -DCMAKE_BUILD_TYPE=${CONFIG})
if(CXX)
    list(APPEND common -DCMAKE_CXX_COMPILER=${CXX})
endif()
# Avoid a second clone of oscpp when the outer build already has one.
if(OSCPP_SOURCE)
    list(APPEND common -DFETCHCONTENT_SOURCE_DIR_OSCPP=${OSCPP_SOURCE})
endif()

set(work ${WORK}/${ROUTE})
file(REMOVE_RECURSE ${work})
file(MAKE_DIRECTORY ${work})

set(consumer_args -DOSCPM_CONSUMER_ROUTE=${ROUTE})

if(ROUTE STREQUAL "install")
    run(${CMAKE_COMMAND} -S ${SOURCE} -B ${work}/oscpm-build ${common}
        -DOSCPM_BUILD_TESTS=OFF -DOSCPM_BUILD_EXAMPLES=OFF -DOSCPM_FETCH_OSCPP=ON)
    run(${CMAKE_COMMAND} --install ${work}/oscpm-build --prefix ${work}/prefix --config ${CONFIG})
    list(APPEND consumer_args -DCMAKE_PREFIX_PATH=${work}/prefix)
else()
    list(APPEND consumer_args -DOSCPM_CONSUMER_OSCPM_SOURCE=${SOURCE})
endif()

run(${CMAKE_COMMAND} -S ${SOURCE}/tests/consumer -B ${work}/consumer-build ${common} ${consumer_args})
run(${CMAKE_COMMAND} --build ${work}/consumer-build --config ${CONFIG})

run(${CMAKE_CTEST_COMMAND} --test-dir ${work}/consumer-build -C ${CONFIG} --output-on-failure)
