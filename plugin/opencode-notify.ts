import type { Plugin } from "@opencode-ai/plugin"

const API_BASE = "http://localhost:7523"
const AGENT_NAME = "agent1"
const API_KEY = ""

type Status = "success" | "working" | "error" | "waiting_approval" | "off"

async function report(status: Status, message: string) {
  try {
    const headers: Record<string, string> = { "Content-Type": "application/json" }
    if (API_KEY) headers["X-Api-Key"] = API_KEY
    await fetch(`${API_BASE}/api/opencode/report`, {
      method: "POST",
      headers,
      body: JSON.stringify({ agent_name: AGENT_NAME, status, message }),
    })
  } catch (e) {
    console.error("[report] failed", e)
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
  console.log("[report] loaded")
  await report("success", "Agent就绪")

  return {
    dispose: async () => {
      await report("off", "")
    },

    event: async ({ event }) => {
      const { type, properties } = event as any

      if (type === "session.status") {
        const st = properties?.status?.type
        if (st === "idle") {
          await report("success", "Agent任务完成")
        } else if (st === "busy") {
          await report("working", "Agent工作中...")
        } else if (st === "retry") {
          if (properties?.status?.action) {
            await report("waiting_approval", "需要人工介入: " + toText(event))
          } else {
            await report("working", "Agent重试中...")
          }
        }
        return
      }

      if (type === "session.idle") {
        await report("success", "Agent任务完成")
        return
      }

      if (type === "server.instance.disposed") {
        await report("off", "")
        return
      }

      if (type === "session.error") {
        await report("error", toText(event))
        return
      }

      if (type === "permission.asked" || type === "permission.v2.asked") {
        await report("waiting_approval", "需要人工审批: " + toText(event))
        return
      }

      if (type === "question.asked") {
        await report("waiting_approval", "Agent等待回答: " + toText(event))
        return
      }

      if (
        type === "permission.replied" ||
        type === "permission.v2.replied" ||
        type === "question.replied" ||
        type === "question.rejected"
      ) {
        await report("working", "Agent继续工作...")
        return
      }
    },
  }
}

export default MyPlugin
