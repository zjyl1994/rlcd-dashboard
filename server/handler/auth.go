package handler

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

func ApiKeyAuth() gin.HandlerFunc {
	return func(c *gin.Context) {
		if vars.Config.ApiKey == "" {
			c.Next()
			return
		}
		key := c.GetHeader("X-Api-Key")
		if key == "" {
			key = c.Query("api_key")
		}
		if key != vars.Config.ApiKey {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			return
		}
		c.Next()
	}
}
