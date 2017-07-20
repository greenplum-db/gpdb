package sshClient

import (
	"errors"
	"os"
	"path"
	"path/filepath"
)

type PrivateKeyGuarantor struct {
}

func NewPrivateKeyGuarantor() *PrivateKeyGuarantor {
	conn := new(PrivateKeyGuarantor)
	return conn
}

func (guarantor PrivateKeyGuarantor) Check(privateKey string) (string, error) {
	if privateKey == "" {
		homePath := os.Getenv("HOME")
		if homePath == "" {
			return "", errors.New("user has not specified a HOME environment value")
		}
		return path.Join(homePath, ".ssh/id_rsa"), nil
	} else if privateKey[:2] == "~/" {
		dir := os.Getenv("HOME")
		return filepath.Join(dir, privateKey[2:]), nil
	}

	if _, err := os.Stat(privateKey); os.IsNotExist(err) {
		return "", err
	}
	return privateKey, nil
}
