// SPDX-License-Identifier: MIT
#include <zeroshift/studio.h>
#include "modules_profile.h"

const char zeroshift_studio_descriptor[] =
#include "descriptor.inc"
;

int zeroshift_studio_profile(bool right, const uint8_t *requested, struct zeroshift_state *state) {
    return !right ? zeroshift_profile(requested, state) : mkb_remote_profile(requested, state);
}
