package config

import (
	"bytes"
	"os"

	"database/sql"
	"encoding/json"
)

type ConfigWriter struct {
	tableJsonData []map[string]interface{}
}

// todo all of this file's error paths are not unit tested. There are 3 stories in backlog to do them.  LAH 28 Mar 2017
func NewConfigWriter(rows *sql.Rows) (*ConfigWriter, error) {
	tableData, err := translateColumnsIntoGenericListStructureForJson(rows)

	if err != nil {
		return nil, err
	}

	return &ConfigWriter{tableJsonData: tableData}, nil
}

func (cmd ConfigWriter) ParseAndWriteConfig() error {
	jsonData, err := json.Marshal(cmd.tableJsonData)
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

	pretty, err := prettyJson(jsonData)
	if err != nil {
		return err
	}

	_, err = f.Write(pretty)
	return err
}

func prettyJson(b []byte) ([]byte, error) {
	var out bytes.Buffer
	err := json.Indent(&out, b, "", "  ")
	return out.Bytes(), err
}

func translateColumnsIntoGenericListStructureForJson(rows *sql.Rows) ([]map[string]interface{}, error) {
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
