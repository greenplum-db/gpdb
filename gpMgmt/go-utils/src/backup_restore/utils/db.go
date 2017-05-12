package utils

/*
 * This file contains structs and functions related to connecting to a database
 * and executing queries.
 */

import (
	"database/sql"
	"fmt"
	"strconv"
	"strings"

	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
)

type DBConn struct {
	Conn   *sqlx.DB
	Driver DBDriver
	User   string
	DBName string
	Host   string
	Port   int
	Tx     *sqlx.Tx
}

func NewDBConn(dbname string) *DBConn {
	username := ""
	host := ""
	port := 0

	currentUser, _, currentHost := GetUserAndHostInfo()
	username = TryEnv("PGUSER", currentUser)
	if dbname == "" {
		dbname = TryEnv("PGDATABASE", "")
	}
	if dbname == "" {
		logger.Fatal("No database provided and PGDATABASE not set")
	}
	host = TryEnv("PGHOST", currentHost)
	port, _ = strconv.Atoi(TryEnv("PGPORT", "5432"))

	return &DBConn{
		Conn:   nil,
		Driver: GPDBDriver{},
		User:   username,
		DBName: dbname,
		Host:   host,
		Port:   port,
		Tx:     nil,
	}
}

/*
 * Wrapper functions for built-in sqlx and database/sql functionality; they will
 * automatically execute the query as part of an existing transaction if one is
 * in progress, to ensure that the whole backup process occurs in one transaction
 * without requiring that to be ensured at the call site.
 */

func (dbconn *DBConn) Begin() {
	if dbconn.Tx != nil {
		logger.Fatal("Cannot begin transaction; there is already a transaction in progress")
	}
	var err error
	dbconn.Tx, err = dbconn.Conn.Beginx()
	CheckError(err)
	/*
	 * This uses a SERIALIZABLE transaction so the backup can effectively take a
	 * "snapshot" of the database via MVCC, to keep backups consistent without
	 * requiring a pg_class lock.
	 */
	_, err = dbconn.Exec("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE")
	CheckError(err)
}

func (dbconn *DBConn) Close() {
	if dbconn.Conn != nil {
		dbconn.Conn.Close()
	}
}

func (dbconn *DBConn) Commit() {
	if dbconn.Tx == nil {
		logger.Fatal("Cannot commit transaction; there is no transaction in progress")
	}
	var err error
	err = dbconn.Tx.Commit()
	CheckError(err)
	dbconn.Tx = nil
}

func (dbconn *DBConn) Connect() {
	dbname := escapeDBName(dbconn.DBName)
	connStr := fmt.Sprintf(`user=%s dbname='%s' host=%s port=%d sslmode=disable`, dbconn.User, dbname, dbconn.Host, dbconn.Port)
	var err error
	dbconn.Conn, err = dbconn.Driver.Connect("postgres", connStr)
	if err != nil {
		if strings.Contains(err.Error(), "does not exist") {
			logger.Fatal("Database %s does not exist, exiting", dbconn.DBName)
		}
		if strings.Contains(err.Error(), "connection refused") {
			logger.Fatal(`could not connect to server: Connection refused
	Is the server running on host "%s" and accepting
	TCP/IP connections on port %d?`, dbconn.Host, dbconn.Port)
		}
	}
	CheckError(err)
}

func (dbconn *DBConn) Exec(query string) (sql.Result, error) {
	if dbconn.Tx != nil {
		return dbconn.Tx.Exec(query)
	}
	return dbconn.Conn.Exec(query)
}

func (dbconn *DBConn) Get(destination interface{}, query string) error {
	if dbconn.Tx != nil {
		return dbconn.Tx.Get(destination, query)
	}
	return dbconn.Conn.Get(destination, query)
}

func (dbconn *DBConn) Select(destination interface{}, query string) error {
	if dbconn.Tx != nil {
		return dbconn.Tx.Select(destination, query)
	}
	return dbconn.Conn.Select(destination, query)
}

/*
 * Other useful/helper functions involving DBConn
 */

func escapeDBName(dbname string) string {
	dbname = strings.Replace(dbname, `\`, `\\`, -1)
	dbname = strings.Replace(dbname, `'`, `\'`, -1)
	return dbname
}

func (dbconn *DBConn) GetDBSize() string {
	size := struct{ DBSize string }{}
	sizeQuery := fmt.Sprintf("SELECT pg_size_pretty(sodddatsize) as dbsize FROM gp_toolkit.gp_size_of_database WHERE sodddatname=E'%s'", escapeDBName(dbconn.DBName))
	err := dbconn.Get(&size, sizeQuery)
	CheckError(err)
	return size.DBSize
}
