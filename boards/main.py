import time

import board_ui
import device_portal
import mqtt_service


STATUS_REFRESH_INTERVAL_MS = 5000
CLOCK_REFRESH_INTERVAL_MS = 1000
POLL_INTERVAL_MS = 100


def _clock_status_text(mqtt):
    return mqtt.status_line()


def _clock_hint_text():
    return "Hold BOOT for settings"


def _show_clock(mqtt):
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
        footer = "KEY read"
        if current < total:
            footer += " next"
        board_ui.show_fullscreen_message(
            "Message {}/{}".format(current, total),
            message_state["queue"][message_state["index"]],
            footer=footer,
        )
        board_ui.set_message_counter(current, total)

    def _show_mqtt_message(topic, payload):
        del topic
        message_state["queue"].append(payload or "(empty)")
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

    last_status_refresh = None
    last_clock_refresh = None
    hold_started = None

    while True:
        mqtt.poll()
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
