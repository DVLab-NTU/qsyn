cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)

project(
  qsyn
  LANGUAGES CXX
  VERSION 0.5.1)

find_package(OpenMP REQUIRED)

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
FetchContent_MakeAvailable(libfort)

FetchContent_Declare(
  fmt
  SYSTEM
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.0.0)
FetchContent_MakeAvailable(fmt)

file(
  GLOB_RECURSE SOURCES
  RELATIVE ${CMAKE_SOURCE_DIR}
  "src/**/*.cpp" "src/**/*.h")
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

add_definitions(-DQSYN_VERSION="${CMAKE_PROJECT_VERSION}")

target_include_directories(${CMAKE_PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${CMAKE_PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/vendor)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  xtl
  xtensor
  xtensor-blas
  lapack
  OpenMP::OpenMP_CXX
  libfort::fort
  fmt::fmt
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES})

# default to ON
option(COPY_EXECUTABLE "Copy executable to project root" ON)
if(COPY_EXECUTABLE)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME}
    COMMENT "Copied executable to ${CMAKE_SOURCE_DIR}/${CMAKE_PROJECT_NAME}")
endif()