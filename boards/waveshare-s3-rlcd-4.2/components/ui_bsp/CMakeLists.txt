file(GLOB_RECURSE scrld ./*.c)
idf_component_register(
    SRCS ${scrld}
    REQUIRES lvgl__lvgl
    INCLUDE_DIRS "."
    EMBED_FILES "MiSans-Regular.ttf")
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
