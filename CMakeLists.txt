cmake_minimum_required(VERSION 3.16)
project(snapctxmanager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(sqldb-elettra REQUIRED
    HINTS /usr/local/cumbia-libs/lib/cmake/sqldb-elettra)

find_package(tango QUIET)
if(NOT tango_FOUND)
    # Fall back to manual tango setup
    find_path(TANGO_INCLUDE_DIR tango/tango.h
        PATHS /usr/local/tango-10.1.1/include)
    find_library(TANGO_LIB tango
        PATHS /usr/local/tango-10.1.1/lib)
    find_library(OMNIORB_LIB omniORB4
        PATHS /usr/local/omniorb-4.3.4/lib)
    find_library(OMNITHREAD_LIB omnithread
        PATHS /usr/local/omniorb-4.3.4/lib)
    find_library(OMNIDYNAMIC_LIB omniDynamic4
        PATHS /usr/local/omniorb-4.3.4/lib)
    find_path(OMNIORB_INCLUDE_DIR omniORB4/acconfig.h
        PATHS /usr/local/omniorb-4.3.4/include)
    add_library(tango INTERFACE IMPORTED)
    target_include_directories(tango INTERFACE
        ${TANGO_INCLUDE_DIR} ${OMNIORB_INCLUDE_DIR} /usr/local/include)
    target_link_libraries(tango INTERFACE ${TANGO_LIB} ${OMNIORB_LIB} ${OMNITHREAD_LIB} ${OMNIDYNAMIC_LIB})
endif()

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
        $<INSTALL_INTERFACE:include/snapctxmanager>
)

target_link_libraries(snapctxmanager PUBLIC sqldb-elettra tango)

set_target_properties(snapctxmanager PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

install(TARGETS snapctxmanager
    EXPORT snapctxmanagerTargets
    LIBRARY DESTINATION lib
)

install(FILES ${HEADERS} DESTINATION include/snapctxmanager)

install(EXPORT snapctxmanagerTargets
    FILE snapctxmanagerTargets.cmake
    NAMESPACE snapctxmanager::
    DESTINATION lib/cmake/snapctxmanager
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/snapctxmanagerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfig.cmake"
    INSTALL_DESTINATION lib/cmake/snapctxmanager
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/snapctxmanagerConfig.cmake"
    DESTINATION lib/cmake/snapctxmanager
)
