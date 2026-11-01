target_sources(app PRIVATE
  ${ERGODOX_SOURCE_DIR}/src/configurator_mode.c
  ${ERGODOX_SOURCE_DIR}/src/main.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_layouts.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_runtime.c
)

target_sources_ifdef(CONFIG_ERGODOX_SPLIT_UART app PRIVATE
  ${ERGODOX_SOURCE_DIR}/src/split_transport_uart.c
)
