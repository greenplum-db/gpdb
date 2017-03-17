package commands

import (
	"errors"
	"os"
)

type PrivateKeyGuarantor struct {
}

func NewPrivateKeyGuarantor() *PrivateKeyGuarantor {
	conn := new(PrivateKeyGuarantor)
	return conn
}

func (guarantor PrivateKeyGuarantor) Check(private_key string) (string, error) {
	if private_key == "" {
		path := os.Getenv("HOME")
		if path == "" {
			return "", errors.New("user has not specified a HOME environment value")
		}
		return path + "/.ssh/id_rsa", nil
	}
	return private_key, nil
}
