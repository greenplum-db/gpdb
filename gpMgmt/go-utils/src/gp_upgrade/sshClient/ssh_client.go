package sshClient

type SSHClient interface {
	NewSession() (SSHSession, error)
}
