package commands

import (
	"fmt"

	_ "github.com/lib/pq"

	"io"

	"gp_upgrade/db"
	"os"

	"regexp"

	"gp_upgrade/utils"

	"github.com/cppforlife/go-semi-semantic/version"
)

type VersionCommand struct {
	Master_host   string `long:"master-host" required:"yes" description:"Domain name or IP of host"`
	Master_port   int    `long:"master-port" required:"no" default:"15432" description:"Port for master database"`
	Database_name string `long:"database-name" default:"template1" hidden:"true"`
}

const (
	MINIMUM_VERSION = "4.3.9.0"
)

func (cmd VersionCommand) Execute([]string) error {
	dbConn := db.NewDBConn(cmd.Master_host, cmd.Master_port, cmd.Database_name, "", "")
	return cmd.execute(dbConn, os.Stdout)
}

func (cmd VersionCommand) execute(dbConn *db.DBConn, outputWriter io.Writer) error {
	err := dbConn.Connect()
	if err != nil {
		return utils.DatabaseConnectionError{Parent: err}
	}
	defer dbConn.Close()

	re := regexp.MustCompile("Greenplum Database (.*) build")

	var row string
	err = dbConn.Conn.QueryRow("SELECT version()").Scan(&row)
	if err != nil {
		return err
	}

	version_string := re.FindStringSubmatch(row)[1]
	version_object := version.MustNewVersionFromString(version_string)

	if version_object.IsGt(version.MustNewVersionFromString(MINIMUM_VERSION)) {
		fmt.Fprintf(outputWriter, "gp_upgrade: Version Compatibility Check [OK]\n")
	} else {
		fmt.Fprintf(outputWriter, "gp_upgrade: Version Compatibility Check [Failed]\n")
	}
	return err
}
