package commands

import (
	"fmt"

	"log"

	"io/ioutil"

	"golang.org/x/crypto/ssh"
)

type SshConnector struct {
}

func NewSshConnector() *SshConnector {
	conn := new(SshConnector)
	return conn
}

func (ssh_connector SshConnector) Connect(Host string, Port int, user string, private_key string) (*ssh.Session, error) {
	pemBytes, err := ioutil.ReadFile(private_key)
	if err != nil {
		panic(fmt.Sprintf("cannot open private key file: %v", private_key))
	}
	signer, err := ssh.ParsePrivateKey(pemBytes)
	if err != nil {
		panic(fmt.Sprintf("parsing private key failed: %v", err))
	}
	config := &ssh.ClientConfig{
		User: user,
		Auth: []ssh.AuthMethod{ssh.PublicKeys(signer)},
	}
	hostAndPort := fmt.Sprintf("%s:%v", Host, Port)
	client, err := ssh.Dial("tcp", hostAndPort, config)
	if err != nil {
		return nil, err
	}

	// Each ClientConn can support multiple interactive sessions,
	// represented by a Session.
	session, err := client.NewSession()
	if err != nil {
		log.Fatal("Failed to create session: ", err)
		return nil, err
	}
	return session, nil
}
