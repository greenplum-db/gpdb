package commands

import (
	"fmt"

	"database/sql"
	"gp_upgrade/config"

	_ "github.com/lib/pq"
	// must have sqlite3 for testing
	_ "github.com/mattn/go-sqlite3"
)

type CheckCommand struct {
	Master_host string `long:"master_host" required:"yes" description:"Domain name or IP of host"`
	Master_port int    `long:"master_port" required:"no" default:"5432" description:"Port for master database"`

	// for testing only, so using hidden:"true"
	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
}

const (
	host   = "localhost"
	port   = 15432
	user   = "pivotal"
	dbname = "template1"
)

func (cmd CheckCommand) Execute([]string) error {
	if cmd.Database_config == "" {
		cmd.Database_config = fmt.Sprintf("host=%s port=%d user=%s "+
			"dbname=%s sslmode=disable",
			host, port, user, dbname)
	}

	db, err := sql.Open(cmd.Database_type, cmd.Database_config)
	if err != nil {
		return err
	}
	defer db.Close()

	rows, err := db.Query(`select * from gp_segment_configuration, pg_filespace_entry where fsedbid = dbid`)
	if err != nil {
		return err
	}
	defer rows.Close()

	configWriter, err := config.NewWriter(rows)
	if err != nil {
		return err
	}

	err = configWriter.Write()
	if err != nil {
		return err
	}

	return nil
}
