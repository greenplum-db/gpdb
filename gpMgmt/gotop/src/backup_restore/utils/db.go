package utils

import (
	"database/sql"
	"fmt"
	"os"
	"os/user"
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

type Table struct {
	Schema string
	Table  string
}

func NewDBConn(dbname string) *DBConn {
	username := ""
	host := ""
	port := 0

	default_user, _ := user.Current()
	default_host, _ := os.Hostname()
	username = TryEnv("PGUSER", default_user.Username)
	if dbname == "" {
		dbname = TryEnv("PGDATABASE", "")
	}
	if dbname == "" {
		Abort("No database provided and PGDATABASE not set")
	}
	host = TryEnv("PGHOST", default_host)
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

func (dbconn *DBConn) Begin() {
	if dbconn.Tx != nil {
		Abort("Cannot begin transation; there is already a transaction in progress")
	}
	var err error
	dbconn.Tx, err = dbconn.Conn.Beginx()
	CheckError(err)
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
		Abort("Cannot commit transation; there is no transaction in progress")
	}
	var err error
	err = dbconn.Tx.Commit()
	CheckError(err)
	dbconn.Tx = nil
}

func (dbconn *DBConn) Connect() {
	connStr := fmt.Sprintf("user=%s dbname=%s host=%s port=%d sslmode=disable", dbconn.User, dbconn.DBName, dbconn.Host, dbconn.Port)
	var err error
	dbconn.Conn, err = dbconn.Driver.Connect("postgres", connStr)
	if err != nil && strings.Contains(err.Error(), "does not exist") {
		Abort("Database %s does not exist, exiting", dbconn.DBName)
	}
	CheckError(err)
}

func (dbconn *DBConn) Exec(query string) (sql.Result, error) {
	if dbconn.Tx != nil {
		return dbconn.Tx.Exec(query)
	}
	return dbconn.Conn.Exec(query)
}

func (dbconn *DBConn) Get(dest interface{}, query string) error {
	if dbconn.Tx != nil {
		return dbconn.Tx.Get(dest, query)
	}
	return dbconn.Conn.Get(dest, query)
}

func (dbconn *DBConn) GetDBSize() string {
	size := struct{DBSize string}{}
	sizeQuery := fmt.Sprintf("SELECT pg_size_pretty(sodddatsize) as dbsize FROM gp_toolkit.gp_size_of_database WHERE sodddatname='%s'", dbconn.DBName)
	err := dbconn.Get(&size, sizeQuery)
	CheckError(err)
	return size.DBSize
}

func (dbconn *DBConn) Select(dest interface{}, query string) error {
	if dbconn.Tx != nil {
		return dbconn.Tx.Select(dest, query)
	}
	return dbconn.Conn.Select(dest, query)
}
