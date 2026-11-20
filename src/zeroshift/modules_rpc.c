// SPDX-License-Identifier: MIT
#include <pb_decode.h>
#include <zmk/studio/custom.h>
#include "modules.pb.h"
#include "modules_profile.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static struct zmk_rpc_custom_subsystem_meta meta = {.security = ZMK_STUDIO_RPC_HANDLER_UNSECURED};
ZMK_RPC_CUSTOM_SUBSYSTEM(mekabu__modules, &meta, handle);
ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(mekabu__modules, mekabu_Response);

static bool handle(const zmk_custom_CallRequest *raw, pb_callback_t *encode) {
    mekabu_Response *resp = ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(mekabu__modules, encode);
    *resp = (mekabu_Response)mekabu_Response_init_zero;
    mekabu_Request req = mekabu_Request_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(raw->payload.bytes, raw->payload.size);
    uint8_t modes[1];
    bool decoded = pb_decode(&stream, mekabu_Request_fields, &req);
    bool write = req.which_operation == mekabu_Request_set_tag;
    if (!decoded || (!write && req.which_operation != mekabu_Request_get_tag) ||
        (write && req.operation.set > UINT8_MAX)) {
        resp->error = -EINVAL;
        return true;
    }
    if (write) {
        modes[0] = req.operation.set;
    }
    struct zeroshift_state state;
    resp->error = req.right ? mkb_remote_profile(write ? modes : NULL, &state)
                            : zeroshift_profile(write ? modes : NULL, &state);
    if (!resp->error) {
        resp->supported = BIT(9) - 1;
        resp->saved = state.saved[0];
        resp->active = state.active[0];
    }
    return true;
}
