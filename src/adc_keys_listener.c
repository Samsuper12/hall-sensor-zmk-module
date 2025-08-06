/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_split

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

#include <zephyr/logging/log.h>

#include <zmk/events/dynamic_keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void adc_keys_input_handler(struct input_event *evt) {
    raise_zmk_dynamic_keycode_state_changed((struct zmk_dynamic_keycode_state_changed){
        .position = evt->code,
        .value = evt->value,
    });
}

INPUT_CALLBACK_DEFINE(NULL, adc_keys_input_handler);
