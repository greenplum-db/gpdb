package backup

import (
	"backup_restore/utils"
	"flag"
	"fmt"
)

var (
	connection *utils.DBConn
	logger     *utils.Logger
)

var ( // Command-line flags
	dbname  = flag.String("dbname", "", "The database to be backed up")
	debug   = flag.Bool("debug", false, "Print verbose and debug log messages")
	dumpDir = flag.String("dumpdir", "", "The directory to which all dump files will be written")
	verbose = flag.Bool("verbose", false, "Print verbose log messages")
)

func DoInit() { // Handles setup that can be done before parsing flags
	SetLogger(utils.InitializeLogging("gpbackup", "", utils.LOGINFO))
}

func SetLogger(log *utils.Logger) {
	logger = log
}

func DoValidation() {
	flag.Parse()
}

func DoSetup() { // Handles setup that must be done after parsing flags
	if *debug {
		logger.SetVerbosity(utils.LOGDEBUG)
	} else if *verbose {
		logger.SetVerbosity(utils.LOGVERBOSE)
	}
	connection = utils.NewDBConn(*dbname)
	connection.Connect()
	connection.Exec("SET application_name TO 'gpbackup'")

	utils.SetDumpTimestamp()

	if *dumpDir != "" {
		utils.BaseDumpDir = *dumpDir
	}
	logger.Verbose("Creating dump directories")
	utils.DumpPathFmtStr = fmt.Sprintf("%s/backups/%s/%s", utils.BaseDumpDir, utils.DumpDatestamp, utils.DumpTimestamp)
	segConfig := utils.GetSegmentConfiguration(connection)
	utils.CreateDumpDirs(segConfig)
}

func DoBackup() {
	logger.Info("Dump Key = %s", utils.CurrentTimestamp())
	logger.Info("Dump Database = %s", utils.QuoteIdent(connection.DBName))
	logger.Info("Database Size = %s", connection.GetDBSize())

	masterDumpDir := utils.SegDirMap[-1]
	predataFilename := fmt.Sprintf("%s/predata.sql", masterDumpDir)
	postdataFilename := fmt.Sprintf("%s/postdata.sql", masterDumpDir)

	connection.Begin()
	tables := GetAllUserTables(connection)

	logger.Info("Writing pre-data metadata to %s", predataFilename)
	backupPredata(predataFilename, tables)
	logger.Info("Pre-data metadata dump complete")

	logger.Info("Writing data to file")
	backupData(tables)
	logger.Info("Data dump complete")

	logger.Info("Writing post-data metadata to %s", postdataFilename)
	backupPostdata(postdataFilename, tables)
	logger.Info("Post-data metadata dump complete")

	connection.Commit()
}

func backupPredata(filename string, tables []utils.Table) {
	predataFile := utils.MustOpenFile(filename)

	logger.Verbose("Writing session GUCs to predata file")
	gucs := GetSessionGUCs(connection)
	PrintSessionGUCs(predataFile, gucs)

	logger.Verbose("Writing CREATE DATABASE statement to predata file")
	PrintCreateDatabaseStatement(predataFile)

	logger.Verbose("Writing CREATE SCHEMA statements to predata file")
	schemas := GetAllUserSchemas(connection)
	PrintCreateSchemaStatements(predataFile, schemas)

	logger.Verbose("Writing CREATE TABLE statements to predata file")
	for _, table := range tables {
		columnDefs, tableDef := ConstructDefinitionsForTable(connection, table)
		PrintCreateTableStatement(predataFile, table, columnDefs, tableDef)
	}

	logger.Verbose("Writing ADD CONSTRAINT statements to predata file")
	allConstraints, allFkConstraints := ConstructConstraintsForAllTables(connection, tables)
	PrintConstraintStatements(predataFile, allConstraints, allFkConstraints)

	logger.Verbose("Writing CREATE SEQUENCE statements to predata file")
	sequenceDefs := GetAllSequenceDefinitions(connection)
	PrintCreateSequenceStatements(predataFile, sequenceDefs)
}

func backupData(tables []utils.Table) {
	for _, table := range tables {
		logger.Verbose("Writing data for table %s to file", table.ToString())
		CopyTableOut(connection, table)
	}
	logger.Verbose("Writing table map file to %s/gpbackup_%s_table_map", utils.SegDirMap[-1], utils.DumpTimestamp)
	WriteTableMapFile(tables)
}

func backupPostdata(filename string, tables []utils.Table) {
	postdataFile := utils.MustOpenFile(filename)

	logger.Verbose("Writing session GUCs to predata file")
	gucs := GetSessionGUCs(connection)
	PrintSessionGUCs(postdataFile, gucs)

	logger.Verbose("Writing CREATE INDEX statements to postdata file")
	indexes := GetIndexesForAllTables(connection, tables)
	PrintCreateIndexStatements(postdataFile, indexes)
}

func DoTeardown() {
	if r := recover(); r != nil {
		fmt.Println(r)
	}
	if connection != nil {
		connection.Close()
	}
	// TODO: Add logic for error codes based on whether we Abort()ed or not
}
