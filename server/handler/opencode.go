package handler

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

type NotifyOpencodeRequest struct {
	Title   string `json:"title" form:"title" binding:"required"`
	Content string `json:"content" form:"content" binding:"required"`
	Type    string `json:"type" form:"type" binding:"required"`
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
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if vars.Mqtt == nil || !vars.Mqtt.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "mqtt not connected"})
		return
	}

	beep, timeout := computeBeepAndTimeout(req.Type)

	msg := NotifyMessage{
		Type:    1,
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

func computeBeepAndTimeout(typeStr string) (beep int, timeout int) {
	switch typeStr {
	case "info":
		return 1, 30
	case "warn":
		return 1, 30
	case "error":
		return 1, 30
	default:
		return 1, 30
	}
}
