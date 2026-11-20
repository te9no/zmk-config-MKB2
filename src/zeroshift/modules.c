#include "../dial_steps.h"
// SPDX-License-Identifier: MIT
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/sensor_event.h>
#include <zmk/matrix_transform.h>
#include <zeroshift/profile.h>

#define DEV(label) DEVICE_DT_GET(DT_NODELABEL(label))
enum module_mode { OFF, TB, JOY, ENC, KEY, RZT, TPD, LPPS, PAT };
BUILD_ASSERT(ZEROSHIFT_SLOT_COUNT == 1);
ZMK_MATRIX_TRANSFORM_EXTERN(DT_NODELABEL(default_transform));

static void key_event(struct input_event *event, void *unused) {
    if (event->type != INPUT_EV_KEY) return;
    int32_t position = zmk_matrix_transform_row_column_to_position(
        ZMK_MATRIX_TRANSFORM_T_FOR_NODE(DT_NODELABEL(default_transform)), 6, 0);
    if (position >= 0) {
        raise_zmk_position_state_changed((struct zmk_position_state_changed){
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL, .position = position,
            .state = event->value != 0, .timestamp = k_uptime_get()});
    }
}
INPUT_CALLBACK_DEFINE(DEV(module_key), key_event, NULL);

static void rotation(const struct device *dev, const struct sensor_trigger *trigger) {
    struct sensor_value value;
    if (sensor_sample_fetch(dev) || sensor_channel_get(dev, trigger->chan, &value)) return;
    if (!value.val1 && !value.val2) return;
    raise_zmk_sensor_event((struct zmk_sensor_event){
        .sensor_index = IS_ENABLED(CONFIG_SHIELD_MKB_R_ZS), .channel_data_size = 1,
        .channel_data = {{.value = value, .channel = trigger->chan}}, .timestamp = k_uptime_get()});
}

static int start_encoder(const struct device *dev) {
    static const struct sensor_trigger trigger = {.type = SENSOR_TRIG_DATA_READY, .chan = SENSOR_CHAN_ROTATION};
    int err = device_init(dev);
    return err ? err : sensor_trigger_set(dev, &trigger, rotation);
}



static int start_on_bus(const struct device *bus, const struct device *device) {
    int err = device_init(bus);
    return err ? err : device_init(device);
}

bool zeroshift_mode_supported(size_t slot, uint8_t mode) {
    return slot == 0 && mode <= PAT;
}

int zeroshift_start_mode(size_t slot, uint8_t mode) {
    int err;
    switch (mode) {
    case TB: return start_on_bus(DEV(tb_bus), DEV(tb));
    case JOY:
        err = start_encoder(DEV(joy_encoder));
        return err ? err : device_init(DEV(anin0));
    case ENC: return start_encoder(DEV(module_encoder));
    case KEY: return device_init(DEV(module_key));
    case RZT: return start_on_bus(DEV(i2c0), DEV(mlx90393_input));
    case TPD:
#if IS_ENABLED(CONFIG_SHIELD_MKB_R_ZS)
        return start_on_bus(DEV(i2c0), DEV(glidepoint_r));
#else
        return start_on_bus(DEV(i2c0), DEV(glidepoint_l));
#endif
    case LPPS:
        err = start_on_bus(DEV(spi2), DEV(adc_ads1220));
        return err ? err : device_init(DEV(lpps_axis));
    case PAT: {
        int ret = start_on_bus(DEV(dial_bus), DEV(module_dial));
        return ret ? ret : device_init(DEV(module_key));
    }
    default: return -EINVAL;
    }
}

static int optical_scroll_event(const struct device *dev, struct input_event *event,
                               uint32_t param1, uint32_t param2,
                               struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL || event->code != INPUT_REL_X) return ZMK_INPUT_PROC_STOP;
    int32_t steps = optical_dial_steps(event->value, *(const int32_t *)dev->config, dev->data);
    if (!steps) return ZMK_INPUT_PROC_STOP;
    event->code = INPUT_REL_WHEEL;
    event->value = -steps;
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api optical_scroll_api = {.handle_event = optical_scroll_event};

static int32_t module_remainder;
static const int32_t module_counts = 1892;
DEVICE_DT_DEFINE(DT_NODELABEL(module_scroll), NULL, NULL, &module_remainder, &module_counts,
                 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &optical_scroll_api);
