#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <driver/gpio.h>
#include <esp_crt_bundle.h>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <mqtt_client.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <cJSON.h>
#include <esp_codec_dev.h>

#include "codec_board.h"
#include "codec_init.h"

#include "sdkconfig.h"

#include "adc_bsp.h"
#include "dashboard_ui.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"

#ifndef CONFIG_LONG_PRESS_MS
#define CONFIG_LONG_PRESS_MS WIFI_PROV_LONG_PRESS_MS
#endif

#ifndef CONFIG_NTP_SYNC_INTERVAL_MIN
#define CONFIG_NTP_SYNC_INTERVAL_MIN 720
#endif

static const char *TAG = "dashboard";

static constexpr int MAX_WIFI_CREDENTIALS = 8;
static constexpr int MAX_SCAN_RESULTS = 20;
static constexpr uint32_t DEVICE_CONFIG_VERSION = 1;
static constexpr uint32_t WIFI_STORE_VERSION = 1;
static constexpr int WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint16_t WIFI_LISTEN_INTERVAL_BEACONS = 10;
static constexpr int STATUS_MESSAGE_TIMEOUT_MS = 10000;
static constexpr int MQTT_MESSAGE_TIMEOUT_MS = 10000;
static constexpr int MQTT_MESSAGE_MAX_LEN = 512;
static constexpr int MQTT_MESSAGE_TITLE_MAX_LEN = 64;
static constexpr int AUDIO_BEEP_SAMPLE_RATE = 24000;
static constexpr int AUDIO_BEEP_CHANNELS = 2;
static constexpr int AUDIO_BEEP_BITS_PER_SAMPLE = 16;
static constexpr int AUDIO_BEEP_DURATION_MS = 140;
static constexpr int AUDIO_BEEP_FRAME_COUNT = (AUDIO_BEEP_SAMPLE_RATE * AUDIO_BEEP_DURATION_MS) / 1000;
static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
static constexpr EventBits_t WIFI_CONNECT_FAIL_BIT = BIT1;

typedef struct {
    uint8_t valid;
    char ssid[33];
    char password[65];
} saved_wifi_credential_t;

typedef struct {
    uint32_t version;
    saved_wifi_credential_t entries[MAX_WIFI_CREDENTIALS];
} saved_wifi_store_t;

typedef struct {
    uint8_t valid;
    uint8_t use_tls;
    uint16_t port;
    char host[128];
    char username[64];
    char password[64];
} mqtt_config_t;

typedef struct {
    uint32_t version;
    int8_t timezone_offset_hours;
    char device_name[32];
} device_config_t;

typedef struct {
    int type;
    int timeout_seconds;
    bool beep;
    char title[MQTT_MESSAGE_TITLE_MAX_LEN + 1];
    char content[MQTT_MESSAGE_MAX_LEN + 1];
} mqtt_overlay_message_t;

static I2cMasterBus *i2cbus = NULL;
static Shtc3Port *shtc3 = NULL;
static EventGroupHandle_t wifi_event_group = NULL;
static SemaphoreHandle_t state_mutex = NULL;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static httpd_handle_t http_server = NULL;
static TaskHandle_t dns_server_task_handle = NULL;
static int dns_server_socket = -1;
static esp_event_handler_instance_t wifi_any_id_handler;
static esp_event_handler_instance_t wifi_got_ip_handler;
static saved_wifi_store_t wifi_store = {};
static mqtt_config_t mqtt_config = {};
static device_config_t device_config = {};
static wifi_ap_record_t provision_scan_records[MAX_SCAN_RESULTS] = {};
static uint16_t provision_scan_count = 0;
static bool wifi_started = false;
static bool wifi_connected = false;
static bool mqtt_connected = false;
static bool mqtt_restart_requested = false;
static bool prov_active = false;
static bool provisioning_action_in_progress = false;
static bool provisioning_action_enter = false;
static bool provisioning_action_reconnect_saved = false;
static bool connect_in_progress = false;
static bool rtc_ready = false;
static bool boot_button_pressed = false;
static bool boot_button_long_handled = false;
static bool key_button_pressed = false;
static bool message_overlay_active = false;
static bool message_overlay_requires_key = false;
static TickType_t boot_button_press_ticks = 0;
static char connected_ssid[33] = {0};
static int connected_rssi = -127;
static char provision_ap_ssid[33] = {0};
static char provision_ap_ip[16] = {0};
static int64_t status_message_expire_at_us = 0;
static int64_t message_overlay_expire_at_us = 0;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static char mqtt_broker_uri[192] = {0};
static char mqtt_message_topic[128] = {0};
static char mqtt_client_id[64] = {0};
static char mqtt_username[64] = {0};
static char mqtt_password[64] = {0};
static char mqtt_message_buffer[MQTT_MESSAGE_MAX_LEN + 1] = {0};
static int mqtt_message_expected_len = 0;
static int mqtt_message_received_len = 0;
static bool mqtt_message_collecting = false;
static bool mqtt_message_topic_match = false;
static bool audio_beep_ready = false;
static bool audio_beep_in_progress = false;
static esp_codec_dev_handle_t audio_playback = NULL;
static int16_t audio_beep_pcm[AUDIO_BEEP_FRAME_COUNT * AUDIO_BEEP_CHANNELS] = {0};

static void wifi_enter_provisioning(void);
static void wifi_exit_provisioning(bool reconnect_saved);
static bool wifi_connect_saved_networks(bool enter_provision_on_fail);
static bool schedule_provisioning_action(bool enter, bool reconnect_saved);
static void ui_show_message_overlay(const char *title, const char *message, int timeout_seconds);
static void ui_hide_message_overlay(void);
static bool mqtt_parse_overlay_message(const char *payload, mqtt_overlay_message_t *out_message);
static void mqtt_handle_received_message(const char *payload);
static void audio_prepare_beep_pcm(void);
static bool audio_init(void);
static void audio_notification_beep_task(void *arg);
static void audio_request_notification_beep(void);
static void ui_set_default_status_message_locked(void);
static bool sync_system_time_from_rtc(void);
static void wifi_apply_power_save(bool connected, bool provisioning);

static uint32_t provision_ip_addr(void)
{
    return inet_addr(provision_ap_ip[0] ? provision_ap_ip : "192.168.4.1");
}

static void state_lock(void)
{
    xSemaphoreTake(state_mutex, portMAX_DELAY);
}

static void state_unlock(void)
{
    xSemaphoreGive(state_mutex);
}

static void mqtt_config_reset(mqtt_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->port = 1883;
}

static void mqtt_config_snapshot(mqtt_config_t *config)
{
    state_lock();
    *config = mqtt_config;
    state_unlock();
}

