"""Board self-check for WAVESHARE_S3_RLCD_42.

Copy this file to the board and run:
    import driver_check
    driver_check.run()
"""

import os
import time

import board
import board_buttons as buttons_drv
import lvgl as lv
from machine import I2C, Pin

try:
    import _board_i2c
except ImportError:
    _board_i2c = None


I2C_FREQ = 400000
EXPECTED_I2C = {
    0x18: "ES8311 audio DAC",
    0x40: "ES7210 audio ADC",
    0x51: "PCF85063 RTC",
    0x70: "SHTC3",
}
SD_MOUNT = "/sd"
BUTTON_TIMEOUT_MS = 10000
DISPLAY_TIMEOUT_MS = 10000
AUDIO_TIMEOUT_MS = 10000
AUDIO_SAMPLE_RATE = 16000
AUDIO_CHUNK_MS = 250
AUDIO_CHUNKS = 6
AUDIO_RECORD_MS = AUDIO_CHUNK_MS * AUDIO_CHUNKS
STATUS_LINE_COUNT = 10


_display = None
_status_screen = None
_status_title = None
_status_labels = None
_transient_screen = None
_audio_capture = None
_audio_capture_nbytes = 0


def _log(message=""):
    print(message)


def _color(value):
    return lv.color_hex(value)


def _make_screen(background=0xFFFFFF):
    screen = lv.obj()
    screen.remove_style_all()
    screen.set_size(400, 300)
    screen.set_style_bg_color(_color(background), 0)
    screen.set_style_bg_opa(lv.OPA.COVER, 0)
    return screen


def _make_box(parent, x, y, width, height, bg=0xFFFFFF, border=0x000000, border_width=0):
    box = lv.obj(parent)
    box.remove_style_all()
    box.set_pos(x, y)
    box.set_size(width, height)
    box.set_style_bg_color(_color(bg), 0)
    box.set_style_bg_opa(lv.OPA.COVER, 0)
    box.set_style_border_color(_color(border), 0)
    box.set_style_border_width(border_width, 0)
    return box


def _make_label(parent, text, x, y, color=0x000000):
    label = lv.label(parent)
    label.set_text(str(text))
    label.set_pos(x, y)
    label.set_style_text_color(_color(color), 0)
    return label


def _set_display(display):
    global _display, _status_screen, _status_title, _status_labels, _transient_screen
    if _display is not display:
        _status_screen = None
        _status_title = None
        _status_labels = None
        _transient_screen = None
    _display = display


def _ensure_status_screen():
    global _status_screen, _status_title, _status_labels

    if _display is None:
        return None

    if _status_screen is None:
        screen = _make_screen()
        _make_box(screen, 0, 0, 400, 24, bg=0x000000, border=0x000000, border_width=0)
        _status_title = _make_label(screen, "", 8, 4, color=0xFFFFFF)

        labels = []
        y = 36
        for _ in range(STATUS_LINE_COUNT):
            labels.append(_make_label(screen, "", 8, y))
            y += 20

        _status_screen = screen
        _status_labels = labels

    return _status_screen


def _delete_screen(screen):
    if screen is None:
        return
    try:
        screen.delete()
    except Exception as exc:
        _log("Screen delete failed: {}".format(exc))


def _discard_transient_screen():
    global _transient_screen
    if _transient_screen is None:
        return
    screen = _transient_screen
    _transient_screen = None
    _delete_screen(screen)


def _load_transient_screen(screen):
    global _transient_screen
    _discard_transient_screen()
    _transient_screen = screen
    lv.screen_load(screen)


def _show_screen(title, lines=None):
    global _transient_screen
    if _display is None:
        return
    if lines is None:
        lines = ()

    try:
        old_transient = _transient_screen
        _transient_screen = None
        screen = _ensure_status_screen()
        _status_title.set_text(str(title)[:48])
        for index, label in enumerate(_status_labels):
            if index < len(lines):
                label.set_text(str(lines[index])[:48])
            else:
                label.set_text("")
        lv.screen_load(screen)
        if old_transient is not None and old_transient is not screen:
            _delete_screen(old_transient)
    except Exception as exc:
        _log("Display update failed: {}".format(exc))


