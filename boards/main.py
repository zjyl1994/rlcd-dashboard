import time

import board
import board_ui
import device_portal
import mqtt_service

try:
    import ujson as json
except ImportError:
    import json


STATUS_REFRESH_INTERVAL_MS = 5000
CLOCK_REFRESH_INTERVAL_MS = 1000
POLL_INTERVAL_MS = 100
NOTIFY_SAMPLE_RATE = 16000


_notify_chunks = None


def _to_text(value):
    if isinstance(value, bytes):
        try:
            return value.decode("utf-8")
        except Exception:
            try:
                return value.decode()
            except Exception:
                return str(value)
    if value is None:
        return ""
    return str(value)


def _parse_message_payload(payload):
    raw_text = _to_text(payload)
    title = "Message"
    content = raw_text or "(empty)"
    bell = 0

    try:
        data = json.loads(raw_text)
    except Exception:
        return {"title": title, "content": content, "bell": bell}

    if not isinstance(data, dict):
        return {"title": title, "content": content, "bell": bell}

    title = _to_text(data.get("title") or "Message")
    content = _to_text(data.get("content") or "") or "(empty)"
    try:
        bell = int(data.get("bell", 0))
    except Exception:
        bell = 0

    return {"title": title, "content": content, "bell": bell}


def _make_tone_chunk(freq_hz, duration_ms, amplitude=5000):
    sample_count = (NOTIFY_SAMPLE_RATE * duration_ms) // 1000
    half_period = max(1, NOTIFY_SAMPLE_RATE // (freq_hz * 2))
    data = bytearray(sample_count * 4)
    value = amplitude
    step = 0
    offset = 0

    for _ in range(sample_count):
        step += 1
        if step >= half_period:
            step = 0
            value = -value

        pcm = value
        if pcm < 0:
            pcm += 65536

        low = pcm & 0xFF
        high = (pcm >> 8) & 0xFF
        data[offset] = low
        data[offset + 1] = high
        data[offset + 2] = low
        data[offset + 3] = high
        offset += 4

    return bytes(data)


def _notification_sound_chunks():
    global _notify_chunks
    if _notify_chunks is None:
        silence = bytes((NOTIFY_SAMPLE_RATE * 4 * 30) // 1000)
        _notify_chunks = (
            _make_tone_chunk(1568, 60),
            silence,
            _make_tone_chunk(2093, 100),
        )
    return _notify_chunks


def _play_notification_sound():
    try:
        audio = board.init_audio(sample_rate=NOTIFY_SAMPLE_RATE)
        try:
            audio.volume(65)
        except Exception:
            pass
        for chunk in _notification_sound_chunks():
            audio.play(chunk)
    except Exception:
        return
    finally:
        try:
            board.deinit_audio()
        except Exception:
            pass


def _clock_status_text(mqtt):
    return mqtt.status_line()


def _clock_hint_text():
    return "Hold BOOT for settings"


def _show_clock(mqtt):
    board_ui.set_mqtt_connected(mqtt.is_connected())
    board_ui.show_clock(
        time_text=board_ui.current_time_text(blink=True),
        date_text=board_ui.current_date_text(),
        status_text=_clock_status_text(mqtt),
        hint_text=_clock_hint_text(),
        refresh_status=False,
    )


def _resume_after_portal(mqtt):
    board_ui.show_panel(
        "Resuming Dashboard",
        (
            "Closing access point...",
            "Connecting saved Wi-Fi...",
        ),
        refresh_status=False,
    )
    board_ui.refresh_status_bar()
    board_ui.poll()
    result = device_portal.connect_saved_networks()
    if result.get("ok"):
        device_portal.sync_time_from_ntp_once()
    mqtt.reset()
    mqtt.ensure_connected(force=True)
    board_ui.set_mqtt_connected(mqtt.is_connected())


def run():
    mqtt = mqtt_service.MQTTService()
    message_state = {"active": False, "queue": [], "index": 0}

    def _show_current_message():
        if not message_state["queue"] or message_state["index"] >= len(message_state["queue"]):
            message_state["queue"] = []
            message_state["index"] = 0
            message_state["active"] = False
            return

        message_state["active"] = True
        current = message_state["index"] + 1
        total = len(message_state["queue"])
        current_message = message_state["queue"][message_state["index"]]
        footer = "KEY read"
        if current < total:
            footer += " next"
        board_ui.show_fullscreen_message(
            current_message["title"],
            current_message["content"],
            footer=footer,
        )
        board_ui.set_message_counter(current, total)

    def _show_mqtt_message(topic, payload):
        del topic
        message = _parse_message_payload(payload)
        message_state["queue"].append(message)
        if message.get("bell") == 1:
            _play_notification_sound()
        if not message_state["active"]:
            message_state["index"] = 0
        _show_current_message()

    try:
        board_ui.ensure_display()
    except Exception as exc:
        print("Display init failed: {}".format(exc))
        return

    mqtt.set_message_callback(_show_mqtt_message)
    mqtt.ensure_connected(force=True)
    board_ui.set_mqtt_connected(mqtt.is_connected())

    last_status_refresh = None
    last_clock_refresh = None
    hold_started = None

    while True:
        mqtt.poll()
        board_ui.set_mqtt_connected(mqtt.is_connected())
        now = time.ticks_ms()

        if message_state["active"] and device_portal.is_key_pressed():
            while device_portal.is_key_pressed():
                time.sleep_ms(50)
            if message_state["queue"] and message_state["index"] < len(message_state["queue"]) - 1:
                message_state["index"] += 1
                _show_current_message()
            else:
                message_state["queue"] = []
                message_state["index"] = 0
                message_state["active"] = False
                last_status_refresh = None
                last_clock_refresh = None
            continue

        if not message_state["active"]:
            if last_clock_refresh is None or time.ticks_diff(now, last_clock_refresh) >= CLOCK_REFRESH_INTERVAL_MS:
                _show_clock(mqtt)
                last_clock_refresh = now

        if last_status_refresh is None or time.ticks_diff(now, last_status_refresh) >= STATUS_REFRESH_INTERVAL_MS:
            board_ui.refresh_status_bar()
            last_status_refresh = now

        if device_portal.is_boot_pressed():
            if hold_started is None:
                hold_started = now
            elif time.ticks_diff(now, hold_started) >= device_portal.BOOT_HOLD_MS:
                mqtt.disconnect("portal open")
                board_ui.set_mqtt_connected(False)
                board_ui.show_panel(
                    "Web Control Panel",
                    (
                        "Starting access point...",
                        "Opening settings portal...",
                    ),
                    refresh_status=False,
                )
                board_ui.refresh_status_bar()
                device_portal.run_configuration_portal(board_ui)
                _resume_after_portal(mqtt)
                if message_state["queue"]:
                    _show_current_message()
                else:
                    message_state["index"] = 0
                    message_state["active"] = False
                while device_portal.is_boot_pressed():
                    time.sleep_ms(50)
                hold_started = None
                last_status_refresh = None
                last_clock_refresh = None
                continue
        else:
            hold_started = None

        board_ui.poll()
        time.sleep_ms(POLL_INTERVAL_MS)


if __name__ == "__main__":
    run()
