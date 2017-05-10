package utils

import (
	"errors"
	"fmt"

	"github.com/jmoiron/sqlx"
)

type DBDriver interface {
	Connect(driverName string, dataSourceName string) (*sqlx.DB, error)
}

type GPDBDriver struct {
}

type TestDriver struct {
	DBExists bool
	DB       *sqlx.DB
	DBName   string
}

type TestResult struct {
	Rows int64
}

func (driver GPDBDriver) Connect(driverName string, dataSourceName string) (*sqlx.DB, error) {
	return sqlx.Connect(driverName, dataSourceName)
}

func (driver TestDriver) Connect(driverName string, dataSourceName string) (*sqlx.DB, error) {
	if driver.DBExists {
		return driver.DB, nil
	} else {
		return nil, errors.New(fmt.Sprintf("Database %s does not exist", driver.DBName))
	}
}

func (result TestResult) LastInsertId() (int64, error) {
	return 0, nil
}

func (result TestResult) RowsAffected() (int64, error) {
	return result.Rows, nil
}
