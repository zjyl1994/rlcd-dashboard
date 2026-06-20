package vars

type ScheduleS struct {
	StartHour     int `json:"start_hour"`
	EndHour       int `json:"end_hour"`
	DayInterval   int `json:"day_interval"`
	NightInterval int `json:"night_interval"`
}

type ConfigS struct {
	Name     string     `json:"name"`
	Listen   string     `json:"listen"`
	Mqtt     MqttS      `json:"mqtt"`
	DeepSeek DeepSeekS  `json:"deepseek"`
	Gold     bool       `json:"gold"`
	Forex    []string   `json:"forex"`
	Stocks   []string   `json:"stocks"`
	Schedule *ScheduleS `json:"schedule"`
}

type DeepSeekS struct {
	Key string `json:"key"`
}

type MqttS struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	TLS      bool   `json:"tls"`
	Username string `json:"username"`
	Password string `json:"password"`
	Topic    string `json:"topic"`
}
