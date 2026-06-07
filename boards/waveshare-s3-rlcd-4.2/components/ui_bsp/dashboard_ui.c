#include "dashboard_ui.h"
#include "unifont_16.h"

#include <stdio.h>
#include <string.h>

#define UI_BG_COLOR lv_color_white()
#define UI_FG_COLOR lv_color_black()
#define CELSIUS_SYMBOL "\xE2\x84\x83"
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300
#define TOP_ROW_Y 8
#define TOP_ITEM_GAP 8
#define TEMP_LABEL_X 10
#define TEMP_LABEL_WIDTH 104
#define DATE_LABEL_X 156
#define DATE_LABEL_Y TOP_ROW_Y
#define DATE_LABEL_WIDTH 88
#define SECOND_LABEL_X 144
#define SECOND_LABEL_Y 244
#define SECOND_LABEL_WIDTH 112
#define TOP_ICON_Y 11
#define TOP_ICON_HEIGHT 14
#define MQTT_BADGE_X 331
#define MQTT_BADGE_Y TOP_ICON_Y
#define MQTT_BADGE_SIZE TOP_ICON_HEIGHT
#define WIFI_ICON_X 350
#define WIFI_BAR_WIDTH 3
#define WIFI_BAR_STEP 4
#define WIFI_BAR_0_Y (TOP_ICON_Y + 10)
#define WIFI_BAR_0_H 4
#define WIFI_BAR_1_Y (TOP_ICON_Y + 7)
#define WIFI_BAR_1_H 7
#define WIFI_BAR_2_Y (TOP_ICON_Y + 4)
#define WIFI_BAR_2_H 10
#define WIFI_BAR_3_Y (TOP_ICON_Y + 1)
#define WIFI_BAR_3_H 13
#define BATTERY_OUTLINE_X 373
#define BATTERY_OUTLINE_Y TOP_ICON_Y
#define BATTERY_OUTLINE_W 18
#define BATTERY_OUTLINE_H TOP_ICON_HEIGHT
#define BATTERY_CAP_X (BATTERY_OUTLINE_X + BATTERY_OUTLINE_W)
#define BATTERY_CAP_Y (TOP_ICON_Y + 4)
#define BATTERY_CAP_W 2
#define BATTERY_CAP_H 6
#define BATTERY_FILL_X (BATTERY_OUTLINE_X + 3)
#define BATTERY_FILL_Y (BATTERY_OUTLINE_Y + 3)
#define BATTERY_FILL_H 8
#define BATTERY_FILL_MAX_W 12
#define CLOCK_CANVAS_X 24
#define CLOCK_CANVAS_Y 78
#define CLOCK_CANVAS_WIDTH 352
#define CLOCK_CANVAS_HEIGHT 152
#define CLOCK_DIGIT_X0 8
#define CLOCK_DIGIT_X1 88
#define CLOCK_DIGIT_X2 192
#define CLOCK_DIGIT_X3 272
#define CLOCK_DIGIT_Y 6
#define CLOCK_DIGIT_WIDTH 72
#define CLOCK_DIGIT_HEIGHT 140
#define CLOCK_SEGMENT_THICKNESS 14
#define CLOCK_SEGMENT_TIP 10
#define CLOCK_HORIZONTAL_X_OFFSET 10
#define CLOCK_HORIZONTAL_LENGTH 52
#define CLOCK_VERTICAL_UPPER_Y 10
#define CLOCK_VERTICAL_LOWER_Y 70
#define CLOCK_VERTICAL_LENGTH 60
#define CLOCK_MIDDLE_SEGMENT_Y 63
#define CLOCK_BOTTOM_SEGMENT_Y 126
#define CLOCK_COLON_CENTER_X 176
#define CLOCK_COLON_TOP_CENTER_Y 50
#define CLOCK_COLON_BOTTOM_CENTER_Y 96
#define CLOCK_COLON_SIZE 10
#define MESSAGE_TITLE_BAR_HEIGHT 20
#define MESSAGE_TITLE_LABEL_Y 2
#define MESSAGE_TITLE_SCALE 256
#define CLOCK_CANVAS_PALETTE_BYTES (LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * sizeof(lv_color32_t))
#define CLOCK_CANVAS_STRIDE_BYTES ((CLOCK_CANVAS_WIDTH + 7) >> 3)
#define CLOCK_DIGIT_BITMAP_STRIDE ((CLOCK_DIGIT_WIDTH + 7) >> 3)
#define CLOCK_COLON_BITMAP_WIDTH CLOCK_COLON_SIZE
#define CLOCK_COLON_BITMAP_HEIGHT CLOCK_CANVAS_HEIGHT
#define CLOCK_COLON_BITMAP_STRIDE ((CLOCK_COLON_BITMAP_WIDTH + 7) >> 3)

