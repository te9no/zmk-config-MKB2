if(CONFIG_ZMK_RUNTIME_COMBO AND CONFIG_ZMK_CUSTOM_SETTINGS)
  zephyr_library()
  zephyr_library_sources(src/runtime_combo_boot.c)
endif()
