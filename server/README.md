# RLCD Dashboard Server API

本文档描述 server 提供给外部程序调用的 HTTP 上报接口。server 接收云端文本后，通过 MQTT 转发给 RLCD 设备。

```text
外部程序 ──HTTP POST──> RLCD server ──MQTT──> RLCD 设备
```

## 1. 接口

| 项目 | 内容 |
| --- | --- |
| 方法 | `POST` |
| 路径 | `/api/info/report` |
| Content-Type | `application/json` |
| 成功响应 | `200 OK`，JSON `{"ok":true}` |

完整 URL 为 `http://{server_host}:{listen_port}/api/info/report`，默认示例端口为 `7523`。

当配置中的 `api_key` 不为空时，需要通过 `X-Api-Key: <api_key>` 请求头鉴权，也兼容 `api_key` 查询参数。

## 2. 请求体

请求体必须包含 `text` 字段；空字符串表示清除屏幕文本。`beep` 可选，仅用于播放设备提示音。

| 字段 | 类型 | 必填 | 取值 | 说明 |
| --- | --- | --- | --- | --- |
| `text` | string | 是 | 任意 UTF-8 文本 | 替换设备全宽云端文本区的内容；支持 `\n`，超出显示区域后自动翻页；空字符串清除内容 |
| `beep` | integer | 否 | `0`～`6` | `0` 不播放，`1`～`6` 播放对应内置提示音 |

设备会按实际像素宽度自动换行，适合中文、英文、数字和标点混合内容。新文本从第一页开始显示；多页内容每 10 秒自动翻页。文本会保存到 NVS，设备重启后恢复最近一次内容。

单次 MQTT JSON payload 最大为 `4096` 字节。未知字段会被忽略；`info`、`message`、`timeout`、`agents`、`agent1`、`agent2` 不再属于支持的协议。

## 3. 示例

```bash
curl -X POST 'http://127.0.0.1:7523/api/info/report' \
  -H 'Content-Type: application/json' \
  -H 'X-Api-Key: your-api-key' \
  -d '{"text":"构建完成\n版本 v1.2.3","beep":1}'
```

Python 调用：

```python
import requests

response = requests.post(
    "http://127.0.0.1:7523/api/info/report",
    headers={
        "Content-Type": "application/json",
        "X-Api-Key": "your-api-key",
    },
    json={"text": "任务完成\n请检查设备"},
    timeout=15,
)
response.raise_for_status()
print(response.json())  # {"ok": True}
```

## 4. 响应和错误

成功表示 server 已接受并发布 MQTT 消息，不代表设备已经完成渲染。

错误响应统一为：

```json
{"error":"错误描述"}
```

- `400`：JSON 格式、字段类型错误，或缺少 `text`。
- `401`：API Key 缺失或错误。
- `500`：MQTT 未连接、topic 未配置、发布超时或 payload 超限。

## 5. Server 配置

server 默认读取当前目录的 `config.json`，也可以通过 `RLCD_CONFIG` 指定配置文件。可从 [`config.example.json`](./config.example.json) 复制配置。

```json
{
  "listen": ":7523",
  "api_key": "change-me",
  "mqtt": {
    "host": "127.0.0.1",
    "port": 1883,
    "tls": false,
    "username": "",
    "password": "",
    "topic": "/rlcd/{device_name}/message"
  }
}
```

当前固件默认订阅 topic 格式为 `/rlcd/{device_name}/message`。MQTT payload 的完整说明见 [`boards/mqtt-commands.md`](../boards/mqtt-commands.md)。
