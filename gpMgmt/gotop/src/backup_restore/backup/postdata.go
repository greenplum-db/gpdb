package backup

import (
	"backup_restore/utils"
	"fmt"
	"io"
	"sort"
)

func GetIndexesForAllTables(connection *utils.DBConn, tables []utils.Table) []string {
	indexes := make([]string, 0)
	for _, table := range tables {
		indexList := GetIndexDefinitions(connection, table.TableOid)
		for _, index := range indexList {
			indexes = append(indexes, fmt.Sprintf("\n\n%s;", index.IndexDef))
		}
	}
	return indexes
}

func PrintCreateIndexStatements(postdataFile io.Writer, indexes []string) {
	sort.Strings(indexes)
	for _, index := range indexes {
		fmt.Fprintln(postdataFile, index)
	}
}
