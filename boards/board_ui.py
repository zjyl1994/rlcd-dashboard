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
LEFT_X = 6
RIGHT_X = 202
TEXT_Y = 4
LEFT_WIDTH = 190
RIGHT_WIDTH = 192
TITLE_Y = 34
BODY_Y = 64
BODY_GAP = 24
BODY_LINES = 8


_display = None
_screen = None
_left_label = None
_right_label = None
_title_label = None
_body_labels = None
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


def _make_label(parent, x, y, width, height, color=0x000000):
    label = lv.label(parent)
    label.set_pos(x, y)
    label.set_size(width, height)
    label.set_style_text_color(_color(color), 0)
    return label


def ensure_display():
    global _display, _screen, _left_label, _right_label, _title_label, _body_labels

    if _screen is not None:
        return _screen

    _display = board.init_display()
    screen = _make_screen()
    _make_box(screen, 0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT, bg=0x000000)

    left_label = _make_label(screen, LEFT_X, TEXT_Y, LEFT_WIDTH, STATUS_BAR_HEIGHT - 8, color=0xFFFFFF)
    right_label = _make_label(screen, RIGHT_X, TEXT_Y, RIGHT_WIDTH, STATUS_BAR_HEIGHT - 8, color=0xFFFFFF)
    title_label = _make_label(screen, 8, TITLE_Y, SCREEN_WIDTH - 16, 22, color=0x000000)

    try:
        right_label.set_style_text_align(lv.TEXT_ALIGN.RIGHT, 0)
    except Exception:
        pass

    body_labels = []
    y = BODY_Y
    for _ in range(BODY_LINES):
        body_labels.append(_make_label(screen, 8, y, SCREEN_WIDTH - 16, 20, color=0x000000))
        y += BODY_GAP

    _screen = screen
    _left_label = left_label
    _right_label = right_label
    _title_label = title_label
    _body_labels = body_labels
    lv.screen_load(screen)
    return screen


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


def _format_time_text():
    try:
        data = _read_rtc().read()
        return "{hour:02d}:{minute:02d}".format(**data)
    except Exception:
        now = time.localtime()
        return "{:02d}:{:02d}".format(now[3], now[4])


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


def _decode_text(value):
    if isinstance(value, bytes):
        try:
            return value.decode()
        except Exception:
            return ""
    if value is None:
        return ""
    return str(value)


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
    if len(value) <= limit:
        return value
    return value[: limit - 3] + "..."


def refresh_status_bar():
    ensure_display()
    left_text = "{} {}".format(_format_time_text(), _format_env_text())
    right_text = "{} {}".format(_trim_text(_current_ssid(), 12), _format_battery_text())
    _left_label.set_text(left_text)
    _right_label.set_text(right_text)


def show_panel(title, lines=None, refresh_status=True):
    ensure_display()
    if lines is None:
        lines = ()

    if refresh_status:
        refresh_status_bar()

    _title_label.set_text(str(title)[:48])
    for index, label in enumerate(_body_labels):
        if index < len(lines):
            label.set_text(str(lines[index])[:52])
        else:
            label.set_text("")


def poll():
    try:
        lv.timer_handler()
    except Exception:
        pass
