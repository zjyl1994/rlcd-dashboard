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

func FetchBalance() (string, error) {
	req, err := http.NewRequest("GET", "https://api.deepseek.com/user/balance", nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Authorization", "Bearer "+vars.Config.DeepSeek.Key)

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("api returned status %d: %s", resp.StatusCode, string(body))
	}

	var result balanceResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", fmt.Errorf("unmarshal response: %w (body: %s)", err, string(body))
	}

	for _, info := range result.BalanceInfos {
		if info.Currency == "CNY" {
			return info.TotalBalance, nil
		}
	}
	if len(result.BalanceInfos) > 0 {
		return result.BalanceInfos[0].TotalBalance, nil
	}

	return "", fmt.Errorf("no balance info found")
}
