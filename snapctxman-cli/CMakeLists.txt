cmake_minimum_required(VERSION 3.16)
project(snapctx-man VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configurable options (mirror meson_options.txt defaults)
set(CONF_FILE "snapctxman-cli.conf"
    CACHE STRING "Configuration file name")
set(CONF_DIR "\$HOME/.config/snapctxman-cli"
    CACHE STRING "Directory containing the configuration file")
set(DATADIR "${CMAKE_INSTALL_PREFIX}/share"
    CACHE STRING "Data directory for installed files (e.g. usage.txt)")

find_package(snapctxmanager REQUIRED
    HINTS /usr/local/cumbia-libs/lib/cmake/snapctxmanager)

add_executable(snapctx-man
    src/main.cpp
    src/utils.cpp
)

target_compile_definitions(snapctx-man PRIVATE
    CONF_FILE="${CONF_FILE}"
    CONF_DIR="${CONF_DIR}"
    DATADIR="${DATADIR}"
)

target_link_libraries(snapctx-man PRIVATE snapctxmanager::snapctxmanager)

install(TARGETS snapctx-man RUNTIME DESTINATION bin)
install(FILES usage.txt DESTINATION share)
