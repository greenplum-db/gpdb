package ssh_client

import (
	"golang.org/x/crypto/ssh"
)

type KeyParser interface {
	ParsePrivateKey(pemBytes []byte) (ssh.Signer, error)
}
