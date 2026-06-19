package deepseek

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/zjyl1994/rlcd-dashboard/server/vars"
)

type balanceInfo struct {
	Currency        string `json:"currency"`
	TotalBalance    string `json:"total_balance"`
	GrantedBalance  string `json:"granted_balance"`
	ToppedUpBalance string `json:"topped_up_balance"`
}

type balanceResponse struct {
	IsAvailable  bool          `json:"is_available"`
	BalanceInfos []balanceInfo `json:"balance_infos"`
}

type mqttPayload struct {
	KV      map[string]interface{} `json:"kv"`
	Timeout int                    `json:"timeout"`
}

func FetchAndPublish() error {
	if vars.Config.DeepSeek.Key == "" {
		return nil
	}

	balance, err := fetchBalance()
	if err != nil {
		return fmt.Errorf("fetch deepseek balance: %w", err)
	}

	var total string
	for _, info := range balance.BalanceInfos {
		if info.Currency == "CNY" {
			total = info.TotalBalance
			break
		}
	}
	if total == "" && len(balance.BalanceInfos) > 0 {
		total = balance.BalanceInfos[0].TotalBalance
	}

	payload := mqttPayload{
		KV: map[string]interface{}{
			"DS余额": total,
		},
		Timeout: 0,
	}

	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("marshal payload: %w", err)
	}

	token := vars.Mqtt.Publish(vars.Config.Mqtt.Topic, 0, false, data)
	token.Wait()
	return token.Error()
}

func fetchBalance() (*balanceResponse, error) {
	req, err := http.NewRequest("GET", "https://api.deepseek.com/user/balance", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization", "Bearer "+vars.Config.DeepSeek.Key)

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("api returned status %d: %s", resp.StatusCode, string(body))
	}

	var result balanceResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("unmarshal response: %w (body: %s)", err, string(body))
	}

	return &result, nil
}
