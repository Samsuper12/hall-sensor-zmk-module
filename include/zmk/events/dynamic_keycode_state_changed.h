/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

struct zmk_dynamic_keycode_state_changed {
    int16_t position;
    int32_t value;
};

ZMK_EVENT_DECLARE(zmk_dynamic_keycode_state_changed);
