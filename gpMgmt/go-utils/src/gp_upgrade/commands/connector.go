package commands

import "golang.org/x/crypto/ssh"

type Connector interface {
	Connect(Host string, Port int, user string, private_key string) (*ssh.Session, error)
}