static esp_err_t mqtt_config_commit(const mqtt_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("mqtt_store", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, "config", config, sizeof(*config));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static void mqtt_config_load(void)
{
    mqtt_config_t loaded;

    mqtt_config_reset(&loaded);

    nvs_handle_t handle;
    if (nvs_open("mqtt_store", NVS_READONLY, &handle) == ESP_OK) {
        size_t blob_size = sizeof(loaded);
        esp_err_t err = nvs_get_blob(handle, "config", &loaded, &blob_size);
        nvs_close(handle);
        if (err != ESP_OK || blob_size != sizeof(loaded)) {
            mqtt_config_reset(&loaded);
        }
    }

    loaded.host[sizeof(loaded.host) - 1] = '\0';
    loaded.username[sizeof(loaded.username) - 1] = '\0';
    loaded.password[sizeof(loaded.password) - 1] = '\0';
    if (loaded.port == 0) {
        loaded.port = loaded.use_tls ? 8883 : 1883;
    }
    if (loaded.host[0] == '\0') {
        loaded.valid = 0;
    }

    state_lock();
    mqtt_config = loaded;
    state_unlock();
}

static bool mqtt_config_update(const mqtt_config_t *config)
{
    if (mqtt_config_commit(config) != ESP_OK) {
        return false;
    }

    state_lock();
    mqtt_config = *config;
    state_unlock();
    return true;
}

static void device_config_reset(device_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->version = DEVICE_CONFIG_VERSION;
    config->timezone_offset_hours = 8;
}

static void build_default_device_name(char *out, size_t out_size)
{
    unsigned int suffix = (unsigned int)(esp_random() % 1000000U);
    snprintf(out, out_size, "RLCD-%06u", suffix);
}

static bool is_generated_device_name(const char *name)
{
    if (strncmp(name, "RLCD-", 5) != 0 || strlen(name) != 11) {
        return false;
    }

    for (size_t index = 5; index < 11; index++) {
        if (!isdigit((unsigned char)name[index])) {
            return false;
        }
    }

    return true;
}

static void apply_timezone_offset(int offset_hours)
{
    char tz_string[16];

    if (offset_hours == 0) {
        strlcpy(tz_string, "UTC0", sizeof(tz_string));
    } else {
        snprintf(tz_string, sizeof(tz_string), "UTC%+d", -offset_hours);
    }

    setenv("TZ", tz_string, 1);
    tzset();
}

static bool sync_system_time_from_rtc(void)
{
    if (!rtc_ready) {
        return false;
    }

    Rtc_SelfCheckAndRecover();
    if (!Rtc_IsTimeReliable()) {
        ESP_LOGW(TAG, "RTC time is not reliable yet");
        return false;
    }

    rtcTimeStruct_t rtc_time = {};
    struct tm time_info = {};
    struct timeval now = {};

    Rtc_GetTime(&rtc_time);
    if (rtc_time.year < 2020 || rtc_time.month < 1 || rtc_time.month > 12 || rtc_time.day < 1 || rtc_time.day > 31) {
        ESP_LOGW(TAG, "RTC returned invalid time: %04d-%02d-%02d %02d:%02d:%02d",
            rtc_time.year, rtc_time.month, rtc_time.day, rtc_time.hour, rtc_time.minute, rtc_time.second);
        return false;
    }

    time_info.tm_year = rtc_time.year - 1900;
    time_info.tm_mon = rtc_time.month - 1;
    time_info.tm_mday = rtc_time.day;
    time_info.tm_hour = rtc_time.hour;
    time_info.tm_min = rtc_time.minute;
    time_info.tm_sec = rtc_time.second;
    time_info.tm_isdst = -1;

    time_t epoch = mktime(&time_info);
    if (epoch == (time_t)-1) {
        ESP_LOGW(TAG, "Failed to convert RTC time to epoch");
        return false;
    }

    now.tv_sec = epoch;
    now.tv_usec = 0;
    if (settimeofday(&now, NULL) != 0) {
        ESP_LOGW(TAG, "Failed to sync system time from RTC");
        return false;
    }

    return true;
}

static void adjust_rtc_timezone_offset(int old_offset, int new_offset)
{
    if (!rtc_ready || old_offset == new_offset) {
        apply_timezone_offset(new_offset);
        return;
    }

    rtcTimeStruct_t rtc_time;
    struct tm time_info = {};
    struct tm adjusted = {};

    Rtc_GetTime(&rtc_time);
    time_info.tm_year = rtc_time.year - 1900;
    time_info.tm_mon = rtc_time.month - 1;
    time_info.tm_mday = rtc_time.day;
    time_info.tm_hour = rtc_time.hour;
    time_info.tm_min = rtc_time.minute;
    time_info.tm_sec = rtc_time.second;

    setenv("TZ", "UTC0", 1);
    tzset();

    time_t epoch = mktime(&time_info);
    if (epoch != (time_t)-1) {
        epoch += (time_t)(new_offset - old_offset) * 3600;
        gmtime_r(&epoch, &adjusted);
        Rtc_SetTime(
            adjusted.tm_year + 1900,
            adjusted.tm_mon + 1,
            adjusted.tm_mday,
            adjusted.tm_hour,
            adjusted.tm_min,
            adjusted.tm_sec);
    }

    apply_timezone_offset(new_offset);
    sync_system_time_from_rtc();
}

static void device_config_snapshot(device_config_t *config)
{
    state_lock();
    *config = device_config;
    state_unlock();
}

static esp_err_t device_config_commit(const device_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("device_store", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, "config", config, sizeof(*config));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static void device_config_load(void)
{
    device_config_t loaded;
    bool should_commit = false;

    device_config_reset(&loaded);

    nvs_handle_t handle;
    if (nvs_open("device_store", NVS_READONLY, &handle) == ESP_OK) {
        size_t blob_size = sizeof(loaded);
        esp_err_t err = nvs_get_blob(handle, "config", &loaded, &blob_size);
        nvs_close(handle);
        if (err != ESP_OK || blob_size != sizeof(loaded) || loaded.version != DEVICE_CONFIG_VERSION) {
            device_config_reset(&loaded);
            should_commit = true;
        }
    } else {
        should_commit = true;
    }

    if (loaded.timezone_offset_hours < -12 || loaded.timezone_offset_hours > 14) {
        loaded.timezone_offset_hours = 8;
        should_commit = true;
    }

    loaded.device_name[sizeof(loaded.device_name) - 1] = '\0';
    if (!is_generated_device_name(loaded.device_name)) {
        build_default_device_name(loaded.device_name, sizeof(loaded.device_name));
        should_commit = true;
    }

    state_lock();
    device_config = loaded;
    state_unlock();
    apply_timezone_offset(loaded.timezone_offset_hours);

    if (should_commit) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(device_config_commit(&loaded));
    }
}

static bool device_config_update(const device_config_t *config)
{
    if (device_config_commit(config) != ESP_OK) {
        return false;
    }

    state_lock();
    device_config = *config;
    state_unlock();
    return true;
}

static void build_mqtt_message_topic(char *out, size_t out_size)
{
    device_config_t config;

    device_config_snapshot(&config);
    snprintf(out, out_size, "/rlcd/%s/message", config.device_name);
}

static void mqtt_request_restart(void)
{
    state_lock();
    mqtt_restart_requested = true;
    state_unlock();
}

static void ui_set_default_status_message(void)
{
    if (Lvgl_lock(-1)) {
        ui_set_default_status_message_locked();
        Lvgl_unlock();
    }
}

static void ui_set_default_status_message_locked(void)
{
    device_config_t config;

    device_config_snapshot(&config);
    dashboard_ui_set_status_message(config.device_name);
}

static void ui_set_status_message(const char *message)
{
    if (message != NULL && message[0] != '\0') {
        if (Lvgl_lock(-1)) {
            dashboard_ui_set_status_message(message);
            Lvgl_unlock();
        }
    } else {
        ui_set_default_status_message();
    }

    state_lock();
    if (message != NULL && message[0] != '\0') {
        status_message_expire_at_us = esp_timer_get_time() + ((int64_t)STATUS_MESSAGE_TIMEOUT_MS * 1000);
    } else {
        status_message_expire_at_us = 0;
    }
    state_unlock();
}

static void ui_set_statusf(const char *fmt, ...)
{
    char buffer[96];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    ui_set_status_message(buffer);
}

static void ui_set_provisioning_screen(bool active)
{
    if (Lvgl_lock(100)) {
        dashboard_ui_set_provisioning(active, active ? provision_ap_ssid : NULL, active ? provision_ap_ip : NULL);
        Lvgl_unlock();
    }
}

static void ui_update_wifi_icon(bool connected, const char *ssid, int rssi)
{
    if (Lvgl_lock(100)) {
        dashboard_ui_update_wifi_status(connected, ssid, rssi);
        Lvgl_unlock();
    }
}

static void ui_update_mqtt_icon(bool connected)
{
    if (Lvgl_lock(100)) {
        dashboard_ui_update_mqtt_status(connected);
        Lvgl_unlock();
    }
}

static void ui_show_message_overlay(const char *title, const char *message, int timeout_seconds)
{
    if (Lvgl_lock(-1)) {
        dashboard_ui_show_message_overlay(title, message);
        Lvgl_unlock();
    }

    state_lock();
    message_overlay_active = true;
    message_overlay_requires_key = (timeout_seconds == 0);
    if (timeout_seconds > 0) {
        message_overlay_expire_at_us = esp_timer_get_time() + ((int64_t)timeout_seconds * 1000000LL);
    } else {
        message_overlay_expire_at_us = 0;
    }
    state_unlock();
}

static void ui_hide_message_overlay(void)
{
    if (Lvgl_lock(-1)) {
        dashboard_ui_hide_message_overlay();
        Lvgl_unlock();
    }

    state_lock();
    message_overlay_active = false;
    message_overlay_requires_key = false;
    message_overlay_expire_at_us = 0;
    state_unlock();
}

static void audio_prepare_beep_pcm(void)
{
    static bool prepared = false;
    const float pi = 3.14159265358979323846f;

    if (prepared) {
        return;
    }

    for (int frame = 0; frame < AUDIO_BEEP_FRAME_COUNT; frame++) {
        float t = (float)frame / (float)AUDIO_BEEP_SAMPLE_RATE;
        float progress = (float)frame / (float)AUDIO_BEEP_FRAME_COUNT;
        float env = (1.0f - progress);
        float freq = (frame < (AUDIO_BEEP_FRAME_COUNT / 2)) ? 1760.0f : 2349.0f;
        float sample = sinf(2.0f * pi * freq * t) * env * 0.32f;
        int16_t pcm = (int16_t)(sample * 32767.0f);

        audio_beep_pcm[frame * 2] = pcm;
        audio_beep_pcm[(frame * 2) + 1] = pcm;
    }

    prepared = true;
}

static bool audio_init(void)
{
    if (audio_beep_ready && audio_playback != NULL) {
        return true;
    }

    set_codec_board_type("S3_RLCD_4_2");

    codec_init_cfg_t codec_cfg = {};
    codec_cfg.in_mode = CODEC_I2S_MODE_NONE;
    codec_cfg.out_mode = CODEC_I2S_MODE_TDM;
    codec_cfg.in_use_tdm = false;
    codec_cfg.reuse_dev = false;
    if (init_codec(&codec_cfg) != 0) {
        ESP_LOGW(TAG, "Failed to init codec board");
        return false;
    }

    audio_playback = get_playback_handle();
    if (audio_playback == NULL) {
        ESP_LOGW(TAG, "No playback codec handle available");
        return false;
    }

    esp_codec_dev_sample_info_t sample_info = {};
    sample_info.sample_rate = AUDIO_BEEP_SAMPLE_RATE;
    sample_info.channel = AUDIO_BEEP_CHANNELS;
    sample_info.bits_per_sample = AUDIO_BEEP_BITS_PER_SAMPLE;
    if (esp_codec_dev_open(audio_playback, &sample_info) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to open playback codec");
        return false;
    }

    esp_codec_dev_set_out_vol(audio_playback, 100);
    audio_prepare_beep_pcm();
    audio_beep_ready = true;
    return true;
}

static void audio_notification_beep_task(void *arg)
{
    LV_UNUSED(arg);

    if (audio_init()) {
        esp_codec_dev_write(audio_playback, audio_beep_pcm, sizeof(audio_beep_pcm));
    }

    state_lock();
    audio_beep_ready = (audio_playback != NULL);
    audio_beep_in_progress = false;
    state_unlock();
    vTaskDelete(NULL);
}

static void audio_request_notification_beep(void)
{
    state_lock();
    if (audio_beep_in_progress) {
        state_unlock();
        return;
    }
    audio_beep_in_progress = true;
    state_unlock();

    if (xTaskCreatePinnedToCore(audio_notification_beep_task, "mqtt_beep", 4 * 1024, NULL, 2, NULL, 1) != pdPASS) {
        state_lock();
        audio_beep_in_progress = false;
        state_unlock();
        ESP_LOGW(TAG, "Failed to start beep task");
    }
}

static bool mqtt_parse_overlay_message(const char *payload, mqtt_overlay_message_t *out_message)
{
    cJSON *root;
    cJSON *type_item;
    cJSON *title_item;
    cJSON *content_item;
    cJSON *timeout_item;
    cJSON *beep_item;

    if (payload == NULL || out_message == NULL) {
        return false;
    }

    memset(out_message, 0, sizeof(*out_message));
    out_message->type = -1;
    out_message->timeout_seconds = MQTT_MESSAGE_TIMEOUT_MS / 1000;
    out_message->beep = false;

    root = cJSON_Parse(payload);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    content_item = cJSON_GetObjectItemCaseSensitive(root, "content");
    title_item = cJSON_GetObjectItemCaseSensitive(root, "title");
    timeout_item = cJSON_GetObjectItemCaseSensitive(root, "timeout");
    beep_item = cJSON_GetObjectItemCaseSensitive(root, "beep");

    if (!cJSON_IsNumber(type_item) || !cJSON_IsString(content_item) || content_item->valuestring == NULL) {
        cJSON_Delete(root);
        return false;
    }

    out_message->type = type_item->valueint;
    strlcpy(out_message->content, content_item->valuestring, sizeof(out_message->content));

    if (title_item != NULL) {
        if (!cJSON_IsString(title_item) || title_item->valuestring == NULL) {
            cJSON_Delete(root);
            return false;
        }
        strlcpy(out_message->title, title_item->valuestring, sizeof(out_message->title));
    }

    if (timeout_item != NULL) {
        if (!cJSON_IsNumber(timeout_item) || timeout_item->valueint < 0) {
            cJSON_Delete(root);
            return false;
        }
        out_message->timeout_seconds = timeout_item->valueint;
    }

    if (beep_item != NULL) {
        if (cJSON_IsBool(beep_item)) {
            out_message->beep = cJSON_IsTrue(beep_item);
        } else if (cJSON_IsNumber(beep_item)) {
            out_message->beep = (beep_item->valueint != 0);
        } else {
            cJSON_Delete(root);
            return false;
        }
    }

    cJSON_Delete(root);
    return true;
}

static void mqtt_handle_received_message(const char *payload)
{
    mqtt_overlay_message_t message = {};

    if (!mqtt_parse_overlay_message(payload, &message)) {
        ESP_LOGW(TAG, "MQTT payload is not valid JSON: %s", payload != NULL ? payload : "<null>");
        ui_set_status_message("MQTT JSON parse failed");
        return;
    }

    if (message.type != 1) {
        ui_set_statusf("Unsupported MQTT type: %d", message.type);
        return;
    }

    if (message.beep) {
        audio_request_notification_beep();
    }
    ui_show_message_overlay(message.title, message.content, message.timeout_seconds);
}

static void html_escape(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;

    if (dst_size == 0) {
        return;
    }

    while (*src != '\0' && out + 1 < dst_size) {
        const char *replacement = NULL;

        switch (*src) {
            case '&': replacement = "&amp;"; break;
            case '<': replacement = "&lt;"; break;
            case '>': replacement = "&gt;"; break;
            case '"': replacement = "&quot;"; break;
            case '\'': replacement = "&#39;"; break;
            default: break;
        }

        if (replacement != NULL) {
            size_t len = strlen(replacement);
            if (out + len >= dst_size) {
                break;
            }
            memcpy(dst + out, replacement, len);
            out += len;
        } else {
            dst[out++] = *src;
        }
        src++;
    }

    dst[out] = '\0';
}

static void url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;

    if (dst_size == 0) {
        return;
    }

    while (*src != '\0' && out + 1 < dst_size) {
        unsigned char ch = (unsigned char)*src;
        bool safe = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
                    || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~';

        if (safe) {
            dst[out++] = (char)ch;
        } else {
            if (out + 3 >= dst_size) {
                break;
            }
            dst[out++] = '%';
            dst[out++] = hex[(ch >> 4) & 0x0F];
            dst[out++] = hex[ch & 0x0F];
        }
        src++;
    }

    dst[out] = '\0';
}

