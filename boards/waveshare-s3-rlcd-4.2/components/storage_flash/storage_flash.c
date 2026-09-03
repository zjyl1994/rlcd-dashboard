#include "storage_flash.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "wear_levelling.h"

#define STORAGE_BASE_PATH "/storage"
#define STORAGE_PARTITION_LABEL "storage"
#define LAST_TEXT_FILE_PATH STORAGE_BASE_PATH "/last_text.txt"
#define LAST_TIME_FILE_PATH STORAGE_BASE_PATH "/last_update.txt"
#define LAST_TEXT_TEMP_PATH STORAGE_BASE_PATH "/last_text.tmp"
#define LAST_TIME_TEMP_PATH STORAGE_BASE_PATH "/last_update.tmp"
#define LAST_TEXT_MAX_BYTES (4096U)

static const char *TAG = "storage_flash";
static SemaphoreHandle_t s_storage_mutex;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_initialized;

bool storage_flash_init(void)
{
    if (s_initialized) {
        return true;
    }

    s_storage_mutex = xSemaphoreCreateMutex();
    if (s_storage_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create storage mutex");
        return false;
    }

    const esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 4096,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        STORAGE_BASE_PATH, STORAGE_PARTITION_LABEL, &config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS storage: %s", esp_err_to_name(err));
        vSemaphoreDelete(s_storage_mutex);
        s_storage_mutex = NULL;
        s_wl_handle = WL_INVALID_HANDLE;
        return false;
    }

    ESP_LOGI(TAG, "Last-message storage ready: %s (FATFS + wear leveling)",
             STORAGE_BASE_PATH);

    s_initialized = true;
    return true;
}

static bool storage_ready_for_app(void)
{
    return s_initialized;
}

static bool write_file_atomically(const char *temp_path, const char *path,
                                  const void *data, size_t data_size)
{
    FILE *file = fopen(temp_path, "w");
    if (file == NULL) {
        return false;
    }

    bool success = fwrite(data, 1, data_size, file) == data_size && fflush(file) == 0;
    if (fclose(file) != 0) {
        success = false;
    }
    if (!success || rename(temp_path, path) != 0) {
        remove(temp_path);
        return false;
    }
    return true;
}

bool storage_flash_save_last_message(const char *text, const struct tm *update_time)
{
    if (!storage_ready_for_app() || text == NULL || strlen(text) > LAST_TEXT_MAX_BYTES) {
        return false;
    }
    if (xSemaphoreTake(s_storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Storage mutex timeout");
        return false;
    }

    bool success = write_file_atomically(LAST_TEXT_TEMP_PATH, LAST_TEXT_FILE_PATH,
                                         text, strlen(text));
    if (success && update_time != NULL) {
        char timestamp[32];
        size_t timestamp_len = strftime(timestamp, sizeof(timestamp),
                                        "%Y-%m-%d %H:%M:%S\n", update_time);
        success = timestamp_len > 0 &&
                  write_file_atomically(LAST_TIME_TEMP_PATH, LAST_TIME_FILE_PATH,
                                        timestamp, timestamp_len);
    } else if (success) {
        remove(LAST_TIME_FILE_PATH);
    }

    xSemaphoreGive(s_storage_mutex);
    return success;
}

bool storage_flash_load_last_message(char *text, size_t text_size,
                                     struct tm *update_time)
{
    if (!s_initialized || text == NULL || text_size < 2) {
        return false;
    }

    if (xSemaphoreTake(s_storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }

    bool success = false;
    FILE *text_file = fopen(LAST_TEXT_FILE_PATH, "r");
    if (text_file != NULL) {
        size_t count = fread(text, 1, text_size - 1, text_file);
        text[count] = '\0';
        struct stat file_stat = {};
        success = ferror(text_file) == 0 &&
                  (stat(LAST_TEXT_FILE_PATH, &file_stat) != 0 ||
                   file_stat.st_size <= (off_t)(text_size - 1));
        fclose(text_file);
    }

    if (success && update_time != NULL) {
        memset(update_time, 0, sizeof(*update_time));
        update_time->tm_isdst = -1;
        FILE *time_file = fopen(LAST_TIME_FILE_PATH, "r");
        char timestamp[32] = {};
        if (time_file != NULL && fgets(timestamp, sizeof(timestamp), time_file) != NULL) {
            int year, month, day, hour, minute, second;
            if (sscanf(timestamp, "%d-%d-%d %d:%d:%d",
                       &year, &month, &day, &hour, &minute, &second) == 6) {
                update_time->tm_year = year - 1900;
                update_time->tm_mon = month - 1;
                update_time->tm_mday = day;
                update_time->tm_hour = hour;
                update_time->tm_min = minute;
                update_time->tm_sec = second;
                (void)mktime(update_time);
            }
        }
        if (time_file != NULL) {
            fclose(time_file);
        }
    }

    xSemaphoreGive(s_storage_mutex);
    return success;
}
