package commands

type UpCommand struct {
	Monitor MonitorCommand `command:"monitor" alias:"m" description:"Monitor Greenplum upgrade process"`
}

var UP UpCommand
