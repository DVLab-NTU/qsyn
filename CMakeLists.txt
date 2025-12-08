cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(FetchContent)
include(CheckCXXCompilerFlag)
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)

# ---- helper: bump "cmake_minimum_required" in a subproject CMakeLists.txt ----
function(qsyn_bump_min_cmake project_source_dir min_ver)
  if(NOT EXISTS "${project_source_dir}/CMakeLists.txt")
    message(WARNING "qsyn_bump_min_cmake: no CMakeLists.txt at ${project_source_dir}")
    return()
  endif()

  file(READ "${project_source_dir}/CMakeLists.txt" _cml_orig)
  set(_cml "${_cml_orig}")

  # First try: canonical VERSION pattern (with optional spaces and range)
  string(REGEX REPLACE
         "cmake_minimum_required[ \t]*\([ \t]*VERSION[ \t]+[0-9]+\.[0-9]+(\.[0-9]+)?([ \t]*\.\.\.[^)]*)?\)"
         "cmake_minimum_required(VERSION ${min_ver})"
         _cml "${_cml}")

  # If unchanged, fall back to replacing the first cmake_minimum_required line generically
  if(_cml STREQUAL _cml_orig)
    string(REGEX REPLACE
           "^[ \t]*cmake_minimum_required[ \t]*\([^\n]*\)"
           "cmake_minimum_required(VERSION ${min_ver})"
           _cml "${_cml}")
  endif()

  file(WRITE "${project_source_dir}/CMakeLists.txt" "${_cml}")
  # Show a short preview of the first line after write for debugging
  file(STRINGS "${project_source_dir}/CMakeLists.txt" _first_line LIMIT_COUNT 1)
  message(STATUS "Bumped cmake_minimum_required in ${project_source_dir}/CMakeLists.txt to >= ${min_ver}; now: '${_first_line}'")
endfunction()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QSYN_PROJECT_NAME qsyn-debug)
else()
    set(QSYN_PROJECT_NAME qsyn)
endif()

project(
    ${QSYN_PROJECT_NAME}
    LANGUAGES CXX
    VERSION 0.8.0)

# Link the correct unwind library for Clang18 on Apple Silicon
# Assumes that Homebrew is used to install LLVM
# https://github.com/llvm/llvm-project/issues/92121#issuecomment-2307947170
if(CMAKE_SYSTEM_NAME MATCHES "Darwin"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"
    AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 18)
    message(
        STATUS 
        "Setting the correct unwind library for Clang18 on Apple Silicon")
# the clang should be installed at .../bin/clang++
# get the path to clang and remove the /bin/clang++
    execute_process(
        COMMAND which clang++
        OUTPUT_VARIABLE CLANG_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "/bin/clang\\+\\+" "" BREW_PREFIX ${CLANG_PATH})
    message(STATUS "BREW_PREFIX: ${BREW_PREFIX}")
    link_libraries(${BREW_PREFIX}/lib/libunwind.dylib)
    set(CMAKE_EXE_LINKER_FLAGS  
        "${CMAKE_EXE_LINKER_FLAGS} -L${BREW_PREFIX}/lib/c++")
endif()

include(scripts/cmake/target_link_libraries_system.cmake)

# find_package(OpenMP REQUIRED)

set(BLA_VENDER OpenBLAS)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

FetchContent_Declare(
    xtl
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
    GIT_TAG 0.8.0)
FetchContent_MakeAvailable(xtl)

FetchContent_Declare(
    xtensor
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
    GIT_TAG 0.26.0)
FetchContent_MakeAvailable(xtensor)

FetchContent_Declare(
    xtensor-blas
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor-blas.git
    GIT_TAG 0.22.0)
FetchContent_MakeAvailable(xtensor-blas)

# --- libfort (patched for new CMake) ---
FetchContent_Declare(
    libfort
    SYSTEM
    GIT_REPOSITORY https://github.com/seleznevae/libfort.git
    GIT_TAG v0.4.2
    GIT_SHALLOW ON
)

# turn off libfort's own tests
set(FORT_ENABLE_TESTING OFF CACHE INTERNAL "")

FetchContent_GetProperties(libfort)
if(NOT libfort_POPULATED)
  FetchContent_Populate(libfort)
