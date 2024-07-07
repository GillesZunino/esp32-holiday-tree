idf_component_register(SRCS
        "gpio/gpio_init.c"
        "button/button_init.c"

        "bt/bt_init.c"
        "bt/bt_utilities.c"
        "bt/bt_work_dispatcher.c"
        "bt/bt_gap.c"
        "bt/bt_avrc.c"
        "bt/bt_avrc_controller.c"
        "bt/bt_avrc_target.c"
        "bt/bt_avrc_volume.c"
        "bt/bt_a2d.c"

        "bt/i2s_output.c"

        "leds/led_init.c"
        "leds/led_internals.c"
        "leds/led_animator.c"
        "leds/progressive_reveal_effect.c"
        
        "main.c"

        INCLUDE_DIRS ".")
