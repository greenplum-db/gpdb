package db

/*
 * This file contains structs and functions related to connecting to a database
 * and executing queries.
 */

import (
	"fmt"
	"strconv"
	"strings"

	"gp_upgrade/utils"

	"github.com/jmoiron/sqlx"
)

type Driver interface {
	Connect(driverName string, dataSourceName string) (*sqlx.DB, error)
}

type RealGPDBDriver struct {
}

func (driver RealGPDBDriver) Connect(driverName string, dataSourceName string) (*sqlx.DB, error) {
	return sqlx.Connect(driverName, dataSourceName)
}

type Connector interface {
	Connect() error
	Close()
	GetConn() *sqlx.DB
}

type GPDBConnector struct {
	conn   *sqlx.DB
	user   string
	dbName string
	host   string
	port   int
	driver Driver // used for testing
}

func NewDBConn(masterHost string, masterPort int, dbname string) Connector {
	currentUser, _, _ := utils.GetUser()
	username := utils.TryEnv("PGUSER", currentUser)
	if dbname == "" {
		dbname = utils.TryEnv("PGDATABASE", "")
	}
	hostname, _ := utils.GetHost()
	if masterHost == "" {
		masterHost = utils.TryEnv("PGHOST", hostname)
	}
	if masterPort == 0 {
		masterPort, _ = strconv.Atoi(utils.TryEnv("PGPORT", "15432"))
	}

	return &GPDBConnector{
		conn:   nil,
		driver: RealGPDBDriver{},
		user:   username,
		dbName: dbname,
		host:   masterHost,
		port:   masterPort,
	}
}

/*
 * Wrapper functions for built-in sqlx and database/sql functionality; they will
 * automatically execute the query as part of an existing transaction if one is
 * in progress, to ensure that the whole backup process occurs in one transaction
 * without requiring that to be ensured at the call site.
 */

func (dbconn *GPDBConnector) Connect() error {
	dbname := escapeDBName(dbconn.dbName)
	connStr := fmt.Sprintf(`user=%s dbname='%s' host=%s port=%d sslmode=disable`,
		dbconn.user, dbname, dbconn.host, dbconn.port)

	var err error
	dbconn.conn, err = dbconn.driver.Connect("postgres", connStr)
	return err
}

func (dbconn *GPDBConnector) Close() {
	if dbconn.conn != nil {
		dbconn.conn.Close()
	}
}

func (dbconn *GPDBConnector) GetConn() *sqlx.DB {
	return dbconn.conn
}

/*
 * Other useful/helper functions involving GPDBConnector
 */

func escapeDBName(dbname string) string {
	dbname = strings.Replace(dbname, `\`, `\\`, -1)
	dbname = strings.Replace(dbname, `'`, `\'`, -1)
	return dbname
}
