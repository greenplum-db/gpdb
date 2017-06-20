package ssh_client

import (
	"errors"
	"os"
	"path/filepath"
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
	} else if private_key[:2] == "~/" {
		dir := os.Getenv("HOME")
		return filepath.Join(dir, private_key[2:]), nil
	}

	if _, err := os.Stat(private_key); os.IsNotExist(err) {
		return "", err
	}
	return private_key, nil
}
