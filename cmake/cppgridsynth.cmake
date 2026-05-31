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

# Auto-install GMP/MPFR when missing (Makefile also runs scripts/ensure_gridsynth_deps.sh).
execute_process(
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/ensure_gridsynth_deps.sh"
    RESULT_VARIABLE _gridsynth_deps_result
    OUTPUT_VARIABLE _gridsynth_deps_out
    ERROR_VARIABLE _gridsynth_deps_err)
if(NOT _gridsynth_deps_result EQUAL 0)
    message(FATAL_ERROR
        "GridSynth dependency setup failed (ensure_gridsynth_deps.sh):\n"
        "${_gridsynth_deps_out}\n${_gridsynth_deps_err}")
endif()
if(_gridsynth_deps_out)
    message(STATUS "${_gridsynth_deps_out}")
endif()

# Homebrew / MacPorts prefixes (MPFR pkg-config is often missing on macOS).
set(_gridsynth_prefix_hints)
if(APPLE)
    foreach(_brew_bin /opt/homebrew/bin/brew /usr/local/bin/brew)
        if(EXISTS "${_brew_bin}")
            execute_process(
                COMMAND "${_brew_bin}" --prefix
                OUTPUT_VARIABLE _brew_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            list(APPEND _gridsynth_prefix_hints "${_brew_prefix}")
            break()
        endif()
    endforeach()
    list(APPEND _gridsynth_prefix_hints /opt/homebrew /usr/local /opt/local)
endif()

find_package(PkgConfig QUIET)
set(_gridsynth_gmp_mpfr_ok FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_GMP IMPORTED_TARGET gmp)
    pkg_check_modules(PC_MPFR IMPORTED_TARGET mpfr)
    if(PC_GMP_FOUND AND PC_MPFR_FOUND)
        set(_gridsynth_gmp_mpfr_ok TRUE)
        set(_gridsynth_use_pkgconfig TRUE)
    endif()
endif()

if(NOT _gridsynth_gmp_mpfr_ok)
    find_path(
        GRIDSYNTH_GMP_INCLUDE
        NAMES gmp.h
        HINTS ${_gridsynth_prefix_hints}
        PATH_SUFFIXES include)
    find_library(
        GRIDSYNTH_GMP_LIB
        NAMES gmp
        HINTS ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)
    find_path(
        GRIDSYNTH_MPFR_INCLUDE
        NAMES mpfr.h
        HINTS ${_gridsynth_prefix_hints}
        PATH_SUFFIXES include)
    find_library(
        GRIDSYNTH_MPFR_LIB
        NAMES mpfr
        HINTS ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)
    find_library(
        GRIDSYNTH_GMPXX_LIB
        NAMES gmpxx
        HINTS ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)
    if(GRIDSYNTH_GMP_INCLUDE
       AND GRIDSYNTH_GMP_LIB
       AND GRIDSYNTH_MPFR_INCLUDE
       AND GRIDSYNTH_MPFR_LIB
       AND GRIDSYNTH_GMPXX_LIB)
        set(_gridsynth_gmp_mpfr_ok TRUE)
    endif()
endif()

if(NOT _gridsynth_gmp_mpfr_ok)
    message(FATAL_ERROR
        "GridSynth requires GMP and MPFR, but they were not found.\n"
        "Install them and reconfigure:\n"
        "  macOS  : brew install gmp mpfr\n"
        "  Debian : sudo apt install libgmp-dev libmpfr-dev\n"
        "  Fedora : sudo dnf install gmp-devel mpfr-devel\n"
        "Or build without GridSynth: cmake -DQSYN_ENABLE_GRIDSYNTH=OFF ...")
endif()

# libgmpxx (GMP's C++ wrapper) has no reliable pkg-config module, so resolve it
# directly in both the pkg-config and find_library branches. Linking it by its
# full path avoids relying on libgmpxx being on the default linker search path.
find_library(
    GRIDSYNTH_GMPXX_LIB
    NAMES gmpxx
    HINTS ${_gridsynth_prefix_hints}
    PATH_SUFFIXES lib)
if(NOT GRIDSYNTH_GMPXX_LIB)
    message(FATAL_ERROR
        "GridSynth requires libgmpxx (GMP's C++ wrapper), which was not found. "
        "It ships alongside GMP (macOS: brew install gmp; "
        "Debian: apt install libgmp-dev; Fedora: dnf install gmp-c++).")
endif()

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

if(_gridsynth_use_pkgconfig)
    target_link_libraries(
        cppgridsynth
        PUBLIC
        PkgConfig::PC_GMP
        PkgConfig::PC_MPFR
        ${GRIDSYNTH_GMPXX_LIB})
else()
    # SYSTEM PRIVATE: external headers; also keeps clang-tidy off GMP/MPFR.
    target_include_directories(
        cppgridsynth
        SYSTEM PRIVATE
        ${GRIDSYNTH_GMP_INCLUDE}
        ${GRIDSYNTH_MPFR_INCLUDE})
    target_link_libraries(
        cppgridsynth
        PUBLIC
        ${GRIDSYNTH_MPFR_LIB}
        ${GRIDSYNTH_GMP_LIB}
        ${GRIDSYNTH_GMPXX_LIB})
endif()

function(qsyn_link_gridsynth target)
    target_compile_definitions(${target} PUBLIC QSYN_ENABLE_GRIDSYNTH=1)
    # PUBLIC so final executables (qsyn, unit-test) inherit GMP/MPFR on Linux.
    target_link_libraries(${target} PUBLIC cppgridsynth::cppgridsynth)
endfunction()

message(STATUS "GridSynth: cppgridsynth from ${QSYN_CPPEGRIDSYNTH_DIR}")
