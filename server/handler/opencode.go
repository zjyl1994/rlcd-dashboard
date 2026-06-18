package handler

import (
	"encoding/json"
	"fmt"

	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

type AgentStatus int

const (
	StatusUndefined       AgentStatus = iota
	StatusSuccess         AgentStatus = iota
	StatusWorking         AgentStatus = iota
	StatusError           AgentStatus = iota
	StatusWaitingApproval AgentStatus = iota
)

var statusNames = map[AgentStatus]string{
	StatusSuccess:         "success",
	StatusWorking:         "working",
	StatusError:           "error",
	StatusWaitingApproval: "waiting_approval",
}

func ParseAgentStatus(s string) (AgentStatus, error) {
	for k, v := range statusNames {
		if v == s {
			return k, nil
		}
	}
	return StatusUndefined, fmt.Errorf("unknown agent status: %s", s)
}

type mqttPayload struct {
	Message string `json:"message,omitempty"`
	Beep    int    `json:"beep,omitempty"`
	Timeout int    `json:"timeout"`
	Agent1  *int   `json:"agent1,omitempty"`
	Agent2  *int   `json:"agent2,omitempty"`
}

func ReportAgentStatus(agentName string, status AgentStatus, message string) error {
	if vars.Mqtt == nil || !vars.Mqtt.IsConnected() {
		return fmt.Errorf("mqtt not connected")
	}

	if vars.Config.Mqtt.Topic == "" {
		return fmt.Errorf("mqtt topic not configured")
	}

	var beep int
	var val int
	var timeout int

	switch status {
	case StatusSuccess:
		val = 1
		beep = 1
		timeout = 15
	case StatusWorking:
		val = 2
		beep = 0
		timeout = 30
	case StatusError:
		val = 3
		beep = 2
		timeout = 30
	case StatusWaitingApproval:
		val = 3
		beep = 3
		timeout = 60
	default:
		return fmt.Errorf("unknown agent status: %d", status)
	}

	payload := mqttPayload{
		Message: message,
		Beep:    beep,
		Timeout: timeout,
	}

	switch agentName {
	case "agent1":
		payload.Agent1 = &val
	case "agent2":
		payload.Agent2 = &val
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("marshal error: %w", err)
	}

	token := vars.Mqtt.Publish(vars.Config.Mqtt.Topic, 0, false, data)
	token.Wait()
	return token.Error()
}
