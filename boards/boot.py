import device_portal


def _load_ui():
    try:
        import board_ui

        board_ui.ensure_display()
        return board_ui
    except Exception as exc:
        print("Display init failed: {}".format(exc))
        return None


def _startup():
    device_portal.get_device_name()
    ui = None

    if device_portal.boot_button_held():
        ui = _load_ui()
        device_portal.run_configuration_portal(ui)

    if ui is not None:
        try:
            ui.show_panel(
                "Resuming Startup",
                (
                    "Closing access point...",
                    "Connecting saved Wi-Fi...",
                ),
                refresh_status=False,
            )
            ui.refresh_status_bar()
            ui.poll()
        except Exception:
            pass

    result = device_portal.connect_saved_networks()
    if result.get("ok"):
        device_portal.sync_time_from_ntp_once()


_startup()
