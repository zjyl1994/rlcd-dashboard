package vars

type ConfigS struct {
	Name   string `json:"name"`
	Listen string `json:"listen"`
	Mqtt   MqttS  `json:"mqtt"`
}

type MqttS struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	TLS      bool   `json:"tls"`
	Username string `json:"username"`
	Password string `json:"password"`
	Topic    string `json:"topic"`
}
