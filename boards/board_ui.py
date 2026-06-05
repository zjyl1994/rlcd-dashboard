import time

import board
import lvgl as lv

try:
    import network
except ImportError:
    network = None


SCREEN_WIDTH = 400
SCREEN_HEIGHT = 300
STATUS_BAR_HEIGHT = 24

STATUS_LEFT_X = 6
STATUS_RIGHT_X = 202
STATUS_TEXT_Y = 4
STATUS_LEFT_WIDTH = 190
STATUS_RIGHT_WIDTH = 192
STATUS_CENTER_X = 164
STATUS_CENTER_WIDTH = 72

CLOCK_TOP = 52
DIGIT_WIDTH = 72
DIGIT_HEIGHT = 120
DIGIT_THICKNESS = 10
DIGIT_GAP = 8
COLON_GAP = 12
COLON_WIDTH = 12
CLOCK_START_X = 30
DATE_Y = 190
STATUS_Y = 220
HINT_Y = 246

PANEL_TITLE_Y = 34
PANEL_BODY_Y = 64
PANEL_BODY_GAP = 24
PANEL_BODY_LINES = 8

MESSAGE_BODY_CHARS = 23
MESSAGE_BODY_LINES = 8
SINGLE_LINE_CHARS = 24

WEEKDAY_NAMES = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
SEGMENT_PATTERNS = {
    "0": (1, 1, 1, 1, 1, 1, 0),
    "1": (0, 1, 1, 0, 0, 0, 0),
    "2": (1, 1, 0, 1, 1, 0, 1),
    "3": (1, 1, 1, 1, 0, 0, 1),
    "4": (0, 1, 1, 0, 0, 1, 1),
    "5": (1, 0, 1, 1, 0, 1, 1),
    "6": (1, 0, 1, 1, 1, 1, 1),
    "7": (1, 1, 1, 0, 0, 0, 0),
    "8": (1, 1, 1, 1, 1, 1, 1),
    "9": (1, 1, 1, 1, 0, 1, 1),
    " ": (0, 0, 0, 0, 0, 0, 0),
}


_display = None

_home_screen = None
_home_left_label = None
_home_right_label = None
_home_digits = None
_home_colon = None
_home_date_label = None
_home_status_label = None
_home_hint_label = None

_panel_screen = None
_panel_left_label = None
_panel_right_label = None
_panel_title_label = None
_panel_body_labels = None

_message_screen = None
_message_left_label = None
_message_center_label = None
_message_right_label = None
_message_title_label = None
_message_body_label = None
_message_footer_label = None

_rtc = None
_sensor = None
_battery = None


def _color(value):
    return lv.color_hex(value)


def _make_screen(background=0xFFFFFF):
    screen = lv.obj()
    screen.remove_style_all()
    screen.set_size(SCREEN_WIDTH, SCREEN_HEIGHT)
    screen.set_style_bg_color(_color(background), 0)
    screen.set_style_bg_opa(lv.OPA.COVER, 0)
    return screen


def _make_box(parent, x, y, width, height, bg=0xFFFFFF):
    box = lv.obj(parent)
    box.remove_style_all()
    box.set_pos(x, y)
    box.set_size(width, height)
    box.set_style_bg_color(_color(bg), 0)
    box.set_style_bg_opa(lv.OPA.COVER, 0)
    box.set_style_border_width(0, 0)
    return box


def _make_frame(parent, x, y, width, height, border=0xFFFFFF, border_width=1):
    frame = lv.obj(parent)
    frame.remove_style_all()
    frame.set_pos(x, y)
    frame.set_size(width, height)
    frame.set_style_bg_opa(lv.OPA.TRANSP, 0)
    frame.set_style_border_color(_color(border), 0)
    frame.set_style_border_width(border_width, 0)
    return frame


def _make_label(parent, x, y, width, height, color=0x000000):
    label = lv.label(parent)
    label.set_pos(x, y)
    label.set_size(width, height)
    label.set_style_text_color(_color(color), 0)
    return label


def _set_text_align(label, align):
    try:
        label.set_style_text_align(align, 0)
    except Exception:
        pass


def _set_label_wrap(label):
    try:
        label.set_long_mode(lv.label.LONG.WRAP)
    except Exception:
        pass


def _decode_text(value):
    if isinstance(value, bytes):
        try:
            return value.decode()
        except Exception:
            return ""
    if value is None:
        return ""
    return str(value)


def _append_ellipsis(text):
    if text.endswith("..."):
        return text
    if len(text) >= 3:
        return text[:-3] + "..."
    return text + "..."


def _fit_single_line(value, limit=SINGLE_LINE_CHARS):
    text = _decode_text(value).replace("\r", " ").replace("\n", " ").strip()
    if len(text) <= limit:
        return text
    return _append_ellipsis(text[:limit])


