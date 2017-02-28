package main

import (
	"fmt"
	"os"
	"runtime/debug"

	"gp_upgrade/commands"

	"github.com/jessevdk/go-flags"
)

func main() {
	debug.SetTraceback("all")
	parser := flags.NewParser(&commands.UP, flags.HelpFlag)

	_, err := parser.Parse()
	if err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(1)
	}
}