static lv_obj_t *screen_obj;
static lv_obj_t *clock_view;
static lv_obj_t *prov_view;
static lv_obj_t *temp_humi_label;
static lv_obj_t *clock_canvas;
static lv_obj_t *date_label;
static lv_obj_t *second_label;
static lv_obj_t *status_label;
static lv_obj_t *prov_title_label;
static lv_obj_t *prov_ssid_label;
static lv_obj_t *prov_ip_label;
static lv_obj_t *prov_hint_label;
static lv_obj_t *wifi_bars[4];
static lv_obj_t *wifi_mqtt_badge;
static lv_obj_t *wifi_mqtt_mark_a;
static lv_obj_t *wifi_mqtt_mark_b;
static lv_obj_t *battery_fill;
static lv_obj_t *message_view;
static lv_obj_t *message_title_bar;
static lv_obj_t *message_title_label;
static lv_obj_t *message_content_view;
static lv_obj_t *message_label;
static int last_clock_hour = -1;
static int last_clock_minute = -1;
static bool clock_bitmap_cache_ready = false;

static uint8_t clock_digit_bitmaps[10][CLOCK_DIGIT_BITMAP_STRIDE * CLOCK_DIGIT_HEIGHT];
static uint8_t clock_colon_bitmap[CLOCK_COLON_BITMAP_STRIDE * CLOCK_COLON_BITMAP_HEIGHT];
static int clock_digit_visible_left[10];
static int clock_digit_visible_right[10];

LV_DRAW_BUF_DEFINE_STATIC(clock_canvas_draw_buf, CLOCK_CANVAS_WIDTH, CLOCK_CANVAS_HEIGHT, LV_COLOR_FORMAT_I1);

static const uint8_t digit_masks[10] = {
    0x77, 0x24, 0x5d, 0x6d, 0x2e,
    0x6b, 0x7b, 0x25, 0x7f, 0x6f,
};

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
    lv_obj_set_style_radius(obj, 2, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *create_label_with_font(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    lv_text_align_t align,
    const lv_font_t *font)
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

static lv_obj_t *create_label(lv_obj_t *parent, int x, int y, int w, lv_text_align_t align)
{
    return create_label_with_font(parent, x, y, w, align, &unifont_16);
}

static void bitmap_set_pixel(uint8_t *bitmap, int stride, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    bitmap[y * stride + (x >> 3)] |= (uint8_t)(1U << (7 - (x & 7)));
}

static bool bitmap_get_pixel(const uint8_t *bitmap, int stride, int x, int y)
{
    return ((bitmap[y * stride + (x >> 3)] >> (7 - (x & 7))) & 0x01U) != 0;
}

static void bitmap_fill_rect(uint8_t *bitmap, int stride, int width, int height, int x, int y, int w, int h)
{
    int x_start = x < 0 ? 0 : x;
    int y_start = y < 0 ? 0 : y;
    int x_end = (x + w) > width ? width : (x + w);
    int y_end = (y + h) > height ? height : (y + h);

    for (int row = y_start; row < y_end; row++) {
        for (int col = x_start; col < x_end; col++) {
            bitmap_set_pixel(bitmap, stride, width, height, col, row);
        }
    }
}

static int triangle_edge(int x0, int y0, int x1, int y1, int x, int y)
{
    return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
}

static void bitmap_fill_triangle(
    uint8_t *bitmap,
    int stride,
    int width,
    int height,
    int x1,
    int y1,
    int x2,
    int y2,
    int x3,
    int y3)
{
    int min_x = x1;
    int max_x = x1;
    int min_y = y1;
    int max_y = y1;

    if (x2 < min_x) min_x = x2;
    if (x3 < min_x) min_x = x3;
    if (x2 > max_x) max_x = x2;
    if (x3 > max_x) max_x = x3;
    if (y2 < min_y) min_y = y2;
    if (y3 < min_y) min_y = y3;
    if (y2 > max_y) max_y = y2;
    if (y3 > max_y) max_y = y3;

    for (int row = min_y; row <= max_y; row++) {
        for (int col = min_x; col <= max_x; col++) {
            int w0 = triangle_edge(x1, y1, x2, y2, col, row);
            int w1 = triangle_edge(x2, y2, x3, y3, col, row);
            int w2 = triangle_edge(x3, y3, x1, y1, col, row);

            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                bitmap_set_pixel(bitmap, stride, width, height, col, row);
            }
        }
    }
}

