package backup

/*
 * This file contains structs and functions related to dumping "pre-data" metadata
 * on the master, which is any metadata that needs to be restored before data is
 * restored, such as table definitions and check constraints.
 */

import (
	"backup_restore/utils"
	"database/sql"
	"fmt"
	"io"
	"sort"
	"strings"
)

type ColumnDefinition struct {
	Num        int
	Name       string
	NotNull    bool
	HasDefault bool
	IsDropped  bool
	TypName    string
	Encoding   sql.NullString
	DefaultVal string
}

type TableDefinition struct {
	DistPolicy      string
	PartDef         string
	PartTemplateDef string
	StorageOpts     string
}

/*
 * This function calls all the functions needed to gather the metadata for a
 * single table and assembles the metadata into ColumnDef and TableDef structs
 * for more convenient handling in the PrintCreateTableStatement() function.
 */
func ConstructDefinitionsForTable(connection *utils.DBConn, table utils.Table) ([]ColumnDefinition, TableDefinition) {
	tableAttributes := GetTableAttributes(connection, table.TableOid)
	tableDefaults := GetTableDefaults(connection, table.TableOid)

	distributionPolicy := GetDistributionPolicy(connection, table.TableOid)
	partitionDef := GetPartitionDefinition(connection, table.TableOid)
	partTemplateDef := GetPartitionTemplateDefinition(connection, table.TableOid)
	storageOptions := GetStorageOptions(connection, table.TableOid)

	columnDefs := ConsolidateColumnInfo(tableAttributes, tableDefaults)
	tableDef := TableDefinition{distributionPolicy, partitionDef, partTemplateDef, storageOptions}
	return columnDefs, tableDef
}

/*
 * This function calls per-table functions to get constraints related to each
 * table, then consolidates them in two slices holding all constraints for all
 * tables.  Two slices are needed because FOREIGN KEY constraints must be dumped
 * after PRIMARY KEY constraints, so they're separated out to be handled last.
 */
func ConstructConstraintsForAllTables(connection *utils.DBConn, tables []utils.Table) ([]string, []string) {
	allConstraints := make([]string, 0)
	allFkConstraints := make([]string, 0)
	for _, table := range tables {
		constraintList := GetConstraints(connection, table.TableOid)
		tableConstraints, tableFkConstraints := ProcessConstraints(table, constraintList)
		allConstraints = append(allConstraints, tableConstraints...)
		allFkConstraints = append(allFkConstraints, tableFkConstraints...)
	}
	return allConstraints, allFkConstraints
}

/*
 * This function zips up table attributes and column default information into
 * one struct, instead of performing an expensive join to get everything in
 * a single query.
 */
func ConsolidateColumnInfo(atts []QueryTableAtts, defs []QueryTableDefault) []ColumnDefinition {
	colDefs := make([]ColumnDefinition, 0)
	/*
	 * The queries to get attributes and defaults ORDER BY oid and then attribute
	 * number, so we can assume the arrays are in the same order without sorting.
	 */
	j := 0
	for i := range atts {
		defaultVal := ""
		if atts[i].AttHasDefault {
			for j < len(defs) {
				if atts[i].AttNum == defs[j].AdNum {
					defaultVal = defs[j].DefaultVal
					break
				}
				j++
			}
		}
		colDef := ColumnDefinition{
			Num:        atts[i].AttNum,
			Name:       atts[i].AttName,
			NotNull:    atts[i].AttNotNull,
			HasDefault: atts[i].AttHasDefault,
			IsDropped:  atts[i].AttIsDropped,
			TypName:    atts[i].AttTypName,
			Encoding:   atts[i].AttEncoding,
			DefaultVal: defaultVal,
		}
		colDefs = append(colDefs, colDef)
	}
	return colDefs
}

/*
 * There's no built-in function to generate constraint definitions like there is for other types of
 * metadata, so this function constructs them.
 */
