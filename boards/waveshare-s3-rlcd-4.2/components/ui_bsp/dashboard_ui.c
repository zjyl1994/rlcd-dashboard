#include "dashboard_ui.h"
#include "libs/tiny_ttf/lv_tiny_ttf.h"

#include <stdio.h>
#include <string.h>

#define UI_BG_COLOR lv_color_white()
#define UI_FG_COLOR lv_color_black()
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300
#define H_SPLIT_Y 134
#define V_SPLIT_X 196

/* top-left: clock + date */
#define CLOCK_X 0
#define CLOCK_Y 0
#define CLOCK_W V_SPLIT_X
#define CLOCK_H H_SPLIT_Y
#define CLOCK_LABEL_Y 10
#define DATE_LABEL_Y 92
#define CLOCK_FONT_SIZE 72
#define DATE_FONT_SIZE 18

/* top-right: status area */
#define STATUS_X (V_SPLIT_X + 1)
#define STATUS_Y 0
#define STATUS_W (SCREEN_WIDTH - V_SPLIT_X - 1)
#define STATUS_H H_SPLIT_Y
#define TOP_ROW_Y 4
#define TOP_ICON_Y 4
#define TOP_ICON_H 14
#define TEMP_LABEL_X (STATUS_X + 8)
#define TEMP_LABEL_W 120
#define MQTT_BADGE_X 338
#define MQTT_BADGE_Y TOP_ICON_Y
#define MQTT_BADGE_SZ TOP_ICON_H
#define WIFI_ICON_X 356
#define WIFI_BAR_W 3
#define WIFI_BAR_STEP 4
#define WIFI_BAR_0_Y (TOP_ICON_Y + 10)
#define WIFI_BAR_0_H 4
#define WIFI_BAR_1_Y (TOP_ICON_Y + 7)
#define WIFI_BAR_1_H 7
#define WIFI_BAR_2_Y (TOP_ICON_Y + 4)
#define WIFI_BAR_2_H 10
#define WIFI_BAR_3_Y (TOP_ICON_Y + 1)
#define WIFI_BAR_3_H 13
#define BATTERY_OUTLINE_X 374
#define BATTERY_OUTLINE_Y TOP_ICON_Y
#define BATTERY_OUTLINE_W 18
#define BATTERY_OUTLINE_H TOP_ICON_H
#define BATTERY_CAP_X (BATTERY_OUTLINE_X + BATTERY_OUTLINE_W)
#define BATTERY_CAP_Y (TOP_ICON_Y + 4)
#define BATTERY_CAP_W 2
#define BATTERY_CAP_H 6
#define BATTERY_FILL_X (BATTERY_OUTLINE_X + 3)
#define BATTERY_FILL_Y (BATTERY_OUTLINE_Y + 3)
#define BATTERY_FILL_H 8
#define BATTERY_FILL_MAX_W 12
/* KV display in top-right below icons */
#define KV_LABEL_X (STATUS_X + 14)
#define KV_LABEL_Y 30
#define KV_LABEL_W (STATUS_W - 28)
#define KV_LABEL_H (H_SPLIT_Y - KV_LABEL_Y - 2)
#define KV_PAGE_INTERVAL_MS 10000
#define KV_MAX_LINES 48
#define KV_MAX_LINE_LEN 80

/* bottom: message area */
#define MSG_X 8
#define MSG_Y (H_SPLIT_Y + 6)
#define MSG_W (SCREEN_WIDTH - 16)
#define MSG_H (SCREEN_HEIGHT - H_SPLIT_Y - 10)
#define MSG_TITLE_Y (H_SPLIT_Y + 4)
#define MSG_CONTENT_Y (H_SPLIT_Y + 32)
#define MSG_FONT_SIZE 24

/* provisioning (same as before) */
#define STAT_LABEL_Y (SCREEN_HEIGHT - 16)

static lv_obj_t *screen_obj;
static lv_obj_t *main_view;
static lv_obj_t *prov_view;
static lv_obj_t *clock_label;
static lv_obj_t *date_label;
static lv_obj_t *temp_humi_label;
static lv_obj_t *wifi_bars[4];
static lv_obj_t *wifi_mqtt_badge;
static lv_obj_t *battery_fill;
static lv_obj_t *msg_title_label;
static lv_obj_t *msg_content_label;
static lv_obj_t *status_label;
static lv_obj_t *prov_title_label;
static lv_obj_t *prov_ssid_label;
static lv_obj_t *prov_ip_label;
static lv_obj_t *prov_hint_label;
static lv_font_t *font_clock = NULL;
static lv_font_t *font_msg = NULL;
static lv_font_t *font_date = NULL;
static lv_obj_t *kv_label;
static lv_timer_t *kv_timer = NULL;
static char kv_lines[KV_MAX_LINES][KV_MAX_LINE_LEN];
static int kv_line_count = 0;
static int kv_lines_per_page = 0;
static int kv_page_count = 0;
static int kv_current_page = 0;
static bool mqtt_badge_connected = false;
static int last_clock_hour = -1;
static int last_clock_minute = -1;

