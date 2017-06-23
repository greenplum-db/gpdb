package commands

type AllCommands struct {
	Monitor MonitorCommand `command:"monitor" alias:"m" description:"Monitor Greenplum upgrade process"`
	Check   CheckCommand   `command:"check" alias:"c" description:"collects information and validates the target Greenplum installation can be upgraded" subcommands-optional:"true"`
	Version VersionCommand `command:"version" alias:"v" description:"Version of gp_upgrade"`
}

var ALL AllCommands
