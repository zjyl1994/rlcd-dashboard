import time

import board

try:
    import esp32
except ImportError:
    esp32 = None

try:
    import machine
except ImportError:
    machine = None

try:
    from machine import I2C, Pin
except ImportError:
    I2C = None
    Pin = None

try:
    import network
except ImportError:
    network = None

try:
    import ntptime
except ImportError:
    ntptime = None

try:
    import _board_i2c
except ImportError:
    _board_i2c = None

try:
    import ujson as json
except ImportError:
    import json

try:
    import usocket as socket
except ImportError:
    import socket


NVS_NAMESPACE = "rlcd"
NVS_DEVICE_SUFFIX_KEY = "dev_suffix"
CONFIG_FILE = "/rlcd_config.json"
BOOT_HOLD_MS = 3000
CONNECT_TIMEOUT_MS = 15000
POLL_INTERVAL_MS = 200
SERVER_HOST = "0.0.0.0"
SERVER_PORT = 80
RTC_I2C_ADDR = 0x51
RTC_TIME_REG = 0x04
RTC_I2C_FREQ = 400000
DEFAULT_TIMEZONE_OFFSET_MINUTES = 480


_ntp_synced_this_boot = False


def _decode_text(value):
    if isinstance(value, bytes):
        try:
            return value.decode()
        except Exception:
            return ""
    if value is None:
        return ""
    return str(value)


def _normalize_network(entry):
    if not isinstance(entry, dict):
        return None

    ssid = _decode_text(entry.get("ssid")).strip()
    password = _decode_text(entry.get("password"))
    if not ssid:
        return None
    return {"ssid": ssid, "password": password}


def _dedupe_networks(networks):
    normalized = []
    for entry in networks:
        item = _normalize_network(entry)
        if item is None:
            continue

        replaced = False
        for index, current in enumerate(normalized):
            if current["ssid"] == item["ssid"]:
                normalized[index] = item
                replaced = True
                break

        if not replaced:
            normalized.append(item)

    return normalized


def _load_legacy_networks():
    networks = []
    for module_name in ("wifi_config", "secrets"):
        try:
            module = __import__(module_name)
        except ImportError:
            continue

        module_networks = getattr(module, "WIFI_NETWORKS", None)
        if isinstance(module_networks, list):
            networks.extend(module_networks)
            continue

        module_wifi = getattr(module, "WIFI", None)
        if isinstance(module_wifi, dict):
            networks.append(module_wifi)
            continue

        ssid = None
        for name in ("WIFI_SSID", "SSID", "ssid"):
            if hasattr(module, name):
                ssid = getattr(module, name)
                break

        password = ""
        for name in ("WIFI_PASSWORD", "PASSWORD", "password"):
            if hasattr(module, name):
                password = getattr(module, name)
                break

        if ssid:
            networks.append({"ssid": ssid, "password": password})

    return _dedupe_networks(networks)


def load_config():
    try:
        with open(CONFIG_FILE, "r") as handle:
            raw = handle.read().strip()
    except OSError:
        return {"wifi_networks": _load_legacy_networks()}

    if not raw:
        return {"wifi_networks": []}

    try:
        data = json.loads(raw)
    except Exception:
        return {"wifi_networks": []}

    if not isinstance(data, dict):
        return {"wifi_networks": []}

    data["wifi_networks"] = _dedupe_networks(data.get("wifi_networks", []))
    return data


def save_config(config):
    data = dict(config or {})
    data["wifi_networks"] = _dedupe_networks(data.get("wifi_networks", []))
    with open(CONFIG_FILE, "w") as handle:
        handle.write(json.dumps(data))


def load_wifi_networks():
    return load_config().get("wifi_networks", [])


def save_wifi_networks(networks):
    config = load_config()
    config["wifi_networks"] = _dedupe_networks(networks)
    save_config(config)


def _coerce_bool(value):
    if isinstance(value, bool):
        return value
    value = _decode_text(value).strip().lower()
    return value in ("1", "true", "yes", "on")


def _normalize_mqtt_config(config):
    data = dict(config or {})
    host = _decode_text(data.get("host")).strip()
    tls = _coerce_bool(data.get("tls"))

    if host.startswith("mqtts://"):
        host = host[8:]
        tls = True
    elif host.startswith("mqtt://"):
        host = host[7:]

    port = _coerce_int(data.get("port"))
    if port is None or port <= 0 or port > 65535:
        port = 8883 if tls else 1883

    return {
        "host": host,
        "port": port,
        "tls": tls,
        "username": _decode_text(data.get("username")),
        "password": _decode_text(data.get("password")),
        "client_id": _decode_text(data.get("client_id")).strip(),
        "keepalive": max(_coerce_int(data.get("keepalive"), 60) or 60, 15),
    }


def load_mqtt_config():
    config = load_config()
    return _normalize_mqtt_config(config.get("mqtt", {}))


def save_mqtt_config(mqtt_config):
    config = load_config()
    config["mqtt"] = _normalize_mqtt_config(mqtt_config)
    save_config(config)


