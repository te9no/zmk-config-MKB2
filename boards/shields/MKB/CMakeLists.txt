# Copyright (c) 2025 The ZMK Contributors
# SPDX-License-Identifier: MIT

# Split builds can select multiple shields from this same directory
# (for example, a base shield plus a module shield), which causes ZMK
# to add this directory more than once. Guard the library definition so
# the shared sources are only registered once.
get_property(MKB_SHIELD_CMAKE_LOADED GLOBAL PROPERTY MKB_SHIELD_CMAKE_LOADED)
if(MKB_SHIELD_CMAKE_LOADED)
  return()
endif()
set_property(GLOBAL PROPERTY MKB_SHIELD_CMAKE_LOADED TRUE)

zephyr_library()

target_sources(app PRIVATE input_processor_trixer.c)

zephyr_include_directories(include)
