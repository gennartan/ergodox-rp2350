cmake_minimum_required(VERSION 3.20.0)

get_filename_component(ERGODOX_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Keep the application configuration and Kconfig in the repository root while
# using this directory solely to give the Sysbuild image a stable name.
set(APPLICATION_CONFIG_DIR "${ERGODOX_SOURCE_DIR}")
set(KCONFIG_ROOT "${ERGODOX_SOURCE_DIR}/Kconfig")

# Discover application bindings even when sysbuild uses its image directory.
list(APPEND DTS_ROOT "${ERGODOX_SOURCE_DIR}")

find_package(Zephyr REQUIRED)
project(ergodoxcustom)

include("${ERGODOX_SOURCE_DIR}/cmake/ergodox_application.cmake")
