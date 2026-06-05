import time

import board_ui
import device_portal


REFRESH_INTERVAL_MS = 5000
POLL_INTERVAL_MS = 100


def _home_lines(device_name):
    connection = device_portal.current_connection()
    if connection["connected"]:
        network_line = "Connected: {}".format(connection["ssid"] or "Wi-Fi")
        ip_line = "IP: {}".format(connection["ip"] or "pending")
    else:
        network_line = "Connected: NoWiFi"
        ip_line = "IP: waiting"

    return (
        "Device: {}".format(device_name),
        network_line,
        ip_line,
        "Saved Wi-Fi: {}".format(len(device_portal.load_wifi_networks())),
        "Hold BOOT to toggle Config AP",
    )


def _refresh_home(device_name):
    board_ui.show_panel("RLCD Dashboard", _home_lines(device_name), refresh_status=False)
    board_ui.refresh_status_bar()


def _resume_after_portal():
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


def run():
    device_name = device_portal.get_device_name()
    try:
        board_ui.ensure_display()
    except Exception as exc:
        print("Display init failed: {}".format(exc))
        return

    last_refresh = None
    hold_started = None

    while True:
        now = time.ticks_ms()
        if last_refresh is None or time.ticks_diff(now, last_refresh) >= REFRESH_INTERVAL_MS:
            _refresh_home(device_name)
            last_refresh = now

        if device_portal.is_boot_pressed():
            if hold_started is None:
                hold_started = now
            elif time.ticks_diff(now, hold_started) >= device_portal.BOOT_HOLD_MS:
                board_ui.show_panel(
                    "Web Control Panel",
                    (
                        "Starting access point...",
                        "Device: {}".format(device_name),
                    ),
                    refresh_status=False,
                )
                board_ui.refresh_status_bar()
                device_portal.run_configuration_portal(board_ui)
                _resume_after_portal()
                while device_portal.is_boot_pressed():
                    time.sleep_ms(50)
                hold_started = None
                last_refresh = None
                continue
        else:
            hold_started = None

        board_ui.poll()
        time.sleep_ms(POLL_INTERVAL_MS)


if __name__ == "__main__":
    run()
