package forex

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"time"
)

type fxItem struct {
	CcyNbr string `json:"ccyNbr"`
	RtbBid string `json:"rtbBid"`
	RthOfr string `json:"rthOfr"`
}

type fxResponse struct {
	ReturnCode string   `json:"returnCode"`
	Body       []fxItem `json:"body"`
}

func FetchRates() (usd, hkd string, err error) {
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get("https://fx.cmbchina.com/api/v1/fx/rate")
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", "", err
	}

	if resp.StatusCode != http.StatusOK {
		return "", "", fmt.Errorf("api returned status %d: %s", resp.StatusCode, string(body))
	}

	var result fxResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", "", fmt.Errorf("unmarshal response: %w (body: %s)", err, string(body))
	}

	if result.ReturnCode != "SUC0000" {
		return "", "", fmt.Errorf("api return code: %s", result.ReturnCode)
	}

	for _, item := range result.Body {
		switch item.CcyNbr {
		case "美元":
			usd = avg(item.RtbBid, item.RthOfr)
		case "港币":
			hkd = avg(item.RtbBid, item.RthOfr)
		}
	}

	if usd == "" {
		return "", "", fmt.Errorf("USD not found")
	}
	if hkd == "" {
		return "", "", fmt.Errorf("HKD not found")
	}

	return usd, hkd, nil
}

func avg(bid, ofr string) string {
	b, _ := strconv.ParseFloat(bid, 64)
	o, _ := strconv.ParseFloat(ofr, 64)
	return strconv.FormatFloat((b+o)/200, 'f', 2, 64)
}
