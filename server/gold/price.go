package gold

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

type goldItem struct {
	Variety  string `json:"variety"`
	CurPrice string `json:"curPrice"`
	GoldNo   string `json:"goldNo"`
}

type goldResponse struct {
	ReturnCode string `json:"returnCode"`
	Body       struct {
		Data []goldItem `json:"data"`
	} `json:"body"`
}

func FetchPrice() (string, error) {
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get("https://m.cmbchina.com/api/rate/gold")
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

	var result goldResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", fmt.Errorf("unmarshal response: %w (body: %s)", err, string(body))
	}

	if result.ReturnCode != "SUC0000" {
		return "", fmt.Errorf("api return code: %s", result.ReturnCode)
	}

	for _, item := range result.Body.Data {
		if item.GoldNo == "AU9999" {
			return item.CurPrice, nil
		}
	}

	return "", fmt.Errorf("AU9999 not found in response")
}