def get_timezone_offset_minutes(default=DEFAULT_TIMEZONE_OFFSET_MINUTES):
    config = load_config()
    try:
        return int(config.get("timezone_offset_minutes", default))
    except Exception:
        return default


def set_timezone_offset_minutes(value):
    config = load_config()
    config["timezone_offset_minutes"] = int(value)
    save_config(config)


def _read_device_suffix():
    if esp32 is None:
        return None

    try:
        nvs = esp32.NVS(NVS_NAMESPACE)
        return nvs.get_i32(NVS_DEVICE_SUFFIX_KEY)
    except Exception:
        return None


def _write_device_suffix(value):
    if esp32 is None:
        return

    try:
        nvs = esp32.NVS(NVS_NAMESPACE)
        nvs.set_i32(NVS_DEVICE_SUFFIX_KEY, int(value))
        nvs.commit()
    except Exception:
        pass


def _random_suffix():
    try:
        import random

        return random.getrandbits(30) % 1000000
    except Exception:
        seed = time.ticks_ms()
        if machine is not None and hasattr(machine, "unique_id"):
            try:
                for part in machine.unique_id():
                    seed = ((seed * 131) ^ part) & 0x7FFFFFFF
            except Exception:
                pass
        return seed % 1000000


def get_device_name():
    suffix = _read_device_suffix()
    if suffix is None or suffix < 0 or suffix > 999999:
        suffix = _random_suffix()
        _write_device_suffix(suffix)
    return "RLCD-{:06d}".format(suffix)


def _station():
    if network is None:
        return None

    sta = network.WLAN(network.STA_IF)
    sta.active(True)

    device_name = get_device_name()
    for key in ("hostname", "dhcp_hostname"):
        try:
            sta.config(**{key: device_name})
            break
        except Exception:
            pass

    return sta


def _access_point():
    if network is None:
        return None
    return network.WLAN(network.AP_IF)


def is_boot_pressed():
    try:
        return board.init_boot().value() == 0
    except Exception:
        return False


def is_key_pressed():
    try:
        return board.init_key().value() == 0
    except Exception:
        return False


def boot_button_held(duration_ms=BOOT_HOLD_MS):
    if not is_boot_pressed():
        return False

    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < duration_ms:
        if not is_boot_pressed():
            return False
        time.sleep_ms(50)
    return is_boot_pressed()


