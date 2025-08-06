zephyr_include_directories(include)

target_sources(app PRIVATE src/adc_key_listener.c)
target_sources(app PRIVATE src/kscan/kscan_gpio_hall.c)
target_sources(app PRIVATE src/events/dynamic_keycode_state_changed.c)
target_sources(app PRIVATE src/behaviors/behavior_dynamic.c)