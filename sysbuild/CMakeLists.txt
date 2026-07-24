cmake_minimum_required(VERSION 3.20.0)

# Sysbuild normally names its primary image after the repository directory.
# Use an explicitly named application entry so both keyboard halves are peers.
set(APP_DIR "${CMAKE_CURRENT_LIST_DIR}/ergodox_left"
    CACHE PATH "Main application source directory" FORCE)

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)
