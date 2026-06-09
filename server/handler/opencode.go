package handler

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

const (
	maxTitleLen    = 64
	maxContentLen  = 512
	maxMessageType = 3
	maxBeepType    = 6
)

type NotifyOpencodeRequest struct {
	Title   string `json:"title" binding:"required"`
	Content string `json:"content" binding:"required"`
	Type    *int   `json:"type"`
	Beep    *int   `json:"beep"`
	Timeout *int   `json:"timeout"`
}

type NotifyMessage struct {
	Type    int    `json:"type"`
	Title   string `json:"title"`
	Content string `json:"content"`
	Beep    int    `json:"beep"`
	Timeout int    `json:"timeout"`
}

func NotifyOpencodeHandler(c *gin.Context) {
	name := strings.TrimSpace(c.Query("name"))
	if name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name is required"})
		return
	}
	if err := validateTopicName(name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	var req NotifyOpencodeRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if vars.Mqtt == nil || !vars.Mqtt.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "mqtt not connected"})
		return
	}

	if len(req.Title) > maxTitleLen {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("title too long (max %d bytes)", maxTitleLen)})
		return
	}
	if len(req.Content) > maxContentLen {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("content too long (max %d bytes)", maxContentLen)})
		return
	}

	msgType := 1
	if req.Type != nil {
		msgType = *req.Type
	}
	if msgType < 1 || msgType > maxMessageType {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("type must be 1-%d", maxMessageType)})
		return
	}

	beep := 0
	timeout := 15
	if req.Beep != nil {
		beep = *req.Beep
	}
	if req.Timeout != nil {
		timeout = *req.Timeout
	}
	if beep < 0 || beep > maxBeepType {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("beep must be 0-%d", maxBeepType)})
		return
	}
	if timeout < 0 || timeout > 60 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "timeout must be 0-60"})
		return
	}

	msg := NotifyMessage{
		Type:    msgType,
		Title:   req.Title,
		Content: req.Content,
		Beep:    beep,
		Timeout: timeout,
	}

	payload, err := json.Marshal(msg)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	topic := fmt.Sprintf("/rlcd/%s/message", name)
	token := vars.Mqtt.Publish(topic, 0, false, payload)
	token.Wait()
	if err := token.Error(); err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func validateTopicName(name string) error {
	if strings.ContainsAny(name, "\x00#+") {
		return fmt.Errorf("name contains invalid characters")
	}
	if strings.ContainsRune(name, '/') {
		return fmt.Errorf("name must not contain '/'")
	}
	return nil
}
