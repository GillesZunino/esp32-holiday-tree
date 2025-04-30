set(srcs
        "gpio/gpio_init.c"
        "button/button_init.c"

        "bt/bt_init.c"
        "bt/bt_utilities.c"
        "bt/bt_bd_addr_utils.c"
        "bt/bt_work_dispatcher.c"
        "bt/bt_gap.c"
        "bt/bt_avrc.c"
        "bt/bt_avrc_controller.c"
        "bt/bt_avrc_target.c"
        "bt/bt_avrc_volume.c"
        "bt/bt_a2d.c"

        "bt/i2s_output.c"

        "bt/bt_device_manager.c"
        "bt/bt_device_preferences.c"

        "configuration/nvs_configuration.c"

        "leds/led_init.c"
        "leds/led_internals.c"
        "leds/led_animator.c"
        "leds/progressive_reveal_effect.c"

        "main.c"
)

idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "."
        PRIV_REQUIRES esp_driver_gpio esp_driver_i2s esp_ringbuf nvs_flash bt
)
