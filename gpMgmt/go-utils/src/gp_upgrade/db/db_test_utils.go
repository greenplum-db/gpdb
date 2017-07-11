package db

import (
	"github.com/greenplum-db/gpbackup/testutils"

	"fmt"

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
	connector := NewDBConn("localhost", 0, "testdb")
	gpdbConnStruct := connector.(*GPDBConnector)
	driver := testutils.TestDriver{DBExists: true, RoleExists: true, DB: mockdb, DBName: "testdb", User: "testrole"}
	gpdbConnStruct.driver = driver
	err := connector.Connect()
	if err != nil {
		Fail(fmt.Sprintf("cannot connect to test mock database: %v", err))
	}
	return connector, mock
}
