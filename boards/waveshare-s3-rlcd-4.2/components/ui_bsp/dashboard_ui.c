#include "dashboard_ui.h"
#include "esp_attr.h"

#include <stdio.h>
#include <string.h>

#define UI_BG_COLOR lv_color_white()
#define UI_FG_COLOR lv_color_black()

#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300

/* Top area: keep the clock wide and reserve only the space needed by the
 * right-side status row; this is a pixel layout rather than a percentage split. */
#define RIGHT_COL_X 240
#define RIGHT_COL_W (SCREEN_WIDTH - RIGHT_COL_X - 4)
#define STATUS_LABEL_Y 8
#define STATUS_LABEL_W (MQTT_BADGE_X - RIGHT_COL_X - 1)
#define DATE_WEEK_LABEL_Y 27
#define UPDATE_LABEL_Y 46
#define STATUS_ICON_Y 8
#define STATUS_ICON_H 14

#define MQTT_BADGE_X 325
#define MQTT_BADGE_SZ STATUS_ICON_H
#define WIFI_ICON_X 343
#define WIFI_BAR_W 4
#define WIFI_BAR_STEP 5
#define WIFI_BAR_0_Y (STATUS_ICON_Y + 10)
#define WIFI_BAR_0_H 4
#define WIFI_BAR_1_Y (STATUS_ICON_Y + 7)
#define WIFI_BAR_1_H 7
#define WIFI_BAR_2_Y (STATUS_ICON_Y + 4)
#define WIFI_BAR_2_H 10
#define WIFI_BAR_3_Y (STATUS_ICON_Y + 1)
#define WIFI_BAR_3_H 13
#define BATTERY_OUTLINE_X 366
#define BATTERY_OUTLINE_Y STATUS_ICON_Y
#define BATTERY_OUTLINE_W 25
#define BATTERY_OUTLINE_H STATUS_ICON_H
#define BATTERY_CAP_X (BATTERY_OUTLINE_X + BATTERY_OUTLINE_W)
#define BATTERY_CAP_Y (STATUS_ICON_Y + 4)
#define BATTERY_CAP_W 1
#define BATTERY_CAP_H 6
#define BATTERY_PAD 2
#define BATTERY_SEG_COUNT 4
#define BATTERY_SEG_GAP 1
#define BATTERY_SEG_W 4
#define BATTERY_SEG_Y (BATTERY_OUTLINE_Y + 1 + BATTERY_PAD)
#define BATTERY_SEG_H (BATTERY_OUTLINE_H - 2 - BATTERY_PAD * 2)
#define BATTERY_SEG_0_X (BATTERY_OUTLINE_X + 1 + BATTERY_PAD)
#define BATTERY_SEG_1_X (BATTERY_SEG_0_X + BATTERY_SEG_W + BATTERY_SEG_GAP)
#define BATTERY_SEG_2_X (BATTERY_SEG_1_X + BATTERY_SEG_W + BATTERY_SEG_GAP)
#define BATTERY_SEG_3_X (BATTERY_SEG_2_X + BATTERY_SEG_W + BATTERY_SEG_GAP)

/* The 54 px DSEG HH:MM glyphs occupy about 220 px. Keep a small margin
 * before the right-side status column instead of tying the layout to a
 * fixed percentage split. */
#define CLOCK_LABEL_X 3
#define CLOCK_LABEL_Y 8
#define CLOCK_LABEL_W 232
#define HEADER_BOTTOM_Y 76

/* Full-width cloud text area. */
#define TEXT_X 8
#define TEXT_Y (HEADER_BOTTOM_Y + 1)
#define TEXT_W (SCREEN_WIDTH - TEXT_X * 2)
#define TEXT_H (SCREEN_HEIGHT - TEXT_Y - 4)
#define TEXT_PAGE_INTERVAL_MS 10000
#define TEXT_LINES_PER_PAGE 8
#define TEXT_MAX_LINES 160
#define TEXT_MAX_LINE_BYTES 256
#define TEXT_PAGE_BUFFER_SIZE 2200

