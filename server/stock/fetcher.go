package stock

type Quote struct {
	Name  string
	Price string
}

type Fetcher interface {
	Name() string
	Fetch(codes []string) (map[string]Quote, error)
}