endif()
# Always bump (even if already populated from a previous configure)
qsyn_bump_min_cmake("${libfort_SOURCE_DIR}" 3.10)
add_subdirectory("${libfort_SOURCE_DIR}" "${libfort_BINARY_DIR}" SYSTEM)

FetchContent_Declare(
    fmt
    SYSTEM
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    spdlog
    SYSTEM
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1)
set(SPDLOG_FMT_EXTERNAL
    ON
    CACHE INTERNAL "")
FetchContent_MakeAvailable(spdlog)

target_compile_definitions(spdlog PUBLIC SPDLOG_FMT_EXTERNAL=1)
target_compile_definitions(
    spdlog
    PRIVATE
    SPDLOG_SHORT_LEVEL_NAMES={\"[trace]\ \ \ \ \",\"[debug]\ \ \ \ \",\"[info]\ \ \ \ \ \",\"[warn]\ \ \ \ \ \",\"[error]\ \ \ \ \",\"[critical]\ \",\"\"}
)
target_link_libraries(spdlog PRIVATE fmt::fmt)

FetchContent_Declare(
  GSL
  SYSTEM
  GIT_REPOSITORY https://github.com/microsoft/GSL.git
  GIT_TAG v4.0.0
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(GSL)

FetchContent_Declare(
  dynamic_bitset
  SYSTEM
  GIT_REPOSITORY https://github.com/pinam45/dynamic_bitset.git
  GIT_TAG v1.2.1)
FetchContent_MakeAvailable(dynamic_bitset)

FetchContent_Declare(
  json
  SYSTEM
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  cadical
  SYSTEM
  # should use the upstream repo once the PR is merged
  GIT_REPOSITORY https://github.com/DVLab-NTU/cadical.git
  GIT_TAG add-cmake-fetchcontent-support)
FetchContent_MakeAvailable(cadical)

# --- libabc (patched for new CMake) ---
FetchContent_Declare(
  libabc
  SYSTEM
  GIT_REPOSITORY https://github.com/DVLab-NTU/abc.git
  GIT_TAG add-cmake-fetchcontent-support
  GIT_SHALLOW ON)

FetchContent_GetProperties(libabc)
if(NOT libabc_POPULATED)
  FetchContent_Populate(libabc)
endif()
# Always bump (even if already popuated from a previous configure)
qsyn_bump_min_cmake("${libabc_SOURCE_DIR}" 3.10)
add_subdirectory("${libabc_SOURCE_DIR}" "${libabc_BINARY_DIR}" SYSTEM)

FetchContent_Declare(
  Catch2
  SYSTEM
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

file(
  GLOB_RECURSE SOURCES
  RELATIVE ${CMAKE_SOURCE_DIR}
  "src/**/*.cpp" "src/**/*.hpp" "src/qsyn/qsynrc.default")

file(
  GLOB_RECURSE LIB_SOURCES
  RELATIVE ${CMAKE_SOURCE_DIR}
  "src/**/*.cpp" "src/**/*.hpp" "src/qsyn/qsynrc.default")

file(
    GLOB_RECURSE APP_SOURCES
    RELATIVE ${CMAKE_SOURCE_DIR}
    "src/argparse/*.cpp" "src/argparse/*.hpp"
    "src/argparse/**/*.cpp" "src/argparse/**/*.hpp"
    "src/cli/*.cpp" "src/cli/*.hpp"
    "src/cli/**/*.cpp" "src/cli/**/*.hpp"
    "src/cmd/*.cpp" "src/cmd/*.hpp"
    "src/cmd/**/*.cpp" "src/cmd/**/*.hpp"
    "src/qsyn/*.cpp" "src/qsyn/*.hpp" 
    "src/qsyn/**/*.cpp" "src/qsyn/**/*.hpp" 
    "src/qsyn/qsynrc.default")

list(REMOVE_ITEM LIB_SOURCES ${APP_SOURCES})

file(
    GLOB_RECURSE TEST_SOURCES
    RELATIVE ${CMAKE_SOURCE_DIR}
    "tests/src/**/*.cpp")

set(QSYN_LIB_NAME libqsyn.a)
set(UNIT_TEST_NAME unit-test)


# ----------------------------------------------------------------------------
# config for qsyn-lib target
# builds libqsyn.a
# ----------------------------------------------------------------------------
add_library(
    ${QSYN_LIB_NAME} STATIC ${LIB_SOURCES})

set_target_properties(
    ${QSYN_LIB_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})

target_include_directories(
    ${QSYN_LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_include_directories(
    ${QSYN_LIB_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/vendor)

target_link_libraries_system(
    ${QSYN_LIB_NAME}
    PRIVATE
    libabc
    xtl
    xtensor
    xtensor-blas
    libfort::fort
    fmt::fmt
    spdlog::spdlog
    Microsoft.GSL::GSL
    sul::dynamic_bitset
    nlohmann_json::nlohmann_json
    cadical::cadical)
target_link_libraries(
    ${QSYN_LIB_NAME} 
    PRIVATE 
    lapack 
# OpenMP::OpenMP_CXX 
    ${BLAS_LIBRARIES}
    ${LAPACK_LIBRARIES})

# ----------------------------------------------------------------------------
# config for qsyn target
# builds qsyn executable
# ----------------------------------------------------------------------------

add_executable(${CMAKE_PROJECT_NAME} ${APP_SOURCES})

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE ABC_NAMESPACE_HEADER_START=
                                ABC_NAMESPACE_HEADER_END=)

target_compile_definitions(
    ${CMAKE_PROJECT_NAME} PRIVATE QSYN_VERSION="v${CMAKE_PROJECT_VERSION}")
target_compile_definitions(
    ${CMAKE_PROJECT_NAME} PRIVATE QSYN_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

# compilers are being too paranoid about missing field initializers
target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE -Wno-missing-field-initializers)

# GCC 12+ is too strict about restrict warnings in std::string operations
# Only apply to GCC, as Clang doesn't support this flag
check_cxx_compiler_flag(-Wno-restrict COMPILER_SUPPORTS_WNO_RESTRICT)
if(COMPILER_SUPPORTS_WNO_RESTRICT)
    target_compile_options(
        ${CMAKE_PROJECT_NAME} PRIVATE -Wno-restrict)
endif()

target_include_directories(
    ${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# specify SYSTEM to suppress warnings from vendor code
target_include_directories(
    ${CMAKE_PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/vendor)

target_link_libraries(
    ${CMAKE_PROJECT_NAME} PRIVATE ${QSYN_LIB_NAME})

target_link_libraries_system(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
    libabc
    xtl
    xtensor
    xtensor-blas
    libfort::fort
    fmt::fmt
    spdlog::spdlog
    Microsoft.GSL::GSL
    sul::dynamic_bitset)
target_link_libraries(
    ${CMAKE_PROJECT_NAME} 
    PRIVATE 
    lapack 
# OpenMP::OpenMP_CXX 
    ${BLAS_LIBRARIES} 
    ${LAPACK_LIBRARIES})

# ----------------------------------------------------------------------------
# config for qsyn unit tests
# builds qsyn unit test executable
# ----------------------------------------------------------------------------

add_executable(${UNIT_TEST_NAME} ${TEST_SOURCES})

set_target_properties(
    ${UNIT_TEST_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME}-unit-test)

target_include_directories(
    ${UNIT_TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_include_directories(
    ${UNIT_TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/tests/src)

target_include_directories(
    ${UNIT_TEST_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/vendor)
target_link_libraries(
    ${UNIT_TEST_NAME} PRIVATE ${QSYN_LIB_NAME})
target_link_libraries(
    ${UNIT_TEST_NAME} PRIVATE Catch2::Catch2WithMain)

target_link_libraries_system(
    ${UNIT_TEST_NAME}
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    Microsoft.GSL::GSL)

target_compile_options(
    ${UNIT_TEST_NAME}
    PRIVATE 
    -Wall -Wextra -Werror)

# compilers are being too paranoid about missing field initializers
target_compile_options(
    ${UNIT_TEST_NAME}
    PRIVATE 
    -Wno-missing-field-initializers)

# GCC 12+ is too strict about restrict warnings in std::string operations
# Only apply to GCC, as Clang doesn't support this flag
if(COMPILER_SUPPORTS_WNO_RESTRICT)
    target_compile_options(
        ${UNIT_TEST_NAME}
        PRIVATE 
        -Wno-restrict)
endif()
