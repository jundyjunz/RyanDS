idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "." "header_files" 
                    REQUIRES esp_adc driver esp_driver_i2s esp_http_client esp_http_server nvs_flash esp_wifi esp_netif esp_lcd driver )
