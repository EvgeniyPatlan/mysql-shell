# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA


#
# Set up gtest for use by targets in given folder and its sub-folders.
#
MACRO(SETUP_GTEST)
  IF (WITH_GTEST)
    INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIR})
  ENDIF (WITH_GTEST)
ENDMACRO(SETUP_GTEST)


SET(WITH_GTEST $ENV{WITH_GTEST} CACHE PATH "Location of gtest build")

#
# TODO: Try to find gtest in system-wide locations if WITH_GTEST is
# not set
#

IF(NOT WITH_GTEST)
  MESSAGE(FATAL_ERROR "This project requires gtest libraries"
          " but WITH_GTEST option was not specified."
          " Please set it to the location where gtest"
          " was built from sources using cmake.")
ENDIF()

#
# TODO: Configure gtest build if sources location is given
#

IF(NOT EXISTS "${WITH_GTEST}/CMakeCache.txt")
MESSAGE(FATAL_ERROR
  "Could not find gtest build in this location: ${WITH_GTEST}"
)
ENDIF()

#
# Read source location from build configuration cache and set
# GTEST_INCLUDE_DIR.
#

LOAD_CACHE(${WITH_GTEST} READ_WITH_PREFIX GTEST_
	  CMAKE_PROJECT_NAME)
#MESSAGE(STATUS "Gtest project name: ${GTEST_CMAKE_PROJECT_NAME}")

LOAD_CACHE(${WITH_GTEST} READ_WITH_PREFIX GTEST_
	  ${GTEST_CMAKE_PROJECT_NAME}_SOURCE_DIR)

FIND_PATH(GTEST_INCLUDE_DIR
        NAMES gtest/gtest.h
        PATHS ${GTEST_${GTEST_CMAKE_PROJECT_NAME}_SOURCE_DIR}/include ${GTEST_${GTEST_CMAKE_PROJECT_NAME}_SOURCE_DIR}/gtest/include
        NO_DEFAULT_PATH
          )

FIND_PATH(GMOCK_INCLUDE_DIR
        NAMES gmock/gmock.h
        PATHS ${GTEST_${GTEST_CMAKE_PROJECT_NAME}_SOURCE_DIR}/include ${GTEST_${GTEST_CMAKE_PROJECT_NAME}_SOURCE_DIR}/gtest/include
        NO_DEFAULT_PATH
          )

IF(NOT EXISTS "${GTEST_INCLUDE_DIR}/gtest/gtest.h")
  MESSAGE(FATAL_ERROR "Could not find gtest headers at: ${GTEST_INCLUDE_DIR}")
ENDIF()

MESSAGE(STATUS "GTEST_INCLUDE_DIR: ${GTEST_INCLUDE_DIR}")

#
# Import gtest and gtest_main libraries as targets of this project
#
# TODO: Run build if libraries can not be found in expected locations
#

FIND_LIBRARY(gtest_location
  NAMES libgtest gtest
  PATHS ${WITH_GTEST} ${WITH_GTEST}/gtest
  PATH_SUFFIXES . Release RelWithDebInfo Debug
  NO_DEFAULT_PATH
)

FIND_LIBRARY(gtest_main_location
  NAMES libgtest_main gtest_main
  PATHS ${WITH_GTEST} ${WITH_GTEST}/gtest
  PATH_SUFFIXES . Release RelWithDebInfo Debug
  NO_DEFAULT_PATH
)

FIND_LIBRARY(gmock_location
  NAMES libmonck gmock
  PATHS ${WITH_GTEST} ${WITH_GTEST}/gtest
  PATH_SUFFIXES . Release RelWithDebInfo Debug
  NO_DEFAULT_PATH
)

FIND_LIBRARY(gmock_main_location
  NAMES libgmock_main gmock_main
  PATHS ${WITH_GTEST} ${WITH_GTEST}/gtest
  PATH_SUFFIXES . Release RelWithDebInfo Debug
  NO_DEFAULT_PATH
)

#MESSAGE("gtest location: ${gtest_location}")
#MESSAGE("gtest_main location: ${gtest_main_location}")


add_library(gtest STATIC IMPORTED)
add_library(gtest_main STATIC IMPORTED)
add_library(gmock STATIC IMPORTED)
add_library(gmock_main STATIC IMPORTED)

set_target_properties(gtest PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
  IMPORTED_LOCATION "${gtest_location}"
)

set_target_properties(gtest_main PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES "gtest"
  IMPORTED_LOCATION "${gtest_main_location}"
)

set_target_properties(gmock PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
  IMPORTED_LOCATION "${gmock_location}"
)

set_target_properties(gmock_main PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES "gtest"
  IMPORTED_LOCATION "${gmock_main_location}"
)
#
#  Setup configuration-specific locations for Win
#  TODO: Should the same be done for OSX?
#

IF(WIN32)

  FOREACH(Config Debug RelWithDebInfo MinSizeRel Release)

  set(GTEST_BINDIR "${WITH_GTEST}")
    IF(EXISTS "${WITH_GTEST}/gtest")  # GTest is a subdirectory in GMock
      set(GTEST_BINDIR "${WITH_GTEST}/gtest")
    ENDIF()

  STRING(TOUPPER ${Config} CONFIG)

  set_property(TARGET gtest APPEND PROPERTY IMPORTED_CONFIGURATIONS ${CONFIG})
  set_target_properties(gtest PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES_${CONFIG} "CXX"
    IMPORTED_LOCATION_${CONFIG} "${GTEST_BINDIR}/${Config}/gtest.lib"
    )

  set_property(TARGET gtest_main APPEND PROPERTY IMPORTED_CONFIGURATIONS ${CONFIG})
  set_target_properties(gtest_main PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES_${CONFIG} "CXX"
    IMPORTED_LINK_INTERFACE_LIBRARIES_${CONFIG} "gtest"
    IMPORTED_LOCATION_${CONFIG} "${GTEST_BINDIR}/${Config}/gtest_main.lib"
    )

  set(GMOCK_BINDIR "${WITH_GTEST}")

  set_property(TARGET gmock APPEND PROPERTY IMPORTED_CONFIGURATIONS ${CONFIG})
  set_target_properties(gmock PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES_${CONFIG} "CXX"
    IMPORTED_LOCATION_${CONFIG} "${GMOCK_BINDIR}/${Config}/gmock.lib"
    )

  set_property(TARGET gmock_main APPEND PROPERTY IMPORTED_CONFIGURATIONS ${CONFIG})
  set_target_properties(gmock_main PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES_${CONFIG} "CXX"
    IMPORTED_LINK_INTERFACE_LIBRARIES_${CONFIG} "gtest"
    IMPORTED_LOCATION_${CONFIG} "${GMOCK_BINDIR}/${Config}/gmock_main.lib"
    )
  ENDFOREACH(Config)

ELSE(WIN32)

  # On unix gtest depends on pthread library
  set_property(TARGET gtest APPEND PROPERTY INTERFACE_LINK_LIBRARIES pthread)
  set_property(TARGET gtest APPEND PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES pthread)

ENDIF(WIN32)

