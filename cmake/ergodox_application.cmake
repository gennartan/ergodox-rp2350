target_sources(app PRIVATE
  ${ERGODOX_SOURCE_DIR}/src/keyboard_config.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_config_store.c
  ${ERGODOX_SOURCE_DIR}/src/configurator_network.c
  ${ERGODOX_SOURCE_DIR}/src/configurator_mode.c
  ${ERGODOX_SOURCE_DIR}/src/main.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_layouts.c
  ${ERGODOX_SOURCE_DIR}/src/keyboard_runtime.c
)

target_sources_ifdef(CONFIG_ERGODOX_SPLIT_UART app PRIVATE
  ${ERGODOX_SOURCE_DIR}/src/split_transport_uart.c
)

generate_inc_file_for_target(app
  ${ERGODOX_SOURCE_DIR}/src/web/index.html
  ${ZEPHYR_BINARY_DIR}/include/generated/configurator_index.html.inc
)
zephyr_linker_sources(SECTIONS ${ERGODOX_SOURCE_DIR}/src/configurator_sections.ld)
zephyr_linker_section(NAME http_resource_desc_configurator_http
  KVMA RAM_REGION GROUP RODATA_REGION
)
