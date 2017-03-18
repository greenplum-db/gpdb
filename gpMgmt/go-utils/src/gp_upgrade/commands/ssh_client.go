package commands

import "golang.org/x/crypto/ssh"

type SshClient interface {
	NewSession() (*ssh.Session, error)
}
