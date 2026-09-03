package handler

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

type infoReportRequest struct {
	Text *string `json:"text"`
	Beep *int    `json:"beep"`
}

// InfoReportHandler forwards cloud text supplied by an external script to the
// device's full-width text area.
func InfoReportHandler(c *gin.Context) {
	var req infoReportRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Text == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "text is required"})
		return
	}
	if req.Beep != nil && (*req.Beep < 0 || *req.Beep > 6) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "beep must be between 0 and 6"})
		return
	}

	payload := mqttDisplayPayload{}
	if req.Text != nil {
		payload.Text = *req.Text
	}
	if req.Beep != nil {
		payload.Beep = *req.Beep
	}

	if err := publishDisplayPayload(payload); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}