static lv_obj_t *screen_obj;
static lv_obj_t *main_view;
static lv_obj_t *prov_view;
static lv_obj_t *clock_label;
static lv_obj_t *temp_humi_label;
static lv_obj_t *date_week_label;
static lv_obj_t *text_update_label;
static lv_obj_t *wifi_bars[4];
static lv_obj_t *wifi_mqtt_badge;
static lv_obj_t *battery_segments[BATTERY_SEG_COUNT];
static lv_obj_t *text_content_label;
static lv_timer_t *text_timer = NULL;
static EXT_RAM_BSS_ATTR char text_lines[TEXT_MAX_LINES][TEXT_MAX_LINE_BYTES];
static int text_line_count = 0;
static int text_page_count = 0;
static int text_current_page = 0;
static bool mqtt_badge_connected = false;
static int last_clock_hour = -1;
static int last_clock_minute = -1;

static lv_obj_t *prov_title_label;
static lv_obj_t *prov_ssid_label;
static lv_obj_t *prov_ip_label;
static lv_obj_t *prov_hint_label;

static const lv_font_t *font_clock = NULL;
static const lv_font_t *font_text = NULL;
static const lv_font_t *font_ui14 = NULL;

extern const lv_font_t noto_mono_14;
extern const lv_font_t noto_mono_22;
extern const lv_font_t noto_dseg_54;

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

static lv_obj_t *create_label(lv_obj_t *parent, int x, int y, int w,
    lv_text_align_t align, const lv_font_t *font)
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
        set_box_filled(wifi_bars[index], connected && index < level);
    }
}

static void set_battery_level(int level)
{
    int filled = (level * BATTERY_SEG_COUNT + 99) / 100;
    if (filled > BATTERY_SEG_COUNT) filled = BATTERY_SEG_COUNT;
    for (int i = 0; i < BATTERY_SEG_COUNT; i++) {
        lv_obj_set_style_bg_color(battery_segments[i],
            i < filled ? UI_FG_COLOR : UI_BG_COLOR, 0);
    }
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
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 2, coords.y1 + 8);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 5, coords.y1 + 12);
        lv_draw_line(layer, &line_dsc);
        lv_point_precise_set(&line_dsc.p1, coords.x1 + 5, coords.y1 + 12);
        lv_point_precise_set(&line_dsc.p2, coords.x1 + 12, coords.y1 + 2);
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

static const char *utf8_next(const char *src, char *glyph, size_t glyph_size)
{
    unsigned char first = (unsigned char)src[0];
    size_t length = 1;

    if (first < 0x80) {
        length = 1;
    } else if ((first & 0xE0) == 0xC0) {
        length = 2;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
    }

    for (size_t i = 1; i < length; i++) {
        unsigned char next = (unsigned char)src[i];
        if (next == '\0' || (next & 0xC0) != 0x80) {
            length = 1;
            break;
        }
    }

    if (length >= glyph_size) length = 1;
    memcpy(glyph, src, length);
    glyph[length] = '\0';
    return src + length;
}

