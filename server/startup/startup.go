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
	"github.com/zjyl1994/rlcd-dashboard/server/deepseek"
	"github.com/zjyl1994/rlcd-dashboard/server/forex"
	"github.com/zjyl1994/rlcd-dashboard/server/gold"
	"github.com/zjyl1994/rlcd-dashboard/server/handler"
	"github.com/zjyl1994/rlcd-dashboard/server/stock"
	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

func Start() error {
	if err := loadConfig(); err != nil {
		return err
	}

	if err := connectMqtt(); err != nil {
		return err
	}

	stock.Register(&stock.TencentFetcher{})
	stock.Register(&stock.SinaFetcher{})

	startCollectTask()

	router := gin.Default()
	handler.RegisterRoute(router)
	return router.Run(vars.Config.Listen)
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

func connectMqtt() error {
	if vars.Config.Mqtt.Host == "" || vars.Config.Mqtt.Port == 0 {
		return fmt.Errorf("config.mqtt.host/port is empty")
	}

	scheme := "tcp"
	if vars.Config.Mqtt.TLS {
		if vars.Config.Mqtt.Port == 1883 {
			return fmt.Errorf("config.mqtt.tls=true but port=1883 (usually plaintext). Set mqtt.tls=false or change port to 8883 (or your broker's TLS port)")
		}
		scheme = "ssl"
	}

	brokerURL := fmt.Sprintf("%s://%s:%d", scheme, vars.Config.Mqtt.Host, vars.Config.Mqtt.Port)

	opts := mqtt.NewClientOptions()
	opts.AddBroker(brokerURL)
	opts.SetUsername(vars.Config.Mqtt.Username)
	opts.SetPassword(vars.Config.Mqtt.Password)
	opts.SetAutoReconnect(true)

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
		return fmt.Errorf("mqtt connect failed (%s): %w", brokerURL, err)
	}

	vars.Mqtt = client
	return nil
}

func startCollectTask() {
	go func() {
		collectAndPublish()
		ticker := time.NewTicker(5 * time.Minute)
		defer ticker.Stop()
		for range ticker.C {
			collectAndPublish()
		}
	}()
}

func collectAndPublish() {
	var lines []string

	if vars.Config.Gold {
		if price, err := gold.FetchPrice(); err != nil {
			log.Printf("gold price: %v", err)
		} else {
			lines = append(lines, "黄金: "+price)
		}
	}

	if len(vars.Config.Forex) > 0 {
		if rates, err := forex.FetchRates(); err != nil {
			log.Printf("forex: %v", err)
		} else {
			for _, name := range vars.Config.Forex {
				if price, ok := rates[strings.ToUpper(name)]; ok {
					lines = append(lines, strings.ToUpper(name)+": "+price)
				}
			}
		}
	}

	if len(vars.Config.Stocks) > 0 {
		if quotes, err := stock.Fetch(vars.Config.Stocks); err != nil {
			log.Printf("stocks: %v", err)
		} else {
			for _, code := range vars.Config.Stocks {
				if q, ok := quotes[code]; ok {
					lines = append(lines, strings.ToUpper(code)+": "+q.Price)
				}
			}
		}
	}

	if vars.Config.DeepSeek.Key != "" {
		if balance, err := deepseek.FetchBalance(); err != nil {
			log.Printf("deepseek balance: %v", err)
		} else {
			lines = append(lines, "DS余额: "+balance)
		}
	}

	if len(lines) == 0 {
		return
	}

	payload := struct {
		Info    string `json:"info"`
		Timeout int    `json:"timeout"`
	}{
		Info:    strings.Join(lines, "\n"),
		Timeout: 0,
	}

	data, err := json.Marshal(payload)
	if err != nil {
		log.Printf("marshal payload: %v", err)
		return
	}

	token := vars.Mqtt.Publish(vars.Config.Mqtt.Topic, 0, false, data)
	token.Wait()
	if err := token.Error(); err != nil {
		log.Printf("mqtt publish: %v", err)
	}
}
