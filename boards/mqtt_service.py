import time

import device_portal

try:
    from umqtt.simple import MQTTClient
except ImportError:
    MQTTClient = None


RECONNECT_INTERVAL_MS = 5000
PING_INTERVAL_MS = 20000


def _decode_text(value):
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


def _trim_text(value, limit=36):
    if len(value) <= limit:
        return value
    return value[: limit - 3] + "..."


def _now_ms():
    return time.ticks_ms()


def _would_block(exc):
    code = None
    if getattr(exc, "args", None):
        code = exc.args[0]
    return code in (11, 35, 110, 115, 116)


class MQTTService:
    def __init__(self):
        self._client = None
        self._config_signature = None
        self._config = None
        self._connected = False
        self._topic = None
        self._message_callback = None
        self._last_error = ""
        self._last_attempt_ms = None
        self._last_ping_ms = None
        self._last_message_ms = None

    def set_message_callback(self, callback):
        self._message_callback = callback

    def is_connected(self):
        return self._connected and self._client is not None

    def reset(self):
        self.disconnect()
        self._config_signature = None
        self._config = None

    def disconnect(self, reason=""):
        if self._client is not None:
            try:
                self._client.disconnect()
            except Exception:
                pass
        self._client = None
        self._connected = False
        self._topic = None
        self._last_ping_ms = None
        if reason:
            self._last_error = _decode_text(reason)

    def subscription_topic(self):
        return "/rlcd/{}/message".format(device_portal.get_device_name())

    def status_line(self):
        config = self._config or device_portal.load_mqtt_config()
        if not config.get("host"):
            return "MQTT: Disabled"
        if MQTTClient is None:
            return "MQTT: Library Missing"
        if self._connected:
            return "MQTT: Connected"
        if not device_portal.current_connection()["connected"]:
            return "MQTT: Waiting Wi-Fi"
        if self._last_error:
            return "MQTT: {}".format(_trim_text(self._last_error, 28))
        return "MQTT: Connecting"

    def detail_line(self):
        config = self._config or device_portal.load_mqtt_config()
        if not config.get("host"):
            return "Broker: not configured"
        detail = "Broker: {}:{}".format(config["host"], config["port"])
        if config.get("tls"):
            detail += " TLS"
        return _trim_text(detail, 48)

    def _config_tuple(self, config):
        return (
            config.get("host", ""),
            int(config.get("port", 0) or 0),
            bool(config.get("tls")),
            config.get("username", ""),
            config.get("password", ""),
            config.get("client_id", ""),
            int(config.get("keepalive", 0) or 0),
        )

    def _refresh_config(self):
        config = device_portal.load_mqtt_config()
        signature = self._config_tuple(config)
        if signature != self._config_signature:
            self.disconnect()
            self._config = config
            self._config_signature = signature
            self._last_error = ""
        elif self._config is None:
            self._config = config
            self._config_signature = signature
        return self._config

    def ensure_connected(self, force=False):
        config = self._refresh_config()

        if not config.get("host"):
            self.disconnect()
            return False

        if MQTTClient is None:
            self._last_error = "umqtt.simple unavailable"
            return False

        connection = device_portal.current_connection()
        if not connection["connected"]:
            self.disconnect("wifi disconnected")
            return False

        if self._connected and self._client is not None:
            return True

        now = _now_ms()
        if not force and self._last_attempt_ms is not None:
            if time.ticks_diff(now, self._last_attempt_ms) < RECONNECT_INTERVAL_MS:
                return False
        self._last_attempt_ms = now

        client_id = (config.get("client_id") or device_portal.get_device_name()).encode("utf-8")
        username = config.get("username") or None
        password = config.get("password") or None
        if username is not None:
            username = username.encode("utf-8")
        if password is not None:
            password = password.encode("utf-8")
        ssl_enabled = bool(config.get("tls"))
        ssl_params = {}
        if ssl_enabled:
            ssl_params = {"server_hostname": config["host"]}

        try:
            client = self._connect_client(
                client_id,
                config,
                username,
                password,
                ssl_enabled,
                ssl_params,
            )
            topic = self.subscription_topic()
        except Exception as exc:
            self.disconnect(str(exc))
            return False

        self._client = client
        self._connected = True
        self._topic = topic
        self._last_error = ""
        self._last_ping_ms = now
        return True

    def _connect_client(self, client_id, config, username, password, ssl_enabled, ssl_params):
        client = MQTTClient(
            client_id,
            config["host"],
            port=int(config["port"]),
            user=username,
            password=password,
            keepalive=int(config.get("keepalive") or 60),
            ssl=ssl_enabled,
            ssl_params=ssl_params,
        )
        client.set_callback(self._on_message)

        try:
            client.connect()
            client.subscribe(self.subscription_topic().encode("utf-8"))
            return client
        except Exception:
            try:
                client.disconnect()
            except Exception:
                pass

            if not ssl_enabled or not ssl_params:
                raise

        client = MQTTClient(
            client_id,
            config["host"],
            port=int(config["port"]),
            user=username,
            password=password,
            keepalive=int(config.get("keepalive") or 60),
            ssl=ssl_enabled,
            ssl_params={},
        )
        client.set_callback(self._on_message)
        client.connect()
        client.subscribe(self.subscription_topic().encode("utf-8"))
        return client

    def _on_message(self, topic, payload):
        self._last_message_ms = _now_ms()
        if self._message_callback is None:
            return

        topic_text = _decode_text(topic)
        payload_text = _decode_text(payload)
        self._message_callback(topic_text, payload_text)

    def poll(self):
        config = self._refresh_config()
        if not config.get("host"):
            self.disconnect()
            return

        if not device_portal.current_connection()["connected"]:
            self.disconnect("wifi disconnected")
            return

        if not self._connected or self._client is None:
            self.ensure_connected()
            return

        now = _now_ms()
        if self._last_ping_ms is not None and time.ticks_diff(now, self._last_ping_ms) >= PING_INTERVAL_MS:
            try:
                self._client.ping()
                self._last_ping_ms = now
            except Exception as exc:
                self.disconnect(str(exc))
                return

        try:
            self._client.check_msg()
        except OSError as exc:
            if _would_block(exc):
                return
            self.disconnect(str(exc))
        except Exception as exc:
            self.disconnect(str(exc))
