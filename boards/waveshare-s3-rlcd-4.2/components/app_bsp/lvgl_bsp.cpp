#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include "lvgl_bsp.h"

static SemaphoreHandle_t lvgl_mux = NULL;
#define I1_PALETTE_BYTES  (LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * sizeof(lv_color32_t))

static const char *TAG = "LvglPort";

static uint8_t *Alloc_lvgl_buf(size_t size)
{
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (buffer == NULL) {
        ESP_LOGW(TAG, "Internal LVGL buffer alloc failed, falling back to PSRAM (%u bytes)", (unsigned)size);
        buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    return buffer;
}

static void Increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool Lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void Lvgl_unlock(void)
{
    assert(lvgl_mux);
    xSemaphoreGive(lvgl_mux);
}

static void Lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        if (Lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            Lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void Lvgl_PortInit(int width, int height, DispFlushCb flush_cb)
{
    lvgl_mux = xSemaphoreCreateMutex();
    lv_init();

    lv_display_t *disp = lv_display_create(width, height);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
    lv_display_set_flush_cb(disp, flush_cb);

    /* I1 partial buffer = pixel data + palette header */
    size_t px_bytes = (size_t)lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1) * LVGL_PARTIAL_BUF_LINES;
    size_t buffer_size = px_bytes + I1_PALETTE_BYTES;
    uint8_t *buffer_1 = Alloc_lvgl_buf(buffer_size);
    uint8_t *buffer_2 = Alloc_lvgl_buf(buffer_size);
    assert(buffer_1);
    assert(buffer_2);
    memset(buffer_1, 0xFF, buffer_size);
    memset(buffer_2, 0xFF, buffer_size);

    lv_display_set_buffers(disp, buffer_1, buffer_2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_LOGI(TAG, "LVGL partial buffers: %u lines, %u bytes each", LVGL_PARTIAL_BUF_LINES, (unsigned)buffer_size);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &Increase_lvgl_tick;
    lvgl_tick_timer_args.name = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(Lvgl_port_task, "LVGL", 8 * 1024, NULL, 5, NULL, 0);
}
