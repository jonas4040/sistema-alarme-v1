idf_component_register(SRCS "mqtt-tls.c"
                    INCLUDE_DIRS "include"
                    EMBED_TXTFILES ca.pem
                    REQUIRES esp_partition nvs_flash esp_event esp_netif mqtt esp-tls app_update)
