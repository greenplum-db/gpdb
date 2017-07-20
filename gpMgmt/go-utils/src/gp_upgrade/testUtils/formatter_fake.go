package testUtils

import (
	"os"
)

type ErrorFormatter struct{}

func (formatter ErrorFormatter) Format(data []byte) ([]byte, error) {
	return nil, os.ErrInvalid
}

type NilFormatter struct{}

func (formatter NilFormatter) Format(data []byte) ([]byte, error) {
	return nil, nil
}
