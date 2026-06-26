package handler

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

type reportRequest struct {
	AgentName string `json:"agent_name" binding:"required"`
	Status    string `json:"status" binding:"required"`
	Message   string `json:"message"`
}

func ReportHandler(c *gin.Context) {
	var req reportRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	req.AgentName = strings.TrimSpace(req.AgentName)
	req.Status = strings.TrimSpace(req.Status)

	status, err := ParseAgentStatus(req.Status)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := ReportAgentStatus(req.AgentName, status, req.Message); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func RegisterRoute(r gin.IRoutes) {
	auth := ApiKeyAuth()
	r.POST("/api/opencode/report", auth, ReportHandler)
	r.POST("/api/cch-quota-report", auth, AiUsageReportHandler)
}