static void format_ip(const esp_netif_ip_info_t *ip_info, char *out, size_t out_size)
{
    uint32_t ip = ip_info->ip.addr;
    snprintf(
        out,
        out_size,
        "%u.%u.%u.%u",
        (unsigned int)(ip & 0xFF),
        (unsigned int)((ip >> 8) & 0xFF),
        (unsigned int)((ip >> 16) & 0xFF),
        (unsigned int)((ip >> 24) & 0xFF));
}

static void wifi_store_reset(saved_wifi_store_t *store)
{
    memset(store, 0, sizeof(*store));
    store->version = WIFI_STORE_VERSION;
}

static void wifi_store_normalize(saved_wifi_store_t *store)
{
    if (store->version != WIFI_STORE_VERSION) {
        wifi_store_reset(store);
        return;
    }

    for (int index = 0; index < MAX_WIFI_CREDENTIALS; index++) {
        store->entries[index].ssid[sizeof(store->entries[index].ssid) - 1] = '\0';
        store->entries[index].password[sizeof(store->entries[index].password) - 1] = '\0';
        if (store->entries[index].ssid[0] == '\0') {
            store->entries[index].valid = 0;
        }
    }
}

static int wifi_store_count(const saved_wifi_store_t *store)
{
    int count = 0;

    for (int index = 0; index < MAX_WIFI_CREDENTIALS; index++) {
        if (store->entries[index].valid && store->entries[index].ssid[0] != '\0') {
            count++;
        }
    }

    return count;
}

static void wifi_store_snapshot(saved_wifi_store_t *store)
{
    state_lock();
    *store = wifi_store;
    state_unlock();
}

static int wifi_store_count_snapshot(void)
{
    saved_wifi_store_t store;

    wifi_store_snapshot(&store);
    return wifi_store_count(&store);
}

static esp_err_t wifi_store_commit(const saved_wifi_store_t *store)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_store", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, "cred_blob", store, sizeof(*store));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static bool wifi_store_import_legacy(saved_wifi_store_t *store)
{
    nvs_handle_t handle;
    wifi_config_t legacy = {};
    size_t blob_size = sizeof(legacy);

    if (nvs_open("wifi_cfg", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_get_blob(handle, "sta_cfg", &legacy, &blob_size);
    nvs_close(handle);
    if (err != ESP_OK || legacy.sta.ssid[0] == '\0') {
        return false;
    }

    store->entries[0].valid = 1;
    strlcpy(store->entries[0].ssid, (const char *)legacy.sta.ssid, sizeof(store->entries[0].ssid));
    strlcpy(store->entries[0].password, (const char *)legacy.sta.password, sizeof(store->entries[0].password));
    return true;
}

static void wifi_store_seed_from_kconfig(saved_wifi_store_t *store)
{
    if (wifi_store_count(store) > 0) {
        return;
    }

    if (CONFIG_WIFI_SSID[0] == '\0') {
        return;
    }

    store->entries[0].valid = 1;
    strlcpy(store->entries[0].ssid, CONFIG_WIFI_SSID, sizeof(store->entries[0].ssid));
    strlcpy(store->entries[0].password, CONFIG_WIFI_PASSWORD, sizeof(store->entries[0].password));
}

static void wifi_store_load(void)
{
    saved_wifi_store_t loaded_store;
    bool should_commit = false;

    wifi_store_reset(&loaded_store);

    nvs_handle_t handle;
    if (nvs_open("wifi_store", NVS_READONLY, &handle) == ESP_OK) {
        size_t blob_size = sizeof(loaded_store);
        esp_err_t err = nvs_get_blob(handle, "cred_blob", &loaded_store, &blob_size);
        nvs_close(handle);
        if (err != ESP_OK || blob_size != sizeof(loaded_store)) {
            wifi_store_reset(&loaded_store);
        }
    }

    wifi_store_normalize(&loaded_store);
    if (wifi_store_count(&loaded_store) == 0 && wifi_store_import_legacy(&loaded_store)) {
        should_commit = true;
    }
    wifi_store_seed_from_kconfig(&loaded_store);
    if (wifi_store_count(&loaded_store) > 0) {
        should_commit = true;
    }

    state_lock();
    wifi_store = loaded_store;
    state_unlock();

    if (should_commit) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_store_commit(&loaded_store));
    }
}

static bool wifi_store_add_or_update(const char *ssid, const char *password, bool *updated_existing)
{
    saved_wifi_store_t local_store;
    int target_index = -1;

    wifi_store_snapshot(&local_store);
    wifi_store_normalize(&local_store);

    for (int index = 0; index < MAX_WIFI_CREDENTIALS; index++) {
        if (local_store.entries[index].valid && strcmp(local_store.entries[index].ssid, ssid) == 0) {
            target_index = index;
            if (updated_existing != NULL) {
                *updated_existing = true;
            }
            break;
        }
        if (target_index < 0 && !local_store.entries[index].valid) {
            target_index = index;
        }
    }

    if (target_index < 0) {
        target_index = MAX_WIFI_CREDENTIALS - 1;
        if (updated_existing != NULL) {
            *updated_existing = false;
        }
    }

    local_store.entries[target_index].valid = 1;
    strlcpy(local_store.entries[target_index].ssid, ssid, sizeof(local_store.entries[target_index].ssid));
    strlcpy(local_store.entries[target_index].password, password, sizeof(local_store.entries[target_index].password));

    if (wifi_store_commit(&local_store) != ESP_OK) {
        return false;
    }

    state_lock();
    wifi_store = local_store;
    state_unlock();
    return true;
}

