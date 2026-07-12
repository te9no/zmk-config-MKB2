#include <zmk/endpoints.h>

/* Compatibility for display modules that still use the pre-v0.4 API name. */
struct zmk_endpoint_instance zmk_endpoints_selected(void) {
    return zmk_endpoint_get_selected();
}
