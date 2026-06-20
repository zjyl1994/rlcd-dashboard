package stock

import (
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"golang.org/x/text/encoding/simplifiedchinese"
)

type SinaFetcher struct{}

func (s *SinaFetcher) Name() string { return "sina" }

func (s *SinaFetcher) Fetch(codes []string) (map[string]Quote, error) {
	if len(codes) == 0 {
		return nil, fmt.Errorf("no codes provided")
	}

	var queryCodes []string
	seen := make(map[string]bool)
	origToQCs := make(map[string][]string, len(codes))

	for _, c := range codes {
		qcs := sinaQueryCodes(c)
		origToQCs[c] = qcs
		for _, qc := range qcs {
			if !seen[qc] {
				seen[qc] = true
				queryCodes = append(queryCodes, qc)
			}
		}
	}

	url := "http://hq.sinajs.cn/list=" + strings.Join(queryCodes, ",")

	client := &http.Client{Timeout: 10 * time.Second}
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Referer", "https://finance.sina.com.cn")

	resp, err := client.Do(req)
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

	rawResults := parseSinaRaw(body)

	result := make(map[string]Quote, len(codes))
	for _, c := range codes {
		for _, qc := range origToQCs[c] {
			val, ok := rawResults[qc]
			if !ok || val == "" {
				continue
			}
			fields := strings.Split(val, ",")
			if len(fields) < 4 {
				continue
			}
			result[c] = Quote{
				Name:  fields[0],
				Price: fields[3],
			}
			break
		}
	}

	if len(result) == 0 {
		return nil, fmt.Errorf("no valid quotes parsed from sina")
	}
	return result, nil
}

func sinaQueryCodes(code string) []string {
	parts := strings.SplitN(code, ".", 2)
	if len(parts) != 2 {
		return []string{code}
	}
	c, market := strings.ToLower(parts[0]), strings.ToLower(parts[1])
	switch market {
	case "sh", "sz":
		return []string{market + c}
	case "us":
		return []string{"gb_$" + c, "gb_" + c}
	case "hk":
		return []string{"hk_" + c}
	default:
		return []string{code}
	}
}

func parseSinaRaw(body []byte) map[string]string {
	result := make(map[string]string)
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

		prefix := "var hq_str_"
		varName := line[:eqIdx]
		if !strings.HasPrefix(varName, prefix) {
			continue
		}
		qc := varName[len(prefix):]

		start := eqIdx + 1
		if start >= len(line) || line[start] != '"' {
			continue
		}
		start++

		end := strings.LastIndex(line, `";`)
		if end < 0 || end <= start {
			continue
		}
		result[qc] = line[start:end]
	}
	return result
}