def _boot_display():
    try:
        display = board.init_display()
        _set_display(display)
        _show_screen("Driver Check", ("LVGL ready", "Starting self-check...", "\u4e2d\u6587\u5b57\u4f53\u6d4b\u8bd5"))
    except Exception as exc:
        _log("Display init failed: {}".format(exc))


def _record_result(results, name, status, detail):
    results.append((name, status, detail))
    if detail:
        _log("[{0}] {1}: {2}".format(status, name, detail))
    else:
        _log("[{0}] {1}".format(status, name))

    if _display is not None:
        lines = (status, detail) if detail else (status,)
        _show_screen(name, lines)


def _pass(results, name, detail=""):
    _record_result(results, name, "PASS", detail)


def _fail(results, name, detail=""):
    _record_result(results, name, "FAIL", detail)


def _skip(results, name, detail=""):
    _record_result(results, name, "SKIP", detail)


def _wait_released(timeout_ms=2000):
    key_pin = board.init_key()
    boot_pin = board.init_boot()
    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        if key_pin.value() and boot_pin.value():
            return True
        time.sleep_ms(20)
    return False


def _button_pin(name):
    if name == "key":
        return board.init_key()
    if name == "boot":
        return board.init_boot()
    raise ValueError("unknown button {}".format(name))


def _wait_for_button(expected_name, prompt, timeout_ms):
    btns = board.init_buttons()
    btns.clear()
    pin = _button_pin(expected_name)
    _wait_released()
    _show_screen("Buttons", (prompt, "Press {}".format(expected_name.upper())))
    _log(prompt)

    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        event = btns.read()
        if event is not None:
            name, code = event
            if name == expected_name:
                _wait_released()
                return True, "{} {}".format(name, buttons_drv.event_name(code))
        if pin.value() == 0:
            _wait_released()
            return True, "{} raw_press".format(expected_name)
        time.sleep_ms(20)

    return False, "timeout waiting for {}".format(expected_name)


def _wait_for_yes_no(title, prompt, timeout_ms, show_prompt_on_display=True):
    key_pin = board.init_key()
    boot_pin = board.init_boot()
    _wait_released()
    if show_prompt_on_display:
        _show_screen(title, (prompt, "KEY=pass", "BOOT=fail"))
    _log(prompt)
    _log("Press KEY for pass, BOOT for fail.")

    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        if key_pin.value() == 0:
            _wait_released()
            return True, "key raw_press"
        if boot_pin.value() == 0:
            _wait_released()
            return False, "boot raw_press"
        time.sleep_ms(20)

    return None, "timeout"


def _pcm_peak(buf, length):
    peak = 0
    end = length - (length % 2)
    for offset in range(0, end, 2):
        value = buf[offset] | (buf[offset + 1] << 8)
        if value & 0x8000:
            value -= 65536
        if value < 0:
            value = -value
        if value > peak:
            peak = value
    return peak


def _normalize_audio_length(requested, result):
    if result is None:
        return 0
    if result == 0:
        return requested
    if result < 0:
        return 0
    if result > requested:
        return requested
    return result


def _format_hms(data):
    return "{hour:02d}:{minute:02d}:{second:02d}".format(**data)


def _store_audio_capture(chunks, nbytes):
    global _audio_capture, _audio_capture_nbytes
    _audio_capture = tuple(chunks)
    _audio_capture_nbytes = nbytes


def _clear_audio_capture():
    global _audio_capture, _audio_capture_nbytes
    _audio_capture = None
    _audio_capture_nbytes = 0


def _get_audio_capture():
    return _audio_capture, _audio_capture_nbytes


def _reset_cached_peripherals():
    _clear_audio_capture()
    for deinit in (board.deinit_audio, board.deinit_buttons, board.deinit_rtc, board.deinit_shtc3):
        try:
            deinit()
        except Exception as exc:
            _log("Peripheral reset failed: {}".format(exc))


