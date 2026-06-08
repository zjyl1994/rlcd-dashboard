package handler

import "github.com/gin-gonic/gin"

func RegisterRoute(r gin.IRoutes) {
	r.POST("/api/notify/opencode", NotifyOpencodeHandler)
}
