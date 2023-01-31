idf_component_register(SRCS "wifi-station.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi
                    nvs_flash
                    wifi-station)
