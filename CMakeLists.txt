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

set(PROJECT_NAME qsyn)

project(
  ${PROJECT_NAME}
  LANGUAGES CXX
  VERSION 1.0.0)

find_package(OpenMP REQUIRED)

set(BLA_VENDER OpenBLAS)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

FetchContent_Declare(
  xtl
  GIT_REPOSITORY https://github.com/xtensor-stack/xtl.git
  GIT_TAG 0.7.5)
FetchContent_MakeAvailable(xtl)

FetchContent_Declare(
  xtensor
  GIT_REPOSITORY https://github.com/xtensor-stack/xtensor.git
  GIT_TAG 0.24.6)
FetchContent_MakeAvailable(xtensor)

FetchContent_Declare(
  xtensor-blas
  GIT_REPOSITORY https://github.com/xtensor-stack/xtensor-blas.git
  GIT_TAG 0.20.0)
FetchContent_MakeAvailable(xtensor-blas)

file(
  GLOB_RECURSE SOURCES
  RELATIVE ${CMAKE_SOURCE_DIR}
  "src/**/*.cpp" "src/**/*.h")
add_executable(${PROJECT_NAME} ${SOURCES})

# lists all subdirectories of a directory
macro(SUBDIRLIST result curdir)
  file(GLOB children ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

subdirlist(INCLUDES ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})
# for tqdm.h
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/vendor/include)

target_link_libraries(
  ${PROJECT_NAME}
  xtl
  xtensor
  xtensor-blas
  lapack
  OpenMP::OpenMP_CXX
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES}
 )

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}
    COMMENT "Copied executable to ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}"
)