extern const uint8_t smiley_ttf_start[] asm("_binary_SmileySans_Oblique_ttf_start");
extern const uint8_t smiley_ttf_end[] asm("_binary_SmileySans_Oblique_ttf_end");

static lv_obj_t *create_box(lv_obj_t *parent, int x, int y, int w, int h, bool outlined)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, UI_FG_COLOR, 0);
    lv_obj_set_style_border_width(obj, outlined ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *create_label(lv_obj_t *parent, int x, int y, int w, lv_text_align_t align, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_color(label, UI_FG_COLOR, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, "");
    return label;
}

static void set_box_filled(lv_obj_t *obj, bool filled)
{
    lv_obj_set_style_bg_color(obj, filled ? UI_FG_COLOR : UI_BG_COLOR, 0);
    lv_obj_set_style_border_width(obj, filled ? 0 : 1, 0);
}

static void set_wifi_level(bool connected, int level)
{
    for (int index = 0; index < 4; index++) {
        bool active = connected && index < level;
        set_box_filled(wifi_bars[index], active);
    }
}

static void set_battery_level(int level)
{
    int width = 0;
    if (level > 0) {
        width = (level * BATTERY_FILL_MAX_W) / 100;
        if (width < 2) width = 2;
    }
    lv_obj_set_width(battery_fill, width);
    lv_obj_set_style_bg_color(battery_fill, width > 0 ? UI_FG_COLOR : UI_BG_COLOR, 0);
}

static void mqtt_badge_draw_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    if (obj == NULL || layer == NULL) return;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_obj_init_draw_line_dsc(obj, LV_PART_MAIN, &line_dsc);
    line_dsc.color = UI_FG_COLOR;
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    if (mqtt_badge_connected) {
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 3, coords.y1 + 7);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 5, coords.y1 + 10);
        lv_draw_line(layer, &line_dsc);
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 5, coords.y1 + 10);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 10, coords.y1 + 4);
        lv_draw_line(layer, &line_dsc);
    } else {
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 3, coords.y1 + 3);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 10, coords.y1 + 10);
        lv_draw_line(layer, &line_dsc);
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 10, coords.y1 + 3);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 3, coords.y1 + 10);
        lv_draw_line(layer, &line_dsc);
    }
}

static void set_mqtt_level(bool connected)
{
    if (wifi_mqtt_badge == NULL) return;
    if (mqtt_badge_connected != connected) {
        mqtt_badge_connected = connected;
        lv_obj_invalidate(wifi_mqtt_badge);
    }
}

static lv_obj_t *create_divider_h(lv_obj_t *parent, int x, int y, int w)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, UI_FG_COLOR, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

