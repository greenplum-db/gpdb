package commands

import (
	"fmt"
)

// This global var GpdbVersion should have a value set at build time.
// see Makefile for -ldflags "-X etc"
var GpdbVersion = ""

type VersionCommand struct{}

const DefaultGpdbVersion = "gp_upgrade unknown version"

func (cmd VersionCommand) Execute([]string) error {
	fmt.Println(versionString())
	return nil
}

func versionString() string {
	if GpdbVersion == "" {
		return DefaultGpdbVersion
	} else {
		return "gp_upgrade version " + GpdbVersion
	}
}
