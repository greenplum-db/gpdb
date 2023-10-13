package main

import (
	"fmt"
	"os"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/cli"
)

func main() {
	root := cli.RootCommand()
	root.SilenceUsage = true
	root.SilenceErrors = true

	err := root.Execute()
	if err != nil {
		// gplog is initialised in the PreRun function in cobra and sometimes when the
		// error is due to the input flags, the cobra pkg would not run the PreRun function.
		// In those cases directly print to the stdout instead of using gplog
		if gplog.GetLogger() != nil {
			gplog.Error(err.Error())
		} else {
			fmt.Println(err)
			err = root.Help(); if err != nil {
				fmt.Println(err.Error())
			}
		}
		os.Exit(1)
	}
}
