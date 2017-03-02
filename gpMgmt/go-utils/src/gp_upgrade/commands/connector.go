package commands

import "golang.org/x/crypto/ssh"

type Connector interface {
	Connect(Host string, Port int) (*ssh.Session, error)
}
