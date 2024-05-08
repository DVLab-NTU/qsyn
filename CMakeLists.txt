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
  VERSION 0.6.3)

# find_package(OpenMP REQUIRED)

set(BLA_VENDER OpenBLAS)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

FetchContent_Declare(
  xtl
  SYSTEM
  GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
  GIT_TAG 0.7.5)
FetchContent_MakeAvailable(xtl)

FetchContent_Declare(
  xtensor
  SYSTEM
  GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
  GIT_TAG 0.24.6)
FetchContent_MakeAvailable(xtensor)

FetchContent_Declare(
  xtensor-blas
  SYSTEM
  GIT_REPOSITORY https://github.com/xtensor-stack/xtensor-blas.git
  GIT_TAG 0.20.0)
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

file(
  GLOB_RECURSE SOURCES
  RELATIVE ${CMAKE_SOURCE_DIR}
  "src/**/*.cpp" "src/**/*.hpp" "src/qsyn/qsynrc.default")
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE ABC_NAMESPACE_HEADER_START=
                                ABC_NAMESPACE_HEADER_END=)

target_compile_definitions(spdlog PUBLIC SPDLOG_FMT_EXTERNAL=1)
target_compile_definitions(
  spdlog
  PUBLIC
    SPDLOG_SHORT_LEVEL_NAMES={\"[trace]\ \ \ \ \",\"[debug]\ \ \ \ \",\"[info]\ \ \ \ \ \",\"[warn]\ \ \ \ \ \",\"[error]\ \ \ \ \",\"[critical]\ \",\"\"}
)

target_link_libraries(spdlog PUBLIC fmt::fmt)

target_compile_definitions(${CMAKE_PROJECT_NAME}
                           PRIVATE QSYN_VERSION="v${CMAKE_PROJECT_VERSION}")
target_compile_definitions(${CMAKE_PROJECT_NAME} 
                           PRIVATE QSYN_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

# compilers are being too paranoid about missing field initializers
target_compile_options(${CMAKE_PROJECT_NAME}
                        PRIVATE -Wno-missing-field-initializers)

target_include_directories(${CMAKE_PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/src)

# specify SYSTEM to suppress warnings from vendor code
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PRIVATE ${CMAKE_SOURCE_DIR}/vendor)

# black magic to link libraries as if they were system libraries
function(target_link_libraries_system target)
  set(options PRIVATE PUBLIC INTERFACE)
  cmake_parse_arguments(TLLS "${options}" "" "" ${ARGN})

  foreach(op ${options})
    if(TLLS_${op})
      set(scope ${op})
    endif()
  endforeach(op)

  set(libs ${TLLS_UNPARSED_ARGUMENTS})

  foreach(lib ${libs})
    get_target_property(lib_include_dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)

    if(lib_include_dirs)
      if(scope)
        target_include_directories(${target} SYSTEM ${scope}
                                   ${lib_include_dirs})
      else()
        target_include_directories(${target} SYSTEM PRIVATE ${lib_include_dirs})
      endif()
    else()
      message(
        "Warning: ${lib} doesn't set INTERFACE_INCLUDE_DIRECTORIES. No include_directories set."
      )
    endif()

    if(scope)
      target_link_libraries(${target} ${scope} ${lib})
    else()
      target_link_libraries(${target} ${lib})
    endif()
  endforeach()
endfunction(target_link_libraries_system)

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
  sul::dynamic_bitset
  nlohmann_json::nlohmann_json
  cadical::cadical)
target_link_libraries(
  ${CMAKE_PROJECT_NAME} PRIVATE lapack 
# OpenMP::OpenMP_CXX 
  ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})

# default to ON
option(COPY_EXECUTABLE " Copy executable to project root " ON)

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION bin)

if(COPY_EXECUTABLE)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
      ${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME}
    COMMENT " Copied executable to ${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME} ")
endif()
