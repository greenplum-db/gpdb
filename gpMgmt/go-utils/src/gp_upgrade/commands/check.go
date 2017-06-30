package commands

import (
	"gp_upgrade/config"

	_ "github.com/lib/pq"

	"gp_upgrade/db"

	"gp_upgrade/utils"

	"github.com/pkg/errors"
)

type CheckCommand struct {
	Object_count ObjectCountCommand  `command:"object-count" alias:"oc" description:"count database objects and numeric objects"`
	GPDB_version CheckVersionCommand `command:"version" alias:"ver" description:"validate current version is upgradable"`

	Master_host string `long:"master-host" required:"no" description:"Domain name or IP of host"`
	Master_port int    `long:"master-port" required:"no" default:"15432" description:"Port for master database"`
}

func (cmd CheckCommand) Execute([]string) error {
	// to work around a bug in go-flags, where an attribute is required in both parent and child command,
	// we make that attribute optional in the command struct used by go-flags
	// but enforce the requirement in our code here.
	if cmd.Master_host == "" {
		return errors.New("the required flag '--master-host' was not specified")
	}

	dbConn := db.NewDBConn(cmd.Master_host, cmd.Master_port, "template1")
	return cmd.execute(dbConn, config.NewWriter())
}

func (cmd CheckCommand) execute(dbConnector db.DBConnector, writer config.Store) error {

	err := dbConnector.Connect()
	if err != nil {
		return utils.DatabaseConnectionError{Parent: err}
	}

	defer dbConnector.Close()

	rows, err := dbConnector.GetConn().Query(`select dbid, content, role, preferred_role,
	mode, status, port, hostname, address, replication_port, san_mounts, fsefsoid, fsedbid, fselocation
	from gp_segment_configuration, pg_filespace_entry
	where fsedbid = dbid`)

	if err != nil {
		return err
	}
	defer rows.Close()

	err = writer.Load(rows)
	if err != nil {
		return err
	}

	err = writer.Write()
	return err
}
