package handler

import "github.com/gin-gonic/gin"

func RegisterRoute(r gin.IRoutes) {
	r.POST("/notify/opencode", NotifyOpencodeHandler)
}
