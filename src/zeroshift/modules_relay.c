// SPDX-License-Identifier: MIT
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/central.h>
#endif
#include "modules_profile.h"
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct profile_query {
    uint32_t id;
    uint8_t source, write, modes[ZEROSHIFT_SLOT_COUNT];
} __packed;
struct profile_reply {
    uint32_t id;
    int32_t result;
    uint8_t source;
    struct zeroshift_state state;
} __packed;
ZMK_EVENT_DECLARE(profile_query);
ZMK_EVENT_DECLARE(profile_reply);
ZMK_EVENT_IMPL(profile_query);
ZMK_EVENT_IMPL(profile_reply);
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(profile_query, Mzq, source)
ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL(profile_reply, Mza, source)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_RELAY_EVENT_HANDLE(profile_reply, Mza, source)
K_MSGQ_DEFINE(replies, sizeof(struct profile_reply), 4, 4);

/* Studio dispatches one request at a time on its dedicated RPC thread. */
int mkb_remote_profile(const uint8_t *requested, struct zeroshift_state *state) {
    static uint32_t sequence;
    struct profile_query query = {.id = ++sequence, .source = ZMK_RELAY_EVENT_SOURCE_SELF,
                                  .write = requested != NULL};
    if (requested) {
        memcpy(query.modes, requested, sizeof(query.modes));
    }
    k_msgq_purge(&replies);
    raise_profile_query(query);
    k_timepoint_t deadline = sys_timepoint_calc(K_SECONDS(2));
    struct profile_reply reply;
    while (!k_msgq_get(&replies, &reply, sys_timepoint_timeout(deadline))) {
        if (reply.id == query.id) {
            *state = reply.state;
            return reply.result;
        }
    }
    return -ETIMEDOUT;
}

static int receive_reply(const zmk_event_t *eh) {
    k_msgq_put(&replies, as_profile_reply(eh), K_NO_WAIT);
    return ZMK_EV_EVENT_HANDLED;
}
ZMK_LISTENER(mkb_profile_reply, receive_reply);
ZMK_SUBSCRIPTION(mkb_profile_reply, profile_reply);
#else
ZMK_RELAY_EVENT_HANDLE(profile_query, Mzq, source)
static int receive_query(const zmk_event_t *eh) {
    const struct profile_query *query = as_profile_query(eh);
    struct profile_reply reply = {.id = query->id, .source = ZMK_RELAY_EVENT_SOURCE_SELF};
    reply.result = zeroshift_profile(query->write ? query->modes : NULL, &reply.state);
    raise_profile_reply(reply);
    return ZMK_EV_EVENT_HANDLED;
}
ZMK_LISTENER(mkb_profile_query, receive_query);
ZMK_SUBSCRIPTION(mkb_profile_query, profile_query);
#endif
