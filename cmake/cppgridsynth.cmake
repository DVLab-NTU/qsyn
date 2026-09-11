# Integrate vendored cppgridsynth (GridSynth) as a static library for qsyn.
#
# GMP/MPFR are linked only into `cppgridsynth`; qsyn core keeps double/float.
# Override QSYN_CPPEGRIDSYNTH_DIR only when testing an external cppgridsynth tree.

option(QSYN_ENABLE_GRIDSYNTH "Build qcir gridsynth (requires GMP and MPFR)" ON)

if(NOT QSYN_ENABLE_GRIDSYNTH)
    message(STATUS "GridSynth integration disabled (QSYN_ENABLE_GRIDSYNTH=OFF)")
    return()
endif()

set(QSYN_CPPEGRIDSYNTH_DIR
    "${CMAKE_SOURCE_DIR}/vendor/cppgridsynth"
    CACHE PATH "cppgridsynth source tree (default: vendored under vendor/)")

if(NOT EXISTS "${QSYN_CPPEGRIDSYNTH_DIR}/include/cppgridsynth/gridsynth.hpp")
    message(FATAL_ERROR
        "Vendored cppgridsynth not found at ${QSYN_CPPEGRIDSYNTH_DIR}. "
        "Ensure vendor/cppgridsynth is present in the qsyn tree.")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/gridsynth-deps/dependencies.cmake)

set(_cppgridsynth_sources
    ${CMAKE_SOURCE_DIR}/src/qcir/gridsynth/gridsynth_adapter.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/mpfloat.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/mymath.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/ring.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/quantum_gate.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/quantum_circuit.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/circuit_stats.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/grid_op.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/region.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/odgp.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/tdgp.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/to_upright.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/diophantine.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/domega_unitary.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/normal_form.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/synthesis_of_cliffordT.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/gridsynth.cpp
    ${QSYN_CPPEGRIDSYNTH_DIR}/src/loop_controller.cpp
)

add_library(cppgridsynth STATIC ${_cppgridsynth_sources})
add_library(cppgridsynth::cppgridsynth ALIAS cppgridsynth)

target_include_directories(
    cppgridsynth
    PUBLIC
    ${QSYN_CPPEGRIDSYNTH_DIR}/include
    ${CMAKE_SOURCE_DIR}/src)

target_compile_features(cppgridsynth PUBLIC cxx_std_20)

target_compile_options(
    cppgridsynth
    PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter)

target_include_directories(cppgridsynth SYSTEM PRIVATE ${GRIDSYNTH_INCLUDE_DIRS})
target_link_libraries(cppgridsynth PUBLIC ${GRIDSYNTH_LIBRARIES})

function(qsyn_link_gridsynth target)
    target_compile_definitions(${target} PUBLIC QSYN_ENABLE_GRIDSYNTH=1)
    # PUBLIC so final executables (qsyn, unit-test) inherit GMP/MPFR on Linux.
    target_link_libraries(${target} PUBLIC cppgridsynth::cppgridsynth)
endfunction()

message(STATUS "GridSynth: cppgridsynth from ${QSYN_CPPEGRIDSYNTH_DIR}")
