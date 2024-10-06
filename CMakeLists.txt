cmake_minimum_required(VERSION 3.19)

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
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QSYN_PROJECT_NAME qsyn-debug)
else()
    set(QSYN_PROJECT_NAME qsyn)
endif()

project(
    ${QSYN_PROJECT_NAME}
    LANGUAGES CXX
    VERSION 0.7.0)

include(scripts/cmake/target_link_libraries_system.cmake)

# find_package(OpenMP REQUIRED)

set(BLA_VENDER OpenBLAS)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

FetchContent_Declare(
    xtl
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
    GIT_TAG 0.7.7)
FetchContent_MakeAvailable(xtl)

FetchContent_Declare(
    xtensor
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
    GIT_TAG 0.25.0)
FetchContent_MakeAvailable(xtensor)

FetchContent_Declare(
    xtensor-blas
    SYSTEM
    GIT_REPOSITORY https://github.com/xtensor-stack/xtensor-blas.git
    GIT_TAG 0.21.0)
FetchContent_MakeAvailable(xtensor-blas)

FetchContent_Declare(
    libfort
    SYSTEM
    GIT_REPOSITORY https://github.com/seleznevae/libfort.git
    GIT_TAG v0.4.2)
set(FORT_ENABLE_TESTING
    OFF
    CACHE INTERNAL "")
FetchContent_MakeAvailable(libfort)

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

FetchContent_Declare(
  libabc
  SYSTEM
  GIT_REPOSITORY https://github.com/DVLab-NTU/abc.git
  GIT_TAG add-cmake-fetchcontent-support)
FetchContent_MakeAvailable(libabc)

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
