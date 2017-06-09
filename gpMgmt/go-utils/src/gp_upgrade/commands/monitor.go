package commands

import (
	"errors"
	"fmt"
	"gp_upgrade/config"
	"gp_upgrade/shell_parsers"
	"gp_upgrade/ssh_client"
	"io"
)

type MonitorCommand struct {
	Host       string `long:"host" required:"yes" description:"Domain name or IP of host"`
	Port       int    `long:"port" default:"22" description:"SSH port for communication"`
	User       string `long:"user" default:"gpadmin" description:"Name of user at ssh destination"`
	PrivateKey string `long:"private_key" description:"Private key for ssh destination"`
	Segment_id int    `long:"segment_id" required:"yes" description:"ID of segment to monitor"`
}

func (cmd MonitorCommand) Execute([]string) error {
	var err error
	cmd.PrivateKey, err = ssh_client.NewPrivateKeyGuarantor().Check(cmd.PrivateKey)
	if err != nil {
		return err
	}

	reader := config.Reader{}
	err = reader.Read()
	if err != nil {
		//return err  // TODO
	}
	targetPort := reader.GetPortForSegment(cmd.Segment_id)
	if targetPort == -1 {
		// TODO test
		return errors.New(fmt.Sprintf("Cannot get port for segment_id %s", cmd.Segment_id))
	}

	connector := ssh_client.NewSshConnector()
	session, err := connector.Connect(cmd.Host, cmd.Port, cmd.User, cmd.PrivateKey)
	if err != nil {
		return err
	}

	defer session.Close()

	// pgrep could be used, but it was messy because of exit code 1 when not found;
	// seems nicer with ps to have 0 exit when not found (but not error)
	result, err := session.Output("ps auxx | grep pg_upgrade")

	output := string(result)
	if err != nil && err != io.EOF {
		msg := "cannot run pgrep command on remote host, output: " + output + "\nError: " + err.Error()
		return errors.New(msg)
	}

	addNot := ""
	shellParser := shell_parsers.NewShellParser(output)
	if !shellParser.IsPgUpgradeRunning(targetPort) {
		addNot = "not "
	}
	fmt.Printf("pg_upgrade is %srunning on host %s\n", addNot, cmd.Host)

	return nil
}
