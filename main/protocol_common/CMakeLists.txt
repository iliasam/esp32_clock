set(srcs "stdin_out.c"
         "addr_from_stdin.c"
         "connect.c"
         "wifi_connect.c")

if(CONFIG_EXAMPLE_PROVIDE_WIFI_CONSOLE_CMD)
    list(APPEND srcs "console_cmd.c")
endif()


idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_netif driver esp_wifi vfs)

if(CONFIG_EXAMPLE_PROVIDE_WIFI_CONSOLE_CMD)
    idf_component_optional_requires(PRIVATE console)
endif()
