package handler

import (
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

type aiUsageRequest struct {
	Daily struct {
		CurrentUsd   float64 `json:"currentUsd"`
		LimitUsd     float64 `json:"limitUsd"`
		RemainingUsd float64 `json:"remainingUsd"`
		Percent      float64 `json:"percent"`
	} `json:"daily"`
	TodayStats struct {
		Calls        int     `json:"calls"`
		InputTokens  int     `json:"inputTokens"`
		OutputTokens int     `json:"outputTokens"`
		CostUsd      float64 `json:"costUsd"`
	} `json:"todayStats"`
	CacheHitRate float64 `json:"cacheHitRate"`
}

func formatAiUsage(req *aiUsageRequest, reportTime time.Time) string {
	var lines []string
	cost := req.TodayStats.CostUsd
	limit := req.Daily.LimitUsd
	remaining := req.Daily.RemainingUsd
	inputTokens := req.TodayStats.InputTokens
	outputTokens := req.TodayStats.OutputTokens
	hitRate := req.CacheHitRate

	lines = append(lines, "== "+reportTime.Format("01.02 15:04")+" ==")

	if limit > 0 {
		exhausted := ""
		if remaining <= 0 {
			exhausted = "!"
		}
		lines = append(lines, fmt.Sprintf("AI $%.2f/%.0f%s", cost, limit, exhausted))
	} else {
		lines = append(lines, fmt.Sprintf("AI $%.2f", cost))
	}

	if inputTokens+outputTokens > 0 {
		lines = append(lines, fmt.Sprintf("In %d", inputTokens))
		lines = append(lines, fmt.Sprintf("Out %d", outputTokens))
	}
	if hitRate > 0 {
		lines = append(lines, fmt.Sprintf("Hit %.0f%%", hitRate))
	}

	return strings.Join(lines, "\n")
}

func AiUsageReportHandler(c *gin.Context) {
	var req aiUsageRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	vars.AiUsageText = "\n" + formatAiUsage(&req, time.Now())
	c.JSON(http.StatusOK, gin.H{"ok": true})
}
