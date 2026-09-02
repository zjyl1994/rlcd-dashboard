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
    if (lvgl_mux == NULL) {
        return false;
    }
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void Lvgl_unlock(void)
{
    if (lvgl_mux != NULL) {
        xSemaphoreGive(lvgl_mux);
    }
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

bool Lvgl_PortInit(int width, int height, DispFlushCb flush_cb)
{
    lvgl_mux = xSemaphoreCreateMutex();
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "LVGL mutex allocation failed");
        return false;
    }
    lv_init();

    lv_display_t *disp = lv_display_create(width, height);
    if (disp == NULL) {
        ESP_LOGE(TAG, "LVGL display allocation failed");
        return false;
    }
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
    lv_display_set_flush_cb(disp, flush_cb);

    /* I1 partial buffer = pixel data + palette header */
    size_t px_bytes = (size_t)lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1) * LVGL_PARTIAL_BUF_LINES;
    size_t buffer_size = px_bytes + I1_PALETTE_BYTES;
    uint8_t *buffer_1 = Alloc_lvgl_buf(buffer_size);
    uint8_t *buffer_2 = Alloc_lvgl_buf(buffer_size);
    if (buffer_1 == NULL || buffer_2 == NULL) {
        ESP_LOGE(TAG, "LVGL buffer allocation failed (%u bytes)", (unsigned)buffer_size);
        free(buffer_1);
        free(buffer_2);
        return false;
    }
    memset(buffer_1, 0xFF, buffer_size);
    memset(buffer_2, 0xFF, buffer_size);

    lv_display_set_buffers(disp, buffer_1, buffer_2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_LOGI(TAG, "LVGL partial buffers: %u lines, %u bytes each", LVGL_PARTIAL_BUF_LINES, (unsigned)buffer_size);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &Increase_lvgl_tick;
    lvgl_tick_timer_args.name = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_err_t timer_err = esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    if (timer_err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL tick timer creation failed: %s", esp_err_to_name(timer_err));
        return false;
    }
    timer_err = esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);
    if (timer_err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL tick timer start failed: %s", esp_err_to_name(timer_err));
        esp_timer_delete(lvgl_tick_timer);
        return false;
    }

    if (xTaskCreatePinnedToCore(Lvgl_port_task, "LVGL", 8 * 1024, NULL, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "LVGL task creation failed");
        return false;
    }
    return true;
}
