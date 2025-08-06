/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_dynamic

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>

#include <zmk/event_manager.h>
#include <zmk/events/dynamic_keycode_state_changed.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Change it if you have more than 10 fingers or playing with friends.
#define MAX_ACTIVE_KEYS 10

// (Almost). 1.6 in all cases will be converted to 2, so let it be quickly.
#define GOLDEN_RATIO 2

enum dynamic_key_state {
    ZMK_DYNAMIC_KEY_STATE_FREE = 0,
    ZMK_DYNAMIC_KEY_STATE_OCCUPIED,
};

struct key_trigger_config {
    struct zmk_behavior_binding binding;
    uint8_t trigger_value;
    uint8_t last_value;
    bool state : 1;
};

struct behavior_dynamic_config {
    uint32_t count;
    bool rapid_trigger : 1;
    bool repeat_keycode : 1;
    uint32_t repeat_delay;
    struct key_trigger_config triggers[];
};

struct active_dynamic_key {
    uint32_t position;
    enum dynamic_key_state state;
    struct behavior_dynamic_config *cfg;
};

static struct active_dynamic_key active_dynamic_keys[MAX_ACTIVE_KEYS] = {};
static bool at_least_one_active = false;

static int add_new_dynamic_key(int position, struct behavior_dynamic_config *cfg) {
    for (size_t i = 0; i < MAX_ACTIVE_KEYS; ++i) {
        if (active_dynamic_keys[i].state == ZMK_DYNAMIC_KEY_STATE_FREE) {
            struct active_dynamic_key *active_key = &active_dynamic_keys[i];
            active_key->position = position;
            active_key->cfg = cfg;
            active_key->state = ZMK_DYNAMIC_KEY_STATE_OCCUPIED;
            at_least_one_active = true;
            return 0;
        }
    }
    return -ENOMEM;
}

static struct active_dynamic_key *find_dynamic_key(int32_t position) {
    for (size_t i = 0; i < MAX_ACTIVE_KEYS; ++i) {
        if (active_dynamic_keys[i].state == ZMK_DYNAMIC_KEY_STATE_OCCUPIED &&
            active_dynamic_keys[i].position == position)
            return &active_dynamic_keys[i];
    }

    return NULL;
}

static void clear_dynamic_key(struct active_dynamic_key *key) {
    at_least_one_active = false;
    key->state = ZMK_DYNAMIC_KEY_STATE_FREE;
}

static void turn_off_all_keys(struct key_trigger_config *cfgs, int count) {
    for (size_t i = 0; i < count; ++count) {
        cfgs[i].state = false;
    }
}

static int zmk_dynamic_keycode_state_changed_listener(const zmk_event_t *eh) {
    // Skip of no keys active just now.
    if (!at_least_one_active)
        return 0;

    const struct zmk_dynamic_keycode_state_changed *ev = as_zmk_dynamic_keycode_state_changed(eh);
    if (ev) {
        // LOG_DBG("amigo! your value! %i", ev->value);
        struct active_dynamic_key *key = find_dynamic_key(ev->position);

        if (!key) {
            // LOG_ERR("Can't find dynamic key with position %i", ev->position);
            return 0;
        }
        for (size_t i = 0; i < key->cfg->count; ++i) {
            struct key_trigger_config *c = &key->cfg->triggers[i];

            bool going_down = (ev->value - c->last_value) > (GOLDEN_RATIO);

            if (!c->state && ev->value > c->trigger_value) {
                c->state = true;
                LOG_DBG("PPPP%i position activates trigger %i with value %i", ev->position, i,
                        ev->value);

                // check current mode. If "last wins" then use wait to full release and then press
                raise_zmk_keycode_state_changed_from_encoded(c->binding.param1, c->state, 0);
            } else if (key->cfg->rapid_trigger && !going_down) {
                LOG_DBG("PPPP rappid");
                // next time this loop eventually will trigger keypress
                turn_off_all_keys(&key->cfg, key->cfg->count);
            }

            if (c->state && (ev->value + GOLDEN_RATIO) < c->trigger_value) {
                c->state = false;
                LOG_DBG("PPPP%i position deactivates trigger %i with value %i, down: ",
                        ev->position, i, ev->value);
                raise_zmk_keycode_state_changed_from_encoded(c->binding.param1, c->state, 0);
            }
            LOG_DBG("PPPP Going down: %i", going_down);

            c->last_value = ev->value;
        }
    }

    return 0;
}
// Maybe ADC_KEY_STATE_CHANGED?
ZMK_LISTENER(behavior_dynamic, zmk_dynamic_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_dynamic, zmk_dynamic_keycode_state_changed);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_dynamic_config *cfg = dev->config;

    //  LOG_DBG("param0: %i", cfg->binding.behavior_dev != NULL);
    // LOG_DBG("param1: %i", cfg->binding.param1);

    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
    if (add_new_dynamic_key(event.position, cfg) == -ENOMEM) {
        LOG_ERR("Unable to create new dynamic key. Insufficient space in active_dynamic_keys[].");
        return ZMK_BEHAVIOR_OPAQUE;
    }
    LOG_DBG("%d created new dynamic_key", event.position);
    // return raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
    // return raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
    struct active_dynamic_key *key = find_dynamic_key(event.position);

    if (!key) {
        LOG_ERR("%i ACTIVE DYNAMIC KEY CLEARED TOO EARLY", event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    clear_dynamic_key(key);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_dynamic_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
// ZMK studio need it
//  .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define _TRANSFORM_ENTRY(idx, node)                                                                \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(node, bindings, idx)),               \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param1), (0),       \
                              (DT_INST_PHA_BY_IDX(node, bindings, idx, param1))),                  \
    }

#define _EXTRACT_TRIGGERS(idx, node)                                                               \
    {                                                                                              \
        .binding = _TRANSFORM_ENTRY(idx, node),                                                    \
        .trigger_value = DT_INST_PROP_BY_IDX(node, trigger_values, idx),                           \
        .last_value = 0,                                                                           \
        .state = false,                                                                            \
    }

#define TRANSFORMED_TRIGGERS(node)                                                                 \
    {LISTIFY(DT_INST_PROP_LEN(node, trigger_values), _EXTRACT_TRIGGERS, (, ), node)}

#define DYN_INST(inst)                                                                             \
    static struct behavior_dynamic_config behavior_dynamic_config_##inst = {                       \
        .count = DT_INST_PROP_LEN(inst, trigger_values),                                           \
        .triggers = TRANSFORMED_TRIGGERS(inst),                                                    \
        .rapid_trigger = DT_INST_PROP(inst, rapid_trigger),                                        \
        .repeat_keycode = DT_INST_PROP(inst, repeat_keycode),                                      \
        .repeat_delay = DT_INST_PROP(inst, repeat_delay)};                                         \
    BEHAVIOR_DT_INST_DEFINE(inst, NULL, NULL, NULL, &behavior_dynamic_config_##inst, POST_KERNEL,  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_dynamic_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DYN_INST)