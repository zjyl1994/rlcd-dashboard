import type { Plugin } from "@opencode-ai/plugin"

const WEBHOOK_URL = "https://example.com/api/opencode/notify?name=YOU_RLCD_DEVICE_NAME"

async function notify(
  title: string,
  content: string,
  beep: 1 | 2 | 3,
) {
  try {
    await fetch(WEBHOOK_URL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        type: 1,
        title,
        content,
        beep,
        timeout: 30,
      }),
    })
  } catch (e) {
    console.error("[webhook] failed", e)
  }
}

function toText(v: any) {
  if (!v) return ""
  if (typeof v === "string") return v
  try {
    return JSON.stringify(v, null, 2)
  } catch {
    return String(v)
  }
}

export const MyPlugin: Plugin = async () => {
  console.log("[webhook] loaded")

  return {
    event: async ({ event }) => {
      const type = event?.type

      // =========================
      // 1. 完成
      // =========================
      if (type === "session.idle") {
        await notify(
          "任务完成",
          "Agent已结束并进入空闲状态",
          1,
        )
        return
      }

      // =========================
      // 2. 报错
      // =========================
      if (type === "session.error") {
        await notify(
          "任务异常",
          toText(event),
          3,
        )
        return
      }

      // =========================
      // 3. 等待权限审批
      // =========================
      if (type === "permission.asked") {
        await notify(
          "需要人工审批",
          toText(event),
          2,
        )
        return
      }

      if (type === "permission.updated") {
        await notify(
          "等待用户授权",
          toText(event),
          2,
        )
        return
      }

      // =========================
      // 4. Agent卡住 / 等输入
      // =========================
      if (type === "tool.execute.before") {
        const tool = event?.tool ?? event?.name

        // question / input / ask 都属于“需要你”
        if (
          tool === "question" ||
          tool === "ask" ||
          tool === "input"
        ) {
          await notify(
            "Agent等待你的回复",
            toText(event),
            2,
          )
        }
        return
      }
    },
  }
}

export default MyPlugin