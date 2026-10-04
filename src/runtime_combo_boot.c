/* Refresh runtime combos after persisted settings are restored.
 * The upstream first-key lazy cache can be populated before settings_load().
 * Keep this shim local until the upstream lifecycle handles that ordering.
 */
#include <cormoran/zmk/custom_settings.h>
#include <cormoran/runtime_combo/runtime_combo.h>
#include <zmk/event_manager.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mkb_combo_boot, CONFIG_ZMK_LOG_LEVEL);

static int mkb_combo_settings_ready(const zmk_event_t *event) {
    if (as_zmk_custom_settings_initialized(event) == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    zmk_runtime_combo_invalidate_cache();
    LOG_INF("Runtime combo cache refreshed after settings load");
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(mkb_combo_boot, mkb_combo_settings_ready);
ZMK_SUBSCRIPTION(mkb_combo_boot, zmk_custom_settings_initialized);
