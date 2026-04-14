cmake_minimum_required(VERSION 3.16)
project(snapctxmanager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# provides CMAKE_INSTALL_LIBDIR, CMAKE_INSTALL_INCLUDEDIR, etc.
include(GNUInstallDirs)

# place headers under include/snapctxmanager (mirrors cumbia-tango convention)
set(CMAKE_INSTALL_INCLUDEDIR "include/${PROJECT_NAME}")

find_package(sqldb-elettra REQUIRED)

include(FindPkgConfig)
pkg_check_modules(tangodep REQUIRED tango)

set(SOURCES
    src/snapctxmanager.cpp
    src/snapdbschema.cpp
    src/ctxfileloader.cpp
    src/tgutils.cpp
)

set(HEADERS
    src/snapctxmanager.h
    src/snapctxmanager_p.h
    src/snapdbschema.h
    src/ctxfileloader.h
    src/snapctxman-listener.h
    src/tgutils.h
)

add_library(snapctxmanager SHARED ${SOURCES})

target_compile_definitions(snapctxmanager PRIVATE ELETTRA_EU_LIBSQLDB_PRINTINFO=1)

target_include_directories(snapctxmanager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Initialize the include directory
set(ADJUSTED_TANGO_INCLUDE_DIRS "")

# Iterate over each directory in the list
foreach(INCLUDE_DIR ${tangodep_INCLUDE_DIRS})
    if(EXISTS "${INCLUDE_DIR}/tango.h")
        message(STATUS "Found tango.h in ${INCLUDE_DIR}")
        list(APPEND ADJUSTED_TANGO_INCLUDE_DIRS "${INCLUDE_DIR}")
    elseif(EXISTS "${INCLUDE_DIR}/tango")
        message(STATUS "Found tango/ directory in ${INCLUDE_DIR}")
        # Append /tango to the include directory
        list(APPEND ADJUSTED_TANGO_INCLUDE_DIRS "${INCLUDE_DIR}/tango")
        message(STATUS "Adjusted include directory: ${INCLUDE_DIR}/tango")
    else()
        #message(WARNING "Neither tango.h nor tango/ directory found in ${INCLUDE_DIR}")
    endif()
    list(APPEND ADJUSTED_TANGO_INCLUDE_DIRS "${INCLUDE_DIR}")
endforeach()

target_include_directories(snapctxmanager PUBLIC ${ADJUSTED_TANGO_INCLUDE_DIRS})
target_link_libraries(snapctxmanager PUBLIC ${tangodep_LIBRARIES})
target_compile_options(snapctxmanager PUBLIC ${tangodep_CFLAGS_OTHER})
target_link_directories(snapctxmanager PUBLIC ${tangodep_LIBRARY_DIRS})


target_link_libraries(snapctxmanager PUBLIC sqldb-elettra)

set_target_properties(snapctxmanager PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${HEADERS}"
)

install(TARGETS snapctxmanager
    EXPORT snapctxmanagerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# export for build-tree usage
export(TARGETS snapctxmanager FILE snapctxmanagerTargets.cmake)

install(EXPORT snapctxmanagerTargets
    FILE snapctxmanagerTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/snapctxmanagerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
