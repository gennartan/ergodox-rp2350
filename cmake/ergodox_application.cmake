target_sources(app PRIVATE
  ${ERGODOX_SOURCE_DIR}/src/keyboard_input_state.c
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

# Serve one compressed document so styling cannot fail independently and a
# browser does not need parallel asset connections to load the configurator.
set(configurator_bundle ${ZEPHYR_BINARY_DIR}/include/generated/configurator_bundle.html)
add_custom_command(
  OUTPUT ${configurator_bundle}
  COMMAND ${PYTHON_EXECUTABLE} ${ERGODOX_SOURCE_DIR}/scripts/bundle_configurator.py
    --source ${ERGODOX_SOURCE_DIR}/src/web --output ${configurator_bundle}
  DEPENDS
    ${ERGODOX_SOURCE_DIR}/scripts/bundle_configurator.py
    ${ERGODOX_SOURCE_DIR}/src/web/index.html
    ${ERGODOX_SOURCE_DIR}/src/web/app.css
    ${ERGODOX_SOURCE_DIR}/src/web/app.js
  VERBATIM
)
generate_inc_file_for_target(app
  ${configurator_bundle}
  ${ZEPHYR_BINARY_DIR}/include/generated/configurator_index.html.inc
  --gzip
)
zephyr_linker_sources(SECTIONS ${ERGODOX_SOURCE_DIR}/src/configurator_sections.ld)
zephyr_linker_section(NAME http_resource_desc_configurator_http
  KVMA RAM_REGION GROUP RODATA_REGION
)
