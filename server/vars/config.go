package vars

type ConfigS struct {
	Listen string `json:"listen"`
	ApiKey string `json:"api_key"`
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
