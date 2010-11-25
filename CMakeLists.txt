project(avon)
SET( V_MAJOR 0 )
SET( V_MINOR 1 )
SET( V_BUGFIX 0 )

SET( AVON_VERSION ${V_MAJOR}.${V_MINOR}.${V_BUGFIX} )
SET( AVON_APIVERSION ${V_MAJOR}.${V_MINOR} )


##############################################################################
# FOO_MAKE_PKGCONFIG
# Make a pkg-config file for a library
MACRO (FOO_MAKE_PKGCONFIG _name _desc _version _extDeps _intDeps _cFlags _libFlags)
  SET (PKG_NAME ${_name})
  SET (PKG_DESC ${_desc})
  SET (PKG_VERSION ${_version})
  SET (PKG_CFLAGS ${_cFlags})
  SET (PKG_LIBFLAGS ${_libFlags})
  SET (PKG_EXTERNAL_DEPS ${_extDeps})
  SET (PKG_INTERNAL_DEPS "")
  IF (${_intDeps})
    FOREACH (A ${_intDeps})
      SET (PKG_INTERNAL_DEPS "${PKG_INTERNAL_DEPS} -l${A}")
    ENDFOREACH (A ${_intDeps})
  ENDIF (${_intDeps})

  CONFIGURE_FILE (pkgconfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/${_name}.pc @ONLY)
  INSTALL (FILES ${CMAKE_CURRENT_BINARY_DIR}/${_name}.pc DESTINATION lib/pkgconfig/ COMPONENT pkgconfig)
ENDMACRO (FOO_MAKE_PKGCONFIG)

#####################################
# Build type cflags
SET (CMAKE_C_FLAGS_RELEASE " -std=c99 -DNDEBUG -Wall " CACHE INTERNAL "C Flags for release" FORCE)
SET (CMAKE_C_FLAGS_DEBUG " -std=c99 -g  -Wall " CACHE INTERNAL "C Flags for debug" FORCE)
SET (CMAKE_C_FLAGS_PROFILE " -std=c99 -g -pg -Wall " CACHE INTERNAL "C Flags for profile" FORCE)

#####################################
# Set the default build type
IF (NOT CMAKE_BUILD_TYPE)
  SET (CMAKE_BUILD_TYPE "release" CACHE STRING 
    "Choose the type of build, options are: release (default) debug profile" FORCE)
ENDIF (NOT CMAKE_BUILD_TYPE)
STRING(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)

MESSAGE( STATUS "Build type ${CMAKE_BUILD_TYPE}" )

# add_definitions(-g -Wall)
cmake_minimum_required( VERSION 2.4 FATAL_ERROR )
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)


include(FindPkgConfig)

pkg_search_module( LIBJSON REQUIRED json )
IF( LIBJSON_FOUND )
  MESSAGE( STATUS ${INDENT} "Libjson-c version ${LIBJSON_VERSION} detected at ${LIBJSON_PREFIX}" )
  MESSAGE( STATUS "    LIBJSON_CFLAGS = ${LIBJSON_CFLAGS}" )
  MESSAGE( STATUS "    LIBJSON_LDFLAGS = ${LIBJSON_LDFLAGS}" )
ELSE( LIBJSON_FOUND )
  MESSAGE( ${INDENT} "Libjson not detected" )
ENDIF( LIBJSON_FOUND )

# libevent doesn't support pkg-config, so we do it the old-fashioned way
message( STATUS "Checking for libevent" )
find_path( EVENT_INCLUDE_DIR event.h DOC "Libevent include dir" )
find_library( EVENT_LIB event DOC "Event lib" )

MESSAGE( STATUS "Libevent" )
MESSAGE( STATUS "  EVENT_INCLUDE_DIR = ${EVENT_INCLUDE_DIR}" )
MESSAGE( STATUS "  EVENT_LIB = ${EVENT_LIB}" )

include_directories(
   .
	src 
   ${LIBJSON_INCLUDE_DIRS}
   ${CMAKE_INCLUDE_PATH}
	${CMAKE_INSTALL_PREFIX}/include
)

link_directories( 
	${LIBJSON_LIBRARY_DIRS} 
	${CMAKE_INSTALL_PREFIX}/lib
)


add_library(avon SHARED src/avon.c src/json.c )
add_library(avon-static STATIC src/avon.c src/json.c )

# Set output name to be the same as shared lib (may not work on Windows)
set_target_properties(avon-static PROPERTIES OUTPUT_NAME avon)

# Prevent deletion of existing lib of same name
set_target_properties(avon-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(avon ${EVENT_LIB} json)

FOO_MAKE_PKGCONFIG( "avon" "HTTP interface for robot simulators" "${AVON_VERSION}" "" "" "-I${EVENT_INCLUDE_DIR}" "${JSON_LDFLAGS} -l${EVENT_LIB}" )

install(TARGETS avon avon-static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES src/avon.h DESTINATION include/avon )

INSTALL (FILES DESTINATION .)
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Avon")
SET(CPACK_PACKAGE_VENDOR "Autonomy Lab & The Player Project")
# SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.txt")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
SET(CPACK_PACKAGE_VERSION_MAJOR ${V_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${V_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${V_BUGFIX})
SET(CPACK_NSIS_DISPLAY_NAME "Avon ${V_MAJOR}.${V_MINOR}")
INCLUDE(CPack)
