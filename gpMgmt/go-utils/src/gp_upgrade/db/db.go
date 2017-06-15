package db

/*
 * This file contains structs and functions related to connecting to a database
 * and executing queries.
 */

import (
	"fmt"
	"strconv"
	"strings"

	"gpbackup/utils"

	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
)

type DBDriver interface {
	Connect(driverName string, dataSourceName string) (*sqlx.DB, error)
}

type GPDBDriver struct {
}

func (driver GPDBDriver) Connect(driverName string, dataSourceName string) (*sqlx.DB, error) {
	return sqlx.Connect(driverName, dataSourceName)
}

type DBConn struct {
	Conn   *sqlx.DB
	Driver DBDriver
	User   string
	DBName string
	Host   string
	Port   int
	Tx     *sqlx.Tx
}

func NewDBConn(masterHost string, masterPort int, dbname string) *DBConn {
	username := ""

	currentUser, _, currentHost := utils.GetUserAndHostInfo()
	username = utils.TryEnv("PGUSER", currentUser)
	if dbname == "" {
		dbname = utils.TryEnv("PGDATABASE", "")
	}
	if masterHost == "" {
		masterHost = utils.TryEnv("PGHOST", currentHost)
	}
	if masterPort == 0 {
		masterPort, _ = strconv.Atoi(utils.TryEnv("PGPORT", "15432"))
	}

	return &DBConn{
		Conn:   nil,
		Driver: GPDBDriver{},
		User:   username,
		DBName: dbname,
		Host:   masterHost,
		Port:   masterPort,
		Tx:     nil,
	}
}

/*
 * Wrapper functions for built-in sqlx and database/sql functionality; they will
 * automatically execute the query as part of an existing transaction if one is
 * in progress, to ensure that the whole backup process occurs in one transaction
 * without requiring that to be ensured at the call site.
 */

func (dbconn *DBConn) Close() {
	if dbconn.Conn != nil {
		dbconn.Conn.Close()
	}
}

func (dbconn *DBConn) Connect() error {
	dbname := escapeDBName(dbconn.DBName)
	connStr := fmt.Sprintf(`user=%s dbname='%s' host=%s port=%d sslmode=disable`, dbconn.User, dbname, dbconn.Host, dbconn.Port)
	var err error
	dbconn.Conn, err = dbconn.Driver.Connect("postgres", connStr)
	return err
}

/*
 * Other useful/helper functions involving DBConn
 */

func escapeDBName(dbname string) string {
	dbname = strings.Replace(dbname, `\`, `\\`, -1)
	dbname = strings.Replace(dbname, `'`, `\'`, -1)
	return dbname
}
