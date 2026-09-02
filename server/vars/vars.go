package vars

import (
	"sync"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

var (
	Config ConfigS
	Mqtt   mqtt.Client
	MqttMu sync.RWMutex
)

func GetMqtt() mqtt.Client {
	MqttMu.RLock()
	defer MqttMu.RUnlock()
	return Mqtt
}

func SetMqtt(client mqtt.Client) {
	MqttMu.Lock()
	Mqtt = client
	MqttMu.Unlock()
}
