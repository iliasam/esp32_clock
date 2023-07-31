idf_component_register(SRCS "nvs_functions.c" "oled_display.c" "display_drawing.c" "light_sensor.c" "menu_selector.c" "mode_controlling.c" "keys_controlling.c" "weather_json_parse.c" "weather_update.c" "mavlink_handling.c"  "co2_sensor.c" "time_update.c" 
                "PCF2129_rtc_driver.c" 
                "main.c" "i2c_driver.c" "co2_sensor.c" 
                "PCF2129_rtc_driver.c" "time_update.c" "max7219.c" 
                "clock_display.c"
                "display_functions.c"
                    INCLUDE_DIRS "." "./mavlink" "./mavlink/mav_home" "./keys")

component_compile_options("-Wno-missing-braces")
