if(CONFIG_ZMK_RUNTIME_COMBO AND CONFIG_ZMK_CUSTOM_SETTINGS)
  zephyr_library()
  zephyr_library_sources(src/runtime_combo_boot.c)
endif()

if(CONFIG_SHIELD_MKB_L_ZS OR CONFIG_SHIELD_MKB_R_ZS)
  target_sources(app PRIVATE src/zeroshift/modules.c src/zeroshift/modules_relay.c)
  if(CONFIG_ZMK_ZEROSHIFT_STUDIO)
    generate_inc_file_for_target(app ${CMAKE_CURRENT_SOURCE_DIR}/config/zeroshift.json
                                 ${CMAKE_CURRENT_BINARY_DIR}/descriptor.inc --format=literal)
    target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_sources(app PRIVATE src/zeroshift/modules_rpc.c)
  endif()
endif()


if(CONFIG_ZMK_STUDIO_RPC AND SNIPPET MATCHES "mkb-sim-left")
  target_sources(app PRIVATE tests/renode/uart_endpoint.c)
endif()

if(CONFIG_ZMK_ZEROSHIFT AND (CONFIG_SHIELD_MKB_L_ZS OR CONFIG_SHIELD_MKB_R_ZS) AND NOT CONFIG_ZMK_SETTINGS_RESET_ON_START)
  zephyr_linker_sources(ROM_START SORT_KEY 0x2volume src/uf2_volume_id.ld)
endif()

# The pinned split central uses zero as both a valid input ID and an unset marker.
if(CONFIG_ZMK_ZEROSHIFT AND CONFIG_ZMK_SPLIT_ROLE_CENTRAL AND CONFIG_ZMK_INPUT_SPLIT AND CONFIG_ZMK_SPLIT_BLE)
  function(patch_split_input_discovery)
    set(dir "${APPLICATION_SOURCE_DIR}/src/split/bluetooth")
    set(original "${dir}/central.c")
    set(patched "${CMAKE_CURRENT_BINARY_DIR}/split_central_fixed.c")
    get_target_property(sources app SOURCES)
    list(FIND sources "${original}" index)
    if(index EQUAL -1)
      message(FATAL_ERROR "Split input source not found")
    endif()
    file(READ "${original}" code)
    macro(replace_split_fragment before after)
      string(FIND "${code}" "${before}" pos)
      if(pos EQUAL -1)
        message(FATAL_ERROR "Review split input ID patch against pinned ZMK")
      endif()
      string(REPLACE "${before}" "${after}" code "${code}")
    endmacro()
    replace_split_fragment("uint8_t reg;\n};" "uint8_t reg;\n    bool reg_known;\n};")
    replace_split_fragment("!peripheral_input_slots[i].reg);" "!peripheral_input_slots[i].reg_known);")
    replace_split_fragment("peripheral_input_slots[i].reg = 0;" "peripheral_input_slots[i].reg_known = false;")
    replace_split_fragment("input_slot->reg = cpf->description;" "input_slot->reg = cpf->description;\n                input_slot->reg_known = true;")
    replace_split_fragment("zmk_input_split_peripheral_disconnected(peripheral_input_slots[i].reg);"
                           "if (peripheral_input_slots[i].reg_known) {\n                zmk_input_split_peripheral_disconnected(peripheral_input_slots[i].reg);\n            }")
    # One ATT reply can hold several attributes. When switching queries, stop
    # parsing the old reply before its next attribute reaches the new handler.
    replace_split_fragment("switch (params->type) {"
                           "const uint8_t original_type = params->type;\n    const struct bt_uuid *original_uuid = params->uuid;\n    switch (params->type) {")
    replace_split_fragment("    bool subscribed = slot->run_behavior_handle"
[=[    if (params->type != original_type || params->uuid != original_uuid) {
        int err = bt_gatt_discover(conn, params);
        if (err) {
            LOG_ERR("Failed to continue split discovery: %d", err);
        }
        return BT_GATT_ITER_STOP;
    }

    bool subscribed = slot->run_behavior_handle]=])
    file(WRITE "${patched}" "${code}")
    list(REMOVE_AT sources ${index})
    list(APPEND sources "${patched}")
    set_property(TARGET app PROPERTY SOURCES "${sources}")
    set_source_files_properties("${patched}" PROPERTIES INCLUDE_DIRECTORIES "${dir}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
  endfunction()
  cmake_language(DEFER DIRECTORY "${APPLICATION_SOURCE_DIR}" CALL patch_split_input_discovery)
endif()
