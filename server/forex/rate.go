package forex

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

type fxItem struct {
	CcyNbr    string `json:"ccyNbr"`
	CcyNbrEng string `json:"ccyNbrEng"`
	RtbBid    string `json:"rtbBid"`
	RthOfr    string `json:"rthOfr"`
}

type fxResponse struct {
	ReturnCode string   `json:"returnCode"`
	Body       []fxItem `json:"body"`
}

func FetchRates() (map[string]string, error) {
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get("https://fx.cmbchina.com/api/v1/fx/rate")
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

	var result fxResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return nil, fmt.Errorf("unmarshal response: %w (body: %s)", err, string(body))
	}

	if result.ReturnCode != "SUC0000" {
		return nil, fmt.Errorf("api return code: %s", result.ReturnCode)
	}

	rates := make(map[string]string, len(result.Body))
	for _, item := range result.Body {
		iso := parseISO(item.CcyNbrEng)
		if iso == "" {
			iso = item.CcyNbr
		}
		rates[iso] = avg(item.RtbBid, item.RthOfr)
	}

	if len(rates) == 0 {
		return nil, fmt.Errorf("no currencies found in response")
	}

	return rates, nil
}

func parseISO(eng string) string {
	parts := strings.Fields(eng)
	if len(parts) >= 2 {
		return parts[len(parts)-1]
	}
	return ""
}

func avg(bid, ofr string) string {
	b, _ := strconv.ParseFloat(bid, 64)
	o, _ := strconv.ParseFloat(ofr, 64)
	return strconv.FormatFloat((b+o)/200, 'f', 2, 64)
}
