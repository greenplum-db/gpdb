package ssh

import "golang.org/x/crypto/ssh"

type SshClient interface {
	NewSession() (*ssh.Session, error)
}