def _record_audio_chunks(audio, sample_rate=AUDIO_SAMPLE_RATE, chunk_ms=AUDIO_CHUNK_MS, chunks=AUDIO_CHUNKS):
    chunk_nbytes = sample_rate * 4 * chunk_ms // 1000
    buf = bytearray(chunk_nbytes)
    captured = []
    total_nread = 0
    max_peak = 0

    for index in range(chunks):
        _show_screen("Audio In", ("Recording...", "Chunk {}/{}".format(index + 1, chunks), "\u6b63\u5728\u5f55\u97f3"))
        raw_nread = audio.readinto(buf)
        nread = _normalize_audio_length(chunk_nbytes, raw_nread)
        peak = _pcm_peak(buf, nread)
        if nread > 0:
            captured.append(bytes(memoryview(buf)[:nread]))
            total_nread += nread
        if peak > max_peak:
            max_peak = peak
        _log(
            "audio in chunk {}/{}: ret={}, bytes={}, peak={}".format(
                index + 1,
                chunks,
                raw_nread,
                nread,
                peak,
            )
        )

    return captured, total_nread, max_peak


def _play_audio_chunks(audio, chunks):
    total_written = 0

    for index, chunk in enumerate(chunks):
        raw_written = audio.play(chunk)
        written = _normalize_audio_length(len(chunk), raw_written)
        total_written += written
        _log(
            "audio out chunk {}/{}: ret={}, bytes={}".format(
                index + 1,
                len(chunks),
                raw_written,
                written,
            )
        )

    return total_written


def test_i2c(results):
    _log("\n== I2C bus ==")
    i2c = None
    try:
        _show_screen("I2C", ("Scanning bus...",))
        i2c = I2C(0, scl=Pin.board.I2C_SCL, sda=Pin.board.I2C_SDA, freq=I2C_FREQ)
        found = sorted(i2c.scan())

        missing = []
        for addr, name in EXPECTED_I2C.items():
            if addr not in found:
                missing.append(name)

        detail = "found {}".format(", ".join("0x{:02X}".format(addr) for addr in found) or "none")
        if missing:
            _fail(results, "I2C", "{}; missing {}".format(detail, ", ".join(missing)))
        else:
            _pass(results, "I2C", detail)
    except Exception as exc:
        _fail(results, "I2C", str(exc))
    finally:
        if i2c is not None:
            try:
                i2c.deinit()
            except Exception as exc:
                _log("I2C deinit failed: {}".format(exc))


def test_battery(results):
    _log("\n== Battery ==")
    try:
        _show_screen("Battery", ("Reading voltage and level...",))
        battery = board.init_battery()
        data = battery.read()
        voltage = data["voltage"]
        level = data["level"]
        raw = data["raw"]
        ok = raw > 0 and 0.0 < voltage < 5.5 and 0 <= level <= 100
        detail = "raw={raw}, voltage={voltage}V, level={level}%".format(**data)
        if ok:
            _pass(results, "Battery", detail)
        else:
            _fail(results, "Battery", detail)
    except Exception as exc:
        _fail(results, "Battery", str(exc))


def test_shtc3(results):
    _log("\n== SHTC3 ==")
    try:
        _show_screen("SHTC3", ("Reading temperature and humidity...",))
        sensor = board.init_shtc3()
        sensor_id = sensor.id()
        data = sensor.read()
        temp = data["temperature"]
        humi = data["humidity"]
        ok = sensor_id not in (0, 0xFFFF) and -50.0 <= temp <= 125.0 and 0.0 <= humi <= 100.0
        detail = "id=0x{:04X}, temp={}C, humidity={}%".format(sensor_id, temp, humi)
        if ok:
            _pass(results, "SHTC3", detail)
        else:
            _fail(results, "SHTC3", detail)
    except Exception as exc:
        _fail(results, "SHTC3", str(exc))


def test_rtc(results):
    _log("\n== RTC ==")
    try:
        _show_screen("RTC", ("Checking clock ticks...",))
        rtc = board.init_rtc()
        was_running = rtc.running()
        if not was_running:
            rtc.start()
            time.sleep_ms(100)

        before = rtc.read()
        rtc.stop()
        stopped = not rtc.running()
        rtc.start()
        restarted = rtc.running()
        time.sleep_ms(1200)
        after = rtc.read()

        if not was_running:
            rtc.stop()

        ticking = before != after
        detail = "running={}, stop={}, start={}, {} -> {}".format(
            was_running,
            stopped,
            restarted,
            _format_hms(before),
            _format_hms(after),
        )
        if stopped and restarted and ticking:
            _pass(results, "RTC", detail)
        else:
            _fail(results, "RTC", detail)
    except Exception as exc:
        _fail(results, "RTC", str(exc))


