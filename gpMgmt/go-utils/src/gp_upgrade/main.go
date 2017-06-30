package main

import (
	"os"
	"runtime/debug"

	"gp_upgrade/commands"
	"gp_upgrade/utils"

	"github.com/jessevdk/go-flags"
)

type AllCommands struct {
	Monitor commands.MonitorCommand `command:"monitor" alias:"m" description:"Monitor Greenplum upgrade process"`
	Check   commands.CheckCommand   `command:"check" alias:"c" description:"collects information and validates the target Greenplum installation can be upgraded" subcommands-optional:"true"`
	Version commands.VersionCommand `command:"version" alias:"v" description:"Version of gp_upgrade"`
}

var ALL AllCommands

func main() {
	debug.SetTraceback("all")
	parser := flags.NewParser(&ALL, flags.HelpFlag|flags.PrintErrors)

	_, err := parser.Parse()
	if err != nil {
		os.Exit(utils.GetExitCodeForError(err))
	}
}