static void bitmap_draw_horizontal_segment(uint8_t *bitmap, int stride, int width, int height, int x, int y, int length)
{
    bitmap_fill_rect(bitmap, stride, width, height, x + CLOCK_SEGMENT_TIP, y, length - (CLOCK_SEGMENT_TIP * 2), CLOCK_SEGMENT_THICKNESS);
    bitmap_fill_triangle(
        bitmap, stride, width, height,
        x + CLOCK_SEGMENT_TIP, y,
        x, y + (CLOCK_SEGMENT_THICKNESS / 2),
        x + CLOCK_SEGMENT_TIP, y + CLOCK_SEGMENT_THICKNESS - 1);
    bitmap_fill_triangle(
        bitmap, stride, width, height,
        x + length - CLOCK_SEGMENT_TIP - 1, y,
        x + length - 1, y + (CLOCK_SEGMENT_THICKNESS / 2),
        x + length - CLOCK_SEGMENT_TIP - 1, y + CLOCK_SEGMENT_THICKNESS - 1);
}

static void bitmap_draw_vertical_segment(uint8_t *bitmap, int stride, int width, int height, int x, int y, int length)
{
    bitmap_fill_rect(bitmap, stride, width, height, x, y + CLOCK_SEGMENT_TIP, CLOCK_SEGMENT_THICKNESS, length - (CLOCK_SEGMENT_TIP * 2));
    bitmap_fill_triangle(
        bitmap, stride, width, height,
        x, y + CLOCK_SEGMENT_TIP,
        x + (CLOCK_SEGMENT_THICKNESS / 2), y,
        x + CLOCK_SEGMENT_THICKNESS - 1, y + CLOCK_SEGMENT_TIP);
    bitmap_fill_triangle(
        bitmap, stride, width, height,
        x, y + length - CLOCK_SEGMENT_TIP - 1,
        x + (CLOCK_SEGMENT_THICKNESS / 2), y + length - 1,
        x + CLOCK_SEGMENT_THICKNESS - 1, y + length - CLOCK_SEGMENT_TIP - 1);
}

static void bitmap_draw_digit_shape(uint8_t *bitmap, int stride, int value)
{
    uint8_t mask = (value >= 0 && value <= 9) ? digit_masks[value] : 0;

    if (mask & (1U << 0)) {
        bitmap_draw_horizontal_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, CLOCK_HORIZONTAL_X_OFFSET, 0, CLOCK_HORIZONTAL_LENGTH);
    }
    if (mask & (1U << 1)) {
        bitmap_draw_vertical_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, 0, CLOCK_VERTICAL_UPPER_Y, CLOCK_VERTICAL_LENGTH);
    }
    if (mask & (1U << 2)) {
        bitmap_draw_vertical_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, CLOCK_DIGIT_WIDTH - CLOCK_SEGMENT_THICKNESS, CLOCK_VERTICAL_UPPER_Y, CLOCK_VERTICAL_LENGTH);
    }
    if (mask & (1U << 3)) {
        bitmap_draw_horizontal_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, CLOCK_HORIZONTAL_X_OFFSET, CLOCK_MIDDLE_SEGMENT_Y, CLOCK_HORIZONTAL_LENGTH);
    }
    if (mask & (1U << 4)) {
        bitmap_draw_vertical_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, 0, CLOCK_VERTICAL_LOWER_Y, CLOCK_VERTICAL_LENGTH);
    }
    if (mask & (1U << 5)) {
        bitmap_draw_vertical_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, CLOCK_DIGIT_WIDTH - CLOCK_SEGMENT_THICKNESS, CLOCK_VERTICAL_LOWER_Y, CLOCK_VERTICAL_LENGTH);
    }
    if (mask & (1U << 6)) {
        bitmap_draw_horizontal_segment(bitmap, stride, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT, CLOCK_HORIZONTAL_X_OFFSET, CLOCK_BOTTOM_SEGMENT_Y, CLOCK_HORIZONTAL_LENGTH);
    }
}

