package stock

import (
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"golang.org/x/text/encoding/simplifiedchinese"
)

type TencentFetcher struct{}

func (t *TencentFetcher) Name() string { return "tencent" }

func (t *TencentFetcher) Fetch(codes []string) (map[string]Quote, error) {
	if len(codes) == 0 {
		return nil, fmt.Errorf("no codes provided")
	}

	queryCodes := make([]string, len(codes))
	qcToOrig := make(map[string]string, len(codes))
	for i, c := range codes {
		qc := toTencentCode(c)
		queryCodes[i] = qc
		qcToOrig[qc] = c
	}

	url := "http://qt.gtimg.cn/q=" + strings.Join(queryCodes, ",")

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	body, err = simplifiedchinese.GBK.NewDecoder().Bytes(body)
	if err != nil {
		return nil, fmt.Errorf("gbk decode: %w", err)
	}

	return parseTencentResponse(body, qcToOrig)
}

func toTencentCode(code string) string {
	parts := strings.SplitN(code, ".", 2)
	if len(parts) != 2 {
		return code
	}
	c, market := parts[0], strings.ToLower(parts[1])
	switch market {
	case "sh", "sz":
		return market + c
	case "us":
		return "us" + strings.ToUpper(c)
	case "hk":
		return "hk" + strings.ToUpper(c)
	default:
		return code
	}
}

func parseTencentResponse(body []byte, qcToOrig map[string]string) (map[string]Quote, error) {
	result := make(map[string]Quote, len(qcToOrig))
	lines := strings.Split(string(body), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		eqIdx := strings.IndexByte(line, '=')
		if eqIdx < 0 {
			continue
		}

		varName := line[:eqIdx]
		if !strings.HasPrefix(varName, "v_") {
			continue
		}
		qc := varName[2:]

		origCode, ok := qcToOrig[qc]
		if !ok {
			continue
		}

		start := eqIdx + 1
		if start >= len(line) || line[start] != '"' {
			continue
		}
		start++

		end := strings.LastIndex(line, `";`)
		if end < 0 || end <= start {
			continue
		}
		val := line[start:end]

		fields := strings.Split(val, "~")
		if len(fields) < 4 {
			continue
		}

		result[origCode] = Quote{
			Name:  fields[1],
			Price: fields[3],
		}
	}

	if len(result) == 0 {
		return nil, fmt.Errorf("no valid quotes parsed from tencent")
	}
	return result, nil
}