static bool wifi_store_remove(const char *ssid)
{
    saved_wifi_store_t local_store;
    bool removed = false;

    wifi_store_snapshot(&local_store);
    wifi_store_normalize(&local_store);

    for (int index = 0; index < MAX_WIFI_CREDENTIALS; index++) {
        if (local_store.entries[index].valid && strcmp(local_store.entries[index].ssid, ssid) == 0) {
            removed = true;
            for (int move = index; move < MAX_WIFI_CREDENTIALS - 1; move++) {
                local_store.entries[move] = local_store.entries[move + 1];
            }
            memset(&local_store.entries[MAX_WIFI_CREDENTIALS - 1], 0, sizeof(local_store.entries[0]));
            break;
        }
    }

    if (!removed) {
        return false;
    }

    if (wifi_store_commit(&local_store) != ESP_OK) {
        return false;
    }

    state_lock();
    wifi_store = local_store;
    state_unlock();
    return true;
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
}

static void button_init(void)
{
    gpio_config_t config = {};
    config.intr_type = GPIO_INTR_DISABLE;
    config.mode = GPIO_MODE_INPUT;
    config.pin_bit_mask = (1ULL << BOOT_BUTTON_PIN) | (1ULL << KEY_BUTTON_PIN);
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    LV_UNUSED(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        bool local_connecting;
        bool local_provisioning;

        state_lock();
        wifi_connected = false;
        mqtt_connected = false;
        connected_ssid[0] = '\0';
        connected_rssi = -127;
        local_connecting = connect_in_progress;
        local_provisioning = prov_active;
        state_unlock();

        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        if (local_connecting) {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECT_FAIL_BIT);
        }

        if (!local_provisioning) {
            ui_update_wifi_icon(false, NULL, 0);
            ui_update_mqtt_icon(false);
            ui_set_status_message("Wi-Fi disconnected");
        }

        wifi_apply_power_save(false, local_provisioning);
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_config_t active_cfg = {};
        char ip_text[16];

        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_get_config(WIFI_IF_STA, &active_cfg));
        format_ip(&event->ip_info, ip_text, sizeof(ip_text));

        state_lock();
        wifi_connected = true;
        strlcpy(connected_ssid, (const char *)active_cfg.sta.ssid, sizeof(connected_ssid));
        state_unlock();

        xEventGroupClearBits(wifi_event_group, WIFI_CONNECT_FAIL_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ui_set_statusf("Connected: %s (%s)", connected_ssid[0] ? connected_ssid : "Wi-Fi", ip_text);
        wifi_apply_power_save(true, false);
    }
}

static void wifi_apply_power_save(bool connected, bool provisioning)
{
    static wifi_ps_type_t current_ps = WIFI_PS_NONE;
    wifi_ps_type_t target_ps = (!provisioning && connected) ? WIFI_PS_MAX_MODEM : WIFI_PS_NONE;

    if (!wifi_started || current_ps == target_ps) {
        return;
    }

    if (esp_wifi_set_ps(target_ps) == ESP_OK) {
        current_ps = target_ps;
        ESP_LOGI(TAG, "Wi-Fi power save -> %s", target_ps == WIFI_PS_MAX_MODEM ? "MAX_MODEM" : "NONE");
    } else {
        ESP_LOGW(TAG, "Failed to set Wi-Fi power save mode");
    }
}

static void wifi_stack_init(void)
{
    if (wifi_started) {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_any_id_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &wifi_got_ip_handler));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_apply_power_save(false, false);

    wifi_started = true;
}

static uint16_t wifi_scan_access_points(wifi_ap_record_t *records, uint16_t max_records)
{
    uint16_t access_point_count = 0;
    wifi_scan_config_t scan_cfg = {};

    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
        return 0;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_get_ap_num(&access_point_count));
    if (access_point_count > max_records) {
        access_point_count = max_records;
    }

    if (access_point_count > 0) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_get_ap_records(&access_point_count, records));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_clear_ap_list());
    return access_point_count;
}

static void provision_refresh_scan_results(void)
{
    wifi_ap_record_t scan_records[MAX_SCAN_RESULTS] = {};
    uint16_t scan_count = wifi_scan_access_points(scan_records, MAX_SCAN_RESULTS);

    state_lock();
    provision_scan_count = scan_count;
    if (scan_count > 0) {
        memcpy(provision_scan_records, scan_records, scan_count * sizeof(wifi_ap_record_t));
    }
    state_unlock();
}

static uint16_t provision_snapshot_scan_results(wifi_ap_record_t *records, uint16_t max_records)
{
    uint16_t copy_count;

    state_lock();
    copy_count = (provision_scan_count < max_records) ? provision_scan_count : max_records;
    if (copy_count > 0) {
        memcpy(records, provision_scan_records, copy_count * sizeof(wifi_ap_record_t));
    }
    state_unlock();

    return copy_count;
}

static int wifi_build_candidate_order(
    const saved_wifi_store_t *store,
    const wifi_ap_record_t *records,
    int record_count,
    int *order,
    int order_capacity)
{
    bool used[MAX_WIFI_CREDENTIALS] = {false};
    int order_count = 0;

    while (order_count < order_capacity) {
        int best_index = -1;
        int best_rssi = -127;

        for (int entry_index = 0; entry_index < MAX_WIFI_CREDENTIALS; entry_index++) {
            if (!store->entries[entry_index].valid || used[entry_index]) {
                continue;
            }

            for (int record_index = 0; record_index < record_count; record_index++) {
                if (strcmp(store->entries[entry_index].ssid, (const char *)records[record_index].ssid) == 0
                    && records[record_index].rssi > best_rssi) {
                    best_rssi = records[record_index].rssi;
                    best_index = entry_index;
                }
            }
        }

        if (best_index < 0) {
            break;
        }

        used[best_index] = true;
        order[order_count++] = best_index;
    }

    for (int entry_index = 0; entry_index < MAX_WIFI_CREDENTIALS && order_count < order_capacity; entry_index++) {
        if (store->entries[entry_index].valid && !used[entry_index]) {
            order[order_count++] = entry_index;
        }
    }

    return order_count;
}

static bool wifi_try_connect_credential(const saved_wifi_credential_t *credential)
{
    wifi_config_t station_cfg = {};

    strlcpy((char *)station_cfg.sta.ssid, credential->ssid, sizeof(station_cfg.sta.ssid));
    strlcpy((char *)station_cfg.sta.password, credential->password, sizeof(station_cfg.sta.password));
    station_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    station_cfg.sta.listen_interval = WIFI_LISTEN_INTERVAL_BEACONS;
    station_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    station_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    station_cfg.sta.pmf_cfg.capable = true;
    station_cfg.sta.pmf_cfg.required = false;

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_CONNECT_FAIL_BIT);

    state_lock();
    connect_in_progress = true;
    state_unlock();

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &station_cfg));
    ui_set_statusf("Connecting: %s", credential->ssid);

    esp_err_t connect_err = esp_wifi_connect();
    if (connect_err != ESP_OK) {
        state_lock();
        connect_in_progress = false;
        state_unlock();
        ui_set_statusf("Connect start failed: %s", credential->ssid);
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_CONNECT_FAIL_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    state_lock();
    connect_in_progress = false;
    state_unlock();

    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    ui_set_statusf("Connect failed: %s", credential->ssid);
    return false;
}

static int captive_dns_build_response(const uint8_t *query, int query_len, uint8_t *response, size_t response_size)
{
    if (query_len < 17 || response_size < (size_t)(query_len + 16)) {
        return -1;
    }

    uint16_t question_count = ((uint16_t)query[4] << 8) | query[5];
    if (question_count == 0) {
        return -1;
    }

    int qname_end = 12;
    while (qname_end < query_len && query[qname_end] != 0) {
        uint8_t label_len = query[qname_end];
        qname_end += label_len + 1;
    }

    if (qname_end + 5 > query_len) {
        return -1;
    }

    uint16_t qtype = ((uint16_t)query[qname_end + 1] << 8) | query[qname_end + 2];
    int question_end = qname_end + 5;

    memcpy(response, query, question_end);
    response[2] = 0x81;
    response[3] = 0x80;
    response[6] = 0x00;
    response[7] = (qtype == 1) ? 0x01 : 0x00;
    response[8] = 0x00;
    response[9] = 0x00;
    response[10] = 0x00;
    response[11] = 0x00;

    if (qtype != 1) {
        return question_end;
    }

    int out = question_end;
    response[out++] = 0xC0;
    response[out++] = 0x0C;
    response[out++] = 0x00;
    response[out++] = 0x01;
    response[out++] = 0x00;
    response[out++] = 0x01;
    response[out++] = 0x00;
    response[out++] = 0x00;
    response[out++] = 0x00;
    response[out++] = 0x3C;
    response[out++] = 0x00;
    response[out++] = 0x04;

    uint32_t ip = provision_ip_addr();
    memcpy(response + out, &ip, sizeof(ip));
    out += sizeof(ip);

    return out;
}

