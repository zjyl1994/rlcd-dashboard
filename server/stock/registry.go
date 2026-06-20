package stock

import (
	"fmt"
	"log"
)

var fetchers []Fetcher

func Register(f Fetcher) {
	fetchers = append(fetchers, f)
}

func Fetch(codes []string) (map[string]Quote, error) {
	for _, f := range fetchers {
		result, err := f.Fetch(codes)
		if err == nil {
			return result, nil
		}
		log.Printf("stock fetcher [%s] failed: %v, trying next", f.Name(), err)
	}
	return nil, fmt.Errorf("all stock fetchers failed")
}
