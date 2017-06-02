package config

type Formatter interface {
	Format([]byte) ([]byte, error)
}