def test_buttons(results):
    _log("\n== Buttons ==")
    try:
        key_pin = board.init_key()
        boot_pin = board.init_boot()
        board.init_buttons()
        _wait_released()
        key_ok, key_detail = _wait_for_button("key", "Press KEY", BUTTON_TIMEOUT_MS)
        _wait_released()
        boot_ok, boot_detail = _wait_for_button("boot", "Press BOOT", BUTTON_TIMEOUT_MS)
        raw_detail = "raw key={}, boot={}".format(key_pin.value(), boot_pin.value())
        detail = "{}; {}; {}".format(raw_detail, key_detail, boot_detail)
        if key_ok and boot_ok:
            _pass(results, "Buttons", detail)
        else:
            _fail(results, "Buttons", detail)
    except Exception as exc:
        _fail(results, "Buttons", str(exc))


def test_display(results):
    _log("\n== LVGL display ==")
    try:
        _show_screen("LVGL display", ("Rendering test pattern...",))
        display = board.init_display()
        _set_display(display)
        _log("\u4e2d\u6587\u663e\u793a\u6d4b\u8bd5\uff1a\u4f60\u597d\uff0c\u4e16\u754c")

        screen = _make_screen()
        _make_box(screen, 16, 16, 368, 268, bg=0xFFFFFF, border=0x000000, border_width=2)
        _make_box(screen, 28, 32, 132, 52, bg=0xFFFFFF, border=0x000000, border_width=2)
        _make_label(screen, "LVGL TEST", 42, 46, color=0x000000)
        _make_box(screen, 284, 32, 76, 76, bg=0x000000)
        _make_box(screen, 312, 60, 20, 20, bg=0xFFFFFF)
        _make_box(screen, 40, 120, 320, 8, bg=0x000000)
        _make_box(screen, 196, 120, 8, 120, bg=0x000000)
        _make_label(screen, "400x300 / ST7305 / LVGL v9", 64, 150)
        _make_label(screen, "\u4e2d\u6587\u6d4b\u8bd5\uff1a\u4f60\u597d\uff0c\u4e16\u754c", 92, 178)
        _make_label(screen, "Confirm blocks and Chinese text", 50, 208)
        _load_transient_screen(screen)

        answer, detail = _wait_for_yes_no(
            "Display",
            "If pattern and Chinese text are visible, press KEY for font preview.",
            DISPLAY_TIMEOUT_MS,
            show_prompt_on_display=False,
        )
        if answer is not True:
            _fail(results, "LVGL Display", "pattern preview: {}".format(detail))
            return

        _show_screen("Font Preview", ("Rendering multilingual samples...",))
        font_screen = _make_screen()
        _make_box(font_screen, 0, 0, 400, 28, bg=0x000000)
        _make_label(font_screen, "Unifont 1bpp multilingual compare", 8, 6, color=0xFFFFFF)

        frame = _make_box(font_screen, 8, 36, 384, 196, bg=0xFFFFFF, border=0x000000, border_width=1)
        rows = (
            ("EN", "Hello 123 ABC xyz"),
            ("ZH", "\u4f60\u597d\uff0c\u4e16\u754c\uff1b\u5355\u8272\u70b9\u9635"),
            ("TW", "\u6f22\u5b57\u986f\u793a\u6548\u679c\u6e2c\u8a66"),
            ("JP", "\u3053\u3093\u306b\u3061\u306f \u6f22\u5b57\u30ab\u30ca"),
            ("KR", "\uc548\ub155\ud558\uc138\uc694 \ud55c\uae00"),
            ("RU", "\u041f\u0440\u0438\u0432\u0435\u0442, \u043c\u0438\u0440"),
            ("EL", "\u0393\u03b5\u03b9\u03b1 \u03c3\u03bf\u03c5 \u03ba\u03bf\u03c3\u03bc\u03b5"),
        )

        row_y = 8
        for index, (tag, text) in enumerate(rows):
            _make_label(frame, tag, 10, row_y)
            _make_label(frame, text, 60, row_y)
            row_y += 24
            if index != len(rows) - 1:
                _make_box(frame, 8, row_y - 4, 368, 1, bg=0x000000, border=0x000000, border_width=0)

        footer = _make_box(font_screen, 8, 240, 384, 52, bg=0x000000, border=0x000000, border_width=0)
        _make_label(footer, "\u53cd\u767d\u6d4b\u8bd5\uff1a\u9ed1\u5e95\u767d\u5b57 White on black", 8, 6, color=0xFFFFFF)
        _make_label(footer, "0123456789  \uff0c\u3002\uff01\uff1f  \u2190\u2191\u2192\u2193  \u25cb\u25cf \u25a1\u25a0", 8, 28, color=0xFFFFFF)
        _load_transient_screen(font_screen)

        answer, font_detail = _wait_for_yes_no(
            "Font Preview",
            "If multilingual text looks correct, press KEY.",
            DISPLAY_TIMEOUT_MS,
            show_prompt_on_display=False,
        )
        detail = "pattern ok; font preview: {}".format(font_detail)
        if answer is True:
            _pass(results, "LVGL Display", detail)
        else:
            _fail(results, "LVGL Display", detail)
    except Exception as exc:
        _fail(results, "LVGL Display", str(exc))


