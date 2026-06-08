package main

import (
	"log"

	"github.com/zjyl1994/rlcd-dashboard/server/startup"
)

func main() {
	err := startup.Start()
	if err != nil {
		log.Fatalln(err.Error())
	}
}
