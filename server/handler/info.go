package handler

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

type infoReportRequest struct {
	Info    *string           `json:"info"`
	Message *string           `json:"message"`
	Timeout *int              `json:"timeout"`
	Beep    *int              `json:"beep"`
	Agents  map[string]string `json:"agents"`
}

// InfoReportHandler forwards text supplied by an external script to the
// device's right-side information panel.
func InfoReportHandler(c *gin.Context) {
	var req infoReportRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Info != nil && strings.TrimSpace(*req.Info) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "info is empty"})
		return
	}
	if req.Message != nil && strings.TrimSpace(*req.Message) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "message is empty"})
		return
	}
	if req.Info == nil && req.Message == nil && req.Beep == nil && len(req.Agents) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "at least one display field is required"})
		return
	}
	if req.Timeout != nil && (*req.Timeout < 0 || *req.Timeout > 180) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "timeout must be between 0 and 180"})
		return
	}
	if req.Beep != nil && (*req.Beep < 0 || *req.Beep > 6) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "beep must be between 0 and 6"})
		return
	}

	payload := mqttDisplayPayload{}
	if req.Info != nil {
		payload.Info = *req.Info
	}
	if req.Message != nil {
		payload.Message = *req.Message
	}
	if req.Beep != nil {
		payload.Beep = *req.Beep
	}
	if req.Timeout != nil {
		payload.Timeout = req.Timeout
	}
	defaultBeep := 0
	defaultTimeout := 0
	hasAgentDefaults := false
	for name, statusName := range req.Agents {
		status, err := ParseAgentStatus(statusName)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
		value, beep, timeout, err := agentStatusValues(status)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
		if !hasAgentDefaults || name == "agent1" {
			defaultBeep = beep
			defaultTimeout = timeout
			hasAgentDefaults = true
		}
		switch name {
		case "agent1":
			payload.Agent1 = &value
		case "agent2":
			payload.Agent2 = &value
		default:
			c.JSON(http.StatusBadRequest, gin.H{"error": "agents only supports agent1 and agent2"})
			return
		}
	}
	if req.Beep == nil && hasAgentDefaults {
		payload.Beep = defaultBeep
	}
	if req.Timeout == nil && req.Message != nil && hasAgentDefaults {
		timeout := defaultTimeout
		payload.Timeout = &timeout
	}

	if err := publishDisplayPayload(payload); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}
