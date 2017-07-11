package ssh_client

import (
	"golang.org/x/crypto/ssh"
)

type RealClientProxy struct {
	client *ssh.Client
}

func (proxy RealClientProxy) NewSession() (SshSession, error) {
	return proxy.client.NewSession()
}

type RealDialer struct{}

func (dial RealDialer) Dial(network, addr string, config *ssh.ClientConfig) (SshClient, error) {
	real_client, err := ssh.Dial(network, addr, config)
	return RealClientProxy{client: real_client}, err
}
