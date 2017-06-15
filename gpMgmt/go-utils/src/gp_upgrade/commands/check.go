package commands

import (
	"database/sql"
	"fmt"
	"gp_upgrade/config"

	_ "github.com/lib/pq"

	"io"

	"gp_upgrade/db"
	"os"

	"regexp"

	"gp_upgrade/utils"

	"github.com/cppforlife/go-semi-semantic/version"
	_ "github.com/mattn/go-sqlite3"
	"github.com/pkg/errors"
)

type ObjectCountCommand struct {
	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
	Database_name   string `long:"database-name" default:"template1" hidden:"true"`
}

type VersionCommand struct {
	Master_host   string `long:"master-host" required:"yes" description:"Domain name or IP of host"`
	Master_port   int    `long:"master-port" required:"no" default:"15432" description:"Port for master database"`
	Database_name string `long:"database-name" default:"template1" hidden:"true"`
}

type CheckCommand struct {
	Object_count ObjectCountCommand `command:"object-count" alias:"oc" description:"count database objects and numeric objects"`
	GPDB_version VersionCommand     `command:"version" alias:"ver" description:"validate current version is upgradable"`

	Master_host string `long:"master-host" required:"no" description:"Domain name or IP of host"`
	Master_port int    `long:"master-port" required:"no" default:"15432" description:"Port for master database"`

	// for testing only, so using hidden:"true"
	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
	Database_name   string `long:"database-name" default:"template1" hidden:"true"`
}

//TODO: these are just defaults for dev work
//TODO: will be replaced as we adopt DBConn
const (
	host = "localhost"
	port = 15432
	user = "pivotal"

	MINIMUM_VERSION = "4.3.9.0"
)

func (cmd CheckCommand) Execute([]string) error {
	// to work around a bug in go-flags, where an attribute is required in both parent and child command,
	// we make that attribute optional in the command struct used by go-flags
	// but enforce the requirement in our code here.
	if cmd.Master_host == "" {
		return errors.New("the required flag '--master-host' was not specified")
	}
	if cmd.Database_config == "" {
		cmd.Database_config = fmt.Sprintf("host=%s port=%d user=%s "+
			"dbname=%s sslmode=disable",
			host, port, user, cmd.Database_name)
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
	return err
}

func (cmd ObjectCountCommand) Execute([]string) error {
	if cmd.Database_config == "" {
		cmd.Database_config = fmt.Sprintf("host=%s port=%d user=%s "+
			"dbname=%s sslmode=disable",
			host, port, user, cmd.Database_name)
	}
	db, err := sql.Open(cmd.Database_type, cmd.Database_config)
	if err != nil {
		return err
	}
	defer db.Close()

	/* "::" casting is specific to Postgres.
	 * changed sql to an ANSI standard casting
	 */
	aoCoTableQueryCount := `
	-- COUNT THE NUMBER OF APPEND ONLY OBJECTS ON THE SYSTEM
	SELECT COUNT(*)
	  FROM pg_class c
	  JOIN pg_namespace n ON c.relnamespace = n.oid
	WHERE c.relkind = cast('r' as CHAR)                       -- All tables (including partitions)
	  AND c.relstorage IN ('a','c')                           -- AO / CO
	  AND n.nspname NOT LIKE 'pg_temp_%'                      -- not temp tables
	  AND c.oid > 16384                                       -- No system tables
	  AND (c.relnamespace > 16384 OR n.nspname = 'public')    -- No system schemas, but include 'public'
	  AND (NOT relhassubclass                                 -- not partition parent tables
	       OR ( relhassubclass
		    AND NOT EXISTS ( SELECT oid FROM pg_partition_rule p WHERE c.oid = p.parchildrelid )
		    AND NOT EXISTS ( SELECT oid FROM pg_partition p WHERE c.oid = p.parrelid )
		)
	);
	`

	heapTableQueryCount := `
	SELECT COUNT(*)
	  FROM pg_class c
	  JOIN pg_namespace n ON c.relnamespace = n.oid
	WHERE c.relkind = cast('r' as CHAR)                       -- All tables (including partitions)
	  AND c.relstorage NOT IN ('a','c')                       -- NON AO / CO
	  AND n.nspname NOT LIKE 'pg_temp_%'                      -- not temp tables
	  AND c.oid > 16384                                       -- No system tables
	  AND (c.relnamespace > 16384 OR n.nspname = 'public')    -- No system schemas, but include 'public'
	  AND (NOT relhassubclass                                 -- not partition parent tables
	       OR ( relhassubclass
		    AND NOT EXISTS ( SELECT oid FROM pg_partition_rule p WHERE c.oid = p.parchildrelid )
		    AND NOT EXISTS ( SELECT oid FROM pg_partition p WHERE c.oid = p.parrelid )
		)
	  );
	`
	var count string
	err = db.QueryRow(aoCoTableQueryCount).Scan(&count)
	if err != nil {
		return err
	}
	fmt.Println("Number of AO objects -", count)

	err = db.QueryRow(heapTableQueryCount).Scan(&count)
	if err != nil {
		return err
	}
	fmt.Println("Number of heap objects -", count)

	return nil
}

func (cmd VersionCommand) Execute([]string) error {
	dbConn := db.NewDBConn(cmd.Master_host, cmd.Master_port, cmd.Database_name)
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
