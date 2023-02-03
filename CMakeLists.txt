# Copyright (c) 2023 Sebastian Pipping <sebastian@pipping.org>
# Licensed under GPL v3 or later

cmake_minimum_required(VERSION 3.6)
# CMake features used that need more than CMake 3.0:
# - pkg_get_variable needs >=3.4
# - pkg_check_modules([..] IMPORTED_TARGET [..]) needs >=3.6

project(libvisual-projectm VERSION 2.1.1)

include(FindPkgConfig)

pkg_check_modules(LIBVISUAL libvisual-0.4 REQUIRED IMPORTED_TARGET)
pkg_get_variable(LIBVISUAL_PLUGINS_BASE_DIR libvisual-0.4 pluginsbasedir)
set(LIBVISUAL_ACTOR_PLUGINS_DIR "${LIBVISUAL_PLUGINS_BASE_DIR}/actor")

pkg_check_modules(LIBPROJECTM libprojectM REQUIRED IMPORTED_TARGET)
pkg_get_variable(LIBPROJECTM_PREFIX libprojectM prefix)

add_library(projectM_libvisual MODULE
        src/actor_projectM.cpp
        src/ConfigFile.cpp
        src/ConfigFile.h
        src/lvtoprojectM.h
        )

target_compile_definitions(projectM_libvisual
        PRIVATE
        PACKAGE_VERSION="${PROJECT_VERSION}"
        PROJECTM_PREFIX="${LIBPROJECTM_PREFIX}"
        )

target_link_libraries(projectM_libvisual
        PUBLIC
        PkgConfig::LIBPROJECTM
        PkgConfig::LIBVISUAL
        )

install(TARGETS projectM_libvisual
        LIBRARY DESTINATION "${LIBVISUAL_ACTOR_PLUGINS_DIR}"
        )

message(STATUS "Configuration:")
message(STATUS "  CMAKE_BUILD_TYPE:             \"${CMAKE_BUILD_TYPE}\"")
message(STATUS "  CMAKE_INSTALL_PREFIX:         \"${CMAKE_INSTALL_PREFIX}\"")
message(STATUS "  LIBPROJECTM_PREFIX:           \"${LIBPROJECTM_PREFIX}\"")
message(STATUS "  LIBVISUAL_ACTOR_PLUGINS_DIR:  \"${LIBVISUAL_ACTOR_PLUGINS_DIR}\"")
message(STATUS "  LIBVISUAL_PLUGINS_BASE_DIR:   \"${LIBVISUAL_PLUGINS_BASE_DIR}\"")