func ProcessConstraints(table utils.Table, constraints []QueryConstraint) ([]string, []string) {
	alterStr := fmt.Sprintf("\n\nALTER TABLE ONLY %s ADD CONSTRAINT", table.ToString())
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

func PrintCreateDatabaseStatement(predataFile io.Writer) {
	fmt.Fprintf(predataFile, "\n\nCREATE DATABASE %s;", utils.QuoteIdent(connection.DBName))
}

/*
 * This function prints CREATE TABLE statements in a format very similar to pg_dump.  Unlike pg_dump,
 * however, table names are printed fully qualified with their schemas instead of relying on setting
 * the search_path; this will aid in later filtering to include or exclude certain tables during the
 * backup process, and allows customers to copy just the CREATE TABLE block in order to use it directly.
 */
func PrintCreateTableStatement(predataFile io.Writer, table utils.Table, columnDefs []ColumnDefinition, tableDef TableDefinition) {
	fmt.Fprintf(predataFile, "\n\nCREATE TABLE %s (\n", table.ToString())
	lines := make([]string, 0)
	for _, column := range columnDefs {
		if !column.IsDropped {
			line := fmt.Sprintf("\t%s %s", utils.QuoteIdent(column.Name), column.TypName)
			if column.HasDefault {
				line += fmt.Sprintf(" DEFAULT %s", column.DefaultVal)
			}
			if column.NotNull {
				line += " NOT NULL"
			}
			if column.Encoding.Valid {
				line += fmt.Sprintf(" ENCODING (%s)", column.Encoding.String)
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

func PrintConstraintStatements(predataFile io.Writer, constraints []string, fkConstraints []string) {
	sort.Strings(constraints)
	sort.Strings(fkConstraints)
	for _, constraint := range constraints {
		fmt.Fprintln(predataFile, constraint)
	}
	for _, constraint := range fkConstraints {
		fmt.Fprintln(predataFile, constraint)
	}
}

func PrintCreateSchemaStatements(predataFile io.Writer, schemas []utils.DBObject) {
	for _, schema := range schemas {
		fmt.Fprintln(predataFile)
		if schema.ObjName != "public" {
			fmt.Fprintf(predataFile, "\nCREATE SCHEMA %s;", schema.ToString())
		}
		if schema.ObjComment.Valid {
			fmt.Fprintf(predataFile, "\nCOMMENT ON SCHEMA %s IS '%s';", schema.ToString(), schema.ObjComment.String)
		}
	}
}

func GetAllSequenceDefinitions(connection *utils.DBConn) []QuerySequence {
	allSequences := GetAllSequences(connection)
	sequenceDefs := make([]QuerySequence, 0)
	for _, sequence := range allSequences {
		sequenceDef := GetSequence(connection, sequence.ObjName)
		sequenceDefs = append(sequenceDefs, sequenceDef)
	}
	return sequenceDefs
}

/*
 * This function is largely derived from the dumpSequence() function in pg_dump.c.  The values of
 * minVal and maxVal come from SEQ_MINVALUE and SEQ_MAXVALUE, defined in include/commands/sequence.h.
 */
func PrintCreateSequenceStatements(predataFile io.Writer, sequences []QuerySequence) {
	maxVal := int64(9223372036854775807)
	minVal := int64(-9223372036854775807)
	for _, sequence := range sequences {
		fmt.Fprintln(predataFile, "\n\nCREATE SEQUENCE", sequence.Name)
		if !sequence.IsCalled {
			fmt.Fprintln(predataFile, "\tSTART WITH", sequence.LastVal)
		}
		fmt.Fprintln(predataFile, "\tINCREMENT BY", sequence.Increment)

		if !((sequence.MaxVal == maxVal && sequence.Increment > 0) || (sequence.MaxVal == -1 && sequence.Increment < 0)) {
			fmt.Fprintln(predataFile, "\tMAXVALUE", sequence.MaxVal)
		} else {
			fmt.Fprintln(predataFile, "\tNO MAXVALUE")
		}
		if !((sequence.MinVal == minVal && sequence.Increment < 0) || (sequence.MinVal == 1 && sequence.Increment > 0)) {
			fmt.Fprintln(predataFile, "\tMINVALUE", sequence.MinVal)
		} else {
			fmt.Fprintln(predataFile, "\tNO MINVALUE")
		}
		cycleStr := ""
		if sequence.IsCycled {
			cycleStr = "\n\tCYCLE"
		}
		fmt.Fprintf(predataFile, "\tCACHE %d%s;", sequence.CacheVal, cycleStr)

		fmt.Fprintf(predataFile, "\n\nSELECT pg_catalog.setval('%s', %d, %v);\n", sequence.Name, sequence.LastVal, sequence.IsCalled)
	}
}

func PrintSessionGUCs(predataFile io.Writer, gucs QuerySessionGUCs) {
	fmt.Fprintf(predataFile, `SET statement_timeout = 0;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET client_encoding = '%s';
SET standard_conforming_strings = %s;
SET default_with_oids = %s
`, gucs.ClientEncoding, gucs.StdConformingStrings, gucs.DefaultWithOids)
}

func PrintDatabaseGUCs(predataFile io.Writer, gucs []QueryDatabaseGUC, dbname string) {
	for _, guc := range gucs {
		fmt.Fprintf(predataFile, "\nALTER DATABASE %s SET %s;", dbname, guc.DatConfig)
	}
}
