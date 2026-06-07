#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"

DisplayPort RlcdPort(12, 11, 5, 40, 41, LCD_WIDTH, LCD_HEIGHT);

#define I1_PALETTE_BYTES (LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * sizeof(lv_color32_t))

static const char *MAIN_TAG = "main";

static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    static int flush_count = 0;
    const uint8_t *pixels = color_map + I1_PALETTE_BYTES;
    uint32_t stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), LV_COLOR_FORMAT_I1);
    bool is_last = lv_display_flush_is_last(drv);

    if (flush_count == 0) {
        ESP_LOGI(MAIN_TAG, "First flush: area(%d,%d)-(%d,%d) w=%d h=%d stride=%lu palette[0]=%02x%02x%02x%02x",
            area->x1, area->y1, area->x2, area->y2,
            lv_area_get_width(area), lv_area_get_height(area),
            (unsigned long)stride,
            color_map[0], color_map[1], color_map[2], color_map[3]);
    }
    flush_count++;

    if (area->x1 == 0 && area->y1 == 0 && area->x2 == (LCD_WIDTH - 1) && area->y2 == (LCD_HEIGHT - 1)) {
        RlcdPort.RLCD_BlitLVGLI1Full(pixels, stride);
    } else {
        RlcdPort.RLCD_BlitLVGLI1Area(area->x1, area->y1, area->x2, area->y2, pixels, stride);
    }

    if (is_last) {
        RlcdPort.RLCD_Display();
    }
    lv_disp_flush_ready(drv);
}

extern "C" void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "=== Dashboard starting ===");
    RlcdPort.RLCD_Init();
    ESP_LOGI(MAIN_TAG, "RLCD init done");
    Lvgl_PortInit(400, 300, Lvgl_FlushCallback);
    ESP_LOGI(MAIN_TAG, "LVGL init done");
    UserApp_AppInit();
    ESP_LOGI(MAIN_TAG, "App init done");
    if (Lvgl_lock(-1)) {
        ESP_LOGI(MAIN_TAG, "Creating UI...");
        UserApp_UiInit();
        Lvgl_unlock();
        ESP_LOGI(MAIN_TAG, "UI created");
    }
    UserApp_TaskInit();
    ESP_LOGI(MAIN_TAG, "=== Dashboard running ===");
}
