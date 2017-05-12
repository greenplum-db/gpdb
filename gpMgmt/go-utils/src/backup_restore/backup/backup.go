package backup

import (
	"backup_restore/utils"
	"flag"
	"fmt"
	"os"
)

var connection *utils.DBConn

var dbname = flag.String("dbname", "", "The database to be backed up")

func DoValidation() {
	flag.Parse()
}

func DoSetup() {
	connection = utils.NewDBConn(*dbname)
	connection.Connect()
	connection.Exec("SET application_name TO 'gpbackup'")
}

func DoBackup() {
	fmt.Println("-- The current time is", utils.CurrentTimestamp())
	fmt.Printf("-- Database %s is %s\n", connection.DBName, connection.GetDBSize())

	connection.Begin()

	allConstraints := make([]string, 0)
	allFkConstraints := make([]string, 0) // Slice for FOREIGN KEY allConstraints, since they must be printed after PRIMARY KEY allConstraints
	tables := GetAllUserTables(connection)
	PrintCreateSchemaStatements(os.Stdout, tables)
	for _, table := range tables {
		tableAttributes := GetTableAttributes(connection, table.Oid)
		tableDefaults := GetTableDefaults(connection, table.Oid)

		distPolicy := GetDistributionPolicy(connection, table.Oid)
		partitionDef := GetPartitionDefinition(connection, table.Oid)
		partTemplateDef := GetPartitionTemplateDefinition(connection, table.Oid)
		storageOpts := GetStorageOptions(connection, table.Oid)

		columnDefs := ConsolidateColumnInfo(tableAttributes, tableDefaults)
		tableDef := TableDefinition{distPolicy, partitionDef, partTemplateDef, storageOpts}
		PrintCreateTableStatement(os.Stdout, table, columnDefs, tableDef) // TODO: Change to write to file
	}
	for _, table := range tables {
		conList := GetConstraints(connection, table.Oid)
		tableCons, tableFkCons := ProcessConstraints(table, conList)
		allConstraints = append(allConstraints, tableCons...)
		allFkConstraints = append(allFkConstraints, tableFkCons...)
	}
	PrintConstraintStatements(os.Stdout, allConstraints, allFkConstraints) // TODO: Change to write to file

	connection.Commit()
}

func DoTeardown() {
	if connection != nil {
		connection.Close()
	}
	// TODO: Add logic for error codes based on whether we Abort()ed or not
}
