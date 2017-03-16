package commands

import (
	"errors"
	"fmt"
	"io"
	"os/user"
)

type MonitorCommand struct {
	Host       string `long:"host" required:"yes" description:"Domain name or IP of host"`
	Port       int    `long:"port" default:"22" description:"SSH port for communication"`
	User       string `long:"user" default:"gpadmin" description:"Name of user at ssh destination"`
	PrivateKey string `long:"private_key" description:"Private key for ssh destination"`
	Segment_id string `long:"segment_id" required:"yes" description:"ID of segment to monitor"`
}

func (cmd MonitorCommand) Execute([]string) error {
	// todo test private key
	cmd.PrivateKey = cmd.assurePrivateKeyPath(cmd.PrivateKey)

	connector := NewSshConnector()
	session, err := connector.Connect(cmd.Host, cmd.Port, cmd.User, cmd.PrivateKey)
	if err != nil {
		fmt.Println(err)
		return err
	}

	defer session.Close()

	// todo use pgrep instead
	result, err := session.Output("ps auxx | grep pg_upgrade")

	output := string(result)
	if err != nil && err != io.EOF {
		msg := "cannot run pgrep command on remote host, output: " + output + "\nError: " + err.Error()
		fmt.Println(msg)
		return errors.New(msg)
	}

	shellParser := ShellParser{Output: output}
	addNot := ""
	if !shellParser.IsPgUpgradeRunning() {
		addNot = "not "
	}
	fmt.Printf("pg_upgrade is %srunning on host %s", addNot, cmd.Host)

	return nil
}

func (cmd MonitorCommand) assurePrivateKeyPath(private_key string) string {
	if private_key == "" {
		usr, err := user.Current()
		if err != nil {
			// todo
			panic("cannot get current user directory")
		}
		return usr.HomeDir + "/.ssh/id_rsa"
	}
	return private_key
}
