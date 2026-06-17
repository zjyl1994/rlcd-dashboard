package handler

import "github.com/gin-gonic/gin"

func RegisterRoute(r gin.IRoutes) {
	r.POST("/api/opencode/notify", NotifyHandler)
	r.POST("/api/opencode/traffic", TrafficHandler)
}
