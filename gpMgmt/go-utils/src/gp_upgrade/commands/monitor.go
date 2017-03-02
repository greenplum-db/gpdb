package commands

import (
	"fmt"
	"io"
)

type MonitorCommand struct {
	Host       string `long:"host" required:"yes" description:"Domain name or IP of host"`
	Port       int    `long:"port" default:"22" description:"SSH port for communication"`
	Segment_id string `long:"segment_id" required:"yes" description:"ID of segment to monitor"`
}

func isPgUpgradeRunning(output, segment_id string) bool {
	return false
}

func (cmd MonitorCommand) Execute([]string) error {
	// todo test err
	connector, _ := GetConnector("ssh")
	session, err := connector.Connect(cmd.Host, cmd.Port)
	if err != nil {
		panic(fmt.Sprintf("cannot connect to host, port: %s, %v\n", cmd.Host, cmd.Port))
	}

	defer session.Close()

	result, err := session.Output("ps auxx | grep pg_upgrade")
	output := string(result)
	if err != nil && err != io.EOF {
		fmt.Println("cannot run ps command on remote host, output: " + output + "\nError: " + err.Error())
	}

	isRunning := isPgUpgradeRunning(output, cmd.Segment_id)
	if !isRunning {
		fmt.Println(fmt.Sprintf("pg_upgrade is not running on host '%s', segment_id '%s'", cmd.Host, cmd.Segment_id))
	}

	// todo parse result -- the response code will be 0 whether or not pg_upgrade is running
	fmt.Println("result: " + output)

	return nil
}