def _dec_to_bcd(dec):
    return ((dec // 10) << 4) | (dec % 10)


def _coerce_int(value, default=None):
    try:
        return int(value)
    except Exception:
        return default


def _normalized_weekday(value):
    weekday = _coerce_int(value, 0)
    if weekday is None:
        return 0
    return weekday % 7


def _time_parts_from_localtime(local_time, timezone_offset_minutes=None):
    parts = {
        "year": int(local_time[0]),
        "month": int(local_time[1]),
        "day": int(local_time[2]),
        "hour": int(local_time[3]),
        "minute": int(local_time[4]),
        "second": int(local_time[5]),
        "weekday": _normalized_weekday(local_time[6] if len(local_time) > 6 else 0),
    }
    if timezone_offset_minutes is not None:
        parts["timezone_offset_minutes"] = int(timezone_offset_minutes)
    return parts


def _validate_time_parts(parts):
    year = _coerce_int(parts.get("year"))
    month = _coerce_int(parts.get("month"))
    day = _coerce_int(parts.get("day"))
    hour = _coerce_int(parts.get("hour"))
    minute = _coerce_int(parts.get("minute"))
    second = _coerce_int(parts.get("second"))
    weekday = _normalized_weekday(parts.get("weekday", 0))

    if year is None or year < 2000 or year > 2099:
        raise ValueError("year out of range")
    if month is None or month < 1 or month > 12:
        raise ValueError("month out of range")
    if day is None or day < 1 or day > 31:
        raise ValueError("day out of range")
    if hour is None or hour < 0 or hour > 23:
        raise ValueError("hour out of range")
    if minute is None or minute < 0 or minute > 59:
        raise ValueError("minute out of range")
    if second is None or second < 0 or second > 59:
        raise ValueError("second out of range")

    normalized = {
        "year": year,
        "month": month,
        "day": day,
        "hour": hour,
        "minute": minute,
        "second": second,
        "weekday": weekday,
    }
    if "timezone_offset_minutes" in parts:
        timezone_offset_minutes = _coerce_int(parts.get("timezone_offset_minutes"))
        if timezone_offset_minutes is not None:
            normalized["timezone_offset_minutes"] = timezone_offset_minutes
    return normalized


def _open_i2c_bus():
    if _board_i2c is not None:
        return _board_i2c, False

    if I2C is None or Pin is None:
        raise OSError("I2C unavailable")

    i2c = I2C(0, scl=Pin.board.I2C_SCL, sda=Pin.board.I2C_SDA, freq=RTC_I2C_FREQ)
    return i2c, True


def _write_rtc_register_block(register, payload):
    i2c, owns_bus = _open_i2c_bus()
    try:
        try:
            i2c.writeto_mem(RTC_I2C_ADDR, register, payload)
        except AttributeError:
            i2c.writeto(RTC_I2C_ADDR, bytes([register]) + payload)
    finally:
        if owns_bus:
            try:
                i2c.deinit()
            except Exception:
                pass


def _set_machine_rtc(parts):
    if machine is None or not hasattr(machine, "RTC"):
        return

    rtc = machine.RTC()
    data = (
        parts["year"],
        parts["month"],
        parts["day"],
        parts["weekday"],
        parts["hour"],
        parts["minute"],
        parts["second"],
        0,
    )

    try:
        rtc.datetime(data)
    except Exception:
        try:
            rtc.init(data)
        except Exception:
            pass


def _write_external_rtc(parts):
    rtc = None
    try:
        rtc = board.init_rtc()
        try:
            rtc.stop()
        except Exception:
            pass
    except Exception:
        rtc = None

    payload = bytes(
        (
            _dec_to_bcd(parts["second"]),
            _dec_to_bcd(parts["minute"]),
            _dec_to_bcd(parts["hour"]),
            _dec_to_bcd(parts["day"]),
            _dec_to_bcd(parts["weekday"]),
            _dec_to_bcd(parts["month"]),
            _dec_to_bcd(parts["year"] % 100),
        )
    )
    _write_rtc_register_block(RTC_TIME_REG, payload)

    if rtc is not None:
        try:
            rtc.start()
        except Exception:
            pass


def _format_time_parts(parts):
    return "{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}".format(**parts)


def set_device_time(parts, persist_timezone=False):
    normalized = _validate_time_parts(parts)
    _set_machine_rtc(normalized)
    _write_external_rtc(normalized)

    if persist_timezone and "timezone_offset_minutes" in normalized:
        set_timezone_offset_minutes(normalized["timezone_offset_minutes"])

    return {"ok": True, "time_text": _format_time_parts(normalized), "parts": normalized}


def sync_time_from_ntp_once(force=False):
    global _ntp_synced_this_boot

    if _ntp_synced_this_boot and not force:
        return {"ok": True, "skipped": True, "reason": "already synced"}

    if ntptime is None:
        return {"ok": False, "reason": "ntptime unavailable"}

    connection = current_connection()
    if not connection["connected"]:
        return {"ok": False, "reason": "wifi not connected"}

    try:
        ntptime.settime()
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}

    timezone_offset_minutes = get_timezone_offset_minutes()
    try:
        local_time = time.localtime(time.time() + (timezone_offset_minutes * 60))
    except Exception:
        local_time = time.localtime()

    try:
        result = set_device_time(
            _time_parts_from_localtime(local_time, timezone_offset_minutes=timezone_offset_minutes),
            persist_timezone=False,
        )
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}

    _ntp_synced_this_boot = True
    result["source"] = "ntp"
    return result


def sync_time_from_browser_form(values):
    try:
        parts = {
            "year": values.get("year"),
            "month": values.get("month"),
            "day": values.get("day"),
            "hour": values.get("hour"),
            "minute": values.get("minute"),
            "second": values.get("second"),
            "weekday": values.get("weekday", 0),
            "timezone_offset_minutes": values.get("timezone_offset_minutes", get_timezone_offset_minutes()),
        }
        result = set_device_time(parts, persist_timezone=True)
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}

    result["source"] = "browser"
    return result


def current_connection():
    sta = _station()
    if sta is None or not sta.isconnected():
        return {"connected": False, "ssid": "", "ip": ""}

    ssid = ""
    for key in ("ssid", "essid"):
        try:
            value = _decode_text(sta.config(key)).strip()
        except Exception:
            continue
        if value:
            ssid = value
            break

    try:
        ip = sta.ifconfig()[0]
    except Exception:
        ip = ""

    return {"connected": True, "ssid": ssid, "ip": ip}


def current_ip():
    return current_connection().get("ip", "")


def scan_networks():
    sta = _station()
    if sta is None:
        return []

    try:
        scanned = sta.scan()
    except Exception:
        return []

    results = []
    for item in scanned:
        ssid = _decode_text(item[0]).strip()
        if not ssid:
            continue
        rssi = item[3] if len(item) > 3 else -999

        existing = None
        for current in results:
            if current["ssid"] == ssid:
                existing = current
                break

        if existing is None:
            results.append({"ssid": ssid, "rssi": rssi})
        elif rssi > existing["rssi"]:
            existing["rssi"] = rssi

    results.sort(key=lambda entry: entry["rssi"], reverse=True)
    return results


def _connect_entry(sta, entry, timeout_ms):
    if sta is None:
        return {"ok": False, "ssid": entry["ssid"], "reason": "network unavailable"}

    current = current_connection()
    if current["connected"] and current.get("ssid") == entry["ssid"]:
        return {"ok": True, "ssid": current["ssid"], "ip": current["ip"]}

    try:
        sta.disconnect()
    except Exception:
        pass

    try:
        sta.connect(entry["ssid"], entry["password"])
    except Exception as exc:
        return {"ok": False, "ssid": entry["ssid"], "reason": str(exc)}

    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        if sta.isconnected():
            info = current_connection()
            return {"ok": True, "ssid": info["ssid"] or entry["ssid"], "ip": info["ip"]}
        time.sleep_ms(250)

    return {"ok": False, "ssid": entry["ssid"], "reason": "timeout"}


