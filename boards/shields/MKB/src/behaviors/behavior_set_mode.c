/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_behavior_set_mode

#include <errno.h>

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <zmk/behavior.h>

#include "mode_manager.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int behavior_set_mode_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

static int on_set_mode_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    uint8_t cmd = (uint8_t)binding->param1;
    uint8_t mode = 0;
    bool set_local = false;
    bool set_remote = false;

    if (cmd < MKB_BUS_MODE_COUNT) {
        mode = cmd;
        set_local = true;
    } else if (cmd >= 10U && cmd < (10U + MKB_BUS_MODE_COUNT)) {
        mode = cmd - 10U;
        set_remote = true;
    } else if (cmd >= 20U && cmd < (20U + MKB_BUS_MODE_COUNT)) {
        mode = cmd - 20U;
        set_local = true;
        set_remote = true;
    } else {
        LOG_ERR("set_mode invalid command: %u", cmd);
        return -EINVAL;
    }

    if (set_local) {
        int ret = mkb_mode_manager_set(mode, true);
        if (ret < 0) {
            LOG_ERR("set_mode local failed: mode=%u ret=%d", mode, ret);
            return ret;
        }
    }

    if (set_remote) {
        int ret = mkb_mode_manager_sync_to_peripheral(mode, true, true);
        if (ret < 0) {
            LOG_ERR("set_mode remote failed: mode=%u ret=%d", mode, ret);
            return ret;
        }
    }

    if (set_local) {
        if (set_remote) {
            /* Let relay payload flush before local reboot. */
            k_msleep(120);
        }
        LOG_INF("Mode switched (local=%u remote=%u) mode=%u; rebooting local", set_local, set_remote,
                mode);
        sys_reboot(SYS_REBOOT_COLD);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_set_mode_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_set_mode_driver_api = {
    .binding_pressed = on_set_mode_binding_pressed,
    .binding_released = on_set_mode_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_set_mode_init, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_set_mode_driver_api);

#endif
