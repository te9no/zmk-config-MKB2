"""Host regression tests; these do not replace a cold-start hardware test."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class StudioPersistenceTests(unittest.TestCase):
    def test_runtime_macro_is_exposed(self):
        overlay = (ROOT / 'snippets/mkb-dya-studio-v2/mkb-dya-studio-v2.overlay').read_text()
        self.assertIn('#include <behaviors/runtime_macro.dtsi>', overlay)

    def test_boot_listener_is_built_and_subscribed(self):
        source = (ROOT / 'src/runtime_combo_boot.c').read_text()
        self.assertIn('ZMK_SUBSCRIPTION(mkb_combo_boot, zmk_custom_settings_initialized)', source)
        self.assertIn('cmake: .', (ROOT / 'zephyr/module.yml').read_text())
        self.assertIn('src/runtime_combo_boot.c', (ROOT / 'CMakeLists.txt').read_text())

    @unittest.skipUnless(shutil.which('cc'), 'Host C compiler required')
    def test_real_callback_refreshes_stale_cache(self):
        # Compile the actual callback against minimal event/storage doubles.
        # Model an early-key cache, then completed loading, without UI re-save.
        source = (ROOT / 'src/runtime_combo_boot.c').read_text()
        source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
        stub = r'''
#include <stddef.h>
#include <assert.h>
typedef int zmk_event_t;
#define ZMK_EV_EVENT_BUBBLE 0
#define CONFIG_ZMK_LOG_LEVEL 0
#define LOG_MODULE_REGISTER(...)
#define LOG_INF(...)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
static int stored_combo, cached_combo, refresh_count;
static const void *as_zmk_custom_settings_initialized(const zmk_event_t *ev) {
    return *ev == 1 ? ev : NULL;
}
static void zmk_runtime_combo_invalidate_cache(void) {
    cached_combo = stored_combo;
    refresh_count++;
}
'''
        harness = r'''
int main(void) {
    const zmk_event_t unrelated = 0, ready = 1;
    stored_combo = 0;
    zmk_runtime_combo_invalidate_cache(); /* first key before restore */
    stored_combo = 42; /* settings_load restored a saved binding */
    assert(cached_combo == 0);
    assert(mkb_combo_settings_ready(&unrelated) == ZMK_EV_EVENT_BUBBLE);
    assert(cached_combo == 0 && refresh_count == 1);
    assert(mkb_combo_settings_ready(&ready) == ZMK_EV_EVENT_BUBBLE);
    assert(cached_combo == 42 && refresh_count == 2);
    assert(stored_combo == 42); /* no persistence writes */
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            test = Path(directory) / 'boot.c'
            binary = Path(directory) / 'boot'
            test.write_text(stub + source + harness)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Werror', str(test), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
