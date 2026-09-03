# MQTT Text Command Reference

## Topic

```text
/rlcd/{device_name}/message
```

## Payload

```json
{
  "text": "构建完成\n版本 v1.2.3",
  "beep": 1
}
```

## Fields

### `text`

| Value | Effect |
|---|---|
| string | Replace the device's cloud text. The text is shown in the full-width content area and automatically paged when it exceeds the visible area. |
| `""` | Clear the cloud text area. |
| omitted or `null` | Do not change the current cloud text. |

Text is UTF-8 and may contain explicit `\n` line breaks. The device also wraps text by rendered pixel width, so mixed Chinese, Latin, numbers and punctuation do not overflow the screen. The complete MQTT JSON payload is limited to 4096 bytes.

The device displays one page immediately after receiving the payload. When there are multiple pages, it advances automatically every 10 seconds and starts again from the first page whenever new `text` arrives. The text is cached in NVS and restored after reboot.

### `beep`

| Value | Effect |
|---|---|
| omitted or `0` | No notification sound. |
| `1` ... `6` | Play the corresponding built-in notification sound. |

`beep` is optional and does not affect the screen layout.

## Behaviour notes

- The screen always keeps the large clock, date, temperature/humidity, MQTT link icon, Wi-Fi signal icon and battery icon visible.
- The Agent traffic lights, `info` field and `message` field are no longer supported.
- `timeout`, `agent1` and `agent2` are ignored by the firmware and are not part of the supported payload.
- A key press no longer opens or recalls a previous message. The key remains available for provisioning and network actions.
- A payload with no `text` field is rejected by the HTTP server; MQTT payloads without a text update are ignored by the firmware.