def test_audio_input(results):
    _log("\n== Audio input ==")
    try:
        _clear_audio_capture()
        audio = board.init_audio(sample_rate=AUDIO_SAMPLE_RATE)
        audio.gain(36)
        _show_screen("Audio In", ("Press KEY to record", "Then speak after release", "\u8bf7\u8bf4\uff1a\u4f60\u597d\uff0c\u4e16\u754c"))
        _log("Press KEY to start recording, then say: \u4f60\u597d\uff0c\u4e16\u754c")

        record_ok, start_detail = _wait_for_button("key", "Press KEY to start recording", AUDIO_TIMEOUT_MS)
        if not record_ok:
            _fail(results, "Audio In", start_detail)
            return

        _show_screen("Audio In", ("Recording...", "Speak now", "\u6b63\u5728\u5f55\u97f3"))
        _log("Recording now...")
        time.sleep_ms(200)

        captured, nread, peak = _record_audio_chunks(audio)
        detail = "start={}; chunks={}, read={} bytes, peak={}".format(start_detail, len(captured), nread, peak)
        if nread > 0:
            _store_audio_capture(captured, nread)
            _pass(results, "Audio In", detail)
        else:
            _fail(results, "Audio In", detail)
    except Exception as exc:
        _fail(results, "Audio In", str(exc))


def test_audio_output(results):
    _log("\n== Audio output ==")
    try:
        capture, capture_nbytes = _get_audio_capture()
        if not capture or capture_nbytes <= 0:
            _skip(results, "Audio Out", "no recorded audio available")
            return

        _show_screen("Audio Out", ("Playing recorded audio...", "\u64ad\u653e\u521a\u624d\u7684\u5f55\u97f3"))
        audio = board.init_audio(sample_rate=AUDIO_SAMPLE_RATE)
        audio.volume(80)
        written = _play_audio_chunks(audio, capture)
        answer, detail = _wait_for_yes_no(
            "Audio Out",
            "If you heard the recorded audio, press KEY.",
            AUDIO_TIMEOUT_MS,
        )
        detail = "chunks={}, write={} bytes; {}".format(len(capture), written, detail)
        if answer is True and written == capture_nbytes:
            _pass(results, "Audio Out", detail)
        elif answer is False:
            _fail(results, "Audio Out", detail)
        else:
            _fail(results, "Audio Out", detail)
    except Exception as exc:
        _fail(results, "Audio Out", str(exc))


def _bcd_to_dec(bcd):
    return ((bcd >> 4) * 10) + (bcd & 0x0F)


