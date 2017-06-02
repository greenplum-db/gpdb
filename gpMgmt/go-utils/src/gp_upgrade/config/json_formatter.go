package config

import (
	"bytes"

	"encoding/json"
)

type JsonFormatter struct {
}

func NewJsonFormatter() Formatter {
	return &JsonFormatter{}
}

func (formatter JsonFormatter) Format(data []byte) ([]byte, error) {
	var out bytes.Buffer
	err := json.Indent(&out, data, "", "  ")
	return out.Bytes(), err
}
