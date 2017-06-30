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
}

func (cmd ObjectCountCommand) Execute([]string) error {
	dbConn := db.NewDBConn(cmd.Master_host, cmd.Master_port, "template1")
	return cmd.execute(dbConn, os.Stdout)
}

func (cmd ObjectCountCommand) execute(dbConnector db.DBConnector, outputWriter io.Writer) error {
	err := dbConnector.Connect()
	if err != nil {
		return utils.DatabaseConnectionError{Parent: err}
	}
	defer dbConnector.Close()

	var count string
	connection := dbConnector.GetConn()
	err = connection.QueryRow(AO_CO_TABLE_QUERY_COUNT).Scan(&count)
	if err != nil {
		return err
	}
	fmt.Fprintf(outputWriter, "Number of AO objects - %v\n", count)

	err = connection.QueryRow(HEAP_TABLE_QUERY_COUNT).Scan(&count)
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
	AO_CO_TABLE_QUERY_COUNT = `
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

	HEAP_TABLE_QUERY_COUNT = `
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
