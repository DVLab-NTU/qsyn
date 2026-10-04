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

foreach(_gridsynth_attempt RANGE 0 1)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_GMP QUIET gmp)
        pkg_check_modules(PC_MPFR QUIET mpfr)
    endif()

    find_path(GRIDSYNTH_GMP_INCLUDE gmp.h
        HINTS ${PC_GMP_INCLUDE_DIRS} ${PC_GMP_INCLUDEDIR} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES include)
    find_path(GRIDSYNTH_GMPXX_INCLUDE gmpxx.h
        HINTS ${PC_GMP_INCLUDE_DIRS} ${PC_GMP_INCLUDEDIR} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES include)
    find_path(GRIDSYNTH_MPFR_INCLUDE mpfr.h
        HINTS ${PC_MPFR_INCLUDE_DIRS} ${PC_MPFR_INCLUDEDIR} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES include)
    find_library(GRIDSYNTH_GMP_LIB gmp
        HINTS ${PC_GMP_LIBRARY_DIRS} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)
    find_library(GRIDSYNTH_GMPXX_LIB gmpxx
        HINTS ${PC_GMP_LIBRARY_DIRS} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)
    find_library(GRIDSYNTH_MPFR_LIB mpfr
        HINTS ${PC_MPFR_LIBRARY_DIRS} ${_gridsynth_prefix_hints}
        PATH_SUFFIXES lib)

    set(_gridsynth_missing)
    foreach(_dependency GMP_INCLUDE GMPXX_INCLUDE MPFR_INCLUDE GMP_LIB GMPXX_LIB MPFR_LIB)
        if(NOT GRIDSYNTH_${_dependency})
            list(APPEND _gridsynth_missing GRIDSYNTH_${_dependency})
        endif()
    endforeach()
    if(NOT _gridsynth_missing)
        break()
    endif()
    if(_gridsynth_attempt EQUAL 1)
        message(FATAL_ERROR
            "GridSynth dependencies still missing after installation: ${_gridsynth_missing}. "
            "Install GMP/MPFR development packages and reconfigure "
            "(Debian/Ubuntu: sudo apt install libgmp-dev libmpfr-dev pkg-config; "
            "macOS: brew install gmp mpfr).")
    endif()

    execute_process(
        COMMAND bash "${CMAKE_CURRENT_LIST_DIR}/../../scripts/install_gridsynth_deps.sh"
        RESULT_VARIABLE _gridsynth_install_result)
    if(NOT _gridsynth_install_result EQUAL 0)
        message(FATAL_ERROR
            "GridSynth: installing missing dependencies (${_gridsynth_missing}) failed "
            "with status ${_gridsynth_install_result}. See installer output above.")
    endif()
endforeach()

set(GRIDSYNTH_INCLUDE_DIRS
    ${GRIDSYNTH_GMP_INCLUDE} ${GRIDSYNTH_GMPXX_INCLUDE} ${GRIDSYNTH_MPFR_INCLUDE})
list(REMOVE_DUPLICATES GRIDSYNTH_INCLUDE_DIRS)
if(PkgConfig_FOUND AND PC_GMP_FOUND AND PC_MPFR_FOUND)
    pkg_check_modules(PC_GMP IMPORTED_TARGET QUIET gmp)
    pkg_check_modules(PC_MPFR IMPORTED_TARGET QUIET mpfr)
    set(GRIDSYNTH_LIBRARIES PkgConfig::PC_MPFR ${GRIDSYNTH_GMPXX_LIB} PkgConfig::PC_GMP)
else()
    set(GRIDSYNTH_LIBRARIES ${GRIDSYNTH_MPFR_LIB} ${GRIDSYNTH_GMPXX_LIB} ${GRIDSYNTH_GMP_LIB})
endif()

block()
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    try_compile(GRIDSYNTH_DEPS_COMPILE
        SOURCE_FROM_CONTENT gridsynth_deps.cpp [[
        #include <gmpxx.h>
        #include <mpfr.h>
        #include <sstream>
        int main() {
            mpz_class value = 42;
            std::ostringstream output;
            output << value;
            mpfr_t number;
            mpfr_init2(number, 128);
            mpfr_set_z(number, value.get_mpz_t(), MPFR_RNDN);
            mpfr_clear(number);
            return output.str() != "42";
        }
    ]]
        NO_CACHE
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${GRIDSYNTH_INCLUDE_DIRS}"
        LINK_LIBRARIES ${GRIDSYNTH_LIBRARIES}
        OUTPUT_VARIABLE _gridsynth_compile_output)
    if(NOT GRIDSYNTH_DEPS_COMPILE)
        message(FATAL_ERROR
            "GridSynth dependencies were found, but the C++ compile/link check failed. "
            "No package installation was attempted for this compile/link failure.\n"
            "${_gridsynth_compile_output}")
    endif()
endblock()