def _truncate_wrapped_text(value, max_columns=MESSAGE_BODY_CHARS, max_lines=MESSAGE_BODY_LINES):
    text = _decode_text(value).replace("\r", "")
    if not text:
        return ""

    lines = []
    current = ""
    truncated = False

    for char in text:
        if char == "\n":
            lines.append(current.rstrip())
            if len(lines) >= max_lines:
                truncated = True
                break
            current = ""
            continue

        if len(current) >= max_columns:
            lines.append(current.rstrip())
            if len(lines) >= max_lines:
                truncated = True
                break
            current = char
            continue

        current += char

    if not truncated and (current or not lines):
        lines.append(current.rstrip())

    if len(lines) > max_lines:
        lines = lines[:max_lines]
        truncated = True

    if truncated and lines:
        lines[-1] = _append_ellipsis(lines[-1])

    return "\n".join(lines[:max_lines])


def _read_rtc():
    global _rtc
    if _rtc is None:
        _rtc = board.init_rtc()
        try:
            if not _rtc.running():
                _rtc.start()
        except Exception:
            pass
    return _rtc


def _read_sensor():
    global _sensor
    if _sensor is None:
        _sensor = board.init_shtc3()
    return _sensor


def _read_battery():
    global _battery
    if _battery is None:
        _battery = board.init_battery()
    return _battery


def _clock_snapshot():
    data = {}

    try:
        now = time.localtime()
        data.update(
            {
                "year": int(now[0]),
                "month": int(now[1]),
                "day": int(now[2]),
                "hour": int(now[3]),
                "minute": int(now[4]),
                "second": int(now[5]),
                "weekday": int(now[6]),
            }
        )
    except Exception:
        pass

    try:
        rtc_data = _read_rtc().read()
        if isinstance(rtc_data, dict):
            for key in ("year", "month", "day", "hour", "minute", "second", "weekday"):
                if key in rtc_data:
                    data[key] = int(rtc_data[key])
    except Exception:
        pass

    data.setdefault("year", 2000)
    data.setdefault("month", 1)
    data.setdefault("day", 1)
    data.setdefault("hour", 0)
    data.setdefault("minute", 0)
    data.setdefault("second", 0)
    data.setdefault("weekday", 0)
    return data


def _weekday_name(index):
    try:
        return WEEKDAY_NAMES[int(index) % len(WEEKDAY_NAMES)]
    except Exception:
        return ""


def _format_time_text(blink=False):
    data = _clock_snapshot()
    separator = ":"
    if blink and data.get("second", 0) % 2:
        separator = " "
    return "{hour:02d}{sep}{minute:02d}".format(sep=separator, **data)


def _format_date_text():
    data = _clock_snapshot()
    weekday = _weekday_name(data.get("weekday", 0))
    return "{year:04d}-{month:02d}-{day:02d} {weekday}".format(weekday=weekday, **data)


def _format_env_text():
    try:
        data = _read_sensor().read()
        return "{:.1f}C {:.1f}%".format(data["temperature"], data["humidity"])
    except Exception:
        return "--.-C --.-%"


def _format_battery_text():
    try:
        data = _read_battery().read()
        return "{:.2f}V {}%".format(data["voltage"], data["level"])
    except Exception:
        return "--.--V --%"


def _current_ssid():
    if network is None:
        return "NoWiFi"

    try:
        sta = network.WLAN(network.STA_IF)
        if not sta.isconnected():
            return "NoWiFi"

        for key in ("ssid", "essid"):
            try:
                value = sta.config(key)
            except Exception:
                continue

            value = _decode_text(value).strip()
            if value:
                return value
    except Exception:
        pass

    return "WiFi"


def _trim_text(value, limit):
    text = _decode_text(value)
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def _make_status_bar(screen, with_center=False):
    _make_box(screen, 0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT, bg=0x000000)
    left = _make_label(screen, STATUS_LEFT_X, STATUS_TEXT_Y, STATUS_LEFT_WIDTH, STATUS_BAR_HEIGHT - 8, color=0xFFFFFF)
    right = _make_label(screen, STATUS_RIGHT_X, STATUS_TEXT_Y, STATUS_RIGHT_WIDTH, STATUS_BAR_HEIGHT - 8, color=0xFFFFFF)
    _set_text_align(right, lv.TEXT_ALIGN.RIGHT)
    center = None
    if with_center:
        center = _make_label(screen, STATUS_CENTER_X, STATUS_TEXT_Y, STATUS_CENTER_WIDTH, STATUS_BAR_HEIGHT - 8, color=0xFFFFFF)
        _set_text_align(center, lv.TEXT_ALIGN.CENTER)
    return left, center, right


