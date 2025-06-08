idf_component_register(
    SRCS "main.c" "sk6812_functions.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_system freertos log
) 