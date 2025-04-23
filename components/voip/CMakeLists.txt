idf_component_register(SRCS "voip.c" "ping_init.c" "md5_digest.c" "rtp.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi wpa_supplicant
                    PRIV_REQUIRES mbedtls)