def connect_saved_networks(timeout_ms=CONNECT_TIMEOUT_MS):
    sta = _station()
    if sta is None:
        return {"ok": False, "reason": "network unavailable"}

    connected = current_connection()
    if connected["connected"]:
        return {"ok": True, "ssid": connected["ssid"], "ip": connected["ip"]}

    saved = load_wifi_networks()
    if not saved:
        return {"ok": False, "reason": "no saved networks"}

    scanned = scan_networks()
    rssi_by_ssid = {}
    for item in scanned:
        rssi_by_ssid[item["ssid"]] = item["rssi"]

    available = []
    fallback = []
    for entry in saved:
        if entry["ssid"] in rssi_by_ssid:
            available.append(entry)
        else:
            fallback.append(entry)

    available.sort(key=lambda entry: rssi_by_ssid.get(entry["ssid"], -999), reverse=True)
    ordered = available + fallback

    last_error = ""
    for entry in ordered:
        result = _connect_entry(sta, entry, timeout_ms)
        if result["ok"]:
            return result
        last_error = "{} ({})".format(entry["ssid"], result.get("reason", "failed"))

    return {"ok": False, "reason": last_error or "unable to connect"}


def start_access_point():
    ap = _access_point()
    if ap is None:
        return {"ok": False, "reason": "network unavailable"}

    ssid = get_device_name()
    ap.active(True)

    try:
        ap.config(essid=ssid)
    except Exception:
        pass

    try:
        ap.config(max_clients=4)
    except Exception:
        pass

    try:
        ip = ap.ifconfig()[0]
    except Exception:
        ip = "192.168.4.1"

    return {"ok": True, "ssid": ssid, "ip": ip, "ap": ap}


def stop_access_point(ap=None):
    if ap is None:
        ap = _access_point()
    if ap is None:
        return

    try:
        ap.active(False)
    except Exception:
        pass


def _url_decode(value):
    value = value.replace("+", " ")
    data = bytearray()
    index = 0
    while index < len(value):
        char = value[index]
        if char == "%" and index + 2 < len(value):
            try:
                data.append(int(value[index + 1 : index + 3], 16))
                index += 3
                continue
            except Exception:
                pass

        if ord(char) < 128:
            data.append(ord(char))
        else:
            data.extend(char.encode("utf-8"))
        index += 1

    try:
        return data.decode("utf-8")
    except Exception:
        return value


def _parse_form(body):
    values = {}
    for item in body.split("&"):
        if not item:
            continue
        if "=" in item:
            key, value = item.split("=", 1)
        else:
            key, value = item, ""
        values[_url_decode(key)] = _url_decode(value)
    return values


def _html_escape(value):
    value = _decode_text(value)
    return value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def _socket_sendall(client, payload):
    if isinstance(payload, str):
        payload = payload.encode("utf-8")

    try:
        client.sendall(payload)
        return
    except AttributeError:
        pass

    offset = 0
    total = len(payload)
    while offset < total:
        sent = client.send(payload[offset:])
        if sent is None:
            break
        if sent <= 0:
            raise OSError("socket send failed")
        offset += sent


def _http_response(client, status_code, body, content_type="text/html; charset=utf-8", extra_headers=None):
    reason = {
        200: "OK",
        302: "Found",
        400: "Bad Request",
        404: "Not Found",
        500: "Internal Server Error",
    }.get(status_code, "OK")

    if isinstance(body, str):
        body = body.encode("utf-8")

    headers = [
        "HTTP/1.1 {} {}".format(status_code, reason),
        "Content-Type: {}".format(content_type),
        "Content-Length: {}".format(len(body)),
        "Connection: close",
    ]
    for header in extra_headers or ():
        headers.append(header)

    _socket_sendall(client, ("\r\n".join(headers) + "\r\n\r\n").encode("utf-8"))
    _socket_sendall(client, body)


def _redirect(client, location="/"):
    _http_response(client, 302, b"", extra_headers=["Location: {}".format(location)])


def _read_request(client):
    data = b""
    while b"\r\n\r\n" not in data and len(data) < 8192:
        chunk = client.recv(512)
        if not chunk:
            break
        data += chunk

    if not data:
        return None

    marker = data.find(b"\r\n\r\n")
    if marker < 0:
        return None

    header_blob = data[:marker].decode("utf-8", "ignore")
    body = data[marker + 4 :]
    lines = header_blob.split("\r\n")
    request_line = lines[0].split()
    if len(request_line) < 2:
        return None

    headers = {}
    for line in lines[1:]:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        headers[key.strip().lower()] = value.strip()

    content_length = 0
    if "content-length" in headers:
        try:
            content_length = int(headers["content-length"])
        except Exception:
            content_length = 0

    while len(body) < content_length:
        chunk = client.recv(min(512, content_length - len(body)))
        if not chunk:
            break
        body += chunk

    path = request_line[1]
    if "?" in path:
        path, query = path.split("?", 1)
    else:
        query = ""

    return {
        "method": request_line[0].upper(),
        "path": path,
        "query": query,
        "headers": headers,
        "body": body.decode("utf-8", "ignore"),
    }


