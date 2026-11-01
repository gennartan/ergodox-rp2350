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

if(NOT TARGET export_uf2)

# Publish the flashable images only after both firmware builds succeed.
# Always copy so the export timestamp reflects the latest successful build.
add_custom_target(export_uf2 ALL
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_BINARY_DIR}/ergodox_left/zephyr/ergodox_left.uf2"
    "${CMAKE_BINARY_DIR}/ergodox_left.uf2"
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_BINARY_DIR}/ergodox_right/zephyr/ergodox_right.uf2"
    "${CMAKE_BINARY_DIR}/ergodox_right.uf2"
  BYPRODUCTS
    "${CMAKE_BINARY_DIR}/ergodox_left.uf2"
    "${CMAKE_BINARY_DIR}/ergodox_right.uf2"
  DEPENDS ergodox_left ergodox_right
  COMMENT "Exporting both keyboard UF2 images to the build directory root"
  VERBATIM
)
endif()
