package commands

import (
	"database/sql"
	"fmt"
	"os"

	"encoding/json"

	"bytes"

	// must have sqlite3 for testing
	_ "github.com/lib/pq"
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

	tableData, err := cmd.translateColumnsIntoGenericListStructureForJson(rows)
	if err != nil {
		return err
	}

	jsonData, err := json.Marshal(tableData)
	if err != nil {
		return err
	}

	upgrade_config_dir := os.Getenv("HOME") + "/.gp_upgrade"
	err = os.MkdirAll(upgrade_config_dir, 0700)
	if err != nil {
		return err
	}
	f, err := os.Create(upgrade_config_dir + "/cluster_config.json")
	if err != nil {
		return err
	}
	defer f.Close()

	pretty, err := cmd.prettyJson(jsonData)
	if err != nil {
		return err
	}

	_, err = f.Write(pretty)
	if err != nil {
		return err
	}

	return nil
}

func (cmd CheckCommand) prettyJson(b []byte) ([]byte, error) {
	var out bytes.Buffer
	err := json.Indent(&out, b, "", "  ")
	return out.Bytes(), err
}

func (cmd CheckCommand) translateColumnsIntoGenericListStructureForJson(rows *sql.Rows) ([]map[string]interface{}, error) {
	columns, err := rows.Columns()
	if err != nil {
		return nil, err
	}
	count := len(columns)
	tableData := make([]map[string]interface{}, 0)
	values := make([]interface{}, count)
	valuePtrs := make([]interface{}, count)
	for rows.Next() {
		for i := 0; i < count; i++ {
			valuePtrs[i] = &values[i]
		}
		rows.Scan(valuePtrs...)
		entry := make(map[string]interface{})
		for i, col := range columns {
			var v interface{}
			val := values[i]
			b, ok := val.([]byte)
			if ok {
				v = string(b)
			} else {
				v = val
			}
			entry[col] = v
		}
		tableData = append(tableData, entry)
	}
	return tableData, nil
}
