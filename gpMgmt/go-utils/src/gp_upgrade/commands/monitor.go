package commands

import (
	"errors"
	"fmt"
	"io"
	//"regexp"
)

type MonitorCommand struct {
	Host       string `long:"host" required:"yes" description:"Domain name or IP of host"`
	Port       int    `long:"port" default:"22" description:"SSH port for communication"`
	Segment_id string `long:"segment_id" required:"yes" description:"ID of segment to monitor"`
}

func (cmd MonitorCommand) Execute([]string) error {
	// TODO idea: refactor away from the connector factory pattern -- unnecessary seam
	connector, _ := GetConnector("ssh")
	session, err := connector.Connect(cmd.Host, cmd.Port)
	if err != nil {
		fmt.Println(err)
		return err
	}

	defer session.Close()

	// TODO idea: put the output gathering into the shell parsing class? Inject session into there as a parameter?
	result, err := session.Output("ps auxx | grep pg_upgrade")

	output := string(result)
	if err != nil && err != io.EOF {
		msg := "cannot run ps command on remote host, output: " + output + "\nError: " + err.Error()
		fmt.Println(msg)
		return errors.New(msg)
	}

	// the response code will be 0 whether or not pg_upgrade is running
	shellParser := ShellParser{Output: output, Segment_id: cmd.Segment_id}

	isRunning := shellParser.IsPgUpgradeRunning()
	if isRunning {
		fmt.Println("pg_upgrade is running on the host")

	} else {
		fmt.Println(fmt.Sprintf("pg_upgrade is not running on host '%s', segment_id '%s'", cmd.Host, cmd.Segment_id))
	}

	// provide default behavior for now that expresses that for this first story we're always going to say it's not running
	fmt.Println("result: " + output)

	return nil
}
