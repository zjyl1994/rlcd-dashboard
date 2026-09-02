package handler

import (
	"encoding/json"
	"fmt"

	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

const mqttPayloadMaxLen = 4096

// mqttDisplayPayload mirrors the payload already understood by the firmware.
// A pointer timeout lets us distinguish an omitted timeout from timeout=0.
type mqttDisplayPayload struct {
	Info    string `json:"info,omitempty"`
	Message string `json:"message,omitempty"`
	Beep    int    `json:"beep,omitempty"`
	Timeout *int   `json:"timeout,omitempty"`
	Agent1  *int   `json:"agent1,omitempty"`
	Agent2  *int   `json:"agent2,omitempty"`
}

type AgentStatus int

const (
	StatusUndefined       AgentStatus = iota
	StatusSuccess         AgentStatus = iota
	StatusWorking         AgentStatus = iota
	StatusError           AgentStatus = iota
	StatusWaitingApproval AgentStatus = iota
	StatusOff             AgentStatus = iota
)

var statusNames = map[AgentStatus]string{
	StatusSuccess:         "success",
	StatusWorking:         "working",
	StatusError:           "error",
	StatusWaitingApproval: "waiting_approval",
	StatusOff:             "off",
}

func ParseAgentStatus(s string) (AgentStatus, error) {
	for k, v := range statusNames {
		if v == s {
			return k, nil
		}
	}
	return StatusUndefined, fmt.Errorf("unknown agent status: %s", s)
}

func agentStatusValues(status AgentStatus) (value int, beep int, timeout int, err error) {
	switch status {
	case StatusSuccess:
		return 1, 1, 15, nil
	case StatusWorking:
		return 2, 0, 30, nil
	case StatusError:
		return 3, 2, 30, nil
	case StatusWaitingApproval:
		return 3, 3, 60, nil
	case StatusOff:
		return 0, 0, 0, nil
	default:
		return 0, 0, 0, fmt.Errorf("unknown agent status: %d", status)
	}
}

func publishDisplayPayload(payload mqttDisplayPayload) error {
	if vars.Mqtt == nil || !vars.Mqtt.IsConnected() {
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

	token := vars.Mqtt.Publish(vars.Config.Mqtt.Topic, 0, false, data)
	token.Wait()
	return token.Error()
}
