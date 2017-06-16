package commands

import (
	"fmt"

	_ "github.com/lib/pq"

	"io"

	"gp_upgrade/db"
	"os"

	"gp_upgrade/utils"
)

type ObjectCountCommand struct {
	Master_host string `long:"master-host" required:"yes" description:"Domain name or IP of host"`
	Master_port int    `long:"master-port" required:"no" default:"15432" description:"Port for master database"`

	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
	Database_name   string `long:"database-name" default:"template1" hidden:"true"`
}

func (cmd ObjectCountCommand) Execute([]string) error {
	dbConn := db.NewDBConn(cmd.Master_host, cmd.Master_port, cmd.Database_name, cmd.Database_type, cmd.Database_config)
	return cmd.execute(dbConn, os.Stdout)
}

func (cmd ObjectCountCommand) execute(dbConn *db.DBConn, outputWriter io.Writer) error {
	err := dbConn.Connect()
	if err != nil {
		return utils.DatabaseConnectionError{Parent: err}
	}
	defer dbConn.Close()

	var count string
	err = dbConn.Conn.QueryRow(aoCoTableQueryCount).Scan(&count)
	if err != nil {
		return err
	}
	fmt.Fprintf(outputWriter, "Number of AO objects - %v\n", count)

	err = dbConn.Conn.QueryRow(heapTableQueryCount).Scan(&count)
	if err != nil {
		return err
	}
	fmt.Fprintf(outputWriter, "Number of heap objects - %v\n", count)

	return nil
}

const (
	/* "::" casting is specific to Postgres.
	 * changed sql to an ANSI standard casting
		-- COUNT THE NUMBER OF APPEND ONLY OBJECTS ON THE SYSTEM
	*/
	aoCoTableQueryCount = `
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

	heapTableQueryCount = `
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
)
