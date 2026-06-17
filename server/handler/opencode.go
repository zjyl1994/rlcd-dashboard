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
	maxContentLen = 512
	maxTimeout    = 180
	maxBeepType   = 6
)

type NotifyOpencodeRequest struct {
	Content string `json:"content" binding:"required"`
	Beep    *int   `json:"beep"`
	Timeout *int   `json:"timeout"`
}

type NotifyMessage struct {
	Message string `json:"message"`
	Beep    int    `json:"beep"`
	Timeout int    `json:"timeout"`
}

func NotifyHandler(c *gin.Context) {
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

	if len(req.Content) > maxContentLen {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("content too long (max %d bytes)", maxContentLen)})
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
	if timeout < 0 || timeout > maxTimeout {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("timeout must be 0-%d", maxTimeout)})
		return
	}

	msg := NotifyMessage{
		Message: req.Content,
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

type TrafficOpencodeRequest struct {
	Agent1 *int `json:"agent1"`
	Agent2 *int `json:"agent2"`
}

type TrafficMessage struct {
	Agent1 *int `json:"agent1,omitempty"`
	Agent2 *int `json:"agent2,omitempty"`
}

func TrafficHandler(c *gin.Context) {
	name := strings.TrimSpace(c.Query("name"))
	if name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name is required"})
		return
	}
	if err := validateTopicName(name); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	var req TrafficOpencodeRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Agent1 == nil && req.Agent2 == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "at least one of agent1/agent2 is required"})
		return
	}

	if vars.Mqtt == nil || !vars.Mqtt.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "mqtt not connected"})
		return
	}

	msg := TrafficMessage{}
	if req.Agent1 != nil {
		if *req.Agent1 < 0 || *req.Agent1 > 3 {
			c.JSON(http.StatusBadRequest, gin.H{"error": "agent1 must be 0-3"})
			return
		}
		msg.Agent1 = req.Agent1
	}
	if req.Agent2 != nil {
		if *req.Agent2 < 0 || *req.Agent2 > 3 {
			c.JSON(http.StatusBadRequest, gin.H{"error": "agent2 must be 0-3"})
			return
		}
		msg.Agent2 = req.Agent2
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
