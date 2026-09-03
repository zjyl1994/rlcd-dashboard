package handler

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

const mqttPayloadMaxLen = 4096

// mqttDisplayPayload mirrors the text payload understood by the firmware.
type mqttDisplayPayload struct {
	Text string `json:"text"`
	Beep int    `json:"beep,omitempty"`
}

func publishDisplayPayload(payload mqttDisplayPayload) error {
	client := vars.GetMqtt()
	if client == nil || !client.IsConnected() {
		return fmt.Errorf("mqtt not connected")
	}
	if vars.Config.Mqtt.Topic == "" {
		return fmt.Errorf("mqtt topic not configured")
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("marshal error: %w", err)
	}
	if len(data) > mqttPayloadMaxLen {
		return fmt.Errorf("mqtt payload is too large: %d bytes, maximum is %d", len(data), mqttPayloadMaxLen)
	}

	token := client.Publish(vars.Config.Mqtt.Topic, 0, false, data)
	if !token.WaitTimeout(10 * time.Second) {
		return fmt.Errorf("mqtt publish timeout")
	}
	return token.Error()
}
