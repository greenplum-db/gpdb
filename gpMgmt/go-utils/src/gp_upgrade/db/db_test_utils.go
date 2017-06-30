package db

import (
	"github.com/greenplum-db/gpbackup/testutils"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

func CreateMockDB() (*sqlx.DB, sqlmock.Sqlmock) {
	db, mock, err := sqlmock.New()
	mockdb := sqlx.NewDb(db, "sqlmock")
	if err != nil {
		Fail("Could not create mock database connection")
	}
	return mockdb, mock
}

func CreateMockDBConn(masterHost string, masterPort int) (DBConnector, sqlmock.Sqlmock) {
	mockdb, mock := CreateMockDB()
	gpdbConnector := NewDBConn(masterHost, masterPort, "testdb")
	gpdbConnStruct := gpdbConnector.(*GPDBConnector)
	gpdbConnStruct.driver = testutils.TestDriver{DBExists: true, DB: mockdb, DBName: "testdb"}
	connection := gpdbConnector.GetConn()
	if connection != nil && connection.Stats().OpenConnections > 0 {
		Fail("connection before connect is called")
	}
	return gpdbConnector, mock
}
