# MQTT Control Commands Reference

## Topic

```
/rlcd/{device_name}/message
```

---

## Payload

```json
{
    "message": "...",
    "kv": { ... },
    "timeout": 10,
    "beep": 1
}
```

---

## Fields

### `message`

| Value | Effect |
|-------|--------|
| `"任何文本"` | 在屏幕中间显示 overlay 文本，最长 512 字节 |
| 不传或 `null` | 不显示 overlay，不清除已有显示 |

### `kv`

| Value | Effect |
|-------|--------|
| `{ "key1": "val1", "key2": 42 }` | 更新屏幕上的 KV 面板，逐行显示 `key: value` |
| 不传或 `null` | 不更新 KV 面板，不清除已有内容 |

`value` 支持的类型：
- `string` → 直接显示
- `number` → 转换为字符串（整数无小数点，浮点保留 1 位小数）

### `timeout`

| Value | Effect |
|-------|--------|
| `0` | 无限期显示 overlay（需手动/后续消息清除） |
| `1` ~ `180` | overlay 在 N 秒后自动消失 |
| 不传 | 默认 10 秒 |
| `<0` 或 `>180` | payload 被拒绝，消息不生效 |

### `beep`

| Value | Effect |
|-------|--------|
| 不传 | 无提示音 |
| `false` / `0` | 无提示音 |
| `true` / `1` | 播放提示音 1 |
| `2` | 播放提示音 2 |
| `3` | 播放提示音 3 |
| `4` | 播放提示音 4 |
| `5` | 播放提示音 5 |
| `6` | 播放提示音 6 |
| 其他数值 | 无提示音 |

---

## Behaviour Notes

- `message` 和 `kv` 可同时传入，两者独立生效
- 设备重启后会自动恢复上一次的 `message` 和 `kv` 显示
- 收到新消息会覆盖之前的显示（包括无限期显示的 overlay）
