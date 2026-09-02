package startup

import (
	"crypto/tls"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
	"strings"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/gin-gonic/gin"
	"github.com/zjyl1994/rlcd-dashboard/server/handler"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

func Start() error {
	if err := loadConfig(); err != nil {
		return err
	}

	router := gin.Default()
	handler.RegisterRoute(router)
	go connectMqttLoop()
	return router.Run(vars.Config.Listen)
}

func connectMqttLoop() {
	const retryDelay = 5 * time.Second

	for {
		client, err := connectMqtt()
		if err == nil {
			vars.SetMqtt(client)
			log.Printf("mqtt connected")
			return
		}
		log.Printf("mqtt unavailable: %v; retrying in %s", err, retryDelay)
		time.Sleep(retryDelay)
	}
}

func loadConfig() error {
	configPath := os.Getenv("RLCD_CONFIG")
	if configPath == "" {
		configPath = "config.json"
	}

	data, err := os.ReadFile(configPath)
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}

		exe, exeErr := os.Executable()
		if exeErr != nil {
			return err
		}

		exeData, exeReadErr := os.ReadFile(filepath.Join(filepath.Dir(exe), "config.json"))
		if exeReadErr != nil {
			return err
		}

		data = exeData
	}

	if err := json.Unmarshal(data, &vars.Config); err != nil {
		return err
	}

	if vars.Config.Listen == "" {
		return fmt.Errorf("config.listen is empty")
	}

	return nil
}

func connectMqtt() (mqtt.Client, error) {
	if vars.Config.Mqtt.Host == "" || vars.Config.Mqtt.Port == 0 {
		return nil, fmt.Errorf("config.mqtt.host/port is empty")
	}

	scheme := "tcp"
	if vars.Config.Mqtt.TLS {
		if vars.Config.Mqtt.Port == 1883 {
			return nil, fmt.Errorf("config.mqtt.tls=true but port=1883 (usually plaintext). Set mqtt.tls=false or change port to 8883 (or your broker's TLS port)")
		}
		scheme = "ssl"
	}

	brokerURL := fmt.Sprintf("%s://%s:%d", scheme, vars.Config.Mqtt.Host, vars.Config.Mqtt.Port)

	opts := mqtt.NewClientOptions()
	opts.AddBroker(brokerURL)
	opts.SetUsername(vars.Config.Mqtt.Username)
	opts.SetPassword(vars.Config.Mqtt.Password)
	opts.SetAutoReconnect(true)
	opts.SetConnectTimeout(10 * time.Second)

	if vars.Config.Mqtt.TLS {
		insecure := false
		switch strings.ToLower(strings.TrimSpace(os.Getenv("RLCD_MQTT_TLS_INSECURE"))) {
		case "1", "true", "yes", "y", "on":
			insecure = true
		}

		tlsConfig := &tls.Config{
			MinVersion:         tls.VersionTLS12,
			InsecureSkipVerify: insecure,
		}
		if net.ParseIP(vars.Config.Mqtt.Host) == nil {
			tlsConfig.ServerName = vars.Config.Mqtt.Host
		}
		opts.SetTLSConfig(tlsConfig)
	}

	client := mqtt.NewClient(opts)
	token := client.Connect()
	token.Wait()
	if err := token.Error(); err != nil {
		client.Disconnect(250)
		return nil, fmt.Errorf("mqtt connect failed (%s): %w", brokerURL, err)
	}

	return client, nil
}
