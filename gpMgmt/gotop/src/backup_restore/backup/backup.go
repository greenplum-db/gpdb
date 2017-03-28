package backup

import (
	"backup_restore/utils"
	"flag"
	"fmt"
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
	fmt.Println("The current time is", utils.CurrentTimestamp())
	fmt.Printf("Database %s is %s\n", connection.DBName, connection.GetDBSize())

	fooArray := make([]struct {
		I int
	}, 0)

	connection.Begin()

	_, err := connection.Exec("SELECT pg_sleep(2)")
	utils.CheckError(err)

	err = connection.Select(&fooArray, "SELECT * FROM foo ORDER BY i")
	utils.CheckError(err)
	for _, datum := range fooArray {
		fmt.Printf("%d\n", datum.I)
	}

	connection.Commit()
}

func DoTeardown() {
	if connection != nil {
		connection.Close()
	}
	// TODO: Add logic for error codes based on whether we Abort()ed or not
}
