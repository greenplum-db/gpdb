package ssh_client

import (
	"fmt"

	"io/ioutil"

	"golang.org/x/crypto/ssh"
)

type SshConnector struct {
	SshDialer    Dialer
	SshKeyParser KeyParser
}

func NewSshConnector() *SshConnector {
	return &SshConnector{
		SshKeyParser: RealKeyParser{},
		SshDialer:    RealDialer{},
	}
}

func (ssh_connector SshConnector) Connect(Host string, Port int, user string, private_key string) (*ssh.Session, error) {
	pemBytes, err := ioutil.ReadFile(private_key)
	if err != nil {
		return nil, err
	}
	signer, err := ssh_connector.SshKeyParser.ParsePrivateKey(pemBytes)
	if err != nil {
		return nil, err
	}
	config := &ssh.ClientConfig{
		User: user,
		Auth: []ssh.AuthMethod{ssh.PublicKeys(signer)},
	}
	hostAndPort := fmt.Sprintf("%s:%v", Host, Port)
	client, err := ssh_connector.SshDialer.Dial("tcp", hostAndPort, config)
	if err != nil {
		return nil, err
	}
	session, err := client.NewSession()
	if err != nil {
		return nil, err
	}
	return session, nil
}

type RealKeyParser struct{}

func (parser RealKeyParser) ParsePrivateKey(pemBytes []byte) (ssh.Signer, error) {
	return ssh.ParsePrivateKey(pemBytes)
}

type RealDialer struct{}

func (dial RealDialer) Dial(network, addr string, config *ssh.ClientConfig) (SshClient, error) {
	return ssh.Dial(network, addr, config)
}