static void bitmap_draw_colon_shape(uint8_t *bitmap, int stride)
{
    int center_x = CLOCK_COLON_BITMAP_WIDTH / 2;
    int half = CLOCK_COLON_SIZE / 2;
    int top_y = CLOCK_COLON_TOP_CENTER_Y;
    int bottom_y = CLOCK_COLON_BOTTOM_CENTER_Y;

    bitmap_fill_triangle(bitmap, stride, CLOCK_COLON_BITMAP_WIDTH, CLOCK_COLON_BITMAP_HEIGHT, center_x, top_y - half, center_x + half, top_y, center_x, top_y + half);
    bitmap_fill_triangle(bitmap, stride, CLOCK_COLON_BITMAP_WIDTH, CLOCK_COLON_BITMAP_HEIGHT, center_x, top_y - half, center_x - half, top_y, center_x, top_y + half);
    bitmap_fill_triangle(bitmap, stride, CLOCK_COLON_BITMAP_WIDTH, CLOCK_COLON_BITMAP_HEIGHT, center_x, bottom_y - half, center_x + half, bottom_y, center_x, bottom_y + half);
    bitmap_fill_triangle(bitmap, stride, CLOCK_COLON_BITMAP_WIDTH, CLOCK_COLON_BITMAP_HEIGHT, center_x, bottom_y - half, center_x - half, bottom_y, center_x, bottom_y + half);
}

static void bitmap_measure_visible_bounds(const uint8_t *bitmap, int stride, int width, int height, int *left, int *right)
{
    int min_x = width;
    int max_x = -1;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (!bitmap_get_pixel(bitmap, stride, x, y)) {
                continue;
            }

            if (x < min_x) {
                min_x = x;
            }
            if (x > max_x) {
                max_x = x;
            }
        }
    }

    if (max_x < min_x) {
        *left = 0;
        *right = width - 1;
        return;
    }

    *left = min_x;
    *right = max_x;
}

static void ensure_clock_bitmap_cache(void)
{
    if (clock_bitmap_cache_ready) {
        return;
    }

    memset(clock_digit_bitmaps, 0, sizeof(clock_digit_bitmaps));
    memset(clock_colon_bitmap, 0, sizeof(clock_colon_bitmap));

    for (int digit = 0; digit < 10; digit++) {
        bitmap_draw_digit_shape(clock_digit_bitmaps[digit], CLOCK_DIGIT_BITMAP_STRIDE, digit);
        bitmap_measure_visible_bounds(
            clock_digit_bitmaps[digit],
            CLOCK_DIGIT_BITMAP_STRIDE,
            CLOCK_DIGIT_WIDTH,
            CLOCK_DIGIT_HEIGHT,
            &clock_digit_visible_left[digit],
            &clock_digit_visible_right[digit]);
    }

    bitmap_draw_colon_shape(clock_colon_bitmap, CLOCK_COLON_BITMAP_STRIDE);
    clock_bitmap_cache_ready = true;
}

static void bitmap_blit(
    uint8_t *dst,
    int dst_stride,
    int dst_width,
    int dst_height,
    int dst_x,
    int dst_y,
    const uint8_t *src,
    int src_stride,
    int src_width,
    int src_height)
{
    for (int y = 0; y < src_height; y++) {
        int dst_row = dst_y + y;

        if (dst_row < 0 || dst_row >= dst_height) {
            continue;
        }

        for (int x = 0; x < src_width; x++) {
            int dst_col = dst_x + x;

            if (dst_col < 0 || dst_col >= dst_width) {
                continue;
            }

            if (bitmap_get_pixel(src, src_stride, x, y)) {
                bitmap_set_pixel(dst, dst_stride, dst_width, dst_height, dst_col, dst_row);
            }
        }
    }
}

