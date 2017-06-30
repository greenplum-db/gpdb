package config

import (
	"os"

	"encoding/json"
	"gp_upgrade/utils"
)

type Store interface {
	Load(rows utils.RowsWrapper) error
	Write() error
}

type Writer struct {
	TableJsonData []map[string]interface{}
	Formatter     Formatter
	FileWriter    FileWriter
}

func NewWriter() *Writer {
	return &Writer{
		Formatter:  NewJsonFormatter(),
		FileWriter: NewRealFileWriter(),
	}
}

func (configWriter *Writer) Load(rows utils.RowsWrapper) error {
	var err error
	configWriter.TableJsonData, err = translateColumnsIntoGenericStructure(rows)
	return err
}

func (configWriter Writer) Write() error {
	jsonData, err := json.Marshal(configWriter.TableJsonData)
	if err != nil {
		return err
	}

	pretty, err := configWriter.Formatter.Format(jsonData)
	if err != nil {
		return err
	}

	err = os.MkdirAll(GetConfigDir(), 0700)
	if err != nil {
		return err
	}
	f, err := os.Create(GetConfigFilePath())
	if err != nil {
		return err
	}
	defer f.Close()

	err = configWriter.FileWriter.Write(f, pretty)
	return err
}

func translateColumnsIntoGenericStructure(rows utils.RowsWrapper) ([]map[string]interface{}, error) {
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
		err = rows.Scan(valuePtrs...)
		if err != nil {
			return nil, err
		}
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
