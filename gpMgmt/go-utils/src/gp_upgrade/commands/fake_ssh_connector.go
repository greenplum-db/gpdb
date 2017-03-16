package commands

import "golang.org/x/crypto/ssh"

type FakeSshConnector struct {
}

func (fakeSshConnector FakeSshConnector) Connect(Host string, Port int, user string, private_key string) (*ssh.Session, error) {
	return &ssh.Session{}, nil
}