def _digit_x(index):
    positions = (
        CLOCK_START_X,
        CLOCK_START_X + DIGIT_WIDTH + DIGIT_GAP,
        CLOCK_START_X + (DIGIT_WIDTH * 2) + DIGIT_GAP + COLON_GAP + COLON_WIDTH + COLON_GAP,
        CLOCK_START_X + (DIGIT_WIDTH * 3) + (DIGIT_GAP * 2) + COLON_GAP + COLON_WIDTH + COLON_GAP,
    )
    return positions[index]


def _create_digit(parent, origin_x, origin_y):
    vertical_height = (DIGIT_HEIGHT - (DIGIT_THICKNESS * 3)) // 2
    mid_y = DIGIT_THICKNESS + vertical_height
    lower_y = mid_y + DIGIT_THICKNESS
    segments = []

    segments.append(_make_box(parent, origin_x + DIGIT_THICKNESS, origin_y, DIGIT_WIDTH - (DIGIT_THICKNESS * 2), DIGIT_THICKNESS, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x + DIGIT_WIDTH - DIGIT_THICKNESS, origin_y + DIGIT_THICKNESS, DIGIT_THICKNESS, vertical_height, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x + DIGIT_WIDTH - DIGIT_THICKNESS, origin_y + lower_y, DIGIT_THICKNESS, vertical_height, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x + DIGIT_THICKNESS, origin_y + DIGIT_HEIGHT - DIGIT_THICKNESS, DIGIT_WIDTH - (DIGIT_THICKNESS * 2), DIGIT_THICKNESS, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x, origin_y + lower_y, DIGIT_THICKNESS, vertical_height, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x, origin_y + DIGIT_THICKNESS, DIGIT_THICKNESS, vertical_height, bg=0xFFFFFF))
    segments.append(_make_box(parent, origin_x + DIGIT_THICKNESS, origin_y + mid_y, DIGIT_WIDTH - (DIGIT_THICKNESS * 2), DIGIT_THICKNESS, bg=0xFFFFFF))
    return segments


def _create_colon(parent):
    x = CLOCK_START_X + (DIGIT_WIDTH * 2) + DIGIT_GAP + COLON_GAP
    upper = _make_box(parent, x, CLOCK_TOP + 36, COLON_WIDTH, COLON_WIDTH, bg=0xFFFFFF)
    lower = _make_box(parent, x, CLOCK_TOP + 76, COLON_WIDTH, COLON_WIDTH, bg=0xFFFFFF)
    return (upper, lower)


def _set_segment_state(segment, enabled):
    color = 0x000000 if enabled else 0xFFFFFF
    segment.set_style_bg_color(_color(color), 0)


def _set_digit_value(digit_segments, value):
    pattern = SEGMENT_PATTERNS.get(value, SEGMENT_PATTERNS[" "])
    for index, enabled in enumerate(pattern):
        _set_segment_state(digit_segments[index], bool(enabled))


def _set_colon_state(colon_segments, enabled):
    for segment in colon_segments:
        _set_segment_state(segment, enabled)


def _ensure_home_screen():
    global _home_screen, _home_left_label, _home_right_label, _home_digits, _home_colon
    global _home_date_label, _home_status_label, _home_hint_label

    if _home_screen is not None:
        return _home_screen

    screen = _make_screen()
    left, _, right = _make_status_bar(screen)

    digits = []
    for index in range(4):
        digits.append(_create_digit(screen, _digit_x(index), CLOCK_TOP))
    colon = _create_colon(screen)

    date_label = _make_label(screen, 0, DATE_Y, SCREEN_WIDTH, 18, color=0x000000)
    status_label = _make_label(screen, 0, STATUS_Y, SCREEN_WIDTH, 18, color=0x000000)
    hint_label = _make_label(screen, 0, HINT_Y, SCREEN_WIDTH, 18, color=0x000000)

    _set_text_align(date_label, lv.TEXT_ALIGN.CENTER)
    _set_text_align(status_label, lv.TEXT_ALIGN.CENTER)
    _set_text_align(hint_label, lv.TEXT_ALIGN.CENTER)

    _home_screen = screen
    _home_left_label = left
    _home_right_label = right
    _home_digits = digits
    _home_colon = colon
    _home_date_label = date_label
    _home_status_label = status_label
    _home_hint_label = hint_label
    return screen


