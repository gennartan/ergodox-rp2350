# Build the same keyboard application once for each physical half.
#
# This file is included for every image that uses this source directory, so
# guard image creation to avoid recursively adding the right-hand image.
if(NOT TARGET ergodox_right)
  ExternalZephyrProject_Add(
    APPLICATION ergodox_right
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
  )

  set_config_bool(${DEFAULT_IMAGE} CONFIG_ERGODOX_KEYBOARD_HAND_LEFT y)
  set_config_bool(${DEFAULT_IMAGE} CONFIG_ERGODOX_KEYBOARD_HAND_RIGHT n)
  set_config_string(${DEFAULT_IMAGE} CONFIG_KERNEL_BIN_NAME "ergodox_left")

  set_config_bool(ergodox_right CONFIG_ERGODOX_KEYBOARD_HAND_LEFT n)
  set_config_bool(ergodox_right CONFIG_ERGODOX_KEYBOARD_HAND_RIGHT y)
  set_config_string(ergodox_right CONFIG_KERNEL_BIN_NAME "ergodox_right")
endif()