static lv_obj_t *create_divider_v(lv_obj_t *parent, int x, int y, int h)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, 1, h);
    lv_obj_set_style_bg_color(line, UI_FG_COLOR, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

void dashboard_ui_init(void)
{
    size_t ttf_size = (size_t)(smiley_ttf_end - smiley_ttf_start);
    font_clock = lv_tiny_ttf_create_data(smiley_ttf_start, ttf_size, CLOCK_FONT_SIZE);
    font_msg = lv_tiny_ttf_create_data(smiley_ttf_start, ttf_size, MSG_FONT_SIZE);
    font_date = lv_tiny_ttf_create_data(smiley_ttf_start, ttf_size, DATE_FONT_SIZE);
    if (font_clock == NULL) font_clock = (lv_font_t *)&lv_font_montserrat_48;
    if (font_msg == NULL) font_msg = (lv_font_t *)&lv_font_montserrat_24;
    if (font_date == NULL) font_date = (lv_font_t *)&lv_font_montserrat_14;

    screen_obj = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_obj);
    lv_obj_set_style_bg_color(screen_obj, UI_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(screen_obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(screen_obj);

    main_view = create_box(screen_obj, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);
    prov_view = create_box(screen_obj, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);

    /* divider lines */
    create_divider_h(screen_obj, 0, H_SPLIT_Y, SCREEN_WIDTH);
    create_divider_v(screen_obj, V_SPLIT_X, 0, H_SPLIT_Y);

    /* ---- top-left: clock + date ---- */
    clock_label = create_label(main_view, 0, CLOCK_LABEL_Y, CLOCK_W, LV_TEXT_ALIGN_CENTER, font_clock);
    date_label = create_label(main_view, 0, DATE_LABEL_Y, CLOCK_W, LV_TEXT_ALIGN_CENTER, font_date);
    lv_label_set_text(clock_label, "--:--");
    lv_label_set_text(date_label, "----.--.--");

    /* ---- top-right: status ---- */
    temp_humi_label = create_label(main_view, TEMP_LABEL_X, TOP_ROW_Y, TEMP_LABEL_W, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_14);

    wifi_bars[0] = create_box(main_view, WIFI_ICON_X, WIFI_BAR_0_Y, WIFI_BAR_W, WIFI_BAR_0_H, true);
    wifi_bars[1] = create_box(main_view, WIFI_ICON_X + WIFI_BAR_STEP, WIFI_BAR_1_Y, WIFI_BAR_W, WIFI_BAR_1_H, true);
    wifi_bars[2] = create_box(main_view, WIFI_ICON_X + (WIFI_BAR_STEP * 2), WIFI_BAR_2_Y, WIFI_BAR_W, WIFI_BAR_2_H, true);
    wifi_bars[3] = create_box(main_view, WIFI_ICON_X + (WIFI_BAR_STEP * 3), WIFI_BAR_3_Y, WIFI_BAR_W, WIFI_BAR_3_H, true);

    wifi_mqtt_badge = create_box(main_view, MQTT_BADGE_X, MQTT_BADGE_Y, MQTT_BADGE_SZ, MQTT_BADGE_SZ, false);
    lv_obj_set_style_bg_opa(wifi_mqtt_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(wifi_mqtt_badge, 1, 0);
    lv_obj_add_event_cb(wifi_mqtt_badge, mqtt_badge_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *battery_outline = create_box(main_view, BATTERY_OUTLINE_X, BATTERY_OUTLINE_Y, BATTERY_OUTLINE_W, BATTERY_OUTLINE_H, true);
    (void)battery_outline;
    lv_obj_t *battery_cap = create_box(main_view, BATTERY_CAP_X, BATTERY_CAP_Y, BATTERY_CAP_W, BATTERY_CAP_H, true);
    lv_obj_set_style_radius(battery_cap, 1, 0);
    (void)battery_cap;
    battery_fill = create_box(main_view, BATTERY_FILL_X, BATTERY_FILL_Y, 0, BATTERY_FILL_H, false);

    kv_label = create_label(main_view, KV_LABEL_X, KV_LABEL_Y, KV_LABEL_W, LV_TEXT_ALIGN_CENTER, font_msg);
    lv_obj_set_style_text_line_space(kv_label, 6, 0);
    lv_obj_set_height(kv_label, KV_LABEL_H);
    lv_label_set_long_mode(kv_label, LV_LABEL_LONG_CLIP);

    /* ---- bottom: message ---- */
    msg_title_label = create_label(main_view, MSG_X, MSG_TITLE_Y, MSG_W, LV_TEXT_ALIGN_CENTER, font_msg);
    msg_content_label = create_label(main_view, MSG_X, MSG_CONTENT_Y, MSG_W, LV_TEXT_ALIGN_LEFT, font_msg);
    lv_obj_set_height(msg_content_label, MSG_H - 30);
    lv_label_set_long_mode(msg_content_label, LV_LABEL_LONG_WRAP);

    /* ---- status + provisioning ---- */
    status_label = create_label(screen_obj, 10, STAT_LABEL_Y, SCREEN_WIDTH - 20, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_12);

    prov_title_label = create_label(prov_view, 40, 42, 320, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_24);
    prov_ssid_label = create_label(prov_view, 24, 104, 352, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_14);
    prov_ip_label = create_label(prov_view, 24, 138, 352, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_14);
    prov_hint_label = create_label(prov_view, 24, 188, 352, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_14);

    lv_label_set_text(temp_humi_label, "--.- C  --%");
    lv_label_set_text(status_label, "Booting...");
    lv_label_set_text(prov_title_label, "Provisioning Mode");
    lv_label_set_text(prov_hint_label, "Open 192.168.4.1 in browser\nAdd Wi-Fi credentials\nHold BOOT to exit.");

    dashboard_ui_update_battery(0);
    dashboard_ui_update_wifi_status(false, NULL, 0);
    dashboard_ui_update_mqtt_status(false);
    lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(main_view, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(prov_ssid_label, "AP SSID: --");
    lv_label_set_text(prov_ip_label, "Address: --");
}

void dashboard_ui_update_time(int hour, int minute, int second)
{
    if (clock_label == NULL) return;

    if (hour != last_clock_hour || minute != last_clock_minute) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
        lv_label_set_text(clock_label, buf);
        last_clock_hour = hour;
        last_clock_minute = minute;
    }
    LV_UNUSED(second);
}

void dashboard_ui_update_date(int year, int month, int day, int week)
{
    if (date_label == NULL) return;
    LV_UNUSED(week);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d.%02d.%02d", year, month, day);
    lv_label_set_text(date_label, buf);
}

void dashboard_ui_show_message(const char *title, const char *content)
{
    if (msg_title_label == NULL || msg_content_label == NULL) return;
    lv_label_set_text(msg_title_label, (title != NULL) ? title : "");
    lv_label_set_text(msg_content_label, (content != NULL) ? content : "");
}

static void kv_show_page(int page)
{
    if (kv_label == NULL || kv_line_count == 0) return;
    if (page < 0 || page >= kv_page_count) {
        lv_label_set_text(kv_label, "");
        return;
    }
    int start = page * kv_lines_per_page;
    int end = start + kv_lines_per_page;
    if (end > kv_line_count) end = kv_line_count;
    size_t offset = 0;
    char buf[512];
    for (int i = start; i < end && offset < sizeof(buf) - 1; i++) {
        int n = snprintf(buf + offset, sizeof(buf) - offset, "%s\n", kv_lines[i]);
        if (n > 0) offset += n;
        if (offset >= sizeof(buf) - 1) break;
    }
    if (offset > 0 && buf[offset - 1] == '\n') buf[offset - 1] = '\0';
    lv_label_set_text(kv_label, buf);
}

static void kv_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (kv_page_count <= 1) return;
    kv_current_page = (kv_current_page + 1) % kv_page_count;
    kv_show_page(kv_current_page);
}

void dashboard_ui_update_kv(const char *kv_text)
{
    if (kv_label == NULL) return;

    if (kv_timer != NULL) {
        lv_timer_pause(kv_timer);
    }
    kv_line_count = 0;
    kv_page_count = 0;
    kv_current_page = 0;

    if (kv_text == NULL || kv_text[0] == '\0') {
        lv_label_set_text(kv_label, "");
        return;
    }

    const char *p = kv_text;
    while (*p != '\0' && kv_line_count < KV_MAX_LINES) {
        const char *nl = strchr(p, '\n');
        int len = (nl != NULL) ? (int)(nl - p) : (int)strlen(p);
        if (len > KV_MAX_LINE_LEN - 1) len = KV_MAX_LINE_LEN - 1;
        memcpy(kv_lines[kv_line_count], p, len);
        kv_lines[kv_line_count][len] = '\0';
        kv_line_count++;
        if (nl != NULL) p = nl + 1;
        else break;
    }

    if (kv_line_count > 0) {
        kv_lines_per_page = 3;
        kv_page_count = (kv_line_count + kv_lines_per_page - 1) / kv_lines_per_page;
        kv_show_page(0);
        if (kv_page_count > 1) {
            if (kv_timer == NULL) {
                kv_timer = lv_timer_create(kv_timer_cb, KV_PAGE_INTERVAL_MS, NULL);
            } else {
                lv_timer_reset(kv_timer);
            }
            lv_timer_resume(kv_timer);
        }
    } else {
        lv_label_set_text(kv_label, "");
    }
}

void dashboard_ui_update_temp_humi(float temp, float humi)
{
    if (temp_humi_label == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%2.1f C  %2.0f%%", temp, humi);
    lv_label_set_text(temp_humi_label, buf);
}

void dashboard_ui_update_wifi_status(bool connected, const char *ssid, int rssi)
{
    if (wifi_bars[0] == NULL) return;
    int level = 0;
    if (connected) {
        if (rssi >= -55) level = 4;
        else if (rssi >= -67) level = 3;
        else if (rssi >= -75) level = 2;
        else level = 1;
    }
    set_wifi_level(connected, level);
}

void dashboard_ui_update_battery(int level)
{
    if (battery_fill == NULL) return;
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    set_battery_level(level);
}

void dashboard_ui_update_mqtt_status(bool connected)
{
    set_mqtt_level(connected);
}

void dashboard_ui_set_provisioning(bool active, const char *ap_ssid, const char *ap_ip)
{
    if (main_view == NULL || prov_view == NULL || prov_ssid_label == NULL || prov_ip_label == NULL) return;

    if (active) {
        lv_obj_add_flag(main_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(main_view, LV_OBJ_FLAG_HIDDEN);
    }

    if (ap_ssid != NULL) {
        char buf[96];
        snprintf(buf, sizeof(buf), "AP SSID: %s", ap_ssid);
        lv_label_set_text(prov_ssid_label, buf);
    } else {
        lv_label_set_text(prov_ssid_label, "AP SSID: --");
    }

    if (ap_ip != NULL) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Address: http://%s", ap_ip);
        lv_label_set_text(prov_ip_label, buf);
    } else {
        lv_label_set_text(prov_ip_label, "Address: --");
    }
}

void dashboard_ui_set_status_message(const char *message)
{
    if (status_label == NULL) return;
    lv_label_set_text(status_label, (message != NULL) ? message : "");
}