static void render_clock_canvas(int hour, int minute)
{
    uint8_t *canvas_buf;
    uint8_t *canvas_pixels;
    int digits[4];
    int slot_x[4] = {CLOCK_DIGIT_X0, CLOCK_DIGIT_X1, CLOCK_DIGIT_X2, CLOCK_DIGIT_X3};

    if (clock_canvas == NULL) {
        return;
    }

    ensure_clock_bitmap_cache();

    canvas_buf = (uint8_t *)lv_canvas_get_buf(clock_canvas);
    canvas_pixels = canvas_buf + CLOCK_CANVAS_PALETTE_BYTES;
    memset(canvas_pixels, 0x00, CLOCK_CANVAS_STRIDE_BYTES * CLOCK_CANVAS_HEIGHT);

    digits[0] = (hour / 10) % 10;
    digits[1] = hour % 10;
    digits[2] = (minute / 10) % 10;
    digits[3] = minute % 10;

    for (int index = 0; index < 4; index++) {
        int digit = digits[index];
        int visible_width = clock_digit_visible_right[digit] - clock_digit_visible_left[digit] + 1;
        int centered_x = slot_x[index] + ((CLOCK_DIGIT_WIDTH - visible_width) / 2) - clock_digit_visible_left[digit];

        bitmap_blit(canvas_pixels, CLOCK_CANVAS_STRIDE_BYTES, CLOCK_CANVAS_WIDTH, CLOCK_CANVAS_HEIGHT,
            centered_x, CLOCK_DIGIT_Y,
            clock_digit_bitmaps[digit], CLOCK_DIGIT_BITMAP_STRIDE, CLOCK_DIGIT_WIDTH, CLOCK_DIGIT_HEIGHT);
    }
    bitmap_blit(canvas_pixels, CLOCK_CANVAS_STRIDE_BYTES, CLOCK_CANVAS_WIDTH, CLOCK_CANVAS_HEIGHT,
        CLOCK_COLON_CENTER_X - (CLOCK_COLON_BITMAP_WIDTH / 2), 0,
        clock_colon_bitmap, CLOCK_COLON_BITMAP_STRIDE, CLOCK_COLON_BITMAP_WIDTH, CLOCK_COLON_BITMAP_HEIGHT);

    lv_obj_invalidate(clock_canvas);
}

static void set_box_filled(lv_obj_t *obj, bool filled)
{
    lv_obj_set_style_bg_color(obj, filled ? UI_FG_COLOR : UI_BG_COLOR, 0);
    lv_obj_set_style_border_width(obj, filled ? 0 : 1, 0);
}

static void configure_mark_stroke(lv_obj_t *obj, int x, int y, int w, int h, int angle)
{
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_FG_COLOR, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_transform_pivot_x(obj, w / 2, 0);
    lv_obj_set_style_transform_pivot_y(obj, h / 2, 0);
    lv_obj_set_style_transform_rotation(obj, angle, 0);
}

static void configure_badge_box(lv_obj_t *obj, int x, int y, int size)
{
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_bg_color(obj, UI_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, UI_FG_COLOR, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 1, 0);
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
        if (width < 2) {
            width = 2;
        }
    }
    lv_obj_set_width(battery_fill, width);
    lv_obj_set_style_bg_color(battery_fill, width > 0 ? UI_FG_COLOR : UI_BG_COLOR, 0);
}

static void set_mqtt_level(bool connected)
{
    if (wifi_mqtt_badge == NULL || wifi_mqtt_mark_a == NULL || wifi_mqtt_mark_b == NULL) {
        return;
    }

    configure_badge_box(wifi_mqtt_badge, MQTT_BADGE_X, MQTT_BADGE_Y, MQTT_BADGE_SIZE);

    if (connected) {
        configure_mark_stroke(wifi_mqtt_mark_a, MQTT_BADGE_X + 2, MQTT_BADGE_Y + 8, 4, 2, 450);
        configure_mark_stroke(wifi_mqtt_mark_b, MQTT_BADGE_X + 4, MQTT_BADGE_Y + 6, 7, 2, -500);
    } else {
        configure_mark_stroke(wifi_mqtt_mark_a, MQTT_BADGE_X + 3, MQTT_BADGE_Y + 5, 7, 2, 450);
        configure_mark_stroke(wifi_mqtt_mark_b, MQTT_BADGE_X + 3, MQTT_BADGE_Y + 5, 7, 2, -450);
    }
}

