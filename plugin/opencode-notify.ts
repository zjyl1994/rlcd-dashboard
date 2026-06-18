import type { Plugin } from "@opencode-ai/plugin"

const API_BASE = "http://localhost:7523"
const AGENT_NAME = "agent1"

type Status = "success" | "working" | "error" | "waiting_approval"

async function report(
  status: Status,
  message: string,
) {
  try {
    await fetch(`${API_BASE}/api/opencode/report`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        agent_name: AGENT_NAME,
        status,
        message,
      }),
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
    event: async ({ event }) => {
      const type = event?.type

      if (
        type === "session.created" ||
        type === "session.status" ||
        type === "session.updated" ||
        type === "session.compacted"
      ) {
        await report("working", "Agent工作中...")
        return
      }

      if (type === "session.deleted" || type === "session.idle") {
        await report("success", "Agent任务完成")
        return
      }

      if (type === "session.error") {
        await report("error", toText(event))
        return
      }

      if (type === "permission.asked" || type === "permission.replied") {
        await report("waiting_approval", "需要人工审批: " + toText(event))
        return
      }

      if (type === "tool.execute.before") {
        const tool = event?.tool ?? event?.name
        if (tool === "question" || tool === "ask" || tool === "input") {
          await report("waiting_approval", "Agent等待你的回复: " + toText(event))
        } else {
          await report("working", "Agent执行: " + tool)
        }
        return
      }

      if (type === "tool.execute.after") {
        await report("working", "Agent思考中...")
        return
      }
    },
  }
}

export default MyPlugin