def _dec_to_bcd(dec):
    return ((dec // 10) << 4) | (dec % 10)


def _shtc3_crc8(data):
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x31) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def _shtc3_py_read():
    _board_i2c.writeto(0x70, b"\x35\x17")
    time.sleep_us(300)
    _board_i2c.writeto(0x70, b"\x78\x66")
    time.sleep_ms(20)
    data = _board_i2c.readfrom(0x70, 6)
    if _shtc3_crc8(data[:2]) != data[2] or _shtc3_crc8(data[3:5]) != data[5]:
        raise ValueError("SHTC3 CRC error")
    raw_t = (data[0] << 8) | data[1]
    raw_h = (data[3] << 8) | data[4]
    temp = round(175.0 * raw_t / 65536.0 - 45.0 - 4.0, 2)
    humi = round(100.0 * raw_h / 65536.0, 2)
    _board_i2c.writeto(0x70, b"\xB0\x98")
    return temp, humi


def _pcf85063_py_read():
    data = bytearray(7)
    _board_i2c.readfrom_mem_into(0x51, 0x04, data)
    return {
        "year": _bcd_to_dec(data[6]) + 2000,
        "month": _bcd_to_dec(data[5] & 0x1F),
        "day": _bcd_to_dec(data[3] & 0x3F),
        "hour": _bcd_to_dec(data[2] & 0x3F),
        "minute": _bcd_to_dec(data[1] & 0x7F),
        "second": _bcd_to_dec(data[0] & 0x7F),
    }


def test_i2c_python(results):
    _log("\n== I2C Python ==")
    if _board_i2c is None:
        _skip(results, "I2C Python", "_board_i2c module not available")
        return

    try:
        _show_screen("I2C Python", ("Scan bus, then", "py-SHTC3 and py-RTC..."))
        found = sorted(_board_i2c.scan())
        scan_detail = "scan {}".format(", ".join("0x{:02X}".format(a) for a in found))
        missing = [n for a, n in EXPECTED_I2C.items() if a not in found]
        if missing:
            _fail(results, "I2C Python", "{}; missing {}".format(scan_detail, ", ".join(missing)))
            return
        _pass(results, "I2C scan", scan_detail)

        # Python SHTC3 via _board_i2c
        try:
            temp, humi = _shtc3_py_read()
            _pass(results, "I2C py-SHTC3", "temp={}C, humidity={}%".format(temp, humi))
        except Exception as exc:
            _fail(results, "I2C py-SHTC3", str(exc))

        # Python PCF85063 via _board_i2c
        try:
            dt = _pcf85063_py_read()
            time_str = "{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}".format(**dt)
            _pass(results, "I2C py-RTC", time_str)
        except Exception as exc:
            _fail(results, "I2C py-RTC", str(exc))
    except Exception as exc:
        _fail(results, "I2C Python", str(exc))


def test_sd(results):
    _log("\n== SD card ==")
    test_path = SD_MOUNT + "/driver_check.txt"
    try:
        _show_screen("SD card", ("Testing read/write...",))
        board.init_sd(SD_MOUNT)
    except Exception as exc:
        _skip(results, "SD", "mount failed: {}".format(exc))
        return

    try:
        payload = "driver_check:{}\n".format(time.ticks_ms())
        with open(test_path, "w") as handle:
            handle.write(payload)
        with open(test_path, "r") as handle:
            read_back = handle.read()
        if read_back == payload:
            _pass(results, "SD", "read/write ok at {}".format(test_path))
        else:
            _fail(results, "SD", "readback mismatch")
    except Exception as exc:
        _fail(results, "SD", str(exc))
    finally:
        try:
            os.remove(test_path)
        except Exception:
            pass
        try:
            board.deinit_sd()
        except Exception:
            pass


def _show_summary(results):
    passed = 0
    failed = 0
    skipped = 0
    for _, status, _ in results:
        if status == "PASS":
            passed += 1
        elif status == "FAIL":
            failed += 1
        else:
            skipped += 1

    _log("\n== Summary ==")
    _log("PASS={}, FAIL={}, SKIP={}".format(passed, failed, skipped))
    for name, status, detail in results:
        if detail:
            _log("- {}: {} ({})".format(name, status, detail))
        else:
            _log("- {}: {}".format(name, status))

    _show_screen(
        "Driver Check",
        (
            "PASS={} FAIL={} SKIP={}".format(passed, failed, skipped),
            "See REPL for details.",
        ),
    )

    return {
        "pass": passed,
        "fail": failed,
        "skip": skipped,
        "results": results,
    }


def run():
    results = []
    _reset_cached_peripherals()
    _boot_display()
    _log("WAVESHARE_S3_RLCD_42 driver check")
    _log("Keep the board powered, and prepare an SD card if you want to test SD.")
    test_display(results)
    test_battery(results)
    test_shtc3(results)
    test_rtc(results)
    test_buttons(results)
    test_audio_input(results)
    test_audio_output(results)
    board.deinit_audio()
    test_sd(results)
    test_i2c_python(results)
    summary = _show_summary(results)
    _reset_cached_peripherals()
    return summary


if __name__ == "__main__":
    run()