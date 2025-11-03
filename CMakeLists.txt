# Copyright (c) 2023 Sebastian Pipping <sebastian@pipping.org>
# Licensed under GPL v3 or later

cmake_minimum_required(VERSION 3.21)

project(libvisual-projectm
    LANGUAGES C CXX
    VERSION 4.1.5
)

include(FindPkgConfig)

pkg_check_modules(LIBVISUAL libvisual-0.4 REQUIRED IMPORTED_TARGET)
pkg_get_variable(LIBVISUAL_PLUGINS_BASE_DIR libvisual-0.4 pluginsbasedir)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${LIBVISUAL_PLUGINS_BASE_DIR}")
endif()

find_package(projectM4 REQUIRED COMPONENTS Playlist)

add_library(actor_projectM SHARED MODULE
    src/actor_projectM.cpp
    src/ConfigFile.cpp
    src/ConfigFile.h
)

target_compile_definitions(actor_projectM
    PRIVATE
    PACKAGE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(actor_projectM
    PUBLIC
    PkgConfig::LIBVISUAL
    libprojectM::playlist
)

# Don't add "lib" prefix, keep name as-is.
set_target_properties(actor_projectM PROPERTIES
    PREFIX ""
)

install(TARGETS actor_projectM
    LIBRARY DESTINATION "actor/"
    RUNTIME DESTINATION "actor/"
)

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

message(STATUS "Configuration:")
if(IS_MULTI_CONFIG)
    message(STATUS "  CMAKE_CONFIGURATION_TYPES:    \"${CMAKE_CONFIGURATION_TYPES}\"")
else()
    message(STATUS "  CMAKE_BUILD_TYPE:             \"${CMAKE_BUILD_TYPE}\"")
endif()
message(STATUS "  CMAKE_INSTALL_PREFIX:         \"${CMAKE_INSTALL_PREFIX}\"")
message(STATUS "  PROJECTM_VERSION:             \"${projectM4_VERSION}\"")
message(STATUS "  LIBVISUAL_ACTOR_PLUGINS_DIR:  \"${LIBVISUAL_ACTOR_PLUGINS_DIR}\"")
message(STATUS "  LIBVISUAL_PLUGINS_BASE_DIR:   \"${LIBVISUAL_PLUGINS_BASE_DIR}\"")
