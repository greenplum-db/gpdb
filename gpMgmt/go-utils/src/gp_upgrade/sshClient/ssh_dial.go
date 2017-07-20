package sshClient

import (
	"golang.org/x/crypto/ssh"
)

type RealClientProxy struct {
	client *ssh.Client
}

func (proxy RealClientProxy) NewSession() (SSHSession, error) {
	return proxy.client.NewSession()
}

type RealDialer struct{}

func (dial RealDialer) Dial(network, addr string, config *ssh.ClientConfig) (SSHClient, error) {
	realClient, err := ssh.Dial(network, addr, config)
	return RealClientProxy{client: realClient}, err
}
