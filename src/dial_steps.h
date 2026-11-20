// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

static inline int32_t optical_dial_steps(int32_t counts, int32_t per_turn, int32_t *remainder) {
    int64_t numerator = *remainder + (int64_t)counts * 24;
    *remainder = numerator % per_turn;
    return numerator / per_turn;
}
