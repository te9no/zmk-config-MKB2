/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>

#include "mode_manager.h"

LOG_MODULE_REGISTER(mkb_mode_manager, CONFIG_ZMK_LOG_LEVEL);

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(spi2), okay)
#error "mkb mode manager requires spi2 status = \"okay\""
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
#error "mkb mode manager requires i2c0 status = \"okay\""
#endif

#if !DT_NODE_HAS_STATUS(DT_NODELABEL(qdec0), okay)
#error "mkb mode manager requires qdec0 status = \"okay\""
#endif

#define SPI_NODE DT_NODELABEL(spi2)
#define I2C_NODE DT_NODELABEL(i2c0)
#define QDEC_NODE DT_NODELABEL(qdec0)
#define MUX_PINCTRL_NODE DT_NODELABEL(mkb_mux_pinctrl)

#define MUX_STATE_SPI 1U
#define MUX_STATE_I2C 2U
#define MUX_STATE_QDEC 3U

#if !DT_NODE_EXISTS(MUX_PINCTRL_NODE)
#error "mkb mode manager requires mkb_mux_pinctrl node"
#endif

static const pinctrl_soc_pin_t mux_default_pins[] =
    Z_PINCTRL_STATE_PINS_INIT(MUX_PINCTRL_NODE, pinctrl_0);
static const pinctrl_soc_pin_t mux_spi_pins[] =
    Z_PINCTRL_STATE_PINS_INIT(MUX_PINCTRL_NODE, pinctrl_1);
static const pinctrl_soc_pin_t mux_i2c_pins[] =
    Z_PINCTRL_STATE_PINS_INIT(MUX_PINCTRL_NODE, pinctrl_2);
static const pinctrl_soc_pin_t mux_qdec_pins[] =
    Z_PINCTRL_STATE_PINS_INIT(MUX_PINCTRL_NODE, pinctrl_3);

#define SETTINGS_ROOT "mkb_mode"
#define SETTINGS_KEY SETTINGS_ROOT "/mode"

struct mode_target {
    const char *name;
    uint8_t mode;
    uint8_t mux_state;
    const struct device *dev;
};

static const struct mode_target targets[] = {
    {
        .name = "SPI",
        .mode = MKB_BUS_MODE_SPI,
        .mux_state = MUX_STATE_SPI,
        .dev = DEVICE_DT_GET(SPI_NODE),
    },
    {
        .name = "I2C",
        .mode = MKB_BUS_MODE_I2C,
        .mux_state = MUX_STATE_I2C,
        .dev = DEVICE_DT_GET(I2C_NODE),
    },
    {
        .name = "QDEC",
        .mode = MKB_BUS_MODE_QDEC,
        .mux_state = MUX_STATE_QDEC,
        .dev = DEVICE_DT_GET(QDEC_NODE),
    },
};

static uint8_t current_mode = MKB_BUS_MODE_SPI;
static bool manager_initialized;

static bool is_valid_mode(uint8_t mode) { return mode < MKB_BUS_MODE_COUNT; }

static int apply_mux_state(uint8_t mux_state) {
    const pinctrl_soc_pin_t *pins = NULL;
    uint8_t pin_cnt = 0;

    switch (mux_state) {
    case MUX_STATE_SPI:
        pins = mux_spi_pins;
        pin_cnt = ARRAY_SIZE(mux_spi_pins);
        break;
    case MUX_STATE_I2C:
        pins = mux_i2c_pins;
        pin_cnt = ARRAY_SIZE(mux_i2c_pins);
        break;
    case MUX_STATE_QDEC:
        pins = mux_qdec_pins;
        pin_cnt = ARRAY_SIZE(mux_qdec_pins);
        break;
    default:
        pins = mux_default_pins;
        pin_cnt = ARRAY_SIZE(mux_default_pins);
        break;
    }

    return pinctrl_configure_pins(pins, pin_cnt, PINCTRL_REG_NONE);
}

static int suspend_target(const struct mode_target *target) {
    if (device_is_ready(target->dev)) {
        int ret = pm_device_action_run(target->dev, PM_DEVICE_ACTION_SUSPEND);
        if (ret < 0 && ret != -ENOSYS && ret != -EALREADY) {
            LOG_WRN("%s suspend failed: %d", target->name, ret);
        }
    }

    return 0;
}

static int resume_target(const struct mode_target *target) {
    if (device_is_ready(target->dev)) {
        int ret = pm_device_action_run(target->dev, PM_DEVICE_ACTION_RESUME);
        if (ret < 0 && ret != -ENOSYS && ret != -EALREADY) {
            LOG_WRN("%s resume failed: %d", target->name, ret);
        }
    }

    return 0;
}

static int apply_mode(uint8_t mode) {
    const struct mode_target *selected = NULL;

    for (size_t i = 0; i < ARRAY_SIZE(targets); i++) {
        const struct mode_target *target = &targets[i];

        if (target->mode == mode) {
            selected = target;
        } else {
            suspend_target(target);
        }
    }

    if (selected == NULL) {
        return -EINVAL;
    }

    int ret = apply_mux_state(selected->mux_state);
    if (ret < 0) {
        LOG_ERR("%s pinctrl state apply failed: %d", selected->name, ret);
        return ret;
    }

    ret = resume_target(selected);
    if (ret < 0) {
        return ret;
    }

    current_mode = mode;
    LOG_INF("Applied shared-pin mode: %u", mode);

    return 0;
}

