package commands

type AllCommands struct {
	Monitor MonitorCommand `command:"monitor" alias:"m" description:"Monitor Greenplum upgrade process"`
}

var ALL AllCommands
