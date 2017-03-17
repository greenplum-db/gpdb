package commands

import (
	"io"
	"os"
)

type PrivateKeyGuarantor struct {
}

// todo functional pointer for testing. TBD on this approach.
var Stdout io.Writer = os.Stdout

func NewPrivateKeyGuarantor() *PrivateKeyGuarantor {
	conn := new(PrivateKeyGuarantor)
	return conn
}

func (guarantor PrivateKeyGuarantor) Check(private_key string) string {
	if private_key == "" {
		path := os.Getenv("HOME")
		Stdout.Write([]byte("environmental variable 'HOME' not set"))
		return path + "/.ssh/id_rsa"
	}
	return private_key
}
