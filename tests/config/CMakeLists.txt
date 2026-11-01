cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard_config_tests)
set(ERGODOX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_include_directories(app PRIVATE ${ERGODOX_SOURCE_DIR}/src)
target_sources(app PRIVATE
  test_config.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_config.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_config_store.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_layouts.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_runtime.c
)