def _ensure_panel_screen():
    global _panel_screen, _panel_left_label, _panel_right_label, _panel_title_label, _panel_body_labels

    if _panel_screen is not None:
        return _panel_screen

    screen = _make_screen()
    left, _, right = _make_status_bar(screen)
    title_label = _make_label(screen, 8, PANEL_TITLE_Y, SCREEN_WIDTH - 16, 20, color=0x000000)

    body_labels = []
    y = PANEL_BODY_Y
    for _ in range(PANEL_BODY_LINES):
        body_labels.append(_make_label(screen, 8, y, SCREEN_WIDTH - 16, 18, color=0x000000))
        y += PANEL_BODY_GAP

    _panel_screen = screen
    _panel_left_label = left
    _panel_right_label = right
    _panel_title_label = title_label
    _panel_body_labels = body_labels
    return screen


def _ensure_message_screen():
    global _message_screen, _message_left_label, _message_center_label, _message_right_label
    global _message_title_label, _message_body_label, _message_footer_label

    if _message_screen is not None:
        return _message_screen

    screen = _make_screen(background=0x000000)
    left, center, right = _make_status_bar(screen, with_center=True)
    _make_frame(screen, 18, 46, SCREEN_WIDTH - 36, 202, border=0xFFFFFF, border_width=1)
    _make_box(screen, 28, 74, SCREEN_WIDTH - 56, 1, bg=0xFFFFFF)
    title_label = _make_label(screen, 24, 54, SCREEN_WIDTH - 48, 16, color=0xFFFFFF)
    body_label = _make_label(screen, 28, 88, SCREEN_WIDTH - 56, 144, color=0xFFFFFF)
    footer_label = _make_label(screen, 10, 264, SCREEN_WIDTH - 20, 18, color=0xFFFFFF)

    _set_text_align(title_label, lv.TEXT_ALIGN.CENTER)
    _set_text_align(footer_label, lv.TEXT_ALIGN.CENTER)
    _set_label_wrap(body_label)

    _message_screen = screen
    _message_left_label = left
    _message_center_label = center
    _message_right_label = right
    _message_title_label = title_label
    _message_body_label = body_label
    _message_footer_label = footer_label
    return screen


def ensure_display():
    global _display

    if _display is None:
        _display = board.init_display()

    _ensure_home_screen()
    _ensure_panel_screen()
    return _home_screen


def refresh_status_bar():
    ensure_display()
    left_text = _fit_single_line(_format_env_text(), limit=16)
    right_text = _fit_single_line("{} {}".format(_trim_text(_current_ssid(), 10), _format_battery_text()), limit=22)

    _home_left_label.set_text(left_text)
    _home_right_label.set_text(right_text)
    _panel_left_label.set_text(left_text)
    _panel_right_label.set_text(right_text)
    if _message_left_label is not None:
        _message_left_label.set_text(left_text)
    if _message_right_label is not None:
        _message_right_label.set_text(right_text)


def current_time_text(blink=False):
    return _format_time_text(blink=blink)


def current_date_text():
    return _format_date_text()


def _parse_clock_text(text):
    value = _decode_text(text)
    digits = []
    colon_on = False

    for char in value:
        if char.isdigit():
            digits.append(char)
        elif char == ":":
            colon_on = True

    while len(digits) < 4:
        digits.append("0")
    return digits[:4], colon_on


def show_clock(time_text=None, date_text=None, status_text="", hint_text="", refresh_status=True):
    screen = _ensure_home_screen()
    if refresh_status:
        refresh_status_bar()

    digits, colon_on = _parse_clock_text(time_text if time_text is not None else current_time_text(blink=True))
    for index, value in enumerate(digits):
        _set_digit_value(_home_digits[index], value)
    _set_colon_state(_home_colon, colon_on)

    _home_date_label.set_text(_fit_single_line(date_text if date_text is not None else current_date_text()))
    _home_status_label.set_text(_fit_single_line(status_text))
    _home_hint_label.set_text(_fit_single_line(hint_text))
    lv.screen_load(screen)


def show_panel(title, lines=None, refresh_status=True):
    screen = _ensure_panel_screen()
    if lines is None:
        lines = ()

    if refresh_status:
        refresh_status_bar()

    _panel_title_label.set_text(_fit_single_line(title))
    for index, label in enumerate(_panel_body_labels):
        if index < len(lines):
            label.set_text(_fit_single_line(lines[index]))
        else:
            label.set_text("")
    lv.screen_load(screen)


def show_fullscreen_message(title, message, footer=""):
    screen = _ensure_message_screen()
    _message_title_label.set_text(_fit_single_line(title))
    _message_body_label.set_text(_truncate_wrapped_text(message))
    _message_footer_label.set_text(_fit_single_line(footer))
    lv.screen_load(screen)


def set_message_counter(current, total):
    _ensure_message_screen()
    if _message_center_label is None:
        return
    _message_center_label.set_text("{}/{}".format(int(current), int(total)))


def poll():
    try:
        lv.timer_handler()
    except Exception:
        pass