static int mode_settings_set(const char *name, size_t len,
                             settings_read_cb read_cb, void *cb_arg) {
    const char *next;

    if (!settings_name_steq(name, "mode", &next) || next != NULL) {
        return -ENOENT;
    }

    if (len != sizeof(current_mode)) {
        return -EINVAL;
    }

    int ret = read_cb(cb_arg, &current_mode, sizeof(current_mode));
    if (ret < 0) {
        return ret;
    }

    if (!is_valid_mode(current_mode)) {
        LOG_WRN("Loaded invalid mode %u, fallback to SPI", current_mode);
        current_mode = MKB_BUS_MODE_SPI;
    }

    return 0;
}

static struct settings_handler mode_settings_handler = {
    .name = SETTINGS_ROOT,
    .h_set = mode_settings_set,
};

struct mkb_mode_sync_changed {
    uint8_t source;
    uint8_t mode;
    uint8_t persist;
    uint8_t reboot_remote;
};

ZMK_EVENT_DECLARE(mkb_mode_sync_changed);
ZMK_EVENT_IMPL(mkb_mode_sync_changed);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)
ZMK_RELAY_EVENT_HANDLE(mkb_mode_sync_changed, mms, source);
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(mkb_mode_sync_changed, mms, source);
#endif

static int mkb_mode_sync_listener_cb(const zmk_event_t *eh) {
    struct mkb_mode_sync_changed *ev = as_mkb_mode_sync_changed(eh);

    if (ev == NULL || !manager_initialized) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Ignore the local event raised on central. */
    if (ev->source == ZMK_RELAY_EVENT_SOURCE_SELF) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    int ret = mkb_mode_manager_set(ev->mode, ev->persist != 0U);
    if (ret < 0) {
        LOG_ERR("Relay mode apply failed: mode=%u ret=%d", ev->mode, ret);
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->reboot_remote != 0U) {
        LOG_INF("Relay mode applied on peripheral; rebooting");
        sys_reboot(SYS_REBOOT_COLD);
    }

    return ZMK_EV_EVENT_HANDLED;
}

ZMK_LISTENER(mkb_mode_sync_listener, mkb_mode_sync_listener_cb);
ZMK_SUBSCRIPTION(mkb_mode_sync_listener, mkb_mode_sync_changed);

int mkb_mode_manager_get(uint8_t *mode) {
    if (mode == NULL) {
        return -EINVAL;
    }

    *mode = current_mode;
    return 0;
}

int mkb_mode_manager_set(uint8_t mode, bool persist) {
    int ret;

    if (!is_valid_mode(mode)) {
        return -EINVAL;
    }

    ret = apply_mode(mode);
    if (ret < 0) {
        return ret;
    }

    if (!persist) {
        return 0;
    }

    ret = settings_save_one(SETTINGS_KEY, &mode, sizeof(mode));
    if (ret < 0) {
        LOG_ERR("Failed to persist mode %u: %d", mode, ret);
        return ret;
    }

    LOG_INF("Persisted mode %u", mode);
    return 0;
}

int mkb_mode_manager_sync_to_peripheral(uint8_t mode, bool persist, bool reboot_remote) {
    if (!is_valid_mode(mode)) {
        return -EINVAL;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct mkb_mode_sync_changed ev = {
        .source = ZMK_RELAY_EVENT_SOURCE_SELF,
        .mode = mode,
        .persist = persist ? 1U : 0U,
        .reboot_remote = reboot_remote ? 1U : 0U,
    };
    return raise_mkb_mode_sync_changed(ev);
#else
    ARG_UNUSED(persist);
    ARG_UNUSED(reboot_remote);
    return -ENOTSUP;
#endif
}

static int mkb_mode_manager_init(void) {
    int ret;

    ret = settings_subsys_init();
    if (ret < 0 && ret != -EALREADY) {
        LOG_ERR("settings_subsys_init failed: %d", ret);
        return ret;
    }

    ret = settings_register(&mode_settings_handler);
    if (ret < 0) {
        LOG_ERR("settings_register failed: %d", ret);
        return ret;
    }

    ret = settings_load_subtree(SETTINGS_ROOT);
    if (ret < 0) {
        LOG_WRN("settings_load_subtree failed: %d (using default mode)", ret);
        current_mode = MKB_BUS_MODE_SPI;
    }

    if (!is_valid_mode(current_mode)) {
        current_mode = MKB_BUS_MODE_SPI;
    }

    int ret_apply = apply_mode(current_mode);
    if (ret_apply == 0) {
        manager_initialized = true;
    }

    return ret_apply;
}

SYS_INIT(mkb_mode_manager_init, POST_KERNEL,
         CONFIG_ZMK_MKB_MODE_MUX_INIT_PRIORITY);