class ControlPanelSession:
    def __init__(self, ui=None):
        self.ui = ui
        self.device_name = get_device_name()
        self.ap = None
        self.ap_ip = ""
        self.server = None
        self.client_ip = ""
        self.message = ""
        self.error = ""
        self.should_exit = False
        self.exit_result = {"ok": False, "reason": "portal closed"}
        self._screen_signature = None
        self._last_status_ms = None
        self._boot_hold_started = None

    def run(self):
        started = start_access_point()
        if not started["ok"]:
            self.error = started.get("reason", "failed to start AP")
            self._refresh_ui(force=True)
            return {"ok": False, "reason": self.error}

        self.ap = started["ap"]
        self.ap_ip = started["ip"]

        try:
            self._open_server()
        except Exception as exc:
            stop_access_point(self.ap)
            self.error = str(exc)
            self._refresh_ui(force=True)
            return {"ok": False, "reason": self.error}

        self.message = "Control panel ready"
        self._refresh_ui(force=True)

        try:
            while not self.should_exit:
                self._refresh_ui()
                self._check_boot_toggle()
                if self.should_exit:
                    break
                try:
                    client, addr = self.server.accept()
                except OSError:
                    time.sleep_ms(POLL_INTERVAL_MS)
                    continue

                self.client_ip = addr[0] if addr else ""
                self._refresh_ui(force=True)
                self._handle_client(client)
                if self.should_exit:
                    time.sleep_ms(1200)
                    break
        finally:
            self._close_server()
            stop_access_point(self.ap)

        return self.exit_result

    def _open_server(self):
        server = socket.socket()
        try:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        except Exception:
            pass
        server.bind((SERVER_HOST, SERVER_PORT))
        server.listen(1)
        server.settimeout(1)
        self.server = server

    def _close_server(self):
        if self.server is None:
            return
        try:
            self.server.close()
        except Exception:
            pass
        self.server = None

    def _refresh_ui(self, force=False):
        if self.ui is None:
            return

        lines = [
            "AP: {}".format(self.device_name),
            "Portal: http://{}".format(self.ap_ip or "192.168.4.1"),
            "Saved Wi-Fi: {}".format(len(load_wifi_networks())),
            "Client: {}".format(self.client_ip or "waiting"),
            "Hold BOOT to close AP",
            self.error or self.message or "Open browser for settings",
        ]
        signature = tuple(lines)
        if force or signature != self._screen_signature:
            try:
                self.ui.show_panel("Web Control Panel", lines, refresh_status=False)
            except Exception:
                self.ui = None
                return
            self._screen_signature = signature

        now = time.ticks_ms()
        if force or self._last_status_ms is None or time.ticks_diff(now, self._last_status_ms) >= 5000:
            try:
                self.ui.refresh_status_bar()
            except Exception:
                self.ui = None
                return
            self._last_status_ms = now

        try:
            self.ui.poll()
        except Exception:
            self.ui = None

    def _check_boot_toggle(self):
        now = time.ticks_ms()
        if is_boot_pressed():
            if self._boot_hold_started is None:
                self._boot_hold_started = now
            elif time.ticks_diff(now, self._boot_hold_started) >= BOOT_HOLD_MS:
                self.message = "BOOT hold detected, closing AP"
                self.error = ""
                self.should_exit = True
                self.exit_result = {"ok": False, "reason": "boot toggle"}
        else:
            self._boot_hold_started = None

    def _handle_client(self, client):
        try:
            client.settimeout(2)
        except Exception:
            pass

        try:
            request = _read_request(client)
            if request is None:
                _http_response(client, 400, "Bad request")
                return
            self._dispatch(client, request)
        except Exception as exc:
            self.error = str(exc)
            _http_response(client, 500, self._render_notice_page("Request failed", self.error))
        finally:
            try:
                client.close()
            except Exception:
                pass

    def _dispatch(self, client, request):
        method = request["method"]
        path = request["path"]

        if method == "GET" and path == "/favicon.ico":
            _http_response(client, 404, b"", content_type="text/plain")
            return

        if method == "GET" and path == "/":
            if not self.message and not self.error:
                self.message = "Control panel ready"
            _http_response(client, 200, self._render_dashboard())
            return

        if method != "POST":
            _http_response(client, 404, self._render_notice_page("Not found", path))
            return

        form = _parse_form(request["body"])

        if path == "/wifi/save":
            self._save_network(form)
            _redirect(client)
            return

        if path == "/wifi/delete":
            self._delete_network(form)
            _redirect(client)
            return

        if path == "/wifi/connect":
            self._connect_saved_networks()
            _http_response(client, 200, self._render_connect_result())
            return

        if path == "/time/sync-browser":
            self._sync_browser_time(form)
            _redirect(client)
            return

        if path == "/mqtt/save":
            self._save_mqtt(form)
            _redirect(client)
            return

        if path == "/portal/exit":
            self.message = "Returning to dashboard"
            self.error = ""
            self.should_exit = True
            self.exit_result = {"ok": False, "reason": "portal exit"}
            _http_response(client, 200, self._render_notice_page("Returning", "Control panel is closing."))
            return

        _http_response(client, 404, self._render_notice_page("Not found", path))

    def _save_network(self, form):
        ssid = _decode_text(form.get("ssid")).strip()
        password = _decode_text(form.get("password"))
        if not ssid:
            self.error = "SSID is required"
            self.message = ""
            return

        networks = load_wifi_networks()
        updated = False
        for index, entry in enumerate(networks):
            if entry["ssid"] == ssid:
                networks[index] = {"ssid": ssid, "password": password}
                updated = True
                break

        if not updated:
            networks.append({"ssid": ssid, "password": password})

        save_wifi_networks(networks)
        self.error = ""
        self.message = "Saved {}".format(ssid)

    def _delete_network(self, form):
        ssid = _decode_text(form.get("ssid")).strip()
        networks = [entry for entry in load_wifi_networks() if entry["ssid"] != ssid]
        save_wifi_networks(networks)
        self.error = ""
        self.message = "Removed {}".format(ssid or "network")

    def _connect_saved_networks(self):
        self.error = ""
        result = connect_saved_networks()
        if result["ok"]:
            time_sync = sync_time_from_ntp_once()
            if time_sync["ok"] and not time_sync.get("skipped"):
                self.message = "Connected to {}, NTP synced".format(result["ssid"])
            elif time_sync["ok"]:
                self.message = "Connected to {}".format(result["ssid"])
            else:
                self.message = "Connected to {}, NTP sync skipped".format(result["ssid"])
            result["time_sync"] = time_sync
            self.should_exit = True
            self.exit_result = result
        else:
            self.message = ""
            self.error = result.get("reason", "Unable to connect")

    def _sync_browser_time(self, form):
        result = sync_time_from_browser_form(form)
        if result["ok"]:
            self.error = ""
            self.message = "Browser time synced: {}".format(result["time_text"])
        else:
            self.message = ""
            self.error = result.get("reason", "Time sync failed")

    def _save_mqtt(self, form):
        config = {
            "host": form.get("host", ""),
            "port": form.get("port", ""),
            "tls": form.get("tls", ""),
            "username": form.get("username", ""),
            "password": form.get("password", ""),
            "client_id": form.get("client_id", ""),
            "keepalive": form.get("keepalive", "60"),
        }
        save_mqtt_config(config)

        mqtt_config = load_mqtt_config()
        self.error = ""
        if mqtt_config["host"]:
            self.message = "Saved MQTT config for {}:{}".format(mqtt_config["host"], mqtt_config["port"])
        else:
            self.message = "MQTT disabled"

    def _render_notice_page(self, title, text):
        return """<!DOCTYPE html>
<html>
<head>
  <meta charset=\"utf-8\">
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
  <title>{title}</title>
  <style>
    body {{ font-family: Arial, sans-serif; background: #111827; color: #f9fafb; margin: 0; padding: 32px; }}
    .card {{ max-width: 560px; margin: 0 auto; background: #1f2937; border-radius: 16px; padding: 24px; }}
    a {{ color: #93c5fd; }}
  </style>
</head>
<body>
  <div class=\"card\">
    <h1>{title}</h1>
    <p>{text}</p>
    <p><a href=\"/\">Back to control panel</a></p>
  </div>
</body>
</html>
""".format(title=_html_escape(title), text=_html_escape(text))

    def _render_connect_result(self):
        if self.exit_result.get("ok"):
            time_sync = self.exit_result.get("time_sync") or {}
            time_line = ""
            if time_sync.get("ok") and not time_sync.get("skipped"):
                time_line = " Clock synced to {}.".format(time_sync.get("time_text", ""))
            elif time_sync.get("ok") and time_sync.get("skipped"):
                time_line = " Clock already synced this boot."
            elif time_sync:
                time_line = " NTP sync skipped: {}.".format(time_sync.get("reason", "unknown"))
            title = "Wi-Fi connected"
            text = "Connected to {} at {}.{} Returning to dashboard...".format(
                self.exit_result.get("ssid", "Wi-Fi"),
                self.exit_result.get("ip", ""),
                time_line,
            )
        else:
            title = "Connection failed"
            text = self.error or "Unable to connect to saved networks."
        return self._render_notice_page(title, text)

    def _render_dashboard(self):
        saved = load_wifi_networks()
        nearby = scan_networks()
        connection = current_connection()
        mqtt_config = load_mqtt_config()

        alert = ""
        if self.error:
            alert = "<div class=\"alert error\">{}</div>".format(_html_escape(self.error))
        elif self.message:
            alert = "<div class=\"alert ok\">{}</div>".format(_html_escape(self.message))

        saved_rows = []
        for entry in saved:
            password_hint = "Open network" if not entry["password"] else "Password saved"
            saved_rows.append(
                """<div class=\"saved-row\">\
<div>\
  <div class=\"ssid\">{ssid}</div>\
  <div class=\"hint\">{hint}</div>\
</div>\
<form method=\"post\" action=\"/wifi/delete\">\
  <input type=\"hidden\" name=\"ssid\" value=\"{value}\">\
  <button class=\"danger\" type=\"submit\">Delete</button>\
</form>\
</div>""".format(
                    ssid=_html_escape(entry["ssid"]),
                    hint=_html_escape(password_hint),
                    value=_html_escape(entry["ssid"]),
                )
            )

        if not saved_rows:
            saved_rows.append("<p class=\"empty\">No saved Wi-Fi yet.</p>")

        nearby_rows = []
        for item in nearby:
            nearby_rows.append(
                "<div class=\"scan-row\"><span>{}</span><span>{} dBm</span></div>".format(
                    _html_escape(item["ssid"]),
                    _html_escape(item["rssi"]),
                )
            )
        if not nearby_rows:
            nearby_rows.append("<p class=\"empty\">No nearby Wi-Fi detected.</p>")

        connection_text = "Not connected"
        if connection["connected"]:
            connection_text = "{} ({})".format(connection["ssid"] or "Wi-Fi", connection["ip"] or "no ip")

        options = []
        for item in nearby:
            options.append("<option value=\"{}\"></option>".format(_html_escape(item["ssid"])))

        timezone_offset_minutes = get_timezone_offset_minutes()
        mqtt_topic = "/rlcd/{}/message".format(self.device_name)
        mqtt_tls_checked = " checked" if mqtt_config.get("tls") else ""
        mqtt_keepalive = mqtt_config.get("keepalive") or 60
        mqtt_port = mqtt_config.get("port") or (8883 if mqtt_config.get("tls") else 1883)

        return """<!DOCTYPE html>
<html>
<head>
  <meta charset=\"utf-8\">
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
  <title>{device_name} Control Panel</title>
  <style>
    body {{ margin: 0; font-family: Arial, sans-serif; background: #0f172a; color: #e2e8f0; }}
    .shell {{ max-width: 980px; margin: 0 auto; padding: 24px; }}
    .hero {{ display: flex; flex-wrap: wrap; gap: 16px; align-items: center; justify-content: space-between; margin-bottom: 16px; }}
    .hero h1 {{ margin: 0; font-size: 28px; }}
    .hero p {{ margin: 6px 0 0; color: #94a3b8; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }}
    .card {{ background: #111827; border: 1px solid #1f2937; border-radius: 18px; padding: 18px; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2); }}
    .card h2 {{ margin: 0 0 14px; font-size: 18px; }}
    .stat {{ margin: 8px 0; color: #cbd5e1; }}
    .muted, .hint, .empty {{ color: #94a3b8; }}
    .saved-row, .scan-row {{ display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 10px 0; border-top: 1px solid #1f2937; }}
    .saved-row:first-of-type, .scan-row:first-of-type {{ border-top: 0; padding-top: 0; }}
    .ssid {{ font-weight: 700; }}
    form {{ margin: 0; }}
    input {{ width: 100%; box-sizing: border-box; padding: 10px 12px; margin: 8px 0 12px; border: 1px solid #334155; border-radius: 10px; background: #0f172a; color: #e2e8f0; }}
    input[type=checkbox] {{ width: auto; margin-right: 8px; }}
    button {{ border: 0; border-radius: 10px; padding: 10px 14px; background: #2563eb; color: #ffffff; font-weight: 700; }}
    button.secondary {{ background: #374151; }}
    button.danger {{ background: #991b1b; }}
    .actions {{ display: flex; flex-wrap: wrap; gap: 10px; }}
    .alert {{ border-radius: 12px; padding: 12px 14px; margin-bottom: 16px; }}
    .alert.ok {{ background: #052e16; color: #bbf7d0; }}
    .alert.error {{ background: #450a0a; color: #fecaca; }}
  </style>
  <script>
    function submitBrowserTime() {{
      var form = document.getElementById('browser-time-form');
      var now = new Date();
      form.elements.year.value = now.getFullYear();
      form.elements.month.value = now.getMonth() + 1;
      form.elements.day.value = now.getDate();
      form.elements.hour.value = now.getHours();
      form.elements.minute.value = now.getMinutes();
      form.elements.second.value = now.getSeconds();
      form.elements.weekday.value = (now.getDay() + 6) % 7;
      form.elements.timezone_offset_minutes.value = -now.getTimezoneOffset();
      form.submit();
    }}
  </script>
</head>
<body>
  <div class=\"shell\">
    <div class=\"hero\">
      <div>
        <h1>{device_name}</h1>
        <p>Unified web control panel for Wi-Fi, maintenance, and future tools.</p>
      </div>
      <div class=\"muted\">AP portal: http://{ap_ip}</div>
    </div>
    {alert}
    <div class=\"grid\">
      <section class=\"card\">
        <h2>Device</h2>
        <div class=\"stat\">AP Name: {device_name}</div>
        <div class=\"stat\">AP Address: {ap_ip}</div>
        <div class=\"stat\">Station: {connection}</div>
        <div class=\"stat\">Saved Wi-Fi: {saved_count}</div>
        <div class=\"stat\">Time Zone Offset: UTC{timezone_sign}{timezone_hours:02d}:{timezone_minutes:02d}</div>
      </section>
      <section class=\"card\">
        <h2>Actions</h2>
        <div class=\"actions\">
          <form method=\"post\" action=\"/wifi/connect\"><button type=\"submit\">Connect Saved Wi-Fi</button></form>
          <form id=\"browser-time-form\" method=\"post\" action=\"/time/sync-browser\">
            <input type=\"hidden\" name=\"year\">
            <input type=\"hidden\" name=\"month\">
            <input type=\"hidden\" name=\"day\">
            <input type=\"hidden\" name=\"hour\">
            <input type=\"hidden\" name=\"minute\">
            <input type=\"hidden\" name=\"second\">
            <input type=\"hidden\" name=\"weekday\">
            <input type=\"hidden\" name=\"timezone_offset_minutes\" value=\"{timezone_offset_minutes}\">
            <button class=\"secondary\" type=\"button\" onclick=\"submitBrowserTime()\">Sync Browser Time</button>
          </form>
          <form method=\"post\" action=\"/portal/exit\"><button class=\"secondary\" type=\"submit\">Return To Dashboard</button></form>
        </div>
      </section>
      <section class=\"card\">
        <h2>MQTT</h2>
        <div class=\"stat\">Topic: {mqtt_topic}</div>
        <div class=\"stat\">Save empty host to disable MQTT.</div>
        <form method=\"post\" action=\"/mqtt/save\">
          <label>Host</label>
          <input name=\"host\" placeholder=\"broker.example.com\" value=\"{mqtt_host}\">
          <label>Port</label>
          <input name=\"port\" placeholder=\"1883 or 8883\" value=\"{mqtt_port}\">
          <label>Username</label>
          <input name=\"username\" placeholder=\"Optional username\" value=\"{mqtt_username}\">
          <label>Password</label>
          <input name=\"password\" type=\"password\" placeholder=\"Optional password\" value=\"{mqtt_password}\">
          <label>Client ID</label>
          <input name=\"client_id\" placeholder=\"Defaults to device name\" value=\"{mqtt_client_id}\">
          <label>Keepalive Seconds</label>
          <input name=\"keepalive\" placeholder=\"60\" value=\"{mqtt_keepalive}\">
          <label><input name=\"tls\" type=\"checkbox\" value=\"1\"{mqtt_tls_checked}> Use TLS for MQTTS</label>
          <div class=\"actions\"><button type=\"submit\">Save MQTT</button></div>
        </form>
      </section>
      <section class=\"card\">
        <h2>Add Wi-Fi</h2>
        <form method=\"post\" action=\"/wifi/save\">
          <label>SSID</label>
          <input name=\"ssid\" list=\"nearby-ssids\" placeholder=\"Wi-Fi name\">
          <label>Password</label>
          <input name=\"password\" type=\"password\" placeholder=\"Password\">
          <button type=\"submit\">Save Network</button>
        </form>
        <datalist id=\"nearby-ssids\">{options}</datalist>
      </section>
      <section class=\"card\">
        <h2>Future Modules</h2>
        <p class=\"muted\">Reserve this panel for logs, calibration, device actions, diagnostics, and content management.</p>
      </section>
      <section class=\"card\">
        <h2>Saved Wi-Fi</h2>
        {saved_rows}
      </section>
      <section class=\"card\">
        <h2>Nearby Wi-Fi</h2>
        {nearby_rows}
      </section>
    </div>
  </div>
</body>
</html>
""".format(
            device_name=_html_escape(self.device_name),
            ap_ip=_html_escape(self.ap_ip or "192.168.4.1"),
            alert=alert,
            connection=_html_escape(connection_text),
            saved_count=_html_escape(len(saved)),
            timezone_offset_minutes=_html_escape(timezone_offset_minutes),
            timezone_sign="+" if timezone_offset_minutes >= 0 else "-",
            timezone_hours=abs(timezone_offset_minutes) // 60,
            timezone_minutes=abs(timezone_offset_minutes) % 60,
            mqtt_topic=_html_escape(mqtt_topic),
            mqtt_host=_html_escape(mqtt_config.get("host", "")),
            mqtt_port=_html_escape(mqtt_port),
            mqtt_username=_html_escape(mqtt_config.get("username", "")),
            mqtt_password=_html_escape(mqtt_config.get("password", "")),
            mqtt_client_id=_html_escape(mqtt_config.get("client_id", "")),
            mqtt_keepalive=_html_escape(mqtt_keepalive),
            mqtt_tls_checked=mqtt_tls_checked,
            saved_rows="".join(saved_rows),
            nearby_rows="".join(nearby_rows),
            options="".join(options),
        )


def run_configuration_portal(ui=None):
    return ControlPanelSession(ui).run()
