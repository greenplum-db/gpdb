package commands

type AllCommands struct {
	Monitor MonitorCommand `command:"monitor" alias:"m" description:"Monitor Greenplum upgrade process"`
	Check   CheckCommand   `command:"check" alias:"c" description:"Run pre-check before upgrading"`
}

var ALL AllCommands
