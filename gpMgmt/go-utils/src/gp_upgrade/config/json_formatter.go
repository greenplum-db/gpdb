package config

import (
	"bytes"

	"encoding/json"
)

type JSONFormatter struct {
}

func NewJSONFormatter() Formatter {
	return &JSONFormatter{}
}

func (formatter JSONFormatter) Format(data []byte) ([]byte, error) {
	var out bytes.Buffer
	err := json.Indent(&out, data, "", "  ")
	return out.Bytes(), err
}
