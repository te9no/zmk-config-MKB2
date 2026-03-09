/* SPDX-License-Identifier: MIT */

#ifndef MKB_MODE_MANAGER_H
#define MKB_MODE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum mkb_bus_mode {
    MKB_BUS_MODE_SPI = 0,
    MKB_BUS_MODE_I2C = 1,
    MKB_BUS_MODE_QDEC = 2,
    MKB_BUS_MODE_COUNT,
};

int mkb_mode_manager_get(uint8_t *mode);
int mkb_mode_manager_set(uint8_t mode, bool persist);
int mkb_mode_manager_sync_to_peripheral(uint8_t mode, bool persist, bool reboot_remote);

#ifdef __cplusplus
}
#endif

#endif /* MKB_MODE_MANAGER_H */
