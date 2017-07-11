package db

import (
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

func CreateMockDBConn() (DBConnector, sqlmock.Sqlmock) {
	mockdb, mock := CreateMockDB()
	connector := NewDBConn("localhost", 0, "testdb")
	gpdbConnStruct := connector.(*GPDBConnector)
	driver := TestDriver{DBExists: true, RoleExists: true, DB: mockdb, DBName: "testdb", User: "testrole"}
	gpdbConnStruct.driver = driver
	err := connector.Connect()
	if err != nil {
		Fail(fmt.Sprintf("cannot connect to test mock database: %v", err))
	}
	return connector, mock
}


type TestDriver struct {
	DBExists   bool
	RoleExists bool
	DB         *sqlx.DB
	DBName     string
	User       string
}

func (driver TestDriver) Connect(driverName string, dataSourceName string) (*sqlx.DB, error) {
	if driver.DBExists && driver.RoleExists {
		return driver.DB, nil
	} else if driver.DBExists {
		return nil, fmt.Errorf("pq: role \"%s\" does not exist", driver.User)
	}
	return nil, fmt.Errorf("pq: database \"%s\" does not exist", driver.DBName)
}
