package handler

import "github.com/gin-gonic/gin"

func RegisterRoute(r gin.IRoutes) {
	auth := ApiKeyAuth()
	r.POST("/api/info/report", auth, InfoReportHandler)
}