static void captive_dns_task(void *arg)
{
    LV_UNUSED(arg);

    struct sockaddr_in bind_addr = {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(53);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        dns_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    dns_server_socket = sock;
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        close(sock);
        dns_server_socket = -1;
        dns_server_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        uint8_t query[256];
        uint8_t response[300];
        struct sockaddr_in source_addr = {};
        socklen_t source_len = sizeof(source_addr);
        int received = recvfrom(sock, query, sizeof(query), 0, (struct sockaddr *)&source_addr, &source_len);

        if (received < 0) {
            bool local_provisioning;
            state_lock();
            local_provisioning = prov_active;
            state_unlock();
            if (!local_provisioning) {
                break;
            }
            continue;
        }

        int response_len = captive_dns_build_response(query, received, response, sizeof(response));
        if (response_len > 0) {
            sendto(sock, response, response_len, 0, (struct sockaddr *)&source_addr, source_len);
        }
    }

    close(sock);
    if (dns_server_socket == sock) {
        dns_server_socket = -1;
    }
    dns_server_task_handle = NULL;
    vTaskDelete(NULL);
}

static void captive_dns_start(void)
{
    if (dns_server_task_handle != NULL) {
        return;
    }

    xTaskCreatePinnedToCore(captive_dns_task, "captive_dns", 4 * 1024, NULL, 3, &dns_server_task_handle, 1);
}

static void captive_dns_stop(void)
{
    if (dns_server_socket >= 0) {
        shutdown(dns_server_socket, SHUT_RDWR);
        close(dns_server_socket);
        dns_server_socket = -1;
    }
}

static esp_err_t send_chunkf(httpd_req_t *req, const char *fmt, ...)
{
    va_list args;
    va_list args_copy;
    char *buffer;
    int needed;
    esp_err_t err;

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return ESP_FAIL;
    }

    buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }

    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    va_end(args);

    err = httpd_resp_sendstr_chunk(req, buffer);
    free(buffer);
    return err;
}

