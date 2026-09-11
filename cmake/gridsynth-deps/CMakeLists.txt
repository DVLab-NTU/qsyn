cmake_minimum_required(VERSION 3.29)
project(qsyn_gridsynth_deps LANGUAGES NONE)

option(QSYN_ENABLE_GRIDSYNTH "Build qcir gridsynth (requires GMP and MPFR)" ON)
if(QSYN_ENABLE_GRIDSYNTH)
    enable_language(CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/dependencies.cmake)
endif()
