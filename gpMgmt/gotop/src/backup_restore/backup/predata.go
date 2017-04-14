package backup

import (
	"backup_restore/utils"
	"database/sql"
	"fmt"
	"io"
	"sort"
	"strings"
)

type ColumnDefinition struct {
	Num       int
	Name      string
	NotNull   bool
	HasDef    bool
	IsDropped bool
	TypName   string
	Encoding  sql.NullString
	DefVal    string
}

type TableDefinition struct {
	DistPolicy      string
	PartDef         string
	PartTemplateDef string
	StorageOpts     string
}

func ConstructDefinitionsForTable(connection *utils.DBConn, table utils.Table) ([]ColumnDefinition, TableDefinition) {
	tableAttributes := GetTableAttributes(connection, table.Oid)
	tableDefaults := GetTableDefaults(connection, table.Oid)

	distPolicy := GetDistributionPolicy(connection, table.Oid)
	partitionDef := GetPartitionDefinition(connection, table.Oid)
	partTemplateDef := GetPartitionTemplateDefinition(connection, table.Oid)
	storageOpts := GetStorageOptions(connection, table.Oid)

	columnDefs := ConsolidateColumnInfo(tableAttributes, tableDefaults)
	tableDef := TableDefinition{distPolicy, partitionDef, partTemplateDef, storageOpts}
	return columnDefs, tableDef
}

func ConstructConstraintsForAllTables(connection *utils.DBConn, tables []utils.Table) ([]string, []string) {
	allConstraints := make([]string, 0)
	allFkConstraints := make([]string, 0) // separate slice for FOREIGN KEY constraints, since they must be printed after PRIMARY KEY constraints
	for _, table := range tables {
		conList := GetConstraints(connection, table.Oid)
		tableCons, tableFkCons := ProcessConstraints(table, conList)
		allConstraints = append(allConstraints, tableCons...)
		allFkConstraints = append(allFkConstraints, tableFkCons...)
	}
	return allConstraints, allFkConstraints
}

func ConsolidateColumnInfo(atts []QueryTableAtts, defs []QueryTableDef) []ColumnDefinition {
	colDefs := make([]ColumnDefinition, 0)
	// The queries to get attributes and defaults ORDER BY oid and then attribute number, so we can assume the arrays are in the same order without sorting
	j := 0
	for i := range atts {
		defVal := ""
		if atts[i].AttHasDef {
			for j < len(defs) {
				if atts[i].AttNum == defs[j].AdNum {
					defVal = defs[j].DefVal
					break
				}
				j++
			}
		}
		colDef := ColumnDefinition{
			Num:       atts[i].AttNum,
			Name:      atts[i].AttName,
			NotNull:   atts[i].AttNotNull,
			HasDef:    atts[i].AttHasDef,
			IsDropped: atts[i].AttIsDropped,
			TypName:   atts[i].AttTypName,
			Encoding:  atts[i].AttEncoding,
			DefVal:    defVal,
		}
		colDefs = append(colDefs, colDef)
	}
	return colDefs
}

func ProcessConstraints(table utils.Table, constraints []QueryConstraint) ([]string, []string) {
	alterStr := fmt.Sprintf("\n\nALTER TABLE ONLY %s ADD CONSTRAINT", table.ToFQN())
	cons := make([]string, 0)
	fkCons := make([]string, 0)
	for _, constraint := range constraints {
		conStr := fmt.Sprintf("%s %s %s;", alterStr, constraint.ConName, constraint.ConDef)
		if constraint.ConType == "f" {
			fkCons = append(fkCons, conStr)
		} else {
			cons = append(cons, conStr)
		}
	}
	return cons, fkCons
}

func PrintCreateTableStatement(predataFile io.Writer, table utils.Table, columnDefs []ColumnDefinition, tableDef TableDefinition) {
	fmt.Fprintf(predataFile, "\n\nCREATE TABLE %s (\n", table.ToFQN())
	lines := make([]string, 0)
	for _, col := range columnDefs {
		if !col.IsDropped {
			line := fmt.Sprintf("\t%s %s", col.Name, col.TypName)
			if col.HasDef {
				line += fmt.Sprintf(" DEFAULT %s", col.DefVal)
			}
			if col.NotNull {
				line += " NOT NULL"
			}
			if col.Encoding.Valid {
				line += fmt.Sprintf(" ENCODING (%s)", col.Encoding.String)
			}
			lines = append(lines, line)
		}
	}
	if len(lines) > 0 {
		fmt.Fprintln(predataFile, strings.Join(lines, ",\n"))
	}
	fmt.Fprintf(predataFile, ") ")
	if tableDef.StorageOpts != "" {
		fmt.Fprintf(predataFile, "WITH (%s) ", tableDef.StorageOpts)
	}
	fmt.Fprintf(predataFile, "%s", tableDef.DistPolicy)
	if tableDef.PartDef != "" {
		fmt.Fprintf(predataFile, " %s", strings.TrimSpace(tableDef.PartDef))
	}
	fmt.Fprintln(predataFile, ";")
	if tableDef.PartTemplateDef != "" {
		fmt.Fprintf(predataFile, "%s;\n", strings.TrimSpace(tableDef.PartTemplateDef))
	}
}

func PrintConstraintStatements(predataFile io.Writer, cons []string, fkCons []string) {
	sort.Strings(cons)
	sort.Strings(fkCons)
	for _, con := range cons {
		fmt.Fprintln(predataFile, con)
	}
	for _, con := range fkCons {
		fmt.Fprintln(predataFile, con)
	}
}

func PrintCreateSchemaStatements(predataFile io.Writer, tables []utils.Table) {
	schemas := utils.GetUniqueSchemas(tables)
	for _, schema := range schemas {
		fmt.Fprintf(predataFile, "\n\nCREATE SCHEMA %s;", schema)
	}
}