static void url_decode_in_place(char *text)
{
    char *src = text;
    char *dst = text;

    while (*src != '\0') {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] != '\0' && src[2] != '\0') {
            int hi = (src[1] <= '9') ? (src[1] - '0') : ((src[1] & ~0x20) - 'A' + 10);
            int lo = (src[2] <= '9') ? (src[2] - '0') : ((src[2] & ~0x20) - 'A' + 10);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

static esp_err_t read_request_body(httpd_req_t *req, char *body, size_t body_size)
{
    int total_received = 0;

    if ((size_t)req->content_len >= body_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    while (total_received < req->content_len) {
        int received = httpd_req_recv(req, body + total_received, req->content_len - total_received);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            return ESP_FAIL;
        }
        total_received += received;
    }

    body[total_received] = '\0';
    return ESP_OK;
}

static esp_err_t send_provision_page(httpd_req_t *req, const char *notice, const char *selected_ssid)
{
    saved_wifi_store_t *store = (saved_wifi_store_t *)malloc(sizeof(saved_wifi_store_t));
    wifi_ap_record_t *records = (wifi_ap_record_t *)calloc(MAX_SCAN_RESULTS, sizeof(wifi_ap_record_t));
    mqtt_config_t mqtt_settings;
    device_config_t device_settings;
    uint16_t record_count;
    char escaped_ssid[256] = {0};
    char escaped_host[256] = {0};
    char escaped_username[192] = {0};
    char escaped_password[192] = {0};
    char escaped_device_name[128] = {0};
    char escaped_topic[192] = {0};
    bool sta_connected = false;
    char current_ssid[33] = {0};

    if (store == NULL || records == NULL) {
        free(store);
        free(records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    wifi_store_snapshot(store);
    mqtt_config_snapshot(&mqtt_settings);
    device_config_snapshot(&device_settings);
    record_count = provision_snapshot_scan_results(records, MAX_SCAN_RESULTS);
    html_escape((selected_ssid != NULL) ? selected_ssid : "", escaped_ssid, sizeof(escaped_ssid));
    html_escape(mqtt_settings.host, escaped_host, sizeof(escaped_host));
    html_escape(mqtt_settings.username, escaped_username, sizeof(escaped_username));
    html_escape(mqtt_settings.password, escaped_password, sizeof(escaped_password));
    html_escape(device_settings.device_name, escaped_device_name, sizeof(escaped_device_name));
    build_mqtt_message_topic(mqtt_message_topic, sizeof(mqtt_message_topic));
    html_escape(mqtt_message_topic, escaped_topic, sizeof(escaped_topic));

    state_lock();
    sta_connected = wifi_connected;
    strlcpy(current_ssid, connected_ssid, sizeof(current_ssid));
    state_unlock();

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    httpd_resp_sendstr_chunk(req, "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    httpd_resp_sendstr_chunk(req, "<title>RLCD Clock Provisioning</title><style>body{font-family:Arial,sans-serif;margin:20px;line-height:1.5}input,select{width:100%;padding:10px;margin:6px 0;box-sizing:border-box}button{padding:10px 14px;margin-top:8px}code{background:#f2f2f2;padding:2px 4px}ul{padding-left:20px}.sel{font-weight:700}.row{display:flex;gap:10px}.row>*{flex:1}.del{margin-left:10px;color:#a00;text-decoration:none}</style></head><body>");
    httpd_resp_sendstr_chunk(req, "<h2>RLCD Clock Provisioning</h2>");

    if (notice != NULL) {
        send_chunkf(req, "<p><strong>%s</strong></p>", notice);
    }

    send_chunkf(req, "<p>AP SSID: <code>%s</code></p>", provision_ap_ssid[0] ? provision_ap_ssid : "--");
    send_chunkf(req, "<p>Address: <code>http://%s</code></p>", provision_ap_ip[0] ? provision_ap_ip : "192.168.4.1");
    send_chunkf(req, "<p>Saved networks: <strong>%d</strong></p>", wifi_store_count(store));
    if (sta_connected && current_ssid[0] != '\0') {
        char escaped_current[128];
        html_escape(current_ssid, escaped_current, sizeof(escaped_current));
        send_chunkf(req, "<p>Connected STA: <strong>%s</strong></p>", escaped_current);
    }

    send_chunkf(req, "<form method='post' action='/save'><label>SSID</label><input name='ssid' maxlength='32' required value='%s' placeholder='Select from scan list or type manually'><label>Password</label><input name='password' type='password' maxlength='64' placeholder='Leave empty for open Wi-Fi'><button type='submit'>Save Wi-Fi</button></form>", escaped_ssid);

    httpd_resp_sendstr_chunk(req, "<h3>Scanned Wi-Fi</h3><p><a href='/refresh'>Refresh scan</a></p><ul>");
    if (record_count == 0) {
        httpd_resp_sendstr_chunk(req, "<li>No scan result yet</li>");
    } else {
        for (uint16_t index = 0; index < record_count; index++) {
            char encoded_ssid[128];
            char escaped_name[256];
            bool is_selected;

            url_encode((const char *)records[index].ssid, encoded_ssid, sizeof(encoded_ssid));
            html_escape((const char *)records[index].ssid, escaped_name, sizeof(escaped_name));
            is_selected = selected_ssid != NULL && strcmp((const char *)records[index].ssid, selected_ssid) == 0;
            send_chunkf(req, "<li%s><a href='/?ssid=%s'>%s</a> (%d dBm)%s</li>", is_selected ? " class='sel'" : "", encoded_ssid, escaped_name, records[index].rssi, is_selected ? " [selected]" : "");
        }
    }
    httpd_resp_sendstr_chunk(req, "</ul>");

    httpd_resp_sendstr_chunk(req, "<h3>Saved Wi-Fi</h3><ul>");
    if (wifi_store_count(store) == 0) {
        httpd_resp_sendstr_chunk(req, "<li>None</li>");
    } else {
        for (int index = 0; index < MAX_WIFI_CREDENTIALS; index++) {
            if (store->entries[index].valid) {
                char encoded_ssid[128];
                char escaped_name[256];
                bool is_connected = sta_connected && strcmp(store->entries[index].ssid, current_ssid) == 0;

                url_encode(store->entries[index].ssid, encoded_ssid, sizeof(encoded_ssid));
                html_escape(store->entries[index].ssid, escaped_name, sizeof(escaped_name));
                send_chunkf(req, "<li>%s%s <a class='del' href='/delete?ssid=%s'>Delete</a></li>", escaped_name, is_connected ? " [connected]" : "", encoded_ssid);
            }
        }
    }
    httpd_resp_sendstr_chunk(req, "</ul>");

    send_chunkf(req, "<h3>Device Settings</h3><p>Device Name: <code>%s</code></p><form method='post' action='/device'><label>Timezone</label><select name='timezone'>", escaped_device_name);
    for (int tz = -12; tz <= 14; tz++) {
        send_chunkf(req, "<option value='%d'%s>GMT%+d</option>", tz, tz == device_settings.timezone_offset_hours ? " selected" : "", tz);
    }
    httpd_resp_sendstr_chunk(req, "</select><button type='submit'>Save Device Settings</button></form>");
    send_chunkf(req, "<p>MQTT topic: <code>%s</code></p>", escaped_topic);

    send_chunkf(req, "<h3>MQTT Settings</h3><form method='post' action='/mqtt'><div class='row'><div><label>Scheme</label><select name='scheme'><option value='mqtt'%s>mqtt://</option><option value='mqtts'%s>mqtts://</option></select></div><div><label>Port</label><input name='port' type='number' min='1' max='65535' value='%u' required></div></div><label>Host</label><input name='host' maxlength='127' value='%s' placeholder='broker.example.com' required><label>Username</label><input name='username' maxlength='63' value='%s'><label>Password</label><input name='password' type='password' maxlength='63' value='%s'><button type='submit'>Save MQTT</button></form>", mqtt_settings.use_tls ? "" : " selected", mqtt_settings.use_tls ? " selected" : "", (unsigned int)mqtt_settings.port, escaped_host, escaped_username, escaped_password);
    if (mqtt_settings.valid && mqtt_settings.host[0] != '\0') {
        send_chunkf(req, "<p>Saved MQTT endpoint: <code>%s://%s:%u</code></p>", mqtt_settings.use_tls ? "mqtts" : "mqtt", escaped_host, (unsigned int)mqtt_settings.port);
    }

    httpd_resp_sendstr_chunk(req, "<p>This AP uses captive portal mode. If your phone does not open it automatically, browse to <code>192.168.4.1</code>.</p>");
    httpd_resp_sendstr_chunk(req, "<p>Hold the BOOT button to exit provisioning and return to the clock.</p>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    esp_err_t result = httpd_resp_sendstr_chunk(req, NULL);
    free(store);
    free(records);
    return result;
}

static esp_err_t provision_get_handler(httpd_req_t *req)
{
    char query[160] = {0};
    char selected_ssid[33] = {0};

    if (strcmp(req->uri, "/delete") == 0) {
        char delete_ssid[33] = {0};

        if (httpd_req_get_url_query_len(req) > 0
            && httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK
            && httpd_query_key_value(query, "ssid", delete_ssid, sizeof(delete_ssid)) == ESP_OK) {
            url_decode_in_place(delete_ssid);
            if (delete_ssid[0] != '\0' && wifi_store_remove(delete_ssid)) {
                return send_provision_page(req, "Removed saved Wi-Fi.", NULL);
            }
        }

        return send_provision_page(req, "Unable to remove saved Wi-Fi.", NULL);
    }

    if (strcmp(req->uri, "/refresh") == 0) {
        provision_refresh_scan_results();
        return send_provision_page(req, "Scan refreshed.", NULL);
    }

    if (httpd_req_get_url_query_len(req) > 0
        && httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK
        && httpd_query_key_value(query, "ssid", selected_ssid, sizeof(selected_ssid)) == ESP_OK) {
        url_decode_in_place(selected_ssid);
    }

    return send_provision_page(req, NULL, selected_ssid[0] ? selected_ssid : NULL);
}

static esp_err_t provision_save_post_handler(httpd_req_t *req)
{
    char body[256];
    char ssid[33] = {0};
    char password[65] = {0};
    bool updated_existing = false;

    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }

    if (httpd_query_key_value(body, "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
        return ESP_FAIL;
    }
    httpd_query_key_value(body, "password", password, sizeof(password));
    url_decode_in_place(ssid);
    url_decode_in_place(password);

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is empty");
        return ESP_FAIL;
    }

    if (!wifi_store_add_or_update(ssid, password, &updated_existing)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save Wi-Fi");
        return ESP_FAIL;
    }

    ui_set_statusf("Saved Wi-Fi: %s", ssid);
    provision_refresh_scan_results();
    return send_provision_page(req, updated_existing ? "Updated existing Wi-Fi credentials." : "Saved new Wi-Fi credentials.", ssid);
}

static esp_err_t device_save_post_handler(httpd_req_t *req)
{
    char body[256];
    char timezone_text[8] = {0};
    device_config_t new_config;
    device_config_t current_config;
    long timezone_value;

    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device settings body");
        return ESP_FAIL;
    }

    if (httpd_query_key_value(body, "timezone", timezone_text, sizeof(timezone_text)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Timezone is required");
        return ESP_FAIL;
    }

    url_decode_in_place(timezone_text);
    timezone_value = strtol(timezone_text, NULL, 10);
    if (timezone_value < -12 || timezone_value > 14) {
        return send_provision_page(req, "Timezone is invalid.", NULL);
    }

    device_config_snapshot(&current_config);
    new_config = current_config;
    new_config.timezone_offset_hours = (int8_t)timezone_value;

    if (!device_config_update(&new_config)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save device settings");
        return ESP_FAIL;
    }

    adjust_rtc_timezone_offset(current_config.timezone_offset_hours, new_config.timezone_offset_hours);
    ui_set_statusf("Saved timezone: GMT%+d", (int)new_config.timezone_offset_hours);
    return send_provision_page(req, "Saved device settings.", NULL);
}

static esp_err_t mqtt_save_post_handler(httpd_req_t *req)
{
    char body[512];
    char scheme[8] = {0};
    char host[128] = {0};
    char port_text[12] = {0};
    char username[64] = {0};
    char password[64] = {0};
    mqtt_config_t new_config;
    long parsed_port;

    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid MQTT request body");
        return ESP_FAIL;
    }

    if (httpd_query_key_value(body, "scheme", scheme, sizeof(scheme)) != ESP_OK
        || httpd_query_key_value(body, "host", host, sizeof(host)) != ESP_OK
        || httpd_query_key_value(body, "port", port_text, sizeof(port_text)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT host, scheme and port are required");
        return ESP_FAIL;
    }

    httpd_query_key_value(body, "username", username, sizeof(username));
    httpd_query_key_value(body, "password", password, sizeof(password));
    url_decode_in_place(scheme);
    url_decode_in_place(host);
    url_decode_in_place(port_text);
    url_decode_in_place(username);
    url_decode_in_place(password);

    parsed_port = strtol(port_text, NULL, 10);
    if (host[0] == '\0' || parsed_port <= 0 || parsed_port > 65535) {
        return send_provision_page(req, "MQTT host or port is invalid.", NULL);
    }

    mqtt_config_reset(&new_config);
    new_config.valid = 1;
    new_config.use_tls = (strcmp(scheme, "mqtts") == 0) ? 1 : 0;
    new_config.port = (uint16_t)parsed_port;
    strlcpy(new_config.host, host, sizeof(new_config.host));
    strlcpy(new_config.username, username, sizeof(new_config.username));
    strlcpy(new_config.password, password, sizeof(new_config.password));

    if (!mqtt_config_update(&new_config)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save MQTT settings");
        return ESP_FAIL;
    }

    mqtt_request_restart();
    ui_set_statusf("Saved MQTT: %s://%s:%u", new_config.use_tls ? "mqtts" : "mqtt", new_config.host, (unsigned int)new_config.port);
    return send_provision_page(req, "Saved MQTT settings.", NULL);
}

static void http_server_start(void)
{
    if (http_server != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 5;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    config.max_req_hdr_len = 2048;
    config.max_uri_len = 1024;

    if (httpd_start(&http_server, &config) != ESP_OK) {
        http_server = NULL;
        ui_set_status_message("Failed to start config page");
        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = provision_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = provision_save_post_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t mqtt_uri = {
        .uri = "/mqtt",
        .method = HTTP_POST,
        .handler = mqtt_save_post_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t device_uri = {
        .uri = "/device",
        .method = HTTP_POST,
        .handler = device_save_post_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(http_server, &root_uri));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(http_server, &save_uri));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(http_server, &mqtt_uri));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(http_server, &device_uri));
}

static void http_server_stop(void)
{
    if (http_server != NULL) {
        httpd_stop(http_server);
        http_server = NULL;
    }
}

static void build_provision_ap_name(void)
{
    device_config_t config;

    device_config_snapshot(&config);
    strlcpy(provision_ap_ssid, config.device_name, sizeof(provision_ap_ssid));
}

static void wifi_update_provision_ip(void)
{
    esp_netif_ip_info_t ip_info = {};

    if (ap_netif != NULL && esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        format_ip(&ip_info, provision_ap_ip, sizeof(provision_ap_ip));
    } else {
        strlcpy(provision_ap_ip, "192.168.4.1", sizeof(provision_ap_ip));
    }
}

static void wifi_enter_provisioning(void)
{
    if (!wifi_started) {
        return;
    }

    state_lock();
    if (prov_active) {
        state_unlock();
        return;
    }
    prov_active = true;
    state_unlock();

    build_provision_ap_name();
    wifi_update_provision_ip();

    wifi_config_t ap_cfg = {};
    strlcpy((char *)ap_cfg.ap.ssid, provision_ap_ssid, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen((char *)ap_cfg.ap.ssid);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    wifi_apply_power_save(false, true);
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_update_provision_ip();
    provision_refresh_scan_results();
    captive_dns_start();
    http_server_start();
    ui_set_provisioning_screen(true);
    ui_update_wifi_icon(false, NULL, 0);
    ui_set_statusf("Provisioning AP: %s", provision_ap_ssid);
}

static void wifi_exit_provisioning(bool reconnect_saved)
{
    bool was_active;

    state_lock();
    was_active = prov_active;
    prov_active = false;
    state_unlock();

    if (!was_active) {
        return;
    }

    captive_dns_stop();
    http_server_stop();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_apply_power_save(false, false);
    ui_set_provisioning_screen(false);

    if (reconnect_saved) {
        if (!wifi_connect_saved_networks(false)) {
            ui_set_status_message("Clock mode, Wi-Fi offline");
        }
    } else {
        ui_set_status_message("Exited provisioning");
    }
}

static bool wifi_connect_saved_networks(bool enter_provision_on_fail)
{
    saved_wifi_store_t store;
    wifi_ap_record_t scan_records[MAX_SCAN_RESULTS];
    int order[MAX_WIFI_CREDENTIALS];
    bool local_provisioning;

    if (!wifi_started) {
        return false;
    }

    state_lock();
    local_provisioning = prov_active;
    state_unlock();
    if (local_provisioning) {
        return false;
    }

    wifi_store_snapshot(&store);
    if (wifi_store_count(&store) == 0) {
        if (enter_provision_on_fail) {
            wifi_enter_provisioning();
        }
        return false;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    vTaskDelay(pdMS_TO_TICKS(100));

    uint16_t scan_count = wifi_scan_access_points(scan_records, MAX_SCAN_RESULTS);
    int order_count = wifi_build_candidate_order(&store, scan_records, scan_count, order, MAX_WIFI_CREDENTIALS);

    for (int index = 0; index < order_count; index++) {
        if (wifi_try_connect_credential(&store.entries[order[index]])) {
            return true;
        }
    }

    ui_set_status_message("Unable to connect saved Wi-Fi");
    if (enter_provision_on_fail) {
        wifi_enter_provisioning();
    }
    return false;
}

static void provisioning_action_task(void *arg)
{
    LV_UNUSED(arg);

    bool local_enter;
    bool local_reconnect_saved;

    state_lock();
    local_enter = provisioning_action_enter;
    local_reconnect_saved = provisioning_action_reconnect_saved;
    state_unlock();

    if (local_enter) {
        ui_set_status_message("Entering provisioning...");
        wifi_enter_provisioning();
    } else {
        ui_set_status_message("Leaving provisioning...");
        wifi_exit_provisioning(local_reconnect_saved);
    }

    state_lock();
    provisioning_action_in_progress = false;
    state_unlock();
    vTaskDelete(NULL);
}

static bool schedule_provisioning_action(bool enter, bool reconnect_saved)
{
    state_lock();
    if (provisioning_action_in_progress) {
        state_unlock();
        return false;
    }
    provisioning_action_in_progress = true;
    provisioning_action_enter = enter;
    provisioning_action_reconnect_saved = reconnect_saved;
    state_unlock();

    if (xTaskCreatePinnedToCore(provisioning_action_task, "prov_action", 6 * 1024, NULL, 3, NULL, 1) != pdPASS) {
        state_lock();
        provisioning_action_in_progress = false;
        state_unlock();
        ui_set_status_message("Provisioning switch failed");
        return false;
    }

    return true;
}

static void ntp_sync_once(void)
{
    ui_set_status_message("Syncing time from ntp.aliyun.com");

    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_init();

    for (int retry = 0; retry < 15; retry++) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            time_t now;
            struct tm time_info;

            time(&now);
            localtime_r(&now, &time_info);
            if (rtc_ready) {
                Rtc_SetTime(
                    time_info.tm_year + 1900,
                    time_info.tm_mon + 1,
                    time_info.tm_mday,
                    time_info.tm_hour,
                    time_info.tm_min,
                    time_info.tm_sec);
            }
            ui_set_status_message("NTP synced");
            esp_sntp_stop();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_sntp_stop();
    ui_set_status_message("NTP sync timeout");
}

static void mqtt_reset_message_accumulator(void)
{
    state_lock();
    mqtt_message_expected_len = 0;
    mqtt_message_received_len = 0;
    mqtt_message_collecting = false;
    mqtt_message_topic_match = false;
    mqtt_message_buffer[0] = '\0';
    state_unlock();
}

static void ui_set_mqtt_error_status(const esp_mqtt_error_codes_t *error)
{
    if (error == NULL) {
        ui_set_status_message("MQTT error");
        return;
    }

    switch (error->error_type) {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            if (error->esp_tls_last_esp_err != ESP_OK) {
                ui_set_statusf(
                    "MQTT transport: %s",
                    esp_err_to_name(error->esp_tls_last_esp_err));
            } else if (error->esp_transport_sock_errno != 0) {
                ui_set_statusf("MQTT socket errno: %d", error->esp_transport_sock_errno);
            } else if (error->esp_tls_stack_err != 0) {
                ui_set_statusf("MQTT TLS stack err: %d", error->esp_tls_stack_err);
            } else {
                ui_set_status_message("MQTT transport error");
            }
            break;

        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            ui_set_statusf("MQTT refused: %d", (int)error->connect_return_code);
            break;

        default:
            ui_set_status_message("MQTT error");
            break;
    }
}

static void log_mqtt_error(const esp_mqtt_error_codes_t *error)
{
    if (error == NULL) {
        ESP_LOGW(TAG, "MQTT error with no details");
        return;
    }

    switch (error->error_type) {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            ESP_LOGW(
                TAG,
                "MQTT transport error: esp_err=%s tls_stack=%d cert_flags=0x%x sock_errno=%d",
                esp_err_to_name(error->esp_tls_last_esp_err),
                error->esp_tls_stack_err,
                error->esp_tls_cert_verify_flags,
                error->esp_transport_sock_errno);
            break;

        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            ESP_LOGW(TAG, "MQTT connection refused: rc=%d", (int)error->connect_return_code);
            break;

        default:
            ESP_LOGW(TAG, "MQTT error: type=%d", (int)error->error_type);
            break;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    LV_UNUSED(handler_args);
    LV_UNUSED(base);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            build_mqtt_message_topic(mqtt_message_topic, sizeof(mqtt_message_topic));
            state_lock();
            mqtt_connected = true;
            state_unlock();
            esp_mqtt_client_subscribe_single(event->client, mqtt_message_topic, 1);
            ESP_LOGI(TAG, "MQTT connected: %s", mqtt_broker_uri);
            ui_update_mqtt_icon(true);
            ui_set_statusf("MQTT connected: %s", mqtt_message_topic);
            break;

        case MQTT_EVENT_DISCONNECTED:
            state_lock();
            mqtt_connected = false;
            state_unlock();
            ESP_LOGW(TAG, "MQTT disconnected");
            ui_update_mqtt_icon(false);
            ui_set_status_message("MQTT disconnected");
            mqtt_reset_message_accumulator();
            break;

        case MQTT_EVENT_ERROR:
            state_lock();
            mqtt_connected = false;
            state_unlock();
            log_mqtt_error(event->error_handle);
            ui_update_mqtt_icon(false);
            ui_set_mqtt_error_status(event->error_handle);
            mqtt_reset_message_accumulator();
            break;

        case MQTT_EVENT_DATA: {
            if (event->topic == NULL || event->data == NULL) {
                break;
            }

            if (event->current_data_offset == 0) {
                bool topic_match = (event->topic_len == (int)strlen(mqtt_message_topic))
                                  && (strncmp(event->topic, mqtt_message_topic, (size_t)event->topic_len) == 0)
                                  && (event->total_data_len <= MQTT_MESSAGE_MAX_LEN);

                state_lock();
                mqtt_message_expected_len = event->total_data_len;
                mqtt_message_received_len = 0;
                mqtt_message_collecting = topic_match;
                mqtt_message_topic_match = topic_match;
                mqtt_message_buffer[0] = '\0';
                state_unlock();

                if (!topic_match) {
                    break;
                }
            }

            state_lock();
            bool collecting = mqtt_message_collecting && mqtt_message_topic_match;
            int current_len = mqtt_message_received_len;
            int expected_len = mqtt_message_expected_len;
            state_unlock();

            if (!collecting) {
                break;
            }

            int copy_len = event->data_len;
            if (copy_len > (MQTT_MESSAGE_MAX_LEN - current_len)) {
                copy_len = MQTT_MESSAGE_MAX_LEN - current_len;
            }
            if (copy_len < 0) {
                copy_len = 0;
            }

            bool complete = false;
            char message_copy[MQTT_MESSAGE_MAX_LEN + 1] = {0};

            state_lock();
            if (copy_len > 0) {
                memcpy(mqtt_message_buffer + mqtt_message_received_len, event->data, (size_t)copy_len);
                mqtt_message_received_len += copy_len;
                mqtt_message_buffer[mqtt_message_received_len] = '\0';
            }

            if (mqtt_message_received_len >= expected_len) {
                strlcpy(message_copy, mqtt_message_buffer, sizeof(message_copy));
                complete = true;
                mqtt_message_expected_len = 0;
                mqtt_message_received_len = 0;
                mqtt_message_collecting = false;
                mqtt_message_topic_match = false;
            }
            state_unlock();

            if (complete) {
                mqtt_handle_received_message(message_copy);
            }
            break;
        }

        default:
            break;
    }
}

static bool mqtt_start_client(void)
{
    mqtt_config_t mqtt_settings;
    device_config_t device_settings;

    mqtt_config_snapshot(&mqtt_settings);
    device_config_snapshot(&device_settings);
    if (!mqtt_settings.valid || mqtt_settings.host[0] == '\0') {
        return false;
    }

    snprintf(
        mqtt_broker_uri,
        sizeof(mqtt_broker_uri),
        "%s://%s:%u",
        mqtt_settings.use_tls ? "mqtts" : "mqtt",
        mqtt_settings.host,
        (unsigned int)mqtt_settings.port);
    build_mqtt_message_topic(mqtt_message_topic, sizeof(mqtt_message_topic));
    strlcpy(mqtt_client_id, device_settings.device_name, sizeof(mqtt_client_id));
    strlcpy(mqtt_username, mqtt_settings.username, sizeof(mqtt_username));
    strlcpy(mqtt_password, mqtt_settings.password, sizeof(mqtt_password));

    esp_mqtt_client_config_t client_cfg = {};
    client_cfg.broker.address.uri = mqtt_broker_uri;
    client_cfg.credentials.client_id = mqtt_client_id;
    client_cfg.credentials.username = mqtt_username[0] ? mqtt_username : NULL;
    client_cfg.credentials.authentication.password = mqtt_password[0] ? mqtt_password : NULL;
    client_cfg.network.timeout_ms = 10000;
    client_cfg.network.reconnect_timeout_ms = 5000;
    client_cfg.task.stack_size = 6144;
    client_cfg.buffer.size = 1024;
    if (mqtt_settings.use_tls) {
        client_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&client_cfg);
    if (client == NULL) {
        ui_set_status_message("MQTT init failed");
        return false;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL));
    if (esp_mqtt_client_start(client) != ESP_OK) {
        esp_mqtt_client_destroy(client);
        ui_set_status_message("MQTT start failed");
        return false;
    }

    state_lock();
    mqtt_client = client;
    mqtt_restart_requested = false;
    state_unlock();
    ui_set_statusf("Connecting MQTT: %s", mqtt_broker_uri);
    return true;
}

static void mqtt_stop_client(void)
{
    esp_mqtt_client_handle_t client;

    state_lock();
    client = mqtt_client;
    mqtt_client = NULL;
    mqtt_connected = false;
    mqtt_restart_requested = false;
    mqtt_message_expected_len = 0;
    mqtt_message_received_len = 0;
    mqtt_message_collecting = false;
    mqtt_message_topic_match = false;
    mqtt_message_buffer[0] = '\0';
    state_unlock();

    ui_update_mqtt_icon(false);
    if (client != NULL) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }
}

static void mqtt_maintenance_task(void *arg)
{
    LV_UNUSED(arg);

    while (1) {
        bool local_wifi_connected;
        bool local_provisioning;
        bool local_restart_requested;
        bool local_mqtt_valid;
        esp_mqtt_client_handle_t local_client;
        mqtt_config_t mqtt_settings;

        mqtt_config_snapshot(&mqtt_settings);
        state_lock();
        local_wifi_connected = wifi_connected;
        local_provisioning = prov_active;
        local_restart_requested = mqtt_restart_requested;
        local_client = mqtt_client;
        local_mqtt_valid = mqtt_settings.valid && mqtt_settings.host[0] != '\0';
        state_unlock();

        if (local_client != NULL && (!local_wifi_connected || local_provisioning || !local_mqtt_valid || local_restart_requested)) {
            mqtt_stop_client();
        } else if (local_client == NULL && local_wifi_connected && !local_provisioning && local_mqtt_valid) {
            mqtt_start_client();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ui_housekeeping_task(void *arg)
{
    LV_UNUSED(arg);

    while (1) {
        bool clear_status = false;
        bool hide_overlay = false;
        int64_t now = esp_timer_get_time();

        state_lock();
        if (status_message_expire_at_us > 0 && now >= status_message_expire_at_us) {
            status_message_expire_at_us = 0;
            clear_status = true;
        }
        if (message_overlay_expire_at_us > 0 && now >= message_overlay_expire_at_us) {
            message_overlay_expire_at_us = 0;
            hide_overlay = true;
        }
        state_unlock();

        if (clear_status) {
            ui_set_status_message("");
        }

        if (hide_overlay) {
            ui_hide_message_overlay();
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void clock_task(void *arg)
{
    LV_UNUSED(arg);
    int last_second = -1;
    int last_year = -1;
    int last_month = -1;
    int last_day = -1;

    while (1) {
        time_t now = time(NULL);
        struct tm time_info = {};
        bool have_time = (now != (time_t)-1) && (localtime_r(&now, &time_info) != NULL);

        if (!have_time && rtc_ready) {
            rtcTimeStruct_t rtc_time = {};
            Rtc_GetTime(&rtc_time);
            time_info.tm_year = rtc_time.year - 1900;
            time_info.tm_mon = rtc_time.month - 1;
            time_info.tm_mday = rtc_time.day;
            time_info.tm_hour = rtc_time.hour;
            time_info.tm_min = rtc_time.minute;
            time_info.tm_sec = rtc_time.second;
            time_info.tm_wday = rtc_time.week;
            have_time = true;
        }

        if (have_time && time_info.tm_sec != last_second) {
            last_second = time_info.tm_sec;
            if (Lvgl_lock(-1)) {
                dashboard_ui_update_time(time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
                if (time_info.tm_year != last_year || time_info.tm_mon != last_month || time_info.tm_mday != last_day) {
                    last_year = time_info.tm_year;
                    last_month = time_info.tm_mon;
                    last_day = time_info.tm_mday;
                    dashboard_ui_update_date(time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday, time_info.tm_wday);
                }
                Lvgl_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void sensor_task(void *arg)
{
    LV_UNUSED(arg);
    float temp = 0.0f;
    float humi = 0.0f;

    vTaskDelay(pdMS_TO_TICKS(1000));
    while (1) {
        if (shtc3 != NULL && shtc3->Shtc3_ReadTempHumi(&temp, &humi) == 0) {
            if (Lvgl_lock(50)) {
                dashboard_ui_update_temp_humi(temp, humi);
                Lvgl_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void battery_task(void *arg)
{
    LV_UNUSED(arg);

    while (1) {
        int level = Adc_GetBatteryLevel();
        if (Lvgl_lock(50)) {
            dashboard_ui_update_battery(level);
            Lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void wifi_monitor_task(void *arg)
{
    LV_UNUSED(arg);
    int reconnect_ticks = 0;

    while (1) {
        bool local_connected;
        bool local_provisioning;
        bool local_connecting;

        state_lock();
        local_connected = wifi_connected;
        local_provisioning = prov_active;
        local_connecting = connect_in_progress;
        state_unlock();

        if (local_connected) {
            wifi_ap_record_t ap_record;
            reconnect_ticks = 0;
            if (esp_wifi_sta_get_ap_info(&ap_record) == ESP_OK) {
                state_lock();
                connected_rssi = ap_record.rssi;
                strlcpy(connected_ssid, (const char *)ap_record.ssid, sizeof(connected_ssid));
                state_unlock();
                ui_update_wifi_icon(true, (const char *)ap_record.ssid, ap_record.rssi);
            }
        } else if (!local_provisioning && !local_connecting && wifi_store_count_snapshot() > 0) {
            reconnect_ticks++;
            if (reconnect_ticks >= 3) {
                reconnect_ticks = 0;
                wifi_connect_saved_networks(false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void ntp_task(void *arg)
{
    LV_UNUSED(arg);
    const TickType_t sync_interval = pdMS_TO_TICKS((uint32_t)CONFIG_NTP_SYNC_INTERVAL_MIN * 60U * 1000U);

    while (1) {
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ntp_sync_once();

        TickType_t elapsed = 0;
        while (elapsed < sync_interval) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            elapsed += pdMS_TO_TICKS(5000);
            if ((xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) == 0) {
                break;
            }
        }
    }
}

static void boot_button_task(void *arg)
{
    LV_UNUSED(arg);
    const TickType_t long_press_ticks = pdMS_TO_TICKS(CONFIG_LONG_PRESS_MS);

    while (1) {
        bool pressed_now = gpio_get_level(BOOT_BUTTON_PIN) == 0;
        bool key_pressed_now = gpio_get_level(KEY_BUTTON_PIN) == 0;

        if (key_pressed_now && !key_button_pressed) {
            bool local_overlay_active;
            bool local_overlay_requires_key;

            key_button_pressed = true;
            state_lock();
            local_overlay_active = message_overlay_active;
            local_overlay_requires_key = message_overlay_requires_key;
            state_unlock();

            if (local_overlay_active && local_overlay_requires_key) {
                ui_hide_message_overlay();
            }
        } else if (!key_pressed_now) {
            key_button_pressed = false;
        }

        if (pressed_now && !boot_button_pressed) {
            boot_button_pressed = true;
            boot_button_long_handled = false;
            boot_button_press_ticks = xTaskGetTickCount();
        } else if (!pressed_now) {
            boot_button_pressed = false;
            boot_button_long_handled = false;
        } else if (!boot_button_long_handled
                   && (xTaskGetTickCount() - boot_button_press_ticks) >= long_press_ticks) {
            bool local_provisioning;

            boot_button_long_handled = true;
            state_lock();
            local_provisioning = prov_active;
            state_unlock();

            if (local_provisioning) {
                schedule_provisioning_action(false, true);
            } else {
                schedule_provisioning_action(true, false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void network_startup_task(void *arg)
{
    LV_UNUSED(arg);

    wifi_stack_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    if (wifi_store_count_snapshot() == 0) {
        wifi_enter_provisioning();
    } else {
        wifi_connect_saved_networks(true);
    }

    vTaskDelete(NULL);
}

void UserApp_AppInit(void)
{
    ESP_LOGI(TAG, "AppInit");

    nvs_init();
    state_mutex = xSemaphoreCreateMutex();
    assert(state_mutex != NULL);
    wifi_event_group = xEventGroupCreate();
    assert(wifi_event_group != NULL);

    wifi_store_load();
    device_config_load();
    mqtt_config_load();

    i2cbus = new I2cMasterBus(ESP32_I2C_SCL_PIN, ESP32_I2C_SDA_PIN, 0);
    Rtc_Setup(i2cbus, 0x51);
    rtc_ready = true;
    sync_system_time_from_rtc();
    shtc3 = new Shtc3Port(*i2cbus);
    Adc_PortInit();
    button_init();
}

void UserApp_UiInit(void)
{
    dashboard_ui_init();
    ui_set_default_status_message_locked();
}

void UserApp_TaskInit(void)
{
    xTaskCreatePinnedToCore(clock_task, "clock", 8 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task, "sensor", 3 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(battery_task, "battery", 2 * 1024, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(wifi_monitor_task, "wifi_mon", 6 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(mqtt_maintenance_task, "mqtt_maint", 5 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(ui_housekeeping_task, "ui_housekeep", 3 * 1024, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(ntp_task, "ntp", 4 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(boot_button_task, "boot_btn", 3 * 1024, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(network_startup_task, "net_start", 6 * 1024, NULL, 3, NULL, 1);
}