static bool text_line_fits(const char *line, size_t line_length)
{
    char candidate[TEXT_MAX_LINE_BYTES];
    lv_point_t size = {0, 0};

    if (line_length >= sizeof(candidate)) return false;
    memcpy(candidate, line, line_length);
    candidate[line_length] = '\0';
    lv_text_get_size(&size, candidate, font_text, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x <= TEXT_W;
}

static void text_push_line(const char *line, size_t line_length)
{
    if (text_line_count >= TEXT_MAX_LINES) return;
    if (line_length >= TEXT_MAX_LINE_BYTES) line_length = TEXT_MAX_LINE_BYTES - 1;
    memcpy(text_lines[text_line_count], line, line_length);
    text_lines[text_line_count][line_length] = '\0';
    text_line_count++;
}

/* Wrap by rendered pixel width while preserving explicit newlines and UTF-8 glyphs. */
static void text_wrap(const char *text)
{
    char line[TEXT_MAX_LINE_BYTES];
    char glyph[8];
    size_t line_length = 0;
    const char *cursor = text != NULL ? text : "";

    text_line_count = 0;
    while (*cursor != '\0' && text_line_count < TEXT_MAX_LINES) {
        if (*cursor == '\r') {
            cursor++;
            continue;
        }
        if (*cursor == '\n') {
            text_push_line(line, line_length);
            line_length = 0;
            cursor++;
            continue;
        }

        const char *next = utf8_next(cursor, glyph, sizeof(glyph));
        size_t glyph_length = (size_t)(next - cursor);
        bool fits = true;
        if (line_length > 0 && line_length + glyph_length < sizeof(line)) {
            char candidate[TEXT_MAX_LINE_BYTES];
            memcpy(candidate, line, line_length);
            memcpy(candidate + line_length, glyph, glyph_length);
            fits = text_line_fits(candidate, line_length + glyph_length);
        }
        if (line_length + glyph_length >= sizeof(line) || !fits) {
            text_push_line(line, line_length);
            line_length = 0;
        }

        if (glyph_length >= sizeof(line)) {
            cursor = next;
            continue;
        }
        memcpy(line + line_length, glyph, glyph_length);
        line_length += glyph_length;
        line[line_length] = '\0';
        cursor = next;
    }

    if (line_length > 0 || text_line_count == 0) {
        text_push_line(line, line_length);
    }
}

static void text_show_page(int page)
{
    if (text_content_label == NULL) return;
    if (text_line_count == 0 || page < 0 || page >= text_page_count) {
        lv_label_set_text(text_content_label, "");
        return;
    }

    int start = page * TEXT_LINES_PER_PAGE;
    int end = start + TEXT_LINES_PER_PAGE;
    if (end > text_line_count) end = text_line_count;

    static char buffer[TEXT_PAGE_BUFFER_SIZE];
    size_t position = 0;
    for (int i = start; i < end; i++) {
        if (position > 0 && position < sizeof(buffer) - 1) buffer[position++] = '\n';
        int written = snprintf(buffer + position, sizeof(buffer) - position,
            "%s", text_lines[i]);
        if (written > 0) position += (size_t)written;
        if (position >= sizeof(buffer) - 1) break;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    lv_label_set_text(text_content_label, buffer);
}

static void text_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (text_page_count <= 1) return;
    text_current_page = (text_current_page + 1) % text_page_count;
    text_show_page(text_current_page);
}

void dashboard_ui_init(void)
{
    font_clock = &noto_dseg_54;
    font_text = &noto_mono_22;
    font_ui14 = &noto_mono_14;

    screen_obj = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_obj);
    lv_obj_set_style_bg_color(screen_obj, UI_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(screen_obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(screen_obj);

    main_view = create_box(screen_obj, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);
    prov_view = create_box(screen_obj, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);

    /* ---- fixed status strip ---- */
    temp_humi_label = create_label(main_view, RIGHT_COL_X, STATUS_LABEL_Y,
        RIGHT_COL_W, LV_TEXT_ALIGN_LEFT, font_ui14);
    date_week_label = create_label(main_view, RIGHT_COL_X, DATE_WEEK_LABEL_Y,
        RIGHT_COL_W, LV_TEXT_ALIGN_LEFT, font_ui14);
    text_update_label = create_label(main_view, RIGHT_COL_X, UPDATE_LABEL_Y,
        RIGHT_COL_W, LV_TEXT_ALIGN_LEFT, font_ui14);

    wifi_bars[0] = create_box(main_view, WIFI_ICON_X, WIFI_BAR_0_Y,
        WIFI_BAR_W, WIFI_BAR_0_H, true);
    wifi_bars[1] = create_box(main_view, WIFI_ICON_X + WIFI_BAR_STEP, WIFI_BAR_1_Y,
        WIFI_BAR_W, WIFI_BAR_1_H, true);
    wifi_bars[2] = create_box(main_view, WIFI_ICON_X + WIFI_BAR_STEP * 2, WIFI_BAR_2_Y,
        WIFI_BAR_W, WIFI_BAR_2_H, true);
    wifi_bars[3] = create_box(main_view, WIFI_ICON_X + WIFI_BAR_STEP * 3, WIFI_BAR_3_Y,
        WIFI_BAR_W, WIFI_BAR_3_H, true);

    wifi_mqtt_badge = create_box(main_view, MQTT_BADGE_X, STATUS_ICON_Y,
        MQTT_BADGE_SZ, MQTT_BADGE_SZ, false);
    lv_obj_set_style_bg_opa(wifi_mqtt_badge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(wifi_mqtt_badge, 1, 0);
    lv_obj_add_event_cb(wifi_mqtt_badge, mqtt_badge_draw_event_cb,
        LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *battery_outline = create_box(main_view, BATTERY_OUTLINE_X,
        BATTERY_OUTLINE_Y, BATTERY_OUTLINE_W, BATTERY_OUTLINE_H, true);
    lv_obj_t *battery_cap = create_box(main_view, BATTERY_CAP_X, BATTERY_CAP_Y,
        BATTERY_CAP_W, BATTERY_CAP_H, true);
    lv_obj_set_style_radius(battery_cap, 1, 0);
    LV_UNUSED(battery_outline);
    LV_UNUSED(battery_cap);
    const int segment_x[BATTERY_SEG_COUNT] = {
        BATTERY_SEG_0_X, BATTERY_SEG_1_X, BATTERY_SEG_2_X, BATTERY_SEG_3_X
    };
    for (int i = 0; i < BATTERY_SEG_COUNT; i++) {
        battery_segments[i] = create_box(main_view, segment_x[i], BATTERY_SEG_Y,
            BATTERY_SEG_W, BATTERY_SEG_H, false);
    }

    /* ---- clock hero ---- */
    clock_label = create_label(main_view, CLOCK_LABEL_X, CLOCK_LABEL_Y,
        CLOCK_LABEL_W, LV_TEXT_ALIGN_LEFT, font_clock);
    lv_label_set_text(clock_label, "--:--");

    /* ---- unified cloud text view ---- */
    text_content_label = create_label(main_view, TEXT_X, TEXT_Y, TEXT_W,
        LV_TEXT_ALIGN_LEFT, font_text);
    lv_obj_set_height(text_content_label, TEXT_H);
    lv_label_set_long_mode(text_content_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_line_space(text_content_label, 0, 0);
    text_timer = lv_timer_create(text_timer_cb, TEXT_PAGE_INTERVAL_MS, NULL);
    lv_timer_pause(text_timer);

    /* ---- provisioning ---- */
    prov_title_label = create_label(prov_view, 40, 42, 320,
        LV_TEXT_ALIGN_CENTER, font_text);
    prov_ssid_label = create_label(prov_view, 24, 104, 352,
        LV_TEXT_ALIGN_LEFT, font_ui14);
    prov_ip_label = create_label(prov_view, 24, 138, 352,
        LV_TEXT_ALIGN_LEFT, font_ui14);
    prov_hint_label = create_label(prov_view, 24, 188, 352,
        LV_TEXT_ALIGN_LEFT, font_ui14);

    lv_label_set_text(temp_humi_label, "--.-℃ --%RH");
    lv_label_set_text(date_week_label, "----.--.-- 星期-");
    lv_label_set_text(text_update_label, "更新 --.-- --:--:--");
    lv_label_set_text(prov_title_label, "Provisioning Mode");
    lv_label_set_text(prov_hint_label,
        "Open 192.168.4.1 in browser\nAdd Wi-Fi credentials\nPress KEY or BOOT to exit.");
    lv_label_set_text(prov_ssid_label, "AP SSID: --");
    lv_label_set_text(prov_ip_label, "Address: --");

    dashboard_ui_update_text("");
    dashboard_ui_update_battery(0);
    dashboard_ui_update_wifi_status(false, NULL, 0);
    dashboard_ui_update_mqtt_status(false);
    lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(main_view, LV_OBJ_FLAG_HIDDEN);
}

void dashboard_ui_update_time(int hour, int minute, int second)
{
    LV_UNUSED(second);
    if (clock_label == NULL) return;
    if (hour == last_clock_hour && minute == last_clock_minute) return;

    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    lv_label_set_text(clock_label, buffer);
    last_clock_hour = hour;
    last_clock_minute = minute;
}

void dashboard_ui_update_text_timestamp(int month, int day, int hour, int minute, int second)
{
    if (text_update_label == NULL) return;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "更新 %02d-%02d %02d:%02d:%02d",
        month, day, hour, minute, second);
    lv_label_set_text(text_update_label, buffer);
}

void dashboard_ui_update_date(int year, int month, int day, int week)
{
    static const char *weekday_names[] = {"日", "一", "二", "三", "四", "五", "六"};
    if (date_week_label == NULL) return;

    const char *weekday = (week >= 0 && week <= 6) ? weekday_names[week] : "-";
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d 星期%s",
        year, month, day, weekday);
    lv_label_set_text(date_week_label, buffer);
}

void dashboard_ui_update_text(const char *text)
{
    if (text_content_label == NULL) return;

    if (text_timer != NULL) lv_timer_pause(text_timer);
    text_current_page = 0;
    text_page_count = 0;
    text_wrap(text);

    if (text_line_count > 0) {
        text_page_count = (text_line_count + TEXT_LINES_PER_PAGE - 1)
            / TEXT_LINES_PER_PAGE;
        text_show_page(0);
        if (text_page_count > 1) {
            if (text_timer == NULL) {
                text_timer = lv_timer_create(text_timer_cb, TEXT_PAGE_INTERVAL_MS, NULL);
            } else {
                lv_timer_reset(text_timer);
            }
            lv_timer_resume(text_timer);
        }
    } else {
        lv_label_set_text(text_content_label, "");
    }
}

void dashboard_ui_next_text_page(void)
{
    if (text_content_label == NULL || text_page_count <= 1) return;

    text_current_page = (text_current_page + 1) % text_page_count;
    text_show_page(text_current_page);
    if (text_timer != NULL) lv_timer_reset(text_timer);
}

void dashboard_ui_update_temp_humi(float temp, float humi)
{
    if (temp_humi_label == NULL) return;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%2.1f℃ %2.0f%%RH", temp, humi);
    lv_label_set_text(temp_humi_label, buffer);
}

void dashboard_ui_update_wifi_status(bool connected, const char *ssid, int rssi)
{
    LV_UNUSED(ssid);
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

void dashboard_ui_update_mqtt_status(bool connected)
{
    set_mqtt_level(connected);
}

void dashboard_ui_update_battery(int level)
{
    if (battery_segments[0] == NULL) return;
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    set_battery_level(level);
}

void dashboard_ui_set_provisioning(bool active, const char *ap_ssid, const char *ap_ip)
{
    if (main_view == NULL || prov_view == NULL || prov_ssid_label == NULL
        || prov_ip_label == NULL) return;

    if (active) {
        lv_obj_add_flag(main_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(main_view, LV_OBJ_FLAG_HIDDEN);
    }

    if (ap_ssid != NULL) {
        char buffer[96];
        snprintf(buffer, sizeof(buffer), "AP SSID: %s", ap_ssid);
        lv_label_set_text(prov_ssid_label, buffer);
    } else {
        lv_label_set_text(prov_ssid_label, "AP SSID: --");
    }

    if (ap_ip != NULL) {
        char buffer[96];
        snprintf(buffer, sizeof(buffer), "Address: http://%s", ap_ip);
        lv_label_set_text(prov_ip_label, buffer);
    } else {
        lv_label_set_text(prov_ip_label, "Address: --");
    }
}
