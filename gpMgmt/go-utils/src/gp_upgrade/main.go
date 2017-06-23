package main

import (
	"os"
	"runtime/debug"

	"gp_upgrade/commands"
	"gp_upgrade/utils"

	"github.com/jessevdk/go-flags"
)

func main() {
	debug.SetTraceback("all")
	parser := flags.NewParser(&commands.ALL, flags.HelpFlag|flags.PrintErrors)

	_, err := parser.Parse()
	if err != nil {
		os.Exit(utils.GetExitCodeForError(err))
	}
}
