cmake_minimum_required(VERSION 3.20.0)

set(BOARD rp2350b_core/rp2350b/m33)
set(ERGODOX_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")

find_package(Zephyr REQUIRED)
project(ergodoxcustom)

include(${ERGODOX_SOURCE_DIR}/cmake/ergodox_application.cmake)
