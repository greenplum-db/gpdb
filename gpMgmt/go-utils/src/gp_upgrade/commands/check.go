package commands

import (
	"fmt"

	"database/sql"
	"gp_upgrade/config"

	_ "github.com/lib/pq"
	// must have sqlite3 for testing
	_ "github.com/mattn/go-sqlite3"
)

type ObjectCountCommand struct {
	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
	Database_name   string `long:"database-name" default:"template1" hidden:"true"`
}

type CheckCommand struct {
	Object_count ObjectCountCommand `command:"object-count" alias:"oc" description:"stuff happened here"`

	Master_host string `long:"master-host" required:"yes" description:"Domain name or IP of host"`
	Master_port int    `long:"master-port" required:"no" default:"5432" description:"Port for master database"`

	// for testing only, so using hidden:"true"
	Database_type   string `long:"database_type" default:"postgres" hidden:"true"`
	Database_config string `long:"database_config_file" hidden:"true"`
	Database_name   string `long:"database-name" default:"template1" hidden:"true"`
}

const (
	host = "localhost"
	port = 15432
	user = "pivotal"
)

func (cmd CheckCommand) Execute([]string) error {
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
	if err != nil {
		return err
	}

	return nil
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