void dashboard_ui_init(void)
{
    screen_obj = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_obj);
    lv_obj_set_style_bg_color(screen_obj, UI_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(screen_obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(screen_obj);

    clock_view = create_box(screen_obj, 0, 0, 400, 300, false);
    prov_view = create_box(screen_obj, 0, 0, 400, 300, false);

    temp_humi_label = create_label(clock_view, TEMP_LABEL_X, TOP_ROW_Y, TEMP_LABEL_WIDTH, LV_TEXT_ALIGN_LEFT);
    date_label = create_label(clock_view, DATE_LABEL_X, DATE_LABEL_Y, DATE_LABEL_WIDTH, LV_TEXT_ALIGN_CENTER);
    second_label = create_label(clock_view, SECOND_LABEL_X, SECOND_LABEL_Y, SECOND_LABEL_WIDTH, LV_TEXT_ALIGN_CENTER);
    status_label = create_label(screen_obj, 10, 280, 380, LV_TEXT_ALIGN_CENTER);

    LV_DRAW_BUF_INIT_STATIC(clock_canvas_draw_buf);
    clock_canvas = lv_canvas_create(clock_view);
    lv_canvas_set_draw_buf(clock_canvas, &clock_canvas_draw_buf);
    lv_canvas_set_palette(clock_canvas, 0, lv_color_to_32(UI_BG_COLOR, LV_OPA_COVER));
    lv_canvas_set_palette(clock_canvas, 1, lv_color_to_32(UI_FG_COLOR, LV_OPA_COVER));
    lv_obj_set_pos(clock_canvas, CLOCK_CANVAS_X, CLOCK_CANVAS_Y);
    lv_obj_clear_flag(clock_canvas, LV_OBJ_FLAG_SCROLLABLE);

    wifi_bars[0] = create_box(clock_view, WIFI_ICON_X, WIFI_BAR_0_Y, WIFI_BAR_WIDTH, WIFI_BAR_0_H, true);
    wifi_bars[1] = create_box(clock_view, WIFI_ICON_X + WIFI_BAR_STEP, WIFI_BAR_1_Y, WIFI_BAR_WIDTH, WIFI_BAR_1_H, true);
    wifi_bars[2] = create_box(clock_view, WIFI_ICON_X + (WIFI_BAR_STEP * 2), WIFI_BAR_2_Y, WIFI_BAR_WIDTH, WIFI_BAR_2_H, true);
    wifi_bars[3] = create_box(clock_view, WIFI_ICON_X + (WIFI_BAR_STEP * 3), WIFI_BAR_3_Y, WIFI_BAR_WIDTH, WIFI_BAR_3_H, true);

    wifi_mqtt_badge = create_box(clock_view, MQTT_BADGE_X, MQTT_BADGE_Y, MQTT_BADGE_SIZE, MQTT_BADGE_SIZE, true);
    wifi_mqtt_mark_a = create_box(clock_view, 0, 0, 1, 2, false);
    wifi_mqtt_mark_b = create_box(clock_view, 0, 0, 1, 2, false);

    lv_obj_t *battery_outline = create_box(clock_view, BATTERY_OUTLINE_X, BATTERY_OUTLINE_Y, BATTERY_OUTLINE_W, BATTERY_OUTLINE_H, true);
    (void)battery_outline;
    lv_obj_t *battery_cap = create_box(clock_view, BATTERY_CAP_X, BATTERY_CAP_Y, BATTERY_CAP_W, BATTERY_CAP_H, true);
    lv_obj_set_style_radius(battery_cap, 1, 0);
    (void)battery_cap;
    battery_fill = create_box(clock_view, BATTERY_FILL_X, BATTERY_FILL_Y, 0, BATTERY_FILL_H, false);

    prov_title_label = create_label(prov_view, 40, 42, 320, LV_TEXT_ALIGN_CENTER);
    prov_ssid_label = create_label(prov_view, 24, 104, 352, LV_TEXT_ALIGN_LEFT);
    prov_ip_label = create_label(prov_view, 24, 138, 352, LV_TEXT_ALIGN_LEFT);
    prov_hint_label = create_label(prov_view, 24, 188, 352, LV_TEXT_ALIGN_LEFT);

    message_view = create_box(screen_obj, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, false);
    message_title_bar = create_box(message_view, 0, 0, SCREEN_WIDTH, MESSAGE_TITLE_BAR_HEIGHT, false);
    lv_obj_set_style_bg_color(message_title_bar, UI_FG_COLOR, 0);
    lv_obj_set_style_bg_opa(message_title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(message_title_bar, 0, 0);
    message_title_label = create_label(message_title_bar, 8, MESSAGE_TITLE_LABEL_Y, 384, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(message_title_label, UI_BG_COLOR, 0);
    lv_obj_set_height(message_title_label, 16);
    lv_obj_set_style_transform_scale(message_title_label, MESSAGE_TITLE_SCALE, 0);
    lv_obj_set_style_transform_pivot_x(message_title_label, 192, 0);
    lv_obj_set_style_transform_pivot_y(message_title_label, 8, 0);
    message_content_view = create_box(message_view, 0, MESSAGE_TITLE_BAR_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - MESSAGE_TITLE_BAR_HEIGHT, false);
    lv_obj_set_style_radius(message_content_view, 0, 0);
    message_label = create_label(message_content_view, 20, 12, 360, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);

    lv_label_set_text(temp_humi_label, "--.-" CELSIUS_SYMBOL "  --%");
    lv_label_set_text(date_label, "----.--.--");
    lv_label_set_text(second_label, "SEC --");
    lv_label_set_text(status_label, "Booting...");
    lv_label_set_text(prov_title_label, "Provisioning Mode");
    lv_label_set_text(prov_hint_label, "Open 192.168.4.1 in browser\nAdd Wi-Fi credentials\nHold BOOT to exit.");
    last_clock_hour = -1;
    last_clock_minute = -1;

    dashboard_ui_update_battery(0);
    dashboard_ui_update_wifi_status(false, NULL, 0);
    dashboard_ui_update_mqtt_status(false);
    dashboard_ui_update_time(0, 0, 0);
    lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(message_view, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(prov_ssid_label, "AP SSID: --");
    lv_label_set_text(prov_ip_label, "Address: --");
}

void dashboard_ui_update_time(int hour, int minute, int second)
{
    if (last_clock_hour != hour || last_clock_minute != minute) {
        render_clock_canvas(hour, minute);
        last_clock_hour = hour;
        last_clock_minute = minute;
    }

    if (second_label == NULL) {
        return;
    }

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "SEC %02d", second);
    lv_label_set_text(second_label, buffer);
}

void dashboard_ui_update_date(int year, int month, int day, int week)
{
    if (date_label == NULL) {
        return;
    }

    LV_UNUSED(week);
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%04d.%02d.%02d", year, month, day);
    lv_label_set_text(date_label, buffer);
}

void dashboard_ui_update_temp_humi(float temp, float humi)
{
    if (temp_humi_label == NULL) {
        return;
    }

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%2.1f" CELSIUS_SYMBOL "  %2.0f%%", temp, humi);
    lv_label_set_text(temp_humi_label, buffer);
}

void dashboard_ui_update_wifi_status(bool connected, const char *ssid, int rssi)
{
    if (wifi_bars[0] == NULL) {
        return;
    }

    LV_UNUSED(ssid);
    int level = 0;

    if (connected) {
        if (rssi >= -55) {
            level = 4;
        } else if (rssi >= -67) {
            level = 3;
        } else if (rssi >= -75) {
            level = 2;
        } else {
            level = 1;
        }
    }

    set_wifi_level(connected, level);
}

void dashboard_ui_update_battery(int level)
{
    if (battery_fill == NULL) {
        return;
    }

    if (level < 0) {
        level = 0;
    }
    if (level > 100) {
        level = 100;
    }
    set_battery_level(level);
}

void dashboard_ui_update_mqtt_status(bool connected)
{
    set_mqtt_level(connected);
}

void dashboard_ui_set_provisioning(bool active, const char *ap_ssid, const char *ap_ip)
{
    if (clock_view == NULL || prov_view == NULL || prov_ssid_label == NULL || prov_ip_label == NULL) {
        return;
    }

    if (active) {
        lv_obj_add_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(prov_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
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

void dashboard_ui_set_status_message(const char *message)
{
    if (status_label == NULL) {
        return;
    }

    lv_label_set_text(status_label, (message != NULL) ? message : "");
}

void dashboard_ui_show_message_overlay(const char *title, const char *message)
{
    if (message_view == NULL || message_label == NULL || message_title_label == NULL) {
        return;
    }

    lv_label_set_text(message_title_label, (title != NULL) ? title : "");
    lv_label_set_text(message_label, (message != NULL) ? message : "");
    lv_obj_move_foreground(message_view);
    lv_obj_clear_flag(message_view, LV_OBJ_FLAG_HIDDEN);
}

void dashboard_ui_hide_message_overlay(void)
{
    if (message_view == NULL) {
        return;
    }

    lv_obj_add_flag(message_view, LV_OBJ_FLAG_HIDDEN);
}
